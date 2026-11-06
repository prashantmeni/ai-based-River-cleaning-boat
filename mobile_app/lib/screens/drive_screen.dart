import 'package:flutter/material.dart';

import '../services/app_settings.dart';
import '../services/connection_service.dart';

class DriveScreen extends StatefulWidget {
  const DriveScreen({super.key});

  @override
  State<DriveScreen> createState() => _DriveScreenState();
}

class _DriveScreenState extends State<DriveScreen> {
  String _feedback = '';

  void _send(int cmd, {int value = 0, int durationMs = 0, String? label}) {
    final err = ConnectionService.instance.sendCommand(
      cmd,
      value: value,
      durationMs: durationMs,
    );
    setState(() => _feedback = err == null
        ? 'Sent: ${label ?? 'cmd $cmd'}'
        : 'Failed: $err');
  }

  void _drive(int cmd, String label) {
    final s = AppSettings.instance;
    _send(
      cmd,
      value: s.manualPwm,
      durationMs: s.commandDurationMs,
      label: '$label (PWM ${s.manualPwm}, ${s.commandDurationMs == 0 ? 'latched' : '${s.commandDurationMs} ms'})',
    );
  }

  @override
  Widget build(BuildContext context) {
    final svc = ConnectionService.instance;
    final s = AppSettings.instance;
    final scheme = Theme.of(context).colorScheme;
    return ListenableBuilder(
      listenable: svc,
      builder: (context, _) {
        final t = svc.last;
        final latched = t?.safetyLatched ?? false;
        final auto = t?.autonomous ?? false;
        final canDrive = !latched && !auto;
        final connected = svc.status == LinkStatus.online;

        return ListView(
          padding: const EdgeInsets.all(16),
          children: [
            if (!connected)
              _banner(
                scheme.errorContainer,
                scheme.onErrorContainer,
                Icons.cloud_off,
                'Link offline — telemetry or websocket is disconnected.',
              ),
            if (latched)
              _banner(
                Colors.red.shade900.withValues(alpha: 0.3),
                Colors.redAccent,
                Icons.warning_amber_rounded,
                t?.estop == true
                    ? 'E-STOP LATCHED — clear fault before manual control.'
                    : 'SAFETY STOP (${t?.state}) — motors are locked.',
              ),
            if (auto && !latched)
              _banner(
                Colors.blue.shade900.withValues(alpha: 0.3),
                Colors.blueAccent,
                Icons.smart_toy,
                'AUTONOMY ENGAGED — manual steering disabled.',
              ),
            const SizedBox(height: 8),

            // Mode & Safety Pills
            Row(
              children: [
                _pill(
                  latched ? 'MOTORS LATCHED' : 'MOTORS READY',
                  latched ? Colors.redAccent : Colors.greenAccent,
                ),
                const SizedBox(width: 8),
                _pill(auto ? 'AUTO MODE' : 'MANUAL DRIVE', auto ? Colors.blueAccent : Colors.grey),
              ],
            ),
            const SizedBox(height: 16),

            // D-Pad / Steering Controls Card
            _modernCard(
              context,
              title: 'MANUAL PROPULSION STEERING',
              child: Column(
                children: [
                  Center(
                    child: _controlButton(
                      context,
                      icon: Icons.navigation,
                      label: 'FORWARD',
                      color: Colors.greenAccent,
                      enabled: canDrive && connected,
                      onTap: () => _drive(DiagCmd.forward, 'Forward'),
                    ),
                  ),
                  const SizedBox(height: 12),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      _controlButton(
                        context,
                        icon: Icons.arrow_left,
                        label: 'LEFT',
                        color: Colors.cyanAccent,
                        enabled: canDrive && connected,
                        onTap: () => _drive(DiagCmd.turnLeft, 'Turn Left'),
                      ),
                      const SizedBox(width: 20),
                      _controlButton(
                        context,
                        icon: Icons.stop_circle_outlined,
                        label: 'STOP',
                        color: Colors.redAccent,
                        enabled: connected,
                        onTap: () => _send(DiagCmd.stop, value: 0, label: 'Stop'),
                      ),
                      const SizedBox(width: 20),
                      _controlButton(
                        context,
                        icon: Icons.arrow_right,
                        label: 'RIGHT',
                        color: Colors.cyanAccent,
                        enabled: canDrive && connected,
                        onTap: () => _drive(DiagCmd.turnRight, 'Turn Right'),
                      ),
                    ],
                  ),
                ],
              ),
            ),
            const SizedBox(height: 16),

            // Throttle & Duration Sliders Card
            _modernCard(
              context,
              title: 'PWM THROTTLE & DURATION',
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceBetween,
                    children: [
                      const Text('Throttle PWM', style: TextStyle(fontSize: 12, color: Colors.grey)),
                      Text('${s.manualPwm} / 255', style: const TextStyle(fontSize: 12, fontFamily: 'monospace', fontWeight: FontWeight.bold)),
                    ],
                  ),
                  Slider(
                    value: s.manualPwm.toDouble(),
                    min: 40,
                    max: 255,
                    divisions: 215,
                    label: '${s.manualPwm}',
                    onChanged: (v) => setState(() => s.manualPwm = v.round()),
                  ),
                  const SizedBox(height: 10),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceBetween,
                    children: [
                      const Text('Run Duration', style: TextStyle(fontSize: 12, color: Colors.grey)),
                      Text(
                        s.commandDurationMs == 0 ? 'Continuous (latched)' : '${s.commandDurationMs} ms',
                        style: const TextStyle(fontSize: 12, fontFamily: 'monospace', fontWeight: FontWeight.bold),
                      ),
                    ],
                  ),
                  Slider(
                    value: s.commandDurationMs.toDouble(),
                    min: 0,
                    max: 5000,
                    divisions: 50,
                    label: s.commandDurationMs == 0 ? 'Continuous' : '${s.commandDurationMs} ms',
                    onChanged: (v) => setState(() => s.commandDurationMs = v.round()),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 16),

            // Conveyor & Autonomy Actions
            _modernCard(
              context,
              title: 'MECHANISM & AUTONOMY',
              child: Column(
                children: [
                  Row(
                    children: [
                      Expanded(
                        child: ElevatedButton.icon(
                          style: ElevatedButton.styleFrom(
                            backgroundColor: Colors.teal.shade800,
                            foregroundColor: Colors.white,
                            padding: const EdgeInsets.symmetric(vertical: 12),
                          ),
                          onPressed: connected ? () => _send(DiagCmd.conveyorOn, value: 1, label: 'Conveyor ON') : null,
                          icon: const Icon(Icons.bolt),
                          label: const Text('Conveyor ON'),
                        ),
                      ),
                      const SizedBox(width: 10),
                      Expanded(
                        child: OutlinedButton.icon(
                          style: OutlinedButton.styleFrom(
                            padding: const EdgeInsets.symmetric(vertical: 12),
                          ),
                          onPressed: connected ? () => _send(DiagCmd.conveyorOff, value: 0, label: 'Conveyor OFF') : null,
                          icon: const Icon(Icons.power_off),
                          label: const Text('Conveyor OFF'),
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 10),
                  Row(
                    children: [
                      Expanded(
                        child: ElevatedButton.icon(
                          style: ElevatedButton.styleFrom(
                            backgroundColor: auto ? Colors.amber.shade800 : Colors.blue.shade800,
                            foregroundColor: Colors.white,
                            padding: const EdgeInsets.symmetric(vertical: 12),
                          ),
                          onPressed: connected ? () => _send(auto ? diagCmdOffAuto : diagCmdOnAuto, value: auto ? 0 : 1, label: auto ? 'Auto OFF' : 'Auto ON') : null,
                          icon: Icon(auto ? Icons.pause : Icons.play_arrow),
                          label: Text(auto ? 'Disable Autonomy' : 'Enable Autonomy'),
                        ),
                      ),
                      const SizedBox(width: 10),
                      Expanded(
                        child: OutlinedButton.icon(
                          style: OutlinedButton.styleFrom(
                            foregroundColor: Colors.greenAccent,
                            side: const BorderSide(color: Colors.greenAccent),
                            padding: const EdgeInsets.symmetric(vertical: 12),
                          ),
                          onPressed: connected ? () => _send(DiagCmd.stop, value: 2, label: 'Clear Faults') : null,
                          icon: const Icon(Icons.check_circle_outline),
                          label: const Text('Clear Faults'),
                        ),
                      ),
                    ],
                  ),
                ],
              ),
            ),

            if (_feedback.isNotEmpty) ...[
              const SizedBox(height: 14),
              Container(
                padding: const EdgeInsets.all(10),
                decoration: BoxDecoration(
                  color: scheme.surfaceContainerHighest,
                  borderRadius: BorderRadius.circular(8),
                ),
                child: Text(
                  _feedback,
                  style: const TextStyle(fontFamily: 'monospace', fontSize: 11),
                ),
              ),
            ],
          ],
        );
      },
    );
  }

