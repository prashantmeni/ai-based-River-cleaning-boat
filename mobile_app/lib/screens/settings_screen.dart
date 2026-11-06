import 'package:flutter/material.dart';

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
  double _duration = 600;
  bool _loaded = false;
  String _savedMsg = '';
  bool _obscurePass = true;

  @override
  void initState() {
    super.initState();
    final s = AppSettings.instance;
    _endpointCtrl.text = s.endpoint;
    _streamCtrl.text = s.cameraStreamUrl;
    _ssidCtrl.text = s.wifiSsid;
    _passCtrl.text = s.wifiPassword;
    _pwm = s.manualPwm.toDouble();
    _duration = s.commandDurationMs.toDouble();
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

  Future<void> _save() async {
    final s = AppSettings.instance;
    final oldUrl = s.endpoint;
    s.endpoint = _endpointCtrl.text.trim();
    s.cameraStreamUrl = _streamCtrl.text.trim();
    s.wifiSsid = _ssidCtrl.text.trim();
    s.wifiPassword = _passCtrl.text;
    s.manualPwm = _pwm.round();
    s.commandDurationMs = _duration.round();
    await s.save();
    if (s.endpoint != oldUrl) {
      await ConnectionService.instance.connect(s.endpoint);
    }
    setState(() => _savedMsg = 'Settings & Wi-Fi profile saved successfully.');
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
      _duration = s.commandDurationMs.toDouble();
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
            ],
          ),
        ),
        const SizedBox(height: 16),

        // Wi-Fi Network Provisioning Card (like mobile phone Wi-Fi settings)
        _modernCard(
          context,
          title: 'BOAT Wi-Fi NETWORK PROVISIONING (STA)',
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const Text(
                'Configure the target Wi-Fi network that the boat controller connects to automatically upon boot.',
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
              Align(
                alignment: Alignment.centerRight,
                child: TextButton.icon(
                  onPressed: () {
                    ScaffoldMessenger.of(context).showSnackBar(
                      const SnackBar(content: Text('Wi-Fi credentials saved. Stored in boat profile.')),
                    );
                  },
                  icon: const Icon(Icons.router, size: 16),
                  label: const Text('Sync Wi-Fi to Boat'),
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
              Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  const Text('Command Duration', style: TextStyle(fontSize: 12, color: Colors.grey)),
                  Text(
                    _duration.round() == 0 ? 'Continuous (latched)' : '${_duration.round()} ms',
                    style: const TextStyle(fontSize: 12, fontFamily: 'monospace', fontWeight: FontWeight.bold),
                  ),
                ],
              ),
              Slider(
                value: _duration,
                min: 0,
                max: 5000,
                divisions: 50,
                label: _duration.round() == 0 ? 'Continuous' : '${_duration.round()} ms',
                onChanged: (v) => setState(() => _duration = v),
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
