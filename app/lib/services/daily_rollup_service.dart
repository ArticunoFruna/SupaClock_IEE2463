import 'dart:math' as math;

import 'package:flutter/foundation.dart';
import 'package:intl/intl.dart';

import '../models/daily_stats_model.dart';
import 'firestore_service.dart';
import 'local_store.dart';

/// Closes out any pending day(s) in `daily_pending` and pushes the rollup
/// to Firestore as `users/{uid}/dailyStats/{YYYY-MM-DD}`.
class DailyRollupService {
  final FirestoreService _fs;

  DailyRollupService(this._fs);

  /// Run on app start and on app-resumed. Only writes to Firestore when a
  /// day is fully closed (today != dateKey).
  Future<void> runRollupIfNeeded(String uid) async {
    final today = _dateKey(DateTime.now());
    final daysWithData = LocalStore.daysWithData();
    for (final dk in daysWithData) {
      if (dk == today) continue; // current day still open
      await _flushDay(uid, dk);
    }
  }

  /// Force-flush today (e.g. user wants to see numbers immediately).
  Future<void> flushTodaySnapshot(String uid) async {
    final dk = _dateKey(DateTime.now());
    final stats = _aggregate(dk);
    if (stats == null) return;
    try {
      await _fs.upsertDailyStats(uid, stats);
    } catch (e) {
      debugPrint('flushTodaySnapshot: $e');
    }
  }

  Future<void> _flushDay(String uid, String dateKey) async {
    final stats = _aggregate(dateKey);
    if (stats == null) {
      await LocalStore.clearDay(dateKey);
      return;
    }
    try {
      await _fs.upsertDailyStats(uid, stats);
      await LocalStore.clearDay(dateKey);
    } catch (e) {
      await LocalStore.enqueue('dailyStats', stats.toJson());
      debugPrint('rollup queued offline: $e');
    }
  }

  DailyStatsModel? _aggregate(String dateKey) {
    final day = LocalStore.getDay(dateKey);
    final hr = (day['hrSamples'] as List).cast<int>();
    final spo2 = (day['spo2Samples'] as List).cast<int>();
    final temp = (day['tempSamples'] as List).cast<double>();
    final zones = (day['hrZoneSeconds'] as List).cast<int>();
    final steps = day['steps'] as int? ?? 0;

    if (hr.isEmpty && spo2.isEmpty && temp.isEmpty && steps == 0) return null;

    final hrStats = hr.isEmpty
        ? HrStats.empty()
        : HrStats(
            avg: hr.reduce((a, b) => a + b) / hr.length,
            min: hr.reduce(math.min),
            max: hr.reduce(math.max),
            resting: _restingHr(hr),
            samples: hr.length,
          );

    final spo2Stats = spo2.isEmpty
        ? Spo2Stats.empty()
        : Spo2Stats(
            avg: spo2.reduce((a, b) => a + b) / spo2.length,
            min: spo2.reduce(math.min),
            samples: spo2.length,
          );

    final tempStats = temp.isEmpty
        ? TempStats.empty()
        : TempStats(
            avg: temp.reduce((a, b) => a + b) / temp.length,
            min: temp.reduce(math.min),
            max: temp.reduce(math.max),
            samples: temp.length,
          );

    // hrZoneSeconds → minutes
    final hrZones = HrZoneMinutes(
      z1: (zones[0] / 60).round(),
      z2: (zones[1] / 60).round(),
      z3: (zones[2] / 60).round(),
      z4: (zones[3] / 60).round(),
      z5: (zones[4] / 60).round(),
    );

    final activeMin = hrZones.total;

    return DailyStatsModel(
      date: dateKey,
      steps: steps,
      activeMinutes: activeMin,
      hr: hrStats,
      spo2: spo2Stats,
      temp: tempStats,
      hrZones: hrZones,
      hourly: _hourly(day),
      computedAt: DateTime.now(),
    );
  }

  /// Average each per-hour accumulator bucket into a sparse list of
  /// [HourPoint]s (empty hours are dropped to keep the doc small).
  List<HourPoint> _hourly(Map<String, dynamic> day) {
    final raw = (day['hourly'] as List?) ?? const [];
    final out = <HourPoint>[];
    for (var h = 0; h < raw.length && h < 24; h++) {
      final b = Map<String, dynamic>.from(raw[h] as Map);
      final hrCount = (b['hrCount'] as int?) ?? 0;
      final spo2Count = (b['spo2Count'] as int?) ?? 0;
      final tempCount = (b['tempCount'] as int?) ?? 0;
      final steps = (b['steps'] as int?) ?? 0;

      final point = HourPoint(
        hour: h,
        hr: hrCount > 0 ? (b['hrSum'] as num) / hrCount : null,
        spo2: spo2Count > 0 ? (b['spo2Sum'] as num) / spo2Count : null,
        temp: tempCount > 0 ? (b['tempSum'] as num) / tempCount : null,
        steps: steps,
      );
      if (!point.isEmpty) out.add(point);
    }
    return out;
  }

  /// Resting HR estimate: take the bottom 5 % of the day's samples (excluding
  /// suspiciously-low <40 bpm noise) and return their median.
  int? _restingHr(List<int> samples) {
    final clean = samples.where((v) => v >= 40 && v <= 200).toList()..sort();
    if (clean.length < 50) return null;
    final tail = clean.sublist(0, math.max(10, (clean.length * 0.05).round()));
    return tail[tail.length ~/ 2];
  }

  static String _dateKey(DateTime t) => DateFormat('yyyy-MM-dd').format(t);
}
