import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:intl/intl.dart';

import '../models/alert_model.dart';
import '../models/user_model.dart';
import 'auth_service.dart';
import 'ble_service.dart';
import 'firestore_service.dart';
import 'local_store.dart';
import 'notifications_service.dart';

/// Quality gate for individual TLV samples (HR / SpO2).
/// Firmware reports a 0..100 byte; below this we treat the sample as suspect
/// and either ignore (clinical mode off) or surface as "weak signal".
const int _kQualityGate = 60;

/// Subscribes to [BleService.telemetryStream] and:
///   • appends valid samples to today's `daily_pending` Hive bucket
///   • evaluates alert thresholds (gated by clinicalMode + quality + stability)
///   • counts seconds spent in each HR zone for the dailyStats rollup
class TelemetryCollector {
  final BleService _ble;
  final FirestoreService _fs;
  final AuthService _auth;
  final NotificationsService _notif;

  StreamSubscription? _sub;

  // ── runtime state ──
  UserModel? _user;
  bool _clinicalMode = false;

  DateTime? _hrHighSince;
  DateTime? _hrLowSince;
  DateTime? _spo2LowSince;
  DateTime? _tempHighSince;

  DateTime? _lastSecondTick;

  TelemetryCollector(this._ble, this._fs, this._auth, this._notif);

  void start() {
    _sub?.cancel();
    _sub = _ble.telemetryStream.listen(_onTelemetry);
  }

  void stop() {
    _sub?.cancel();
    _sub = null;
  }

  /// Update collector context. Pass `user` to replace the cached profile
  /// (e.g. after a Firestore stream tick); pass `clinicalMode` to flip
  /// alert evaluation. Both are independent — leave one out and the
  /// previous value is kept.
  void updateContext({UserModel? user, bool? clinicalMode}) {
    if (user != null) _user = user;
    if (clinicalMode != null) _clinicalMode = clinicalMode;
  }

  Future<void> _onTelemetry(SupaClockTelemetry t) async {
    final now = DateTime.now();
    final dateKey = _dateKey(now);
    final day = LocalStore.getDay(dateKey);

    final hr = t.heartRate ?? 0;
    final hrQ = t.hrQuality ?? 0;
    final spo2 = t.spo2 ?? 0;
    final spo2Q = t.spo2Quality ?? 0;
    final temp = t.temperature ?? 0.0;
    final steps = t.steps ?? 0;

    var dirty = false;

    // Per-hour accumulator for the intraday curve (hourly[0..23]).
    final hour = now.hour;
    final hourly = (day['hourly'] as List);
    final hb = Map<String, dynamic>.from(hourly[hour] as Map);
    var hbDirty = false;

    if (hr > 0 && hrQ >= _kQualityGate) {
      (day['hrSamples'] as List).add(hr);
      _accumulateHrZone(day, hr, now);
      hb['hrSum'] = (hb['hrSum'] as num) + hr;
      hb['hrCount'] = (hb['hrCount'] as int) + 1;
      hbDirty = true;
      dirty = true;
    }
    if (spo2 > 0 && spo2Q >= _kQualityGate) {
      (day['spo2Samples'] as List).add(spo2);
      hb['spo2Sum'] = (hb['spo2Sum'] as num) + spo2;
      hb['spo2Count'] = (hb['spo2Count'] as int) + 1;
      hbDirty = true;
      dirty = true;
    }
    if (temp > 30 && temp < 45) {
      (day['tempSamples'] as List).add(temp);
      hb['tempSum'] = (hb['tempSum'] as num) + temp;
      hb['tempCount'] = (hb['tempCount'] as int) + 1;
      hbDirty = true;
      dirty = true;
    }
    if (steps > 0 && steps != day['steps']) {
      day['steps'] = steps;
      hb['steps'] = steps; // cumulative snapshot within this hour
      hbDirty = true;
      dirty = true;
    }

    if (hbDirty) {
      hourly[hour] = hb;
      day['hourly'] = hourly;
    }

    if (dirty) {
      day['lastTouchEpoch'] = now.millisecondsSinceEpoch;
      await LocalStore.putDay(dateKey, day);
    }

    if (_clinicalMode) {
      await _evaluateAlerts(t, now);
    } else {
      // Clear stability timers so we don't fire stale alerts when user
      // re-enables clinical mode after a noisy period.
      _hrHighSince = null;
      _hrLowSince = null;
      _spo2LowSince = null;
      _tempHighSince = null;
    }
  }

