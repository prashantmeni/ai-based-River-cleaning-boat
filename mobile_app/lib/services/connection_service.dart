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
}

/// Connection service — one endpoint, two transports, auto-selected exactly
/// like the dashboard (`transportForUrl()` in connectionManager.ts):
/// - URL containing `broker.hivemq.com` or `/mqtt` -> cloud MQTT over WSS,
///   telemetry subscribed on [telemetryTopic], commands published as plain
///   `CMD:` frames (the firmware accepts an optional `#id ` prefix but none
///   is required on either path).
/// - any other `ws://`/`wss://` endpoint -> direct WebSocket to the boat's
///   own gateway (ws://192.168.4.1:81, no internet; join the RiverClean-Boat
///   AP or the same router). Same JSON schema in, same `CMD:` frames out.
class ConnectionService extends ChangeNotifier {
  ConnectionService._();
  static final ConnectionService instance = ConnectionService._();

  static const telemetryTopic = 'riverclean/boat/telemetry';
  static const commandTopic = 'riverclean/boat/command';
  static const _liveMs = 3000;

  LinkStatus status = LinkStatus.idle;
  Telemetry? last;
  DateTime? lastAt;
  String? lastError;
  int telemetryCount = 0;
  int parseErrors = 0;
  String lastCommandSent = '';

  MqttServerClient? _client;
  WebSocket? _ws;
  Timer? _retry;
  Timer? _tick;
  bool _want = false;
  bool _updatesAttached = false;
  String _url = '';

  String get url => _url.isEmpty ? AppSettings.instance.endpoint : _url;

  /// True when the endpoint is a direct WebSocket (boat gateway) rather
  /// than a cloud MQTT broker. Mirrors the dashboard's transportForUrl().
  static bool transportIsWs(String url) {
    final u = url.trim().toLowerCase();
    if (u.contains('broker.hivemq.com') || u.contains('/mqtt')) return false;
    if (u.startsWith('ws://') || u.startsWith('wss://')) return true;
    return false;
  }

  /// "ws" for the direct boat gateway, "mqtt" for a broker.
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

  /// Connect (or reconnect with a new URL).
  Future<void> connect([String? url]) async {
    _url = url ?? AppSettings.instance.endpoint;
    _want = true;
    _retry?.cancel();
    await _tryConnect();
  }

  Future<void> disconnect() async {
    _want = false;
    _retry?.cancel();
    _tick?.cancel();
    _closeClient();
    status = LinkStatus.idle;
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
      return;
    }
    await _tryConnectMqtt(uri);
  }

  Future<void> _tryConnectWs() async {
    try {
      final ws = await WebSocket.connect(_url)
          .timeout(const Duration(seconds: 10));
      if (!_want) {
        try {
          ws.close();
        } on Exception {
          // ignore
        }
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
        },
        cancelOnError: false,
      );
      status = LinkStatus.online;
      lastError = null;
      _tick ??= Timer.periodic(const Duration(seconds: 1), (_) {
        if (_want) notifyListeners();
      });
      notifyListeners();
    } on Exception catch (e) {
      if (!_want) return;
      status = LinkStatus.error;
      lastError = e.toString();
      notifyListeners();
      _scheduleRetry();
    }
  }

  void _onWsClosed() {
    if (_ws == null) return;
    _ws = null;
    if (!_want) return;
    status = LinkStatus.offline;
    notifyListeners();
    _scheduleRetry();
  }

  Future<void> _tryConnectMqtt(Uri uri) async {
    final port = uri.hasPort ? uri.port : (uri.isScheme('wss') ? 443 : 80);
    final id =
        'RiverCleanApp-${Random().nextInt(0xffffff).toRadixString(16)}';

    // The WS connection parses the server string as a URI and requires a
    // ws/wss scheme, so pass the full URL here.
    final client = MqttServerClient.withPort(_url, id, port);
    client.useWebSocket = true;
    client.logging(on: false);
    client.keepAlivePeriod = 60;
    client.connectionTimeout = 30000; // 30 seconds in milliseconds
    client.onConnected = _onConnected;
    client.onDisconnected = _onDisconnected;
    _updatesAttached = false;
    _attachUpdates(client);

    try {
      await client.connect().timeout(const Duration(seconds: 15));
    } on Exception catch (e) {
      if (!_want) return;
      lastError = e.toString();
      status = LinkStatus.error;
      try {
        client.disconnect();
      } on Exception {
        // ignore
      }
      notifyListeners();
      _scheduleRetry();
      return;
    }
    _client = client;
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
          final text =
              utf8.decode(payload.payload.message, allowMalformed: true);
          _ingest(text);
        }
      }
    });
    _updatesAttached = true;
  }

  void _onConnected() {
    status = LinkStatus.online;
    lastError = null;
    _client?.subscribe(telemetryTopic, MqttQos.atMostOnce);
    final c = _client;
    if (c != null) _attachUpdates(c);
    notifyListeners();
  }

  void _onDisconnected() {
    if (!_want) return;
    status = LinkStatus.offline;
    notifyListeners();
    _scheduleRetry();
  }

  void _scheduleRetry() {
    if (!_want) return;
    _retry?.cancel();
    _retry = Timer(const Duration(seconds: 3), _tryConnect);
  }

  void _closeClient() {
    final c = _client;
    _client = null;
    if (c != null) {
      try {
        c.disconnect();
      } on Exception {
        // ignore
      }
    }
    final w = _ws;
    _ws = null;
    if (w != null) {
      try {
        w.close();
      } on Exception {
        // ignore
      }
    }
  }

  void _ingest(String text) {
    try {
      final decoded = jsonDecode(text);
      if (decoded is! Map<String, dynamic>) return;
      if (decoded['type'] != 'telemetry') return;
      last = Telemetry.fromJson(decoded);
      lastAt = DateTime.now();
      telemetryCount++;
      notifyListeners();
    } on Exception {
      parseErrors++;
    }
  }

  /// Send a bounded command. Returns null on success, or an error string.
  /// Payload is a plain `CMD:cmd,value,duration` frame — accepted by the
  /// firmware MQTT callback and the WebSocket gateway alike.
  String? sendCommand(int cmd, {int value = 0, int durationMs = 0}) {
    if (status != LinkStatus.online) {
      return 'Not connected';
    }
    final v = value.clamp(0, 255).toInt();
    final d = durationMs.clamp(0, 60000).toInt();
    final text = 'CMD:$cmd,$v,$d';

    final w = _ws;
    if (w != null) {
      try {
        w.add(text);
        lastCommandSent = text;
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
      c.publishMessage(
        commandTopic,
        MqttQos.atMostOnce,
        builder.payload!,
      );
      lastCommandSent = text;
      notifyListeners();
      return null;
    } on Exception catch (e) {
      return e.toString();
    }
  }

  @override
  void dispose() {
    disconnect();
    super.dispose();
  }
}
