import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';

class SettingsProvider extends ChangeNotifier {
  static const _kThemeMode = 'themeMode';
  static const _kClinicalMode = 'clinicalMode';
  static const _kDevModeUnlocked = 'devModeUnlocked';
  static const _kDevModeEnabled = 'devModeEnabled';
  static const _kLastRollupDate = 'lastRollupDate';

  ThemeMode _themeMode = ThemeMode.system;
  bool _clinicalMode = false;
  bool _devModeUnlocked = false;
  bool _devModeEnabled = false;
  String? _lastRollupDate;

  bool _loaded = false;
  int _devUnlockTaps = 0;

  SettingsProvider() {
    _load();
  }

  bool get isLoaded => _loaded;
  ThemeMode get themeMode => _themeMode;

  /// When OFF, alerts are suppressed and the dashboard shows a "prototype"
  /// banner. Recommended OFF until hardware is permanently mounted.
  bool get clinicalMode => _clinicalMode;

  /// True after user taps "version" 7×. Survives app restarts.
  bool get devModeUnlocked => _devModeUnlocked;

  /// True if dev mode is currently active (visible in UI).
  bool get devModeEnabled => _devModeEnabled && _devModeUnlocked;

  String? get lastRollupDate => _lastRollupDate;

  Future<void> _load() async {
    final p = await SharedPreferences.getInstance();
    final tm = p.getString(_kThemeMode);
    if (tm != null) {
      _themeMode = ThemeMode.values.firstWhere(
        (e) => e.toString() == tm,
        orElse: () => ThemeMode.system,
      );
    }
    _clinicalMode = p.getBool(_kClinicalMode) ?? false;
    _devModeUnlocked = p.getBool(_kDevModeUnlocked) ?? false;
    _devModeEnabled = p.getBool(_kDevModeEnabled) ?? false;
    _lastRollupDate = p.getString(_kLastRollupDate);
    _loaded = true;
    notifyListeners();
  }

  Future<void> setThemeMode(ThemeMode m) async {
    if (_themeMode == m) return;
    _themeMode = m;
    notifyListeners();
    final p = await SharedPreferences.getInstance();
    await p.setString(_kThemeMode, m.toString());
  }

  Future<void> setClinicalMode(bool v) async {
    if (_clinicalMode == v) return;
    _clinicalMode = v;
    notifyListeners();
    final p = await SharedPreferences.getInstance();
    await p.setBool(_kClinicalMode, v);
  }

  Future<void> setDevModeEnabled(bool v) async {
    if (!_devModeUnlocked) return;
    _devModeEnabled = v;
    notifyListeners();
    final p = await SharedPreferences.getInstance();
    await p.setBool(_kDevModeEnabled, v);
  }

  /// Call from a tap on the version label. Returns true after the 7th tap
  /// to let the UI show "developer mode unlocked".
  Future<bool> registerVersionTap() async {
    if (_devModeUnlocked) return false;
    _devUnlockTaps++;
    if (_devUnlockTaps >= 7) {
      _devModeUnlocked = true;
      _devModeEnabled = true;
      _devUnlockTaps = 0;
      notifyListeners();
      final p = await SharedPreferences.getInstance();
      await p.setBool(_kDevModeUnlocked, true);
      await p.setBool(_kDevModeEnabled, true);
      return true;
    }
    return false;
  }

  /// Hide developer-mode entry points and forget the unlock.
  Future<void> lockDevMode() async {
    _devModeUnlocked = false;
    _devModeEnabled = false;
    _devUnlockTaps = 0;
    notifyListeners();
    final p = await SharedPreferences.getInstance();
    await p.setBool(_kDevModeUnlocked, false);
    await p.setBool(_kDevModeEnabled, false);
  }

  Future<void> setLastRollupDate(String date) async {
    _lastRollupDate = date;
    final p = await SharedPreferences.getInstance();
    await p.setString(_kLastRollupDate, date);
  }

  /// Tap counter visible in UI for feedback ("4 más para activar dev mode").
  int get devUnlockTapsRemaining => _devModeUnlocked ? 0 : (7 - _devUnlockTaps).clamp(0, 7);
}