  void _accumulateHrZone(Map<String, dynamic> day, int hr, DateTime now) {
    final hrMax = _user?.hrMax ?? 190;
    final pct = hr / hrMax;
    int zone;
    if (pct < 0.50) {
      return; // below z1, ignore
    } else if (pct < 0.60) {
      zone = 0;
    } else if (pct < 0.70) {
      zone = 1;
    } else if (pct < 0.80) {
      zone = 2;
    } else if (pct < 0.90) {
      zone = 3;
    } else {
      zone = 4;
    }

    // Add 1 second per zone tick (the watch's HR record cadence is roughly 1 Hz).
    final last = _lastSecondTick;
    if (last == null || now.difference(last).inMilliseconds >= 900) {
      final zones = (day['hrZoneSeconds'] as List).cast<int>();
      zones[zone] = (zones[zone]) + 1;
      day['hrZoneSeconds'] = zones;
      _lastSecondTick = now;
    }
  }

  Future<void> _evaluateAlerts(SupaClockTelemetry t, DateTime now) async {
    final user = _user;
    if (user == null) return;
    final th = user.thresholds;
    final stab = Duration(seconds: th.stabilitySeconds);

    final hr = t.heartRate ?? 0;
    final hrQ = t.hrQuality ?? 0;
    final spo2 = t.spo2 ?? 0;
    final spo2Q = t.spo2Quality ?? 0;
    final temp = t.temperature ?? 0.0;
    final batSoc = t.batterySoc ?? 100;

    // High HR
    if (hr > 0 && hrQ >= _kQualityGate && hr > th.hrHigh) {
      _hrHighSince ??= now;
      if (now.difference(_hrHighSince!) >= stab) {
        await _fireAlert(AlertType.hrHigh, hr.toDouble(), th.hrHigh.toDouble());
        _hrHighSince = now.add(stab); // re-arm after stability window
      }
    } else {
      _hrHighSince = null;
    }

    // Low HR
    if (hr > 0 && hrQ >= _kQualityGate && hr < th.hrLow) {
      _hrLowSince ??= now;
      if (now.difference(_hrLowSince!) >= stab) {
        await _fireAlert(AlertType.hrLow, hr.toDouble(), th.hrLow.toDouble());
        _hrLowSince = now.add(stab);
      }
    } else {
      _hrLowSince = null;
    }

    // Low SpO2
    if (spo2 > 0 && spo2Q >= _kQualityGate && spo2 < th.spo2Low) {
      _spo2LowSince ??= now;
      if (now.difference(_spo2LowSince!) >= stab) {
        await _fireAlert(AlertType.spo2Low, spo2.toDouble(), th.spo2Low.toDouble());
        _spo2LowSince = now.add(stab);
      }
    } else {
      _spo2LowSince = null;
    }

    // High temperature
    if (temp > 30 && temp > th.tempHigh) {
      _tempHighSince ??= now;
      if (now.difference(_tempHighSince!) >= stab) {
        await _fireAlert(AlertType.tempHigh, temp, th.tempHigh);
        _tempHighSince = now.add(stab);
      }
    } else {
      _tempHighSince = null;
    }

    // Low battery — fires once per crossing (no stability window needed)
    if (batSoc > 0 && batSoc < th.batteryLow) {
      final lastFiredKey = 'alert_battery_low_at_pct';
      final lastPct = LocalStore.getMeta<int>(lastFiredKey) ?? -1;
      if (lastPct != batSoc) {
        await _fireAlert(AlertType.batteryLow, batSoc.toDouble(), th.batteryLow.toDouble());
        await LocalStore.putMeta(lastFiredKey, batSoc);
      }
    }
  }

  Future<void> _fireAlert(AlertType type, double value, double threshold) async {
    final uid = _auth.currentUser?.uid;
    if (uid == null) return;
    final alert = AlertModel(
      id: '',
      type: type,
      value: value,
      threshold: threshold,
      triggeredAt: DateTime.now(),
    );

    try {
      await _fs.createAlert(uid, alert);
    } catch (e) {
      await LocalStore.enqueue('alert', alert.toJson());
      debugPrint('Alert queue (offline): $e');
    }

    await _notif.showAlert(type, value);
  }

  static String _dateKey(DateTime t) => DateFormat('yyyy-MM-dd').format(t);
}
