import 'package:flutter/material.dart';

import '../models/telemetry.dart';
import '../services/connection_service.dart';

class MonitorScreen extends StatelessWidget {
  const MonitorScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final svc = ConnectionService.instance;
    return ListenableBuilder(
      listenable: svc,
      builder: (context, _) {
        final t = svc.last;
        final scheme = Theme.of(context).colorScheme;
        return ListView(
          padding: const EdgeInsets.all(16),
          children: [
            // Connection Status Header Card
            _modernCard(
              context,
              title: 'SYSTEM LINK & REFRESH',
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    children: [
                      _statusDot(svc, scheme),
                      const SizedBox(width: 8),
                      Text(
                        _statusText(svc),
                        style: TextStyle(
                          fontWeight: FontWeight.bold,
                          fontSize: 13,
                          color: _statusColor(svc, scheme),
                        ),
                      ),
                      const Spacer(),
                      ElevatedButton.icon(
                        onPressed: () => svc.connect(),
                        icon: const Icon(Icons.refresh, size: 14),
                        label: const Text('Reconnect', style: TextStyle(fontSize: 11)),
                        style: ElevatedButton.styleFrom(
                          padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 0),
                          minimumSize: const Size(0, 30),
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 8),
                  Row(
                    children: [
                      Expanded(
                        child: Text(
                          svc.url,
                          style: TextStyle(
                            fontSize: 11,
                            color: scheme.onSurfaceVariant,
                            fontFamily: 'monospace',
                          ),
                          overflow: TextOverflow.ellipsis,
                        ),
                      ),
                      Text(
                        svc.lastAt == null
                            ? 'no telemetry'
                            : svc.lastAge.inMilliseconds < 1000
                                ? '${svc.lastAge.inMilliseconds}ms ago'
                                : '${svc.lastAge.inSeconds}s ago',
                        style: TextStyle(fontSize: 11, color: scheme.primary),
                      ),
                    ],
                  ),
                  if (svc.lastError != null) ...[
                    const SizedBox(height: 6),
                    Text(
                      svc.lastError!,
                      style: TextStyle(fontSize: 11, color: scheme.error),
                    ),
                  ],
                ],
              ),
            ),
            const SizedBox(height: 14),

            if (t == null)
              _modernCard(
                context,
                title: 'VESSEL TELEMETRY',
                child: Padding(
                  padding: const EdgeInsets.symmetric(vertical: 20),
                  child: Center(
                    child: Text(
                      'Waiting for live telemetry stream…',
                      style: TextStyle(color: scheme.onSurfaceVariant),
                    ),
                  ),
                ),
              )
            else ...[
              // Operational State Card
              _modernCard(
                context,
                title: 'OPERATIONAL STATE & MODE',
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Row(
                      children: [
                        _stateChip(t.state, scheme),
                        const SizedBox(width: 8),
                        _pill(t.commsOk ? 'ESP-NOW LINK ACTIVE' : 'ESP-NOW LOST', t.commsOk ? Colors.green : Colors.red),
                        const SizedBox(width: 8),
                        if (t.estop) _pill('E-STOP', Colors.red),
                        if (t.autonomous) _pill('AUTO', Colors.blue),
                      ],
                    ),
                    const SizedBox(height: 14),
                    Row(
                      mainAxisAlignment: MainAxisAlignment.spaceBetween,
                      children: [
                        _miniStat('Uptime', formatUptime(t.uptimeMs)),
                        _miniStat('Conveyor', t.conveyor),
                        _miniStat('Collections', '${t.collectionCount}'),
                        _miniStat('Msgs Rx', '${svc.telemetryCount}'),
                      ],
                    ),
                  ],
                ),
              ),
              const SizedBox(height: 14),

              // Motor Output Bars
              _modernCard(
                context,
                title: 'PROPULSION MOTORS OUTPUT',
                child: Column(
                  children: [
                    _pwmBar(context, 'Left Motor', t.leftPwm, t.leftDir, scheme),
                    const SizedBox(height: 12),
                    _pwmBar(context, 'Right Motor', t.rightPwm, t.rightDir, scheme),
                  ],
                ),
              ),
              const SizedBox(height: 14),

              // Water Quality & Sensors Card (TDS + Ultrasonic)
              _modernCard(
                context,
                title: 'WATER QUALITY & OBSTACLES',
                child: Column(
                  children: [
                    Row(
                      children: [
                        Expanded(
                          child: Container(
                            padding: const EdgeInsets.all(12),
                            decoration: BoxDecoration(
                              color: scheme.surfaceContainerHighest.withValues(alpha: 0.4),
                              borderRadius: BorderRadius.circular(10),
                            ),
                            child: Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                const Text('TDS WATER PURITY', style: TextStyle(fontSize: 10, color: Colors.grey)),
                                const SizedBox(height: 4),
                                Text(
                                  t.tdsPpm >= 0 ? '${t.tdsPpm} ppm' : 'Invalid',
                                  style: TextStyle(
                                    fontSize: 18,
                                    fontWeight: FontWeight.bold,
                                    color: t.tdsPpm >= 0 && t.tdsPpm < 500 ? Colors.cyanAccent : Colors.amber,
                                  ),
                                ),
                                Text('Raw: ${t.tdsMv} mV', style: const TextStyle(fontSize: 10, color: Colors.grey)),
                              ],
                            ),
                          ),
                        ),
                        const SizedBox(width: 10),
                        Expanded(
                          child: Container(
                            padding: const EdgeInsets.all(12),
                            decoration: BoxDecoration(
                              color: scheme.surfaceContainerHighest.withValues(alpha: 0.4),
                              borderRadius: BorderRadius.circular(10),
                            ),
                            child: Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                const Text('ACTIVE FAULTS', style: TextStyle(fontSize: 10, color: Colors.grey)),
                                const SizedBox(height: 4),
                                Text(
                                  t.activeFaults.isEmpty ? 'None (Clear)' : t.activeFaults.join(', '),
                                  style: TextStyle(
                                    fontSize: 13,
                                    fontWeight: FontWeight.bold,
                                    color: t.activeFaults.isEmpty ? Colors.greenAccent : Colors.redAccent,
                                  ),
                                  overflow: TextOverflow.ellipsis,
                                ),
                                Text('Flags: ${t.faultFlags}', style: const TextStyle(fontSize: 10, color: Colors.grey)),
                              ],
                            ),
                          ),
                        ),
                      ],
                    ),
                    const SizedBox(height: 12),
                    _distanceBar('Ultrasonic Left', t.leftCm, scheme),
                    const SizedBox(height: 8),
                    _distanceBar('Ultrasonic Right', t.rightCm, scheme),
                  ],
                ),
              ),
              const SizedBox(height: 14),

              // Collection Bin Level Card (Ultrasonic level + Buzzer alarm)
              _modernCard(
                context,
                title: 'COLLECTION BIN LEVEL STATUS',
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Row(
                      mainAxisAlignment: MainAxisAlignment.spaceBetween,
                      children: [
                        Row(
                          children: [
                            Icon(
                              t.binFull ? Icons.warning_rounded : Icons.delete_outline,
                              color: t.binFull ? Colors.redAccent : Colors.cyanAccent,
                              size: 24,
                            ),
                            const SizedBox(width: 10),
                            Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                Text(
                                  t.binFull ? 'BIN FULL (BUZZER ACTIVE)' : 'BIN CAPACITY NORMAL',
                                  style: TextStyle(
                                    fontSize: 13,
                                    fontWeight: FontWeight.bold,
                                    color: t.binFull ? Colors.redAccent : Colors.greenAccent,
                                  ),
                                ),
                                Text(
                                  t.binDistanceCm > 0 ? 'Debris clearance: ${t.binDistanceCm.toStringAsFixed(1)} cm' : 'Sensor idle',
                                  style: const TextStyle(fontSize: 11, color: Colors.grey),
                                ),
                              ],
                            ),
                          ],
                        ),
                        _pill(t.binFull ? 'FULL' : 'OK', t.binFull ? Colors.red : Colors.green),
                      ],
                    ),
                    const SizedBox(height: 12),
                    ClipRRect(
                      borderRadius: BorderRadius.circular(6),
                      child: LinearProgressIndicator(
                        value: t.binFull ? 1.0 : (t.binDistanceCm > 0 ? (1.0 - (t.binDistanceCm / 30.0)).clamp(0.0, 1.0) : 0.0),
                        minHeight: 8,
                        backgroundColor: scheme.surfaceContainerHighest,
                        color: t.binFull ? Colors.redAccent : Colors.cyanAccent,
                      ),
                    ),
                  ],
                ),
              ),
              const SizedBox(height: 14),

              // Interactive GPS Satellite Navigation & Map Widget
              _modernCard(
                context,
                title: 'GPS SATELLITE NAVIGATION & LIVE RADAR MAP',
                child: Column(
                  children: [
                    // Radar Map Widget
                    Container(
                      height: 180,
                      decoration: BoxDecoration(
                        color: const Color(0xFF090D16),
                        borderRadius: BorderRadius.circular(12),
                        border: Border.all(color: scheme.outlineVariant.withValues(alpha: 0.5)),
                      ),
                      child: Stack(
                        children: [
                          Positioned.fill(
                            child: CustomPaint(
                              painter: _GpsRadarPainter(
                                fix: t.gpsFix,
                                satellites: t.satellites,
                              ),
                            ),
                          ),
                          Padding(
                            padding: const EdgeInsets.all(12),
                            child: Row(
                              mainAxisAlignment: MainAxisAlignment.spaceBetween,
                              children: [
                                _pill(t.gpsFix ? 'GPS LOCKED' : 'NO FIX', t.gpsFix ? Colors.greenAccent : Colors.orange),
                                Text(
                                  'Sats: ${t.satellites}  |  Age: ${t.gpsAgeMs}ms',
                                  style: const TextStyle(fontSize: 10, fontFamily: 'monospace', color: Colors.white70),
                                ),
                              ],
                            ),
                          ),
                          Positioned(
                            bottom: 10,
                            left: 12,
                            child: Text(
                              t.gpsFix
                                  ? 'Lat: ${t.latitude.toStringAsFixed(6)}° N\nLon: ${t.longitude.toStringAsFixed(6)}° E'
                              : 'Acquiring GPS satellite fix…',
                              style: const TextStyle(fontSize: 11, fontFamily: 'monospace', color: Colors.cyanAccent),
                            ),
                          ),
                        ],
                      ),
                    ),
                  ],
                ),
              ),
              const SizedBox(height: 14),

              // ESP-NOW Link Diagnostics
              _modernCard(
                context,
                title: 'ESP-NOW WIRELESS LINK METRICS',
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    _miniStat('RX Bytes', formatBytes(t.rxBytes)),
                    _miniStat('RX Errors', '${t.rxErrors}'),
                    _miniStat('Parse Err', '${svc.parseErrors}'),
                    _miniStat('Drops', '${t.drops}'),
                  ],
                ),
              ),
            ],
          ],
        );
      },
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

  Widget _miniStat(String label, String value) => Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        mainAxisSize: MainAxisSize.min,
        children: [
          Text(label, style: const TextStyle(fontSize: 10, color: Colors.grey)),
          const SizedBox(height: 2),
          Text(value, style: const TextStyle(fontWeight: FontWeight.w600, fontSize: 13)),
        ],
      );

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

  Widget _stateChip(String state, ColorScheme scheme) {
    final color = switch (state) {
      'FAULT' || 'EMERGENCY_STOP' => Colors.red,
      'OBSTACLE_AVOID' => Colors.orange,
      'COLLECT' || 'VERIFY_COLLECT' => Colors.green,
      'TARGET_DETECTED' || 'ALIGN' || 'APPROACH' => Colors.teal,
      'SEARCH' || 'TARGET_LOST' => Colors.amber,
      _ => Colors.blueGrey,
    };
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 5),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.2),
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: color),
      ),
      child: Text(
        state,
        style: TextStyle(fontSize: 12, fontWeight: FontWeight.bold, color: color),
      ),
    );
  }

  Widget _pwmBar(BuildContext context, String label, int pwm, int dir, ColorScheme scheme) {
    final pct = (pwm.clamp(0, 255) / 255.0);
    final dirStr = dir > 0 ? 'FORWARD' : dir < 0 ? 'REVERSE' : 'STOPPED';
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Text(label, style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w500)),
            Text('$pwm / 255 ($dirStr)', style: const TextStyle(fontSize: 11, fontFamily: 'monospace', color: Colors.grey)),
          ],
        ),
        const SizedBox(height: 6),
        ClipRRect(
          borderRadius: BorderRadius.circular(6),
          child: LinearProgressIndicator(
            value: pct,
            minHeight: 8,
            backgroundColor: scheme.surfaceContainerHighest,
            color: dir != 0 ? Colors.cyanAccent : Colors.grey,
          ),
        ),
      ],
    );
  }

  Widget _distanceBar(String label, double cm, ColorScheme scheme) {
    final invalid = cm < 0 || !cm.isFinite;
    final pct = invalid ? 0.0 : (cm / 200.0).clamp(0.0, 1.0);
    final color = invalid ? Colors.grey : cm < 30 ? Colors.redAccent : cm < 60 ? Colors.amber : Colors.greenAccent;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Text(label, style: const TextStyle(fontSize: 11, color: Colors.grey)),
            Text(invalid ? 'No Echo' : '${cm.toStringAsFixed(1)} cm', style: TextStyle(fontSize: 11, fontWeight: FontWeight.bold, color: color)),
          ],
        ),
        const SizedBox(height: 4),
        ClipRRect(
          borderRadius: BorderRadius.circular(6),
          child: LinearProgressIndicator(
            value: pct,
            minHeight: 6,
            backgroundColor: scheme.surfaceContainerHighest,
            color: color,
          ),
        ),
      ],
    );
  }

  Widget _statusDot(ConnectionService svc, ColorScheme scheme) {
    final color = switch (svc.status) {
      LinkStatus.online when svc.freshness == Freshness.live => Colors.green,
      LinkStatus.online => Colors.orange,
      LinkStatus.connecting => Colors.blue,
      LinkStatus.offline => Colors.orange,
      LinkStatus.error => Colors.red,
      LinkStatus.idle => Colors.grey,
    };
    return Container(
      width: 10,
      height: 10,
      decoration: BoxDecoration(color: color, shape: BoxShape.circle),
    );
  }

  String _statusText(ConnectionService svc) {
    if (svc.status == LinkStatus.online) {
      return switch (svc.freshness) {
        Freshness.live => 'CONNECTED (Live)',
        Freshness.stale => 'CONNECTED (Stale)',
        Freshness.waiting => 'CONNECTED (Waiting)',
      };
    }
    return switch (svc.status) {
      LinkStatus.idle => 'DISCONNECTED',
      LinkStatus.connecting => 'CONNECTING…',
      LinkStatus.offline => 'RECONNECTING…',
      LinkStatus.error => 'CONNECTION ERROR',
      LinkStatus.online => '',
    };
  }

  Color _statusColor(ConnectionService svc, ColorScheme scheme) {
    return switch (svc.status) {
      LinkStatus.online when svc.freshness == Freshness.live => Colors.green,
      LinkStatus.online => Colors.orange,
      LinkStatus.connecting => Colors.blue,
      LinkStatus.error => scheme.error,
      _ => Colors.orange,
    };
  }
}

