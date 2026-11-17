import 'package:flutter/material.dart';
import 'dart:convert';
import 'package:flutter_bluetooth_serial/flutter_bluetooth_serial.dart';

import '../services/app_settings.dart';
import '../services/connection_service.dart';

class SettingsScreen extends StatefulWidget {
  const SettingsScreen({super.key});

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  final _endpointCtrl = TextEditingController();
  final _streamCtrl = TextEditingController();
  final _ssidCtrl = TextEditingController();
  final _passCtrl = TextEditingController();
  double _pwm = 150;
  bool _loaded = false;
  String _savedMsg = '';
  bool _streamSaved = false;
  bool _obscurePass = true;
  bool _provisioning = false;
  bool _discovering = false;
  BluetoothDevice? _selectedDevice;
  List<BluetoothDiscoveryResult> _discoveredDevices = [];

  @override
  void initState() {
    super.initState();
    final s = AppSettings.instance;
    _endpointCtrl.text = s.endpoint;
    _streamCtrl.text = s.cameraStreamUrl;
    _ssidCtrl.text = s.wifiSsid;
    _passCtrl.text = s.wifiPassword;
    _pwm = s.manualPwm.toDouble();
    _loaded = true;
  }

  @override
  void dispose() {
    _endpointCtrl.dispose();
    _streamCtrl.dispose();
    _ssidCtrl.dispose();
    _passCtrl.dispose();
    super.dispose();
  }

  Future<void> _saveStreamUrl() async {
    final s = AppSettings.instance;
    s.cameraStreamUrl = _streamCtrl.text.trim();
    await s.save();
    setState(() => _streamSaved = true);
    await Future<void>.delayed(const Duration(seconds: 2));
    if (mounted) setState(() => _streamSaved = false);
  }

  Future<void> _save() async {
    final s = AppSettings.instance;
    s.endpoint = _endpointCtrl.text.trim();
    s.cameraStreamUrl = _streamCtrl.text.trim();
    s.wifiSsid = _ssidCtrl.text.trim();
    s.wifiPassword = _passCtrl.text;
    s.manualPwm = _pwm.round();
    await s.save();
    final svc = ConnectionService.instance;
    svc.connect(_endpointCtrl.text.trim());
    setState(() => _savedMsg = 'Settings saved & reconnecting…');
    await Future<void>.delayed(const Duration(seconds: 2));
    if (mounted) setState(() => _savedMsg = '');
  }

  Future<void> _provisionWifi() async {
    final s = AppSettings.instance;
    if (s.wifiSsid.isEmpty) {
      setState(() => _savedMsg = 'Enter a Wi-Fi SSID first.');
      await Future<void>.delayed(const Duration(seconds: 2));
      if (mounted) setState(() => _savedMsg = '');
      return;
    }
    setState(() => _provisioning = true);
    final payload = jsonEncode({
      'ssid': s.wifiSsid,
      'password': s.wifiPassword,
    });
    final svc = ConnectionService.instance;
    final err = svc.publishRaw('riverclean/boat/wifi_config', payload);
    if (err != null) {
      setState(() => _savedMsg = 'Provisioning failed: $err');
    } else {
      setState(() => _savedMsg = 'Wi-Fi config sent to both boards.');
    }
    setState(() => _provisioning = false);
    await Future<void>.delayed(const Duration(seconds: 2));
    if (mounted) setState(() => _savedMsg = '');
  }

  Future<void> _discoverStream() async {
    setState(() => _discovering = true);
    final found = await ConnectionService.instance.discoverCamStreamUrl();
    if (found != null && mounted) {
      setState(() {
        _streamCtrl.text = found;
        _savedMsg = 'CAM stream found: $found';
      });
    } else if (mounted) {
      setState(() => _savedMsg = 'No CAM stream found on the local network.');
    }
    setState(() => _discovering = false);
    await Future<void>.delayed(const Duration(seconds: 2));
    if (mounted) setState(() => _savedMsg = '');
  }

