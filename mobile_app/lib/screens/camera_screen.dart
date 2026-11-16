import 'dart:async';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter/material.dart';

import '../services/app_settings.dart';
import '../services/connection_service.dart';

/// AI Computer Vision screen — live ESP32-CAM MJPEG stream with real-time
/// detection bounding-box overlay, telemetry HUD, flashlight control, and
/// waste separation counters.
class CameraScreen extends StatefulWidget {
  const CameraScreen({super.key});

  @override
  State<CameraScreen> createState() => _CameraScreenState();
}

class _CameraScreenState extends State<CameraScreen> {
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
              height: 320,
              width: double.infinity,
              decoration: BoxDecoration(
                color: const Color(0xFF090D16),
                borderRadius: BorderRadius.circular(16),
                border: Border.all(
                  color: (t != null && t.detection)
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
                      child: MjpegViewer(
                        url: s.cameraStreamUrl,
                        width: double.infinity,
                        height: 320,
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
                  // Flash toggle button
                  if (hasStream)
                    Positioned(
                      bottom: 12,
                      right: 12,
                      child: _FlashToggleButton(svc: svc),
                    ),
                  // Auto-flash indicator
                  if (hasStream && s.flashAuto)
                    Positioned(
                      bottom: 12,
                      left: 12,
                      child: Container(
                        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
                        decoration: BoxDecoration(
                          color: Colors.amber.withValues(alpha: 0.2),
                          borderRadius: BorderRadius.circular(6),
                          border: Border.all(color: Colors.amber),
                        ),
                        child: const Text(
                          'AUTO-FLASH',
                          style: TextStyle(
                            fontSize: 9,
                            fontWeight: FontWeight.bold,
                            color: Colors.amber,
                          ),
                        ),
                      ),
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
                                color: (t != null && t.detection)
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
                    bottom: 52,
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

            // Flashlight Controls Card
            _modernCard(
              context,
              title: 'CAMERA FLASHLIGHT (GPIO 4)',
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    children: [
                      Expanded(
                        child: _flashBtn(
                          icon: Icons.flash_off,
                          label: 'FLASH OFF',
                          active: !svc.flashOn,
                          onTap: () => svc.toggleFlash(false),
                          color: svc.flashOn ? Colors.grey : Colors.amber,
                        ),
                      ),
                      const SizedBox(width: 10),
                      Expanded(
                        child: _flashBtn(
                          icon: Icons.flash_on,
                          label: 'FLASH ON',
                          active: svc.flashOn,
                          onTap: () => svc.toggleFlash(true),
                          color: svc.flashOn ? Colors.amber : Colors.grey,
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 10),
                  Row(
                    children: [
                      Expanded(
                        child: OutlinedButton.icon(
                          onPressed: svc.status != LinkStatus.online
                              ? null
                              : () async {
                                  final found =
                                      await svc.discoverCamStreamUrl();
                                  if (found != null && mounted) {
                                    final st = AppSettings.instance;
                                    st.cameraStreamUrl = found;
                                    await st.save();
                                    if (mounted) setState(() {});
                                  }
                                },
                          icon: const Icon(Icons.wifi, size: 16),
                          label: const Text('Auto-Detect Stream'),
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 8),
                  Text(
                    'Flash toggles the ESP32-CAM onboard LED (GPIO 4) via ESP-NOW.\n'
                    'Auto-flash (in Settings) turns it on when waste is detected at night.',
                    style: TextStyle(fontSize: 11, color: scheme.onSurfaceVariant),
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
                          // navigate handled by parent tab; show hint
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

  Widget _flashBtn({
    required IconData icon,
    required String label,
    required bool active,
    required VoidCallback onTap,
    required Color color,
  }) {
    return ElevatedButton.icon(
      onPressed: onTap,
      style: ElevatedButton.styleFrom(
        backgroundColor: active ? color : Colors.grey.withValues(alpha: 0.2),
        foregroundColor: active ? Colors.black : Colors.white,
        padding: const EdgeInsets.symmetric(vertical: 12),
      ),
      icon: Icon(icon, size: 16),
      label: Text(label),
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
              mainAxisSize: MainAxisSize.min,
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

/// Toggle button for the ESP32-CAM flash LED (GPIO 4).
/// Sends CAM_FLASH diag command via MQTT/WebSocket → main board → ESP-NOW → CAM.
class _FlashToggleButton extends StatefulWidget {
  final ConnectionService svc;
  const _FlashToggleButton({required this.svc});

  @override
  State<_FlashToggleButton> createState() => _FlashToggleButtonState();
}

class _FlashToggleButtonState extends State<_FlashToggleButton> {
  @override
  Widget build(BuildContext context) {
    final svc = widget.svc;
    return FloatingActionButton.small(
      onPressed: () => svc.toggleFlash(!svc.flashOn),
      backgroundColor: svc.flashOn ? Colors.amber : Colors.black87,
      foregroundColor: svc.flashOn ? Colors.black : Colors.white,
      tooltip: svc.flashOn ? 'Flash ON' : 'Flash OFF',
      child: Icon(svc.flashOn ? Icons.highlight : Icons.flash_off),
    );
  }
}

/// Custom MJPEG viewer — parses multipart/x-mixed-replace stream and
/// renders each JPEG frame. Flutter's Image.network does not support MJEP
/// natively, so we extract SOI/EOI markers from the raw byte stream.
class MjpegViewer extends StatefulWidget {
  final String url;
  final double width;
  final double height;

  const MjpegViewer({
    super.key,
    required this.url,
    required this.width,
    required this.height,
  });

  @override
  State<MjpegViewer> createState() => _MjpegViewerState();
}

class _MjpegViewerState extends State<MjpegViewer> {
  Uint8List? _currentFrame;
  String? _error;

  @override
  void initState() {
    super.initState();
    _connect();
  }

  Future<void> _connect() async {
    try {
      final client = HttpClient();
      final uri = Uri.parse(widget.url);
      final request = await client.openUrl('GET', uri);
      final response = await request.close();

      if (response.statusCode != 200) {
        if (mounted) {
          setState(() => _error = 'HTTP ${response.statusCode}');
        }
        return;
      }

      if (mounted) setState(() => _error = null);

      await _readFrames(response);
    } catch (e) {
      if (mounted) {
        setState(() => _error = e.toString());
      }
    }
  }

  Future<void> _readFrames(HttpClientResponse response) async {
    final buffer = <int>[];

    await for (final chunk in response) {
      buffer.addAll(chunk);

      while (true) {
        // Find JPEG SOI (0xFF 0xD8)
        int soi = -1;
        for (int i = 0; i < buffer.length - 1; i++) {
          if (buffer[i] == 0xFF && buffer[i + 1] == 0xD8) {
            soi = i;
            break;
          }
        }
        if (soi == -1) break;

        // Discard multipart headers between frames
        if (soi > 0) buffer.removeRange(0, soi);

        // Find JPEG EOI (0xFF 0xD9)
        int eoi = -1;
        for (int i = 0; i < buffer.length - 1; i++) {
          if (buffer[i] == 0xFF && buffer[i + 1] == 0xD9) {
            eoi = i;
            break;
          }
        }
        if (eoi == -1) break;

        // Extract complete JPEG frame
        final frameBytes = buffer.sublist(0, eoi + 2);
        buffer.removeRange(0, eoi + 2);

        if (mounted) {
          setState(() => _currentFrame = Uint8List.fromList(frameBytes));
        }
      }

      // Keep buffer bounded
      if (buffer.length > 200000) {
        buffer.removeRange(0, buffer.length - 50000);
      }
    }

    if (mounted) {
      setState(() {});
    }
  }

  @override
  void dispose() {
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Container(
      width: widget.width,
      height: widget.height,
      color: const Color(0xFF090D16),
      child: _currentFrame == null
          ? Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                if (_error != null) ...[
                  const Icon(Icons.signal_wifi_off,
                      color: Colors.redAccent, size: 36),
                  const SizedBox(height: 8),
                  Text(_error!,
                      style: const TextStyle(
                          color: Colors.redAccent,
                          fontSize: 11,
                          fontFamily: 'monospace')),
                ] else
                  const CircularProgressIndicator(
                    strokeWidth: 2,
                    valueColor: AlwaysStoppedAnimation(Color(0xFF19C7A1)),
                  ),
              ],
            )
          : Image.memory(
              _currentFrame!,
              fit: BoxFit.cover,
              width: double.infinity,
              height: double.infinity,
            ),
    );
  }
}
