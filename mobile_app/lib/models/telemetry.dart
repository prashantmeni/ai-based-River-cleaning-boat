/// Telemetry model — mirrors firmware/esp32_main_controller/src/telemetry.cpp
/// and dashboard-ui/src/services/communication/types.ts exactly.
/// Never invent fields the firmware does not send.
library;

class Telemetry {
  const Telemetry({
    required this.state,
    required this.commsOk,
    required this.estop,
    required this.autonomous,
    required this.detection,
    required this.confidence,
    required this.classId,
    required this.centerX,
    required this.leftCm,
    required this.rightCm,
    required this.binFull,
    required this.binDistanceCm,
    required this.gpsFix,
    required this.latitude,
    required this.longitude,
    required this.satellites,
    required this.gpsAgeMs,
    required this.collectionCount,
    required this.bottleCount,
    required this.bagCount,
    required this.debrisCount,
    required this.tdsPpm,
    required this.tdsMv,
    required this.conveyor,
    required this.faultFlags,
    required this.uptimeMs,
    required this.leftPwm,
    required this.rightPwm,
    required this.leftDir,
    required this.rightDir,
    required this.rxBytes,
    required this.rxErrors,
    required this.perr,
    required this.drops,
    required this.dbg,
  });

  final String state;
  final bool commsOk;
  final bool estop;
  final bool autonomous;
  final bool detection;
  final int confidence;
  final int classId;
  final int centerX;
  final double leftCm;
  final double rightCm;
  final bool binFull;
  final double binDistanceCm;
  final bool gpsFix;
  final double latitude;
  final double longitude;
  final int satellites;
  final int gpsAgeMs;
  final int collectionCount;
  final int bottleCount;
  final int bagCount;
  final int debrisCount;
  final int tdsPpm;
  final int tdsMv;
  final String conveyor;
  final int faultFlags;
  final int uptimeMs;
  final int leftPwm;
  final int rightPwm;
  final int leftDir;
  final int rightDir;
  final int rxBytes;
  final int rxErrors;
  final int perr;
  final int drops;
  final String dbg;

  factory Telemetry.fromJson(Map<String, dynamic> j) => Telemetry(
        state: _s(j['state'], 'UNKNOWN'),
        commsOk: j['comms_ok'] == true,
        estop: j['estop'] == true,
        autonomous: j['autonomous'] == true,
        detection: j['detection'] == true,
        confidence: _i(j['confidence']),
        classId: _i(j['class_id']),
        centerX: _i(j['center_x']),
        leftCm: _d(j['left_cm'], -1),
        rightCm: _d(j['right_cm'], -1),
        binFull: j['bin_full'] == true,
        binDistanceCm: _d(j['bin_distance_cm'], -1),
        gpsFix: j['gps_fix'] == true,
        latitude: _d(j['latitude']),
        longitude: _d(j['longitude']),
        satellites: _i(j['satellites']),
        gpsAgeMs: _i(j['gps_age_ms']),
        collectionCount: _i(j['collection_count']),
        bottleCount: _i(j['bottle_count']),
        bagCount: _i(j['bag_count']),
        debrisCount: _i(j['debris_count']),
        tdsPpm: _i(j['tds_ppm'], -1), // -1 = invalid/unavailable
        tdsMv: _i(j['tds_mv']),
        conveyor: _s(j['conveyor'], 'OFF'),
        faultFlags: _i(j['fault_flags']),
        uptimeMs: _i(j['uptime_ms']),
        leftPwm: _i(j['left_pwm']),
        rightPwm: _i(j['right_pwm']),
        leftDir: _i(j['left_dir']),
        rightDir: _i(j['right_dir']),
        rxBytes: _i(j['rx_bytes']),
        rxErrors: _i(j['rx_errors']),
        perr: _i(j['perr']),
        drops: _i(j['drops']),
        dbg: _s(j['dbg']),
      );

  static int _i(dynamic v, [int d = 0]) => v is num ? v.toInt() : d;
  static double _d(dynamic v, [double d = 0]) => v is num ? v.toDouble() : d;
  static String _s(dynamic v, [String d = '']) => v is String ? v : d;

  /// Fault bits from firmware safety.h.
  static const Map<int, String> faultNames = {
    0x01: 'COMMS_LOST',
    0x02: 'OBSTACLE',
    0x04: 'SENSOR_BAD',
    0x08: 'GPS_NO_FIX',
    0x10: 'WATCHDOG',
  };

  List<String> get activeFaults => [
        for (final e in faultNames.entries)
          if (faultFlags & e.key != 0) e.value,
      ];

  /// True when the nav state latches motors off (mirrors DrivePage gating).
  bool get safetyLatched =>
      estop || state == 'EMERGENCY_STOP' || state == 'FAULT';

  String get classLabel => const {
        0: 'Background',
        1: 'Plastic bottle',
        2: 'Plastic bag',
        3: 'General debris',
      }[classId] ??
      'Unknown';
}

String formatUptime(int ms) {
  final s = ms ~/ 1000;
  final h = s ~/ 3600;
  final m = (s % 3600) ~/ 60;
  final sec = s % 60;
  if (h > 0) return '${h}h ${m}m ${sec}s';
  if (m > 0) return '${m}m ${sec}s';
  return '${sec}s';
}

String formatBytes(int n) {
  if (n < 1024) return '$n B';
  if (n < 1024 * 1024) return '${(n / 1024).toStringAsFixed(1)} KB';
  return '${(n / (1024 * 1024)).toStringAsFixed(2)} MB';
}
