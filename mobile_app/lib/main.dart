import 'package:flutter/material.dart';

import 'screens/camera_screen.dart';
import 'screens/drive_screen.dart';
import 'screens/monitor_screen.dart';
import 'screens/settings_screen.dart';
import 'services/app_settings.dart';
import 'services/connection_service.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await AppSettings.instance.load();
  // Connect after the first frame so the UI can render the status.
  runApp(const RiverCleanApp());
  ConnectionService.instance.connect();
}

class RiverCleanApp extends StatelessWidget {
  const RiverCleanApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'RiverClean',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(
          seedColor: const Color(0xFF00695C),
          brightness: Brightness.dark,
        ),
        useMaterial3: true,
        cardTheme: const CardThemeData(
          elevation: 0,
          margin: EdgeInsets.zero,
        ),
      ),
      home: const HomeShell(),
    );
  }
}

class HomeShell extends StatefulWidget {
  const HomeShell({super.key});

  @override
  State<HomeShell> createState() => _HomeShellState();
}

class _HomeShellState extends State<HomeShell> {
  int _index = 0;

  static const _titles = ['Monitor', 'AI Camera', 'Drive', 'Settings'];

  @override
  Widget build(BuildContext context) {
    final svc = ConnectionService.instance;
    return Scaffold(
      appBar: AppBar(
        title: Row(
          children: [
            const Icon(Icons.sailing),
            const SizedBox(width: 8),
            Text(_titles[_index]),
          ],
        ),
        actions: [
          ListenableBuilder(
            listenable: svc,
            builder: (context, _) => Padding(
              padding: const EdgeInsets.only(right: 14),
              child: Row(
                children: [
                  Container(
                    width: 10,
                    height: 10,
                    decoration: BoxDecoration(
                      color: _statusColor(svc),
                      shape: BoxShape.circle,
                    ),
                  ),
                  const SizedBox(width: 6),
                  Text(
                    _statusLabel(svc),
                    style: const TextStyle(fontSize: 11),
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
      body: IndexedStack(
        index: _index,
        children: [
          const MonitorScreen(),
          const CameraScreen(),
          const DriveScreen(),
          const SettingsScreen(),
        ],
      ),
      bottomNavigationBar: NavigationBar(
        selectedIndex: _index,
        onDestinationSelected: (i) => setState(() => _index = i),
        destinations: const [
          NavigationDestination(
            icon: Icon(Icons.monitor_heart_outlined),
            selectedIcon: Icon(Icons.monitor_heart),
            label: 'Monitor',
          ),
          NavigationDestination(
            icon: Icon(Icons.center_focus_strong_outlined),
            selectedIcon: Icon(Icons.center_focus_strong),
            label: 'AI Camera',
          ),
          NavigationDestination(
            icon: Icon(Icons.gamepad_outlined),
            selectedIcon: Icon(Icons.gamepad),
            label: 'Drive',
          ),
          NavigationDestination(
            icon: Icon(Icons.settings_outlined),
            selectedIcon: Icon(Icons.settings),
            label: 'Settings',
          ),
        ],
      ),
    );
  }

  static Color _statusColor(ConnectionService svc) {
    if (svc.status == LinkStatus.online) {
      return svc.freshness == Freshness.live ? Colors.green : Colors.orange;
    }
    return switch (svc.status) {
      LinkStatus.connecting => Colors.blue,
      LinkStatus.error => Colors.red,
      _ => Colors.grey,
    };
  }

  static String _statusLabel(ConnectionService svc) {
    return switch (svc.status) {
      LinkStatus.online =>
        svc.freshness == Freshness.live ? 'LIVE' : 'STALE',
      LinkStatus.connecting => '…',
      LinkStatus.offline => 'OFFLINE',
      LinkStatus.error => 'ERROR',
      LinkStatus.idle => 'IDLE',
    };
  }
}
