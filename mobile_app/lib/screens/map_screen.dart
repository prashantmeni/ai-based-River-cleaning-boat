import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';

import '../services/connection_service.dart';

class MapScreen extends StatelessWidget {
  const MapScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final svc = ConnectionService.instance;
    final scheme = Theme.of(context).colorScheme;

    return ListenableBuilder(
      listenable: svc,
      builder: (context, _) {
        final t = svc.last;
        final hasFix = t != null && t.gpsFix;
        final center = hasFix
            ? LatLng(t.latitude, t.longitude)
            : const LatLng(0.0, 0.0);

        return Scaffold(
          appBar: AppBar(
            title: const Text('GPS Live Map (OpenStreetMap)'),
            actions: [
              if (t != null)
                IconButton(
                  icon: const Icon(Icons.my_location),
                  onPressed: () {},
                  tooltip: 'Center on boat',
                ),
            ],
          ),
          body: FlutterMap(
            options: MapOptions(
              initialCenter: center,
              initialZoom: 16,
              interactionOptions: const InteractionOptions(
                flags: InteractiveFlag.all,
              ),
            ),
            children: [
              TileLayer(
                urlTemplate: 'https://tile.openstreetmap.org/{z}/{x}/{y}.png',
                userAgentPackageName: 'com.riverclean.mobile',
                maxZoom: 19,
                minZoom: 1,
              ),
              if (hasFix)
                MarkerLayer(
                  markers: [
                    Marker(
                      point: center,
                      width: 40,
                      height: 40,
                      child: Container(
                        decoration: BoxDecoration(
                          color: Colors.cyanAccent,
                          shape: BoxShape.circle,
                          border: Border.all(color: Colors.white, width: 2),
                          boxShadow: [
                            BoxShadow(
                              color: Colors.black.withValues(alpha: 0.5),
                              blurRadius: 6,
                              offset: Offset(0, 2),
                            ),
                          ],
                        ),
                        child: const Icon(
                          Icons.navigation,
                          color: Colors.black,
                          size: 24,
                        ),
                      ),
                    ),
                  ],
                ),
              if (hasFix)
                PolylineLayer(
                  polylines: [
                    Polyline(
                      points: [center],
                      color: Colors.cyanAccent,
                      strokeWidth: 3,
                    ),
                  ],
                ),
            ],
          ),
          bottomSheet: _buildBottomSheet(context, t, scheme, hasFix),
        );
      },
    );
  }

  Widget _buildBottomSheet(
      BuildContext context, dynamic t, ColorScheme scheme, bool hasFix) {
    if (hasFix && t != null) {
      return Container(
        padding: const EdgeInsets.all(16),
        decoration: BoxDecoration(
          color: scheme.surfaceContainerLow,
          borderRadius: const BorderRadius.only(
            topLeft: Radius.circular(16),
            topRight: Radius.circular(16),
          ),
        ),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                const Text(
                  'BOAT POSITION',
                  style: TextStyle(
                    fontSize: 11,
                    fontWeight: FontWeight.bold,
                    letterSpacing: 1,
                    color: Colors.grey,
                  ),
                ),
                Container(
                  padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
                  decoration: BoxDecoration(
                    color: Colors.greenAccent.withValues(alpha: 0.2),
                    borderRadius: BorderRadius.circular(6),
                    border: Border.all(color: Colors.greenAccent),
                  ),
                  child: const Text(
                    'GPS FIXED',
                    style: TextStyle(
                      fontSize: 10,
                      fontWeight: FontWeight.bold,
                      color: Colors.greenAccent,
                    ),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 10),
            Text(
              'Lat: ${t.latitude.toStringAsFixed(7)}',
              style: const TextStyle(fontFamily: 'monospace', fontSize: 12),
            ),
            Text(
              'Lon: ${t.longitude.toStringAsFixed(7)}',
              style: const TextStyle(fontFamily: 'monospace', fontSize: 12),
            ),
            Text(
              'Sats: ${t.satellites}  |  Fix age: ${t.gpsAgeMs} ms',
              style: const TextStyle(fontSize: 11, color: Colors.grey),
            ),
            const SizedBox(height: 8),
            Text(
              'No map tiles? Ensure internet/MQTT connectivity for tile downloads.',
              style: TextStyle(fontSize: 9, color: scheme.onSurfaceVariant),
            ),
          ],
        ),
      );
    }
    return Container(
      padding: const EdgeInsets.all(24),
      decoration: BoxDecoration(
        color: scheme.surfaceContainerLow,
        borderRadius: const BorderRadius.only(
          topLeft: Radius.circular(16),
          topRight: Radius.circular(16),
        ),
      ),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          const Icon(Icons.gps_off, size: 48, color: Colors.grey),
          const SizedBox(height: 12),
          Text(
            'No GPS Fix',
            style: TextStyle(
              fontSize: 18,
              fontWeight: FontWeight.bold,
              color: scheme.onSurface,
            ),
          ),
          const SizedBox(height: 8),
          Text(
            'Waiting for GPS satellite fix from ESP32-CAM/main controller.',
            style: TextStyle(fontSize: 12, color: scheme.onSurfaceVariant),
            textAlign: TextAlign.center,
          ),
        ],
      ),
    );
  }
}