  Future<void> _scanBluetooth() async {
    final svc = ConnectionService.instance;
    setState(() => _discovering = true);
    _discoveredDevices = [];
    setState(() {});
    final devices = await svc.scanBluetooth();
    setState(() {
      _discoveredDevices = devices;
      _discovering = false;
    });
    if (devices.isEmpty && mounted) {
      setState(() => _savedMsg = 'No Bluetooth devices found.');
    } else if (mounted) {
      setState(() => _savedMsg = 'Found ${devices.length} device(s).');
    }
    await Future<void>.delayed(const Duration(seconds: 2));
    if (mounted) setState(() => _savedMsg = '');
  }

  Future<void> _reset() async {
    await AppSettings.instance.reset();
    final s = AppSettings.instance;
    _endpointCtrl.text = s.endpoint;
    _streamCtrl.text = s.cameraStreamUrl;
    _ssidCtrl.text = s.wifiSsid;
    _passCtrl.text = s.wifiPassword;
    setState(() {
      _pwm = s.manualPwm.toDouble();
      _savedMsg = 'Defaults restored.';
    });
    await ConnectionService.instance.connect(s.endpoint);
  }

  @override
  Widget build(BuildContext context) {
    if (!_loaded) return const SizedBox.shrink();
    final scheme = Theme.of(context).colorScheme;
    final conn = ConnectionService.instance;

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        // Connection Endpoint Card
        _modernCard(
          context,
          title: 'COMMUNICATION ENDPOINT',
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              TextField(
                controller: _endpointCtrl,
                decoration: const InputDecoration(
                  border: OutlineInputBorder(),
                  labelText: 'Endpoint URL',
                  hintText: 'wss://broker.hivemq.com:8884/mqtt',
                  helperText: 'Auto-detects Cloud MQTT (/mqtt) or Direct Wi-Fi (ws://192.168.4.1:81)',
                  helperMaxLines: 2,
                ),
                style: const TextStyle(fontSize: 13, fontFamily: 'monospace'),
              ),
              const SizedBox(height: 12),
              Row(
                children: [
                  Expanded(
                    child: ElevatedButton.icon(
                      onPressed: () {
                        conn.disconnect();
                        conn.connect(_endpointCtrl.text.trim());
                        setState(() => _savedMsg = 'Reconnecting…');
                      },
                      icon: const Icon(Icons.link, size: 16),
                      label: const Text('Connect Now'),
                    ),
                  ),
                  const SizedBox(width: 10),
                  Expanded(
                    child: OutlinedButton.icon(
                      onPressed: () {
                        conn.disconnect();
                        setState(() => _savedMsg = 'Disconnected.');
                      },
                      icon: const Icon(Icons.link_off, size: 16),
                      label: const Text('Disconnect'),
                    ),
                  ),
                ],
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),

        // Camera Stream URL Card
        _modernCard(
          context,
          title: 'AI CAMERA MJPEG STREAM',
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const Text(
                'HTTP MJPEG stream URL from the ESP32-CAM /stream endpoint. '
                'Example: http://192.168.4.100:8080/stream. '
                'The phone must be on the same network as the CAM.',
                style: TextStyle(fontSize: 12, color: Colors.grey),
              ),
              const SizedBox(height: 12),
              TextField(
                controller: _streamCtrl,
                decoration: const InputDecoration(
                  border: OutlineInputBorder(),
                  labelText: 'Camera Stream URL',
                  hintText: 'http://192.168.4.100:8080/stream',
                  helperText: 'Leave blank to hide the live video view',
                  helperMaxLines: 2,
                ),
                style: const TextStyle(fontSize: 13, fontFamily: 'monospace'),
              ),
              const SizedBox(height: 10),
              Row(
                children: [
                  Expanded(
                    child: ElevatedButton.icon(
                      onPressed: _saveStreamUrl,
                      icon: const Icon(Icons.save, size: 16),
                      label: const Text('Apply Stream URL'),
                    ),
                  ),
                  const SizedBox(width: 10),
                  Expanded(
                    child: OutlinedButton.icon(
                      onPressed: _discovering ? null : _discoverStream,
                      icon: _discovering
                          ? const SizedBox(
                              width: 16, height: 16,
                              child: CircularProgressIndicator(strokeWidth: 2),
                            )
                          : const Icon(Icons.wifi, size: 16),
                      label: Text(_discovering ? 'Detecting…' : 'Auto-Detect CAM Stream'),
                    ),
                  ),
                ],
              ),
              if (_streamSaved)
                const Padding(
                  padding: EdgeInsets.only(top: 6),
                  child: Text(
                    'Stream URL saved.',
                    style: TextStyle(fontSize: 11, color: Colors.greenAccent, fontFamily: 'monospace'),
                  ),
                ),
            ],
          ),
        ),
        const SizedBox(height: 16),

