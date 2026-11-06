import 'package:flutter/material.dart';

import '../services/connection_service.dart';

/// Modern AI Computer Vision screen — displays the live detection viewfinder
/// telemetry, bounding box analytics, waste separation counts, and detection status.
class CameraScreen extends StatelessWidget {
  const CameraScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final svc = ConnectionService.instance;
    final scheme = Theme.of(context).colorScheme;
    return ListenableBuilder(
      listenable: svc,
      builder: (context, _) {
        final t = svc.last;
        if (t == null) {
          return Center(
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                const CircularProgressIndicator(),
                const SizedBox(height: 16),
                Text(
                  'Waiting for telemetry & AI feed…',
                  style: TextStyle(color: scheme.onSurfaceVariant, fontSize: 13),
                ),
              ],
            ),
          );
        }
        return ListView(
          padding: const EdgeInsets.all(16),
          children: [
            // Viewfinder Camera Simulation Card
            Container(
              height: 220,
              decoration: BoxDecoration(
                color: const Color(0xFF090D16),
                borderRadius: BorderRadius.circular(16),
                border: Border.all(
                  color: t.detection ? Colors.greenAccent.withValues(alpha: 0.5) : scheme.outlineVariant,
                  width: 1.5,
                ),
              ),
              child: Stack(
                children: [
                  // Grid overlay lines
                  Positioned.fill(
                    child: CustomPaint(
                      painter: _ViewfinderGridPainter(
                        hasLock: t.detection,
                        centerX: t.centerX,
                      ),
                    ),
                  ),
                  // Top HUD bar
                  Padding(
                    padding: const EdgeInsets.all(16),
                    child: Row(
                      mainAxisAlignment: MainAxisAlignment.spaceBetween,
                      children: [
                        Row(
                          children: [
                            Container(
                              width: 10,
                              height: 10,
                              decoration: BoxDecoration(
                                color: t.detection ? Colors.greenAccent : Colors.amber,
                                shape: BoxShape.circle,
                                boxShadow: [
                                  BoxShadow(
                                    color: (t.detection ? Colors.greenAccent : Colors.amber).withValues(alpha: 0.6),
                                    blurRadius: 8,
                                  ),
                                ],
                              ),
                            ),
                            const SizedBox(width: 8),
                            Text(
                              t.detection ? 'LIVE TRACKING' : 'SCANNING AREA',
                              style: const TextStyle(
                                fontSize: 11,
                                fontWeight: FontWeight.bold,
                                fontFamily: 'monospace',
                                color: Colors.white,
                              ),
                            ),
                          ],
                        ),
                        Container(
                          padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
                          decoration: BoxDecoration(
                            color: Colors.black.withValues(alpha: 0.6),
                            borderRadius: BorderRadius.circular(6),
                            border: Border.all(color: Colors.white24),
                          ),
                          child: Text(
                            '160×120 • 150ms',
                            style: TextStyle(
                              fontSize: 10,
                              fontFamily: 'monospace',
                              color: scheme.onSurfaceVariant,
                            ),
                          ),
                        ),
                      ],
                    ),
                  ),
                  // Center HUD Content if target locked
                  Center(
                    child: t.detection
                        ? Column(
                            mainAxisSize: MainAxisSize.min,
                            children: [
                              Container(
                                padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
                                decoration: BoxDecoration(
                                  color: Colors.greenAccent.withValues(alpha: 0.15),
                                  borderRadius: BorderRadius.circular(8),
                                  border: Border.all(color: Colors.greenAccent.withValues(alpha: 0.5)),
                                ),
                                child: Text(
                                  'TARGET: ${t.classLabel.toUpperCase()}',
                                  style: const TextStyle(
                                    fontSize: 14,
                                    fontWeight: FontWeight.bold,
                                    fontFamily: 'monospace',
                                    color: Colors.greenAccent,
                                  ),
                                ),
                              ),
                              const SizedBox(height: 6),
                              Text(
                                'Confidence: ${t.confidence}%  |  Offset X: ${t.centerX}',
                                style: TextStyle(
                                  fontSize: 11,
                                  fontFamily: 'monospace',
                                  color: Colors.white.withValues(alpha: 0.8),
                                ),
                              ),
                            ],
                          )
                        : Text(
                            'SEARCHING FOR DEBRIS…',
                            style: TextStyle(
                              fontSize: 12,
                              fontFamily: 'monospace',
                              color: Colors.white.withValues(alpha: 0.4),
                              letterSpacing: 2,
                            ),
                          ),
                  ),
                  // Bottom HUD bar
                  Positioned(
                    bottom: 12,
                    left: 16,
                    right: 16,
                    child: Row(
                      mainAxisAlignment: MainAxisAlignment.spaceBetween,
                      children: [
                        Text(
                          'CAM LINK: ${t.commsOk ? 'ONLINE' : 'DISCONNECTED'}',
                          style: TextStyle(
                            fontSize: 10,
                            fontFamily: 'monospace',
                            color: t.commsOk ? Colors.greenAccent : Colors.redAccent,
                          ),
                        ),
                        Text(
                          'STATE: ${t.state}',
                          style: const TextStyle(
                            fontSize: 10,
                            fontFamily: 'monospace',
                            color: Colors.white70,
                          ),
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 16),

            // Detection Metrics Card
            _modernCard(
              context,
              title: 'CURRENT TARGET METRICS',
              child: Column(
                children: [
                  Row(
                    children: [
                      Expanded(
                        child: _metricTile(
                          'Classification',
                          t.detection ? t.classLabel : 'None',
                          t.detection ? Colors.greenAccent : scheme.onSurfaceVariant,
                        ),
                      ),
                      Expanded(
                        child: _metricTile(
                          'Confidence Score',
                          '${t.confidence}%',
                          t.confidence > 70 ? Colors.greenAccent : Colors.amber,
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 12),
                  Row(
                    children: [
                      Expanded(
                        child: _metricTile('Center Offset X', '${t.centerX}', scheme.onSurface),
                      ),
                      Expanded(
                        child: _metricTile('Vessel Nav State', t.state, scheme.onSurface),
                      ),
                    ],
                  ),
                  const SizedBox(height: 14),
                  ClipRRect(
                    borderRadius: BorderRadius.circular(999),
                    child: LinearProgressIndicator(
                      value: (t.confidence.clamp(0, 100)) / 100,
                      minHeight: 8,
                      backgroundColor: scheme.surfaceContainerHighest,
                      color: t.detection ? Colors.greenAccent : scheme.primary,
                    ),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 16),

            // Waste Separated Counters
            _modernCard(
              context,
              title: 'WASTE SEPARATED COUNTERS',
              child: GridView.count(
                crossAxisCount: 2,
                shrinkWrap: true,
                physics: const NeverScrollableScrollPhysics(),
                mainAxisSpacing: 10,
                crossAxisSpacing: 10,
                childAspectRatio: 2.2,
                children: [
                  _counterBox('Plastic Bottles', '${t.bottleCount}', Icons.wine_bar, Colors.cyan),
                  _counterBox('Plastic Bags', '${t.bagCount}', Icons.shopping_bag_outlined, Colors.purpleAccent),
                  _counterBox('General Debris', '${t.debrisCount}', Icons.delete_outline, Colors.amber),
                  _counterBox('Total Collections', '${t.collectionCount}', Icons.done_all, Colors.greenAccent),
                ],
              ),
            ),
            const SizedBox(height: 16),

            // Architecture Note Card
            _modernCard(
              context,
              title: 'EMBEDDED AI ARCHITECTURE',
              child: Text(
                'The ESP32-CAM captures 160×120 grayscale frames and runs an optimized '
                'on-device blob segmentation and shape classifier over ESP-NOW. '
                'Metadata (class, confidence, center position) is streamed in real-time '
                'without bandwidth-heavy MJPEG video streams.',
                style: TextStyle(fontSize: 12, color: scheme.onSurfaceVariant, height: 1.4),
              ),
            ),
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

  Widget _metricTile(String label, String value, Color valueColor) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(label, style: const TextStyle(fontSize: 11, color: Colors.grey)),
        const SizedBox(height: 2),
        Text(
          value,
          style: TextStyle(fontSize: 15, fontWeight: FontWeight.bold, color: valueColor),
        ),
      ],
    );
  }

  Widget _counterBox(String label, String count, IconData icon, Color color) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.08),
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: color.withValues(alpha: 0.3)),
      ),
      child: Row(
        children: [
          Icon(icon, color: color, size: 22),
          const SizedBox(width: 10),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Text(
                  count,
                  style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold, color: color),
                ),
                Text(
                  label,
                  style: const TextStyle(fontSize: 10, color: Colors.grey),
                  overflow: TextOverflow.ellipsis,
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _ViewfinderGridPainter extends CustomPainter {
  final bool hasLock;
  final int centerX;

  _ViewfinderGridPainter({required this.hasLock, required this.centerX});

  @override
  void paint(Canvas canvas, Size size) {
    final paintGrid = Paint()
      ..color = Colors.white.withValues(alpha: 0.05)
      ..strokeWidth = 1.0;

    // Draw grid lines
    const step = 30.0;
    for (double i = 0; i < size.width; i += step) {
      canvas.drawLine(Offset(i, 0), Offset(i, size.height), paintGrid);
    }
    for (double i = 0; i < size.height; i += step) {
      canvas.drawLine(Offset(0, i), Offset(size.width, i), paintGrid);
    }

    // Draw crosshair center
    final paintCross = Paint()
      ..color = (hasLock ? Colors.greenAccent : Colors.white24).withValues(alpha: 0.4)
      ..strokeWidth = 1.5;

    final cx = size.width / 2;
    final cy = size.height / 2;
    canvas.drawLine(Offset(cx - 15, cy), Offset(cx + 15, cy), paintCross);
    canvas.drawLine(Offset(cx, cy - 15), Offset(cx, cy + 15), paintCross);

    // If target locked, draw target box
    if (hasLock) {
      final paintBox = Paint()
        ..color = Colors.greenAccent
        ..style = PaintingStyle.stroke
        ..strokeWidth = 2.0;

      // map centerX (-100 to 100 approx) to canvas offset
      final boxX = cx + (centerX * 0.8) - 30;
      final boxRect = Rect.fromLTWH(boxX.clamp(20.0, size.width - 80), cy - 35, 60, 70);
      canvas.drawRect(boxRect, paintBox);
    }
  }

  @override
  bool shouldRepaint(covariant _ViewfinderGridPainter oldDelegate) {
    return oldDelegate.hasLock != hasLock || oldDelegate.centerX != centerX;
  }
}
