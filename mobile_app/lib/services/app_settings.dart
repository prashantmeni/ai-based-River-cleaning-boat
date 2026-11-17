import 'package:shared_preferences/shared_preferences.dart';

/// App settings, persisted with shared_preferences.
class AppSettings {
  AppSettings._();
  static final AppSettings instance = AppSettings._();

  static const _kMqttUrl = 'riverclean.mqttUrl';
  static const _kManualPwm = 'riverclean.manualPwm';
  static const _kCameraStreamUrl = 'riverclean.cameraStreamUrl';
  static const _kWifiSsid = 'riverclean.wifiSsid';
  static const _kWifiPassword = 'riverclean.wifiPassword';
  static const _kFlashAuto = 'riverclean.flashAuto';
  static const _kFlashManual = 'riverclean.flashManual';
  static const _kMapProvider = 'riverclean.mapProvider';
  static const _kSerialLogEnabled = 'riverclean.serialLogEnabled';
  static const _kBtDeviceAddress = 'riverclean.btDeviceAddress';

  String endpoint = 'wss://broker.hivemq.com:8884/mqtt';
  int manualPwm = 150;
  String cameraStreamUrl = '';
  String wifiSsid = 'Laxmi_Venkateshwar_Krupa';
  String wifiPassword = '';

  bool flashAuto = false;
  bool flashManual = false;
  String mapProvider = 'osm';
  bool serialLogEnabled = true;
  String btDeviceAddress = '';

  Future<void> load() async {
    final p = await SharedPreferences.getInstance();
    endpoint = p.getString(_kMqttUrl) ?? endpoint;
    manualPwm = p.getInt(_kManualPwm) ?? manualPwm;
    cameraStreamUrl = p.getString(_kCameraStreamUrl) ?? cameraStreamUrl;
    wifiSsid = p.getString(_kWifiSsid) ?? wifiSsid;
    wifiPassword = p.getString(_kWifiPassword) ?? wifiPassword;
    flashAuto = p.getBool(_kFlashAuto) ?? flashAuto;
    flashManual = p.getBool(_kFlashManual) ?? flashManual;
    mapProvider = p.getString(_kMapProvider) ?? mapProvider;
    serialLogEnabled = p.getBool(_kSerialLogEnabled) ?? serialLogEnabled;
    btDeviceAddress = p.getString(_kBtDeviceAddress) ?? btDeviceAddress;
  }

  Future<void> save() async {
    final p = await SharedPreferences.getInstance();
    await p.setString(_kMqttUrl, endpoint);
    await p.setInt(_kManualPwm, manualPwm);
    await p.setString(_kCameraStreamUrl, cameraStreamUrl);
    await p.setString(_kWifiSsid, wifiSsid);
    await p.setString(_kWifiPassword, wifiPassword);
    await p.setBool(_kFlashAuto, flashAuto);
    await p.setBool(_kFlashManual, flashManual);
    await p.setString(_kMapProvider, mapProvider);
    await p.setBool(_kSerialLogEnabled, serialLogEnabled);
    await p.setString(_kBtDeviceAddress, btDeviceAddress);
  }

  Future<void> reset() async {
    final p = await SharedPreferences.getInstance();
    await p.remove(_kMqttUrl);
    await p.remove(_kManualPwm);
    await p.remove(_kCameraStreamUrl);
    await p.remove(_kWifiSsid);
    await p.remove(_kWifiPassword);
    await p.remove(_kFlashAuto);
    await p.remove(_kFlashManual);
    await p.remove(_kMapProvider);
    await p.remove(_kSerialLogEnabled);
    await p.remove(_kBtDeviceAddress);
    endpoint = 'wss://broker.hivemq.com:8884/mqtt';
    manualPwm = 150;
    cameraStreamUrl = '';
    wifiSsid = 'Laxmi_Venkateshwar_Krupa';
    wifiPassword = '';
    flashAuto = false;
    flashManual = false;
    mapProvider = 'osm';
    serialLogEnabled = true;
    btDeviceAddress = '';
  }
}
