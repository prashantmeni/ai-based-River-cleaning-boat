import 'package:shared_preferences/shared_preferences.dart';

/// App settings, persisted with shared_preferences.
/// Includes endpoint URL, manual drive PWM, command duration, and
/// Wi-Fi network provisioning for the boat (SSID & Password).
class AppSettings {
  AppSettings._();
  static final AppSettings instance = AppSettings._();

  static const _kMqttUrl = 'riverclean.mqttUrl';
  static const _kManualPwm = 'riverclean.manualPwm';
  static const _kDurationMs = 'riverclean.durationMs';
  static const _kWifiSsid = 'riverclean.wifiSsid';
  static const _kWifiPassword = 'riverclean.wifiPassword';

  String endpoint = 'wss://broker.hivemq.com:8884/mqtt';
  int manualPwm = 150;
  int commandDurationMs = 600;
  String wifiSsid = 'Laxmi_Venkateshwar_Krupa';
  String wifiPassword = '';

  Future<void> load() async {
    final p = await SharedPreferences.getInstance();
    endpoint = p.getString(_kMqttUrl) ?? endpoint;
    manualPwm = p.getInt(_kManualPwm) ?? manualPwm;
    commandDurationMs = p.getInt(_kDurationMs) ?? commandDurationMs;
    wifiSsid = p.getString(_kWifiSsid) ?? wifiSsid;
    wifiPassword = p.getString(_kWifiPassword) ?? wifiPassword;
  }

  Future<void> save() async {
    final p = await SharedPreferences.getInstance();
    await p.setString(_kMqttUrl, endpoint);
    await p.setInt(_kManualPwm, manualPwm);
    await p.setInt(_kDurationMs, commandDurationMs);
    await p.setString(_kWifiSsid, wifiSsid);
    await p.setString(_kWifiPassword, wifiPassword);
  }

  Future<void> reset() async {
    final p = await SharedPreferences.getInstance();
    await p.remove(_kMqttUrl);
    await p.remove('riverclean.streamUrl');
    await p.remove(_kManualPwm);
    await p.remove(_kDurationMs);
    await p.remove(_kWifiSsid);
    await p.remove(_kWifiPassword);
    endpoint = 'wss://broker.hivemq.com:8884/mqtt';
    manualPwm = 150;
    commandDurationMs = 600;
    wifiSsid = 'Laxmi_Venkateshwar_Krupa';
    wifiPassword = '';
  }
}