        // Wi-Fi Network Provisioning Card (STA)
        _modernCard(
          context,
          title: 'BOAT Wi-Fi NETWORK PROVISIONING (STA)',
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const Text(
                'Configure the target Wi-Fi network that the boat controller connects to automatically upon boot. '
                'Sent to both main controller and ESP32-CAM over MQTT.',
                style: TextStyle(fontSize: 12, color: Colors.grey),
              ),
              const SizedBox(height: 12),
              TextField(
                controller: _ssidCtrl,
                decoration: const InputDecoration(
                  border: OutlineInputBorder(),
                  labelText: 'Network SSID (Wi-Fi Name)',
                  prefixIcon: Icon(Icons.wifi),
                ),
                style: const TextStyle(fontSize: 13, fontFamily: 'monospace'),
              ),
              const SizedBox(height: 12),
              TextField(
                controller: _passCtrl,
                obscureText: _obscurePass,
                decoration: InputDecoration(
                  border: const OutlineInputBorder(),
                  labelText: 'Wi-Fi Password',
                  prefixIcon: const Icon(Icons.lock_outline),
                  suffixIcon: IconButton(
                    icon: Icon(_obscurePass ? Icons.visibility : Icons.visibility_off),
                    onPressed: () => setState(() => _obscurePass = !_obscurePass),
                  ),
                ),
                style: const TextStyle(fontSize: 13, fontFamily: 'monospace'),
              ),
              const SizedBox(height: 10),
              Row(
                children: [
                  Expanded(
                    child: ElevatedButton.icon(
                      onPressed: _provisioning ? null : _provisionWifi,
                      icon: _provisioning
                          ? const SizedBox(
                              width: 16, height: 16,
                              child: CircularProgressIndicator(strokeWidth: 2),
                            )
                          : const Icon(Icons.router, size: 16),
                      label: Text(_provisioning ? 'Provisioning…' : 'Provision Wi-Fi to Both Boards'),
                    ),
                  ),
                  const SizedBox(width: 10),
                  Expanded(
                    child: OutlinedButton.icon(
                      onPressed: () {
                        setState(() => _savedMsg = 'Credentials saved locally.');
                      },
                      icon: const Icon(Icons.save, size: 16),
                      label: const Text('Save Locally'),
                    ),
                  ),
                ],
              ),
              if (_savedMsg.isNotEmpty)
                Padding(
                  padding: const EdgeInsets.only(top: 6),
                  child: Text(
                    _savedMsg,
                    style: TextStyle(fontSize: 11, color: Colors.greenAccent, fontFamily: 'monospace'),
                  ),
                ),
            ],
          ),
        ),
        const SizedBox(height: 16),

        // Camera Flash Settings Card
        _modernCard(
          context,
          title: 'CAMERA FLASHLIGHT (GPIO 4)',
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                children: [
                  Expanded(
                    child: SwitchListTile(
                      title: const Text('Auto-Flash On Waste Detection'),
                      subtitle: const Text('Turns flash on when waste is detected at night'),
                      value: AppSettings.instance.flashAuto,
                      onChanged: (v) async {
                        final s = AppSettings.instance;
                        s.flashAuto = v;
                        await s.save();
                        setState(() {});
                      },
                      controlAffinity: ListTileControlAffinity.leading,
                      contentPadding: EdgeInsets.zero,
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 8),
              Row(
                children: [
                  Expanded(
                    child: ElevatedButton.icon(
                      onPressed: () async {
                        final svc = ConnectionService.instance;
                        final st = AppSettings.instance;
                        final err = svc.toggleFlash(!st.flashManual);
                        if (err == null) {
                          st.flashManual = !st.flashManual;
                          await st.save();
                          setState(() {});
                        }
                      },
                      icon: Icon(AppSettings.instance.flashManual ? Icons.highlight : Icons.flash_off),
                      label: Text(AppSettings.instance.flashManual ? 'Flash ON' : 'Flash OFF'),
                    ),
                  ),
                ],
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),

        // Bluetooth Device Selection Card
        _modernCard(
          context,
          title: 'BLUETOOTH DEVICE (ESP32 SPP)',
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const Text(
                'Pair your phone with the ESP32-CAM (device name: RiverClean-Boat) '
                'in your phone Bluetooth settings, then select it here.',
                style: TextStyle(fontSize: 12, color: Colors.grey),
              ),
              const SizedBox(height: 12),
              Row(
                children: [
                  Expanded(
                    child: ElevatedButton.icon(
                      onPressed: _discovering ? null : _scanBluetooth,
                      icon: _discovering
                          ? const SizedBox(
                              width: 16, height: 16,
                              child: CircularProgressIndicator(strokeWidth: 2),
                            )
                          : const Icon(Icons.bluetooth, size: 16),
                      label: Text(_discovering ? 'Scanning…' : 'Scan for ESP32-CAM'),
                    ),
                  ),
                  const SizedBox(width: 10),
                  Expanded(
                    child: OutlinedButton.icon(
                      onPressed: _selectedDevice == null ? null : () async {
                        await ConnectionService.instance.selectDevice(_selectedDevice!);
                        setState(() => _savedMsg = 'Bluetooth device saved.');
                      },
                      icon: const Icon(Icons.device_hub, size: 16),
                      label: const Text('Connect Selected'),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 10),
              if (_discoveredDevices.isNotEmpty) ...[
                const Text(
                  'Available Devices:',
                  style: TextStyle(fontSize: 11, color: Colors.grey),
                ),
                const SizedBox(height: 6),
                ..._discoveredDevices.map((d) {
                  return ListTile(
                    leading: const Icon(Icons.devices, color: Colors.cyanAccent),
                    title: Text(d.device.name ?? 'Unknown'),
                    subtitle: Text(d.device.address),
                    trailing: _selectedDevice?.address == d.device.address
                        ? const Icon(Icons.check, color: Colors.greenAccent)
                        : null,
                    onTap: () => setState(() => _selectedDevice = d.device),
                    contentPadding: EdgeInsets.zero,
                  );
                }).toList(),
              ],
              if (_selectedDevice != null)
                Padding(
                  padding: const EdgeInsets.only(top: 6),
                  child: Text(
                    'Selected: ${_selectedDevice!.name ?? _selectedDevice!.address}',
                    style: TextStyle(fontSize: 11, color: Colors.greenAccent, fontFamily: 'monospace'),
                  ),
                ),
              if (_savedMsg.isNotEmpty)
                Padding(
                  padding: const EdgeInsets.only(top: 6),
                  child: Text(
                    _savedMsg,
                    style: TextStyle(fontSize: 11, color: Colors.greenAccent, fontFamily: 'monospace'),
                  ),
                ),
            ],
          ),
        ),
        const SizedBox(height: 16),

        // Map Provider Card
        _modernCard(
          context,
          title: 'MAP PROVIDER (GPS RADAR)',
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const Text(
                'OpenStreetMap (OSM) is a free, open-source map provider requiring no API key. '
                'Map tiles are downloaded over the internet when the boat has GPS fix.',
                style: TextStyle(fontSize: 12, color: Colors.grey),
              ),
              const SizedBox(height: 10),
              ListTile(
                leading: const Icon(Icons.map),
                title: const Text('OpenStreetMap'),
                trailing: AppSettings.instance.mapProvider == 'osm'
                    ? const Icon(Icons.check, color: Colors.greenAccent)
                    : null,
                onTap: () async {
                  final s = AppSettings.instance;
                  s.mapProvider = 'osm';
                  await s.save();
                  setState(() {});
                },
                contentPadding: EdgeInsets.zero,
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),

        // Serial Monitor Widget
        _modernCard(
          context,
          title: 'SERIAL / DEBUG LOG',
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  const Text(
                    'Live debug messages from connection service',
                    style: TextStyle(fontSize: 12, color: Colors.grey),
                  ),
                  IconButton(
                    icon: const Icon(Icons.clear, size: 18),
                    onPressed: () {
                      ConnectionService.instance.debugLog;
                      setState(() {});
                    },
                  ),
                ],
              ),
              const SizedBox(height: 8),
              Container(
                height: 200,
                decoration: BoxDecoration(
                  color: const Color(0xFF090D16),
                  borderRadius: BorderRadius.circular(10),
                  border: Border.all(color: Colors.grey.withValues(alpha: 0.3)),
                ),
                child: ConnectionService.instance.debugLog.lines.isEmpty
                    ? const Center(
                        child: Text(
                          'No debug messages yet.',
                          style: TextStyle(fontSize: 11, color: Colors.grey, fontFamily: 'monospace'),
                        ),
                      )
                    : ListView.builder(
                        itemCount: ConnectionService.instance.debugLog.lines.length,
                        itemBuilder: (_, i) {
                          final line = ConnectionService.instance.debugLog.lines[i];
                          return Padding(
                            padding: const EdgeInsets.symmetric(vertical: 1),
                            child: Text(
                              line,
                              style: const TextStyle(
                                fontSize: 9,
                                fontFamily: 'monospace',
                                color: Colors.greenAccent,
                              ),
                            ),
                          );
                        },
                      ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),

        // Drive Parameters Card
        _modernCard(
          context,
          title: 'DRIVE & DURATION DEFAULTS',
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  const Text('Default Throttle PWM', style: TextStyle(fontSize: 12, color: Colors.grey)),
                  Text('${_pwm.round()} / 255', style: const TextStyle(fontSize: 12, fontFamily: 'monospace', fontWeight: FontWeight.bold)),
                ],
              ),
              Slider(
                value: _pwm,
                min: 40,
                max: 255,
                divisions: 215,
                label: '${_pwm.round()}',
                onChanged: (v) => setState(() => _pwm = v),
              ),
              const SizedBox(height: 10),
              Text(
                'Manual commands in Drive run continuously until STOP is pressed.',
                style: TextStyle(fontSize: 11, color: scheme.onSurfaceVariant, fontStyle: FontStyle.italic),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),

        // Action Buttons (Save & Reset)
        Row(
          children: [
            Expanded(
              child: ElevatedButton(
                style: ElevatedButton.styleFrom(
                  backgroundColor: scheme.primary,
                  foregroundColor: scheme.onPrimary,
                  padding: const EdgeInsets.symmetric(vertical: 14),
                ),
                onPressed: _save,
                child: const Text('Save Changes', style: TextStyle(fontWeight: FontWeight.bold)),
              ),
            ),
            const SizedBox(width: 10),
            Expanded(
              child: OutlinedButton(
                style: OutlinedButton.styleFrom(
                  padding: const EdgeInsets.symmetric(vertical: 14),
                ),
                onPressed: _reset,
                child: const Text('Reset Defaults'),
              ),
            ),
          ],
        ),

        if (_savedMsg.isNotEmpty) ...[
          const SizedBox(height: 12),
          Center(
            child: Text(
              _savedMsg,
              style: TextStyle(fontSize: 12, fontWeight: FontWeight.bold, color: scheme.primary),
            ),
          ),
        ],
        const SizedBox(height: 20),

        // Reference Info Card
        _modernCard(
          context,
          title: 'REFERENCE NETWORK DEFAULTS',
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              _refRow('Boat AP SSID', 'RiverClean-Boat'),
              _refRow('SoftAP IP Address', '192.168.4.1'),
              _refRow('WebSocket Port', '81'),
              _refRow('ESP-NOW Channel', '1 (fixed)'),
              _refRow('MQTT Broker', 'broker.hivemq.com:1883'),
              _refRow('MQTT WSS', 'broker.hivemq.com:8884/mqtt'),
            ],
          ),
        ),
      ],
    );
  }

  Widget _modernCard(BuildContext context, {required String title, required Widget child}) {
    final scheme = Theme.of(context).colorScheme;
    return Container(
      decoration: BoxDecoration(
        color: scheme.surfaceContainerLow,
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: scheme.outlineVariant.withValues(alpha: 0.5)),
      ),
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            title,
            style: TextStyle(
              fontSize: 11,
              fontWeight: FontWeight.bold,
              letterSpacing: 1.2,
              color: scheme.primary,
            ),
          ),
          const SizedBox(height: 12),
          child,
        ],
      ),
    );
  }

  Widget _refRow(String label, String value) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(label, style: const TextStyle(fontSize: 12, color: Colors.grey)),
          Text(value, style: const TextStyle(fontSize: 12, fontFamily: 'monospace', fontWeight: FontWeight.w600)),
        ],
      ),
    );
  }
}
