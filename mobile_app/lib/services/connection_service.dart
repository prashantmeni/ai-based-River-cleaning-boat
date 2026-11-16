import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:math';

import 'package:flutter/foundation.dart';
import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';

import '../models/telemetry.dart';
import 'app_settings.dart';

enum LinkStatus { idle, connecting, online, offline, error }

/// Telemetry freshness derived from the last message age.
enum Freshness { waiting, live, stale }

/// Diagnostic command codes (shared/protocol/protocol.h).
class DiagCmd {
  static const stop = 1;
  static const forward = 2;
  static const turnLeft = 3;
  static const turnRight = 4;
  static const conveyorOn = 5;
  static const conveyorOff = 6;
  static const autonomousOn = 7;
  static const autonomousOff = 8;
  static const camFlash = 9;
}

/// Individual connection link statuses for the dashboard.
class LinkState {
  final String name;
  LinkStatus status;
  String? detail;
  DateTime? lastSeen;

  LinkState({required this.name, this.status = LinkStatus.idle, this.detail, this.lastSeen});
}

/// Circular debug/serial log buffer.
class DebugLog {
  static const int _maxLines = 500;
  final List<String> _lines = [];

  void add(String message) {
    final ts = DateTime.now().toString().substring(11, 19);
    _lines.add('[$ts] $message');
    if (_lines.length > _maxLines) _lines.removeRange(0, _lines.length - _maxLines);
  }

  List<String> get lines => _lines.reversed.toList();
  int get count => _lines.length;
}

/// Connection service — one endpoint, two transports, auto-selected exactly
/// like the dashboard (`transportForUrl()` in connectionManager.ts):
/// - URL containing `broker.hivemq.com` or `/mqtt` -> cloud MQTT over WSS,
///   telemetry subscribed on [telemetryTopic], commands published as plain
///   `CMD:` frames.
/// - any other `ws://`/`wss://` endpoint -> direct WebSocket to the boat's
///   own gateway (ws://192.168.4.1:81, no internet; join the RiverClean-Boat
///   AP or the same router). Same JSON schema in, same `CMD:` frames out.
class ConnectionService extends ChangeNotifier {
  ConnectionService._();
  static final ConnectionService instance = ConnectionService._();

  static const telemetryTopic = 'riverclean/boat/telemetry';
  static const commandTopic = 'riverclean/boat/command';
  static const _liveMs = 3000;

  // Per-link status
  LinkState linkMainInternet = LinkState(name: 'Main → Internet');
  LinkState linkMainCam = LinkState(name: 'Main → CAM (ESP-NOW)');
  LinkState linkMainMqtt = LinkState(name: 'Main → MQTT Broker');

  // Debug log
  final DebugLog debugLog = DebugLog();
  bool get serialLogEnabled => AppSettings.instance.serialLogEnabled;

  LinkStatus status = LinkStatus.idle;
  Telemetry? last;
  DateTime? lastAt;
  String? lastError;
  int telemetryCount = 0;
  int parseErrors = 0;
  String lastCommandSent = '';

  bool flashOn = false; // tracked CAM flash state

  MqttServerClient? _client;
  WebSocket? _ws;
  Timer? _retry;
  Timer? _tick;
  bool _want = false;
  bool _updatesAttached = false;
  String _url = '';
  int _retryBackoff = 3; // seconds, grows on failure
  final Random _rnd = Random();

  String get url => _url.isEmpty ? AppSettings.instance.endpoint : _url;

  static bool transportIsWs(String url) {
    final u = url.trim().toLowerCase();
    if (u.contains('broker.hivemq.com') || u.contains('/mqtt')) return false;
    if (u.startsWith('ws://') || u.startsWith('wss://')) return true;
    return false;
  }

  String get transport => transportIsWs(url) ? 'ws' : 'mqtt';

  Freshness get freshness {
    if (status != LinkStatus.online) return Freshness.waiting;
    final at = lastAt;
    if (at == null) return Freshness.waiting;
    return DateTime.now().difference(at).inMilliseconds < _liveMs
        ? Freshness.live
        : Freshness.stale;
  }

  Duration get lastAge =>
      lastAt == null ? const Duration(hours: 99) : DateTime.now().difference(lastAt!);

  void _log(String msg) {
    debugLog.add(msg);
    notifyListeners();
  }

  Future<void> connect([String? url]) async {
    _url = url ?? AppSettings.instance.endpoint;
    _want = true;
    _retryBackoff = 3;
    _retry?.cancel();
    await _tryConnect();
  }

  Future<void> disconnect() async {
    _want = false;
    _retry?.cancel();
    _tick?.cancel();
    _closeClient();
    status = LinkStatus.idle;
    linkMainMqtt.status = LinkStatus.idle;
    notifyListeners();
  }