class _GpsRadarPainter extends CustomPainter {
  final bool fix;
  final int satellites;

  _GpsRadarPainter({required this.fix, required this.satellites});

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final paintRing = Paint()
      ..color = Colors.cyanAccent.withValues(alpha: 0.15)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.0;

    // Draw concentric radar circles
    canvas.drawCircle(center, 30, paintRing);
    canvas.drawCircle(center, 60, paintRing);
    canvas.drawCircle(center, 90, paintRing);

    // Crosshairs
    final paintCross = Paint()
      ..color = Colors.cyanAccent.withValues(alpha: 0.2)
      ..strokeWidth = 1.0;
    canvas.drawLine(Offset(0, center.dy), Offset(size.width, center.dy), paintCross);
    canvas.drawLine(Offset(center.dx, 0), Offset(center.dx, size.height), paintCross);

    // Draw vessel marker in center if fixed
    final paintVessel = Paint()
      ..color = fix ? Colors.greenAccent : Colors.orangeAccent
      ..style = PaintingStyle.fill;

    canvas.drawCircle(center, 6, paintVessel);

    // Pulse ring around vessel
    final paintPulse = Paint()
      ..color = (fix ? Colors.greenAccent : Colors.orangeAccent).withValues(alpha: 0.4)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.5;
    canvas.drawCircle(center, 14, paintPulse);
  }

  @override
  bool shouldRepaint(covariant _GpsRadarPainter oldDelegate) {
    return oldDelegate.fix != fix || oldDelegate.satellites != satellites;
  }
}