  static const int diagCmdOnAuto = 7;
  static const int diagCmdOffAuto = 8;

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

  Widget _controlButton(
    BuildContext context, {
    required IconData icon,
    required String label,
    required Color color,
    required bool enabled,
    required VoidCallback onTap,
  }) {
    return InkWell(
      onTap: enabled ? onTap : null,
      borderRadius: BorderRadius.circular(12),
      child: Container(
        width: 95,
        height: 75,
        decoration: BoxDecoration(
          color: enabled ? color.withValues(alpha: 0.15) : Colors.grey.withValues(alpha: 0.05),
          borderRadius: BorderRadius.circular(12),
          border: Border.all(
            color: enabled ? color.withValues(alpha: 0.5) : Colors.grey.withValues(alpha: 0.2),
          ),
        ),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(icon, color: enabled ? color : Colors.grey, size: 28),
            const SizedBox(height: 4),
            Text(
              label,
              style: TextStyle(
                fontSize: 11,
                fontWeight: FontWeight.bold,
                color: enabled ? color : Colors.grey,
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _banner(Color bg, Color fg, IconData icon, String text) {
    return Container(
      margin: const EdgeInsets.only(bottom: 12),
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: bg,
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: fg.withValues(alpha: 0.4)),
      ),
      child: Row(
        children: [
          Icon(icon, color: fg, size: 20),
          const SizedBox(width: 10),
          Expanded(child: Text(text, style: TextStyle(color: fg, fontSize: 12, fontWeight: FontWeight.w600))),
        ],
      ),
    );
  }

  Widget _pill(String text, Color color) => Container(
        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
        decoration: BoxDecoration(
          color: color.withValues(alpha: 0.15),
          borderRadius: BorderRadius.circular(999),
          border: Border.all(color: color.withValues(alpha: 0.6)),
        ),
        child: Text(
          text,
          style: TextStyle(fontSize: 11, color: color, fontWeight: FontWeight.bold),
        ),
      );
}