  Future<void> _tryConnect() async {
    if (!_want) return;
    _closeClient();
    status = LinkStatus.connecting;
    lastError = null;
    notifyListeners();

    final uri = Uri.tryParse(_url);
    if (uri == null || !(uri.isScheme('ws') || uri.isScheme('wss'))) {
      status = LinkStatus.error;
      lastError = 'Invalid endpoint (need ws:// or wss://)';
      notifyListeners();
      return;
    }

    if (transportIsWs(_url)) {
      await _tryConnectWs();
    } else {
      await _tryConnectMqtt(uri);
    }
  }

  Future<void> _tryConnectWs() async {
    try {
      final ws = await WebSocket.connect(_url).timeout(const Duration(seconds: 10));
      if (!_want) {
        try { ws.close(); } on Exception {}
        return;
      }
      _ws = ws;
      ws.listen(
        (data) {
          if (data is String) _ingest(data);
        },
        onDone: _onWsClosed,
        onError: (Object e) {
          lastError = e.toString();
          _log('WS error: $e');
        },
        cancelOnError: false,
      );
      status = LinkStatus.online;
      linkMainMqtt.status = LinkStatus.online;
      lastError = null;
      _retryBackoff = 3;
      _tick ??= Timer.periodic(const Duration(seconds: 1), (_) {
        if (_want) notifyListeners();
      });
      notifyListeners();
    } on Exception catch (e) {
      if (!_want) return;
      status = LinkStatus.error;
      linkMainMqtt.status = LinkStatus.error;
      lastError = e.toString();
      _log('WS connect failed: $e');
      notifyListeners();
      _scheduleRetry();
    }
  }

  void _onWsClosed() {
    if (_ws == null) return;
    _ws = null;
    if (!_want) return;
    status = LinkStatus.offline;
    linkMainMqtt.status = LinkStatus.offline;
    notifyListeners();
    _scheduleRetry();
  }

  Future<void> _tryConnectMqtt(Uri uri) async {
    final port = uri.hasPort ? uri.port : (uri.isScheme('wss') ? 443 : 80);
    // Unique client ID to prevent broker collisions
    final id = 'RiverCleanApp-${DateTime.now().millisecondsSinceEpoch}-${_rnd.nextInt(0xffffff).toRadixString(16)}';

    final client = MqttServerClient.withPort(_url, id, port);
    client.useWebSocket = true;
    client.logging(on: false);
    client.keepAlivePeriod = 60;
    client.connectionTimeout = 30000;
    client.onConnected = _onConnected;
    client.onDisconnected = _onDisconnected;
    _updatesAttached = false;
    _attachUpdates(client);

    try {
      await client.connect().timeout(const Duration(seconds: 15));
    } on Exception catch (e) {
      if (!_want) return;
      status = LinkStatus.error;
      linkMainMqtt.status = LinkStatus.error;
      lastError = e.toString();
      _log('MQTT connect failed: $e');
      notifyListeners();
      _scheduleRetry();
      return;
    }
    _client = client;
    _retryBackoff = 3;
    _tick ??= Timer.periodic(const Duration(seconds: 1), (_) {
      if (_want) notifyListeners();
    });
  }

  void _attachUpdates(MqttServerClient client) {
    if (_updatesAttached) return;
    final stream = client.updates;
    if (stream == null) return;
    stream.listen((List<MqttReceivedMessage<MqttMessage>> messages) {
      for (final m in messages) {
        final payload = m.payload;
        if (payload is MqttPublishMessage) {
          final text = utf8.decode(payload.payload.message, allowMalformed: true);
          if (m.topic == telemetryTopic) {
            _ingestTelemetry(text);
          } else {
            _log('MQTT ${m.topic}: $text');
          }
        }
      }
    }, onError: (e) {
      _log('MQTT stream error: $e');
    });
    _updatesAttached = true;
  }

  void _ingestTelemetry(String text) {
    try {
      final decoded = jsonDecode(text);
      if (decoded is! Map<String, dynamic>) return;
      if (decoded['type'] != 'telemetry') return;
      last = Telemetry.fromJson(decoded);
      lastAt = DateTime.now();
      telemetryCount++;
      linkMainCam.status = LinkStatus.online;
      linkMainCam.lastSeen = lastAt;
      linkMainCam.detail = 'comms_ok=${decoded['comms_ok']}, state=${decoded['state']}';
      linkMainMqtt.status = LinkStatus.online;
      linkMainMqtt.lastSeen = lastAt;
      notifyListeners();
    } on Exception {
      parseErrors++;
    }
  }

  void _ingest(String text) {
    try {
      final decoded = jsonDecode(text);
      if (decoded is! Map<String, dynamic>) return;
      if (decoded['type'] == 'telemetry') {
        _ingestTelemetry(text);
      } else if (decoded['type'] == 'ack') {
        _log('ACK: ${decoded['id']} status=${decoded['status']} cmd=${decoded['cmd']}');
      } else {
        _log('MSG: $text');
      }
    } on Exception {
      parseErrors++;
    }
  }

