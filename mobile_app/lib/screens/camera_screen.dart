import 'package:flutter/material.dart';

import '../services/app_settings.dart';
import '../services/connection_service.dart';

/// AI Computer Vision screen — displays the live ESP32-CAM MJPEG stream
/// with a real-time detection bounding-box overlay, telemetry HUD, and
/// waste separation counters.
class CameraScreen extends StatelessWidget {
  const CameraScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final svc = ConnectionService.instance;
    final s = AppSettings.instance;
    final scheme = Theme.of(context).colorScheme;
    return ListenableBuilder(
      listenable: svc,
      builder: (context, _) {
        final t = svc.last;
        final hasStream = s.cameraStreamUrl.isNotEmpty;
        return ListView(
          padding: const EdgeInsets.all(12),
          children: [
            // Live Camera Stream Card
            Container(
              height: 260,
              width: double.infinity,
              decoration: BoxDecoration(
                color: const Color(0xFF090D16),
                borderRadius: BorderRadius.circular(16),
                border: Border.all(
                  color: t != null && t.detection
                      ? Colors.greenAccent.withValues(alpha: 0.6)
                      : scheme.outlineVariant,
                  width: 1.5,
                ),
              ),
              child: Stack(
                children: [
                  // MJPEG video stream
                  if (hasStream)
                    ClipRRect(
                      borderRadius: BorderRadius.circular(14.5),
                      child: Image.network(
                        s.cameraStreamUrl,
                        fit: BoxFit.cover,
                        width: double.infinity,
                        height: double.infinity,
                        loadingBuilder: (context, child, loadingProgress) {
                          if (loadingProgress == null) return child;
                          return Center(
                            child: Column(
                              mainAxisSize: MainAxisSize.min,
                              children: [
                                const CircularProgressIndicator(
                                  strokeWidth: 2,
                                  valueColor: AlwaysStoppedAnimation(
                                    Color(0xFF19C7A1),
                                  ),
                                ),
                                const SizedBox(height: 8),
                                Text(
                                  'Connecting to CAM stream…',
                                  style: TextStyle(
                                    color: Colors.white.withValues(alpha: 0.6),
                                    fontSize: 11,
                                  ),
                                ),
                              ],
                            ),
                          );
                        },
                        errorBuilder: (context, error, stackTrace) {
                          return Center(
                            child: Column(
                              mainAxisSize: MainAxisSize.min,
                              children: [
                                const Icon(
                                  Icons.signal_wifi_off,
                                  color: Colors.redAccent,
                                  size: 36,
                                ),
                                const SizedBox(height: 8),
                                Text(
                                  'Stream unavailable',
                                  style: TextStyle(
                                    color: Colors.redAccent,
                                    fontSize: 11,
                                    fontFamily: 'monospace',
                                  ),
                                ),
                                Text(
                                  s.cameraStreamUrl,
                                  style: TextStyle(
                                    color: Colors.white.withValues(alpha: 0.5),
                                    fontSize: 9,
                                    fontFamily: 'monospace',
                                  ),
                                  textAlign: TextAlign.center,
                                  maxLines: 2,
                                ),
                              ],
                            ),
                          );
                        },
                      ),
                    )
                  else
                    Container(
                      color: const Color(0xFF090D16),
                      child: Center(
                        child: Column(
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            const Icon(
                              Icons.videocam_off,
                              color: Colors.white24,
                              size: 40,
                            ),
                            const SizedBox(height: 8),
                            Text(
                              'No CAM stream URL',
                              style: TextStyle(
                                color: Colors.white.withValues(alpha: 0.5),
                                fontSize: 11,
                                fontFamily: 'monospace',
                              ),
                            ),
                            Text(
                              'Set it in Settings',
                              style: TextStyle(
                                color: Colors.white.withValues(alpha: 0.4),
                                fontSize: 10,
                                fontFamily: 'monospace',
                              ),
                            ),
                          ],
                        ),
                      ),
                    ),
                  // Detection bounding box overlay
                  if (t != null && t.detection && hasStream)
                    _DetectionBoxOverlay(
                      centerX: t.centerX,
                      width: double.infinity,
                      height: 260,
                    ),
                  // Top HUD bar
                  Padding(
                    padding: const EdgeInsets.all(12),
                    child: Row(
                      mainAxisAlignment: MainAxisAlignment.spaceBetween,
                      children: [
                        Row(
                          children: [
                            Container(
                              width: 10,
                              height: 10,
                              decoration: BoxDecoration(
                                color: t != null && t.detection
                                    ? Colors.greenAccent
                                    : Colors.amber,
                                shape: BoxShape.circle,
                                boxShadow: [
                                  BoxShadow(
                                    color: (t != null && t.detection
                                            ? Colors.greenAccent
                                            : Colors.amber)
                                        .withValues(alpha: 0.6),
                                    blurRadius: 8,
                                  ),
                                ],
                              ),
                            ),
                            const SizedBox(width: 8),
                            Text(
                              (t != null && t.detection)
                                  ? 'LIVE TRACKING'
                                  : 'SCANNING AREA',
                              style: const TextStyle(
                                fontSize: 11,
                                fontWeight: FontWeight.bold,
                                fontFamily: 'monospace',
                                color: Colors.white,
                              ),
                            ),
                          ],
                        ),
                        if (t != null)
                          Container(
                            padding: const EdgeInsets.symmetric(
                                horizontal: 8, vertical: 4),
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
                  if (t != null && t.detection && hasStream)
                    Center(
                      child: Column(
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          Container(
                            padding: const EdgeInsets.symmetric(
                                horizontal: 14, vertical: 7),
                            decoration: BoxDecoration(
                              color: Colors.greenAccent.withValues(alpha: 0.15),
                              borderRadius: BorderRadius.circular(8),
                              border: Border.all(
                                  color: Colors.greenAccent.withValues(alpha: 0.5)),
                            ),
                            child: Text(
                              'TARGET: ${t.classLabel.toUpperCase()}',
                              style: const TextStyle(
                                fontSize: 13,
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
                              color: Colors.white.withValues(alpha: 0.85),
                            ),
                          ),
                        ],
                      ),
                    ),
                  if (t != null && !t.detection && hasStream)
                    Center(
                      child: Text(
                        'SEARCHING FOR DEBRIS…',
                        style: TextStyle(
                          fontSize: 12,
                          fontFamily: 'monospace',
                          color: Colors.white.withValues(alpha: 0.45),
                          letterSpacing: 2,
                        ),
                      ),
                    ),
                  // Bottom HUD bar
                  Positioned(
                    bottom: 10,
                    left: 12,
                    right: 12,
                    child: Row(
                      mainAxisAlignment: MainAxisAlignment.spaceBetween,
                      children: [
                        if (t != null)
                          Text(
                            'CAM LINK: ${t.commsOk ? 'ONLINE' : 'DISCONNECTED'}',
                            style: TextStyle(
                              fontSize: 10,
                              fontFamily: 'monospace',
                              color: t.commsOk
                                  ? Colors.greenAccent
                                  : Colors.redAccent,
                            ),
                          ),
                        if (t != null)
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
            if (t != null)
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
                            t.detection ? Colors.greenAccent
                                : scheme.onSurfaceVariant,
                          ),
                        ),
                        Expanded(
                          child: _metricTile(
                            'Confidence Score',
                            '${t.confidence}%',
                            t.confidence > 70
                                ? Colors.greenAccent
                                : Colors.amber,
                          ),
                        ),
                      ],
                    ),
                    const SizedBox(height: 12),
                    Row(
                      children: [
                        Expanded(
                          child: _metricTile('Center Offset X',
                              '${t.centerX}', scheme.onSurface),
                        ),
                        Expanded(
                          child: _metricTile('Vessel Nav State', t.state,
                              scheme.onSurface),
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
                        color: t.detection
                            ? Colors.greenAccent
                            : scheme.primary,
                      ),
                    ),
                  ],
                ),
              ),
            const SizedBox(height: 16),

            // Waste Separated Counters
            if (t != null)
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
                    _counterBox(
                        'Plastic Bottles', '${t.bottleCount}',
                        Icons.wine_bar, Colors.cyan),
                    _counterBox(
                        'Plastic Bags', '${t.bagCount}',
                        Icons.shopping_bag_outlined, Colors.purpleAccent),
                    _counterBox(
                        'General Debris', '${t.debrisCount}',
                        Icons.delete_outline, Colors.amber),
                    _counterBox(
                        'Total Collections', '${t.collectionCount}',
                        Icons.done_all, Colors.greenAccent),
                  ],
                ),
              ),
            const SizedBox(height: 16),

            // Stream setup hint
            if (!hasStream)
              _modernCard(
                context,
                title: 'ENABLE LIVE CAMERA',
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      'The ESP32-CAM hosts an MJPEG stream at '
                      'http://<cam-ip>:8080/stream. '
                      'Enter the URL in Settings → AI CAMERA MJPEG STREAM, '
                      'then return here to view live video with detection overlay.',
                      style: TextStyle(
                          fontSize: 12,
                          color: scheme.onSurfaceVariant,
                          height: 1.4),
                    ),
                    const SizedBox(height: 10),
                    Align(
                      alignment: Alignment.centerRight,
                      child: TextButton.icon(
                        onPressed: () {
                          // Navigate handled by parent tab index; show hint
                          ScaffoldMessenger.of(context).showSnackBar(
                            const SnackBar(
                                content: Text(
                                    'Open Settings to enter the CAM stream URL')),
                          );
                        },
                        icon: const Icon(Icons.settings, size: 16),
                        label: const Text('Open Settings'),
                      ),
                    ),
                  ],
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

/// Draws the detection bounding box over the video stream, positioned by
/// the telemetry centerX (-1000..1000 mapped across the stream width).
class _DetectionBoxOverlay extends StatelessWidget {
  final int centerX;
  final double width;
  final double height;

  const _DetectionBoxOverlay({
    required this.centerX,
    required this.width,
    required this.height,
  });

  @override
  Widget build(BuildContext context) {
    // Map centerX (-500..500 approx) to a horizontal offset across the card
    final cx = width / 2 + (centerX * (width * 0.0008));
    return Positioned(
      left: cx.clamp(20.0, width - 90) - 45,
      top: height / 2 - 40,
      child: SizedBox(
        width: 90,
        height: 80,
        child: Stack(
          children: [
            Container(
              decoration: BoxDecoration(
                border: Border.all(color: Colors.greenAccent, width: 2.5),
                borderRadius: BorderRadius.circular(6),
              ),
            ),
            Positioned.fill(
              child: CustomPaint(
                painter: _CornerMarkerPainter(),
              ),
            ),
            const Center(
              child: Text(
                'TARGET',
                style: TextStyle(
                  color: Colors.greenAccent,
                  fontSize: 9,
                  fontWeight: FontWeight.bold,
                  fontFamily: 'monospace',
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _CornerMarkerPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()
      ..color = Colors.greenAccent
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2.5;
    final m = 10.0; // corner marker length
    // top-left
    canvas.drawLine(Offset(0, m), Offset(0, 0), paint);
    canvas.drawLine(Offset(0, 0), Offset(m, 0), paint);
    // top-right
    canvas.drawLine(Offset(size.width - m, 0), Offset(size.width, 0), paint);
    canvas.drawLine(Offset(size.width, 0), Offset(size.width, m), paint);
    // bottom-left
    canvas.drawLine(Offset(0, size.height - m), Offset(0, size.height), paint);
    canvas.drawLine(Offset(0, size.height), Offset(m, size.height), paint);
    // bottom-right
    canvas.drawLine(Offset(size.width - m, size.height), Offset(size.width, size.height), paint);
    canvas.drawLine(Offset(size.width, size.height - m), Offset(size.width, size.height), paint);
  }

  @override
  bool shouldRepaint(covariant _CornerMarkerPainter oldDelegate) => false;
}