  void _onConnected() {
    status = LinkStatus.online;
    linkMainInternet.status = LinkStatus.online;
    linkMainMqtt.status = LinkStatus.online;
    lastError = null;
    _client?.subscribe(telemetryTopic, MqttQos.atMostOnce);
    _client?.subscribe(commandTopic, MqttQos.atMostOnce);
    final c = _client;
    if (c != null) _attachUpdates(c);
    _log('MQTT connected: $url');
    notifyListeners();
  }

  void _onDisconnected() {
    if (!_want) return;
    status = LinkStatus.offline;
    linkMainInternet.status = LinkStatus.offline;
    linkMainMqtt.status = LinkStatus.offline;
    notifyListeners();
    _scheduleRetry();
  }

  void _scheduleRetry() {
    if (!_want) return;
    _retry?.cancel();
    _retry = Timer(Duration(seconds: _retryBackoff), _tryConnect);
    _retryBackoff = (_retryBackoff * 2).clamp(3, 60);
    _log('Reconnecting in $_retryBackoff s...');
  }

  void _closeClient() {
    final c = _client;
    _client = null;
    if (c != null) {
      try { c.disconnect(); } on Exception {}
    }
    final w = _ws;
    _ws = null;
    if (w != null) {
      try { w.close(); } on Exception {}
    }
  }

  /// Send a toggle command (continuous until explicit stop). No duration.
  String? sendToggle(int cmd, {int value = 0}) {
    if (status != LinkStatus.online) return 'Not connected';
    final v = value.clamp(0, 255).toInt();
    final text = 'CMD:$cmd,$v,0';
    return _sendRaw(text);
  }

  /// Send a bounded command with duration.
  String? sendCommand(int cmd, {int value = 0, int durationMs = 0}) {
    if (status != LinkStatus.online) return 'Not connected';
    final v = value.clamp(0, 255).toInt();
    final d = durationMs.clamp(0, 60000).toInt();
    final text = 'CMD:$cmd,$v,$d';
    return _sendRaw(text);
  }

  String? _sendRaw(String text) {
    final w = _ws;
    if (w != null) {
      try {
        w.add(text);
        lastCommandSent = text;
        _log('TX(WS): $text');
        notifyListeners();
        return null;
      } on Exception catch (e) {
        return e.toString();
      }
    }
    final c = _client;
    if (c == null) return 'Not connected';
    final builder = MqttClientPayloadBuilder()..addString(text);
    try {
      c.publishMessage(commandTopic, MqttQos.atMostOnce, builder.payload!);
      lastCommandSent = text;
      _log('TX(MQTT): $text');
      notifyListeners();
      return null;
    } on Exception catch (e) {
      return e.toString();
    }
  }

  /// Publish a raw string payload to an arbitrary MQTT topic.
  String? publishRaw(String topic, String payload) {
    if (status != LinkStatus.online || _client == null) {
      return 'Not connected';
    }
    final builder = MqttClientPayloadBuilder()..addString(payload);
    try {
      _client!.publishMessage(topic, MqttQos.atMostOnce, builder.payload!);
      notifyListeners();
      return null;
    } on Exception catch (e) {
      return e.toString();
    }
  }

  /// Toggle CAM flash via DIAG_CMD_CAM_FLASH (cmd 9).
  String? toggleFlash(bool on) {
    flashOn = on;
    final err = sendToggle(DiagCmd.camFlash, value: on ? 1 : 0);
    if (err == null) {
      _log('CAM flash ${on ? 'ON' : 'OFF'}');
    }
    return err;
  }

  /// Auto-discover the ESP32-CAM MJPEG stream URL by scanning the local
  /// /24 subnet for port 8080.
  Future<String?> discoverCamStreamUrl() async {
    try {
      final interfaces = await NetworkInterface.list();
      for (final iface in interfaces) {
        for (final addr in iface.addresses) {
          if (addr.type == InternetAddressType.IPv4 && !addr.isLoopback) {
            final ip = addr.address;
            final parts = ip.split('.');
            if (parts.length != 4) continue;
            final subnet = '${parts[0]}.${parts[1]}.${parts[2]}.';
            for (int batchStart = 1; batchStart < 255; batchStart += 20) {
              final batchEnd = batchStart + 20 > 255 ? 255 : batchStart + 20;
              final futures = <Future<String?>>[];
              for (int i = batchStart; i < batchEnd; i++) {
                futures.add(_probeStreamPort('$subnet$i'));
              }
              final results = await Future.wait(futures);
              for (final r in results) {
                if (r != null) return r;
              }
            }
          }
        }
      }
    } catch (e) {
      _log('Stream discovery error: $e');
    }
    return null;
  }

  Future<String?> _probeStreamPort(String ip) async {
    try {
      final sock = await Socket.connect(ip, 8080).timeout(const Duration(milliseconds: 300));
      await sock.close();
      return 'http://$ip:8080/stream';
    } on Exception {
      return null;
    }
  }

  @override
  void dispose() {
    disconnect();
    super.dispose();
  }
}
