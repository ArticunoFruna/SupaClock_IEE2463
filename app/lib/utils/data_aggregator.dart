import '../models/daily_stats_model.dart';

enum TimeFrame { today, week, month }

class DataPoint {
  final double x;
  final double y;
  final DateTime originalTime;

  DataPoint(this.x, this.y, this.originalTime);
}

class AggregatedMetrics {
  final List<DataPoint> heartRate;
  final List<DataPoint> spo2;
  final List<DataPoint> temperature;
  final List<DataPoint> steps;

  AggregatedMetrics({
    required this.heartRate,
    required this.spo2,
    required this.temperature,
    required this.steps,
  });

  static AggregatedMetrics empty() =>
      AggregatedMetrics(heartRate: [], spo2: [], temperature: [], steps: []);
}

/// Builds chart series from edge-summarised data — never from raw samples.
///
/// • **Hoy**  → the in-progress day's per-hour buckets, read live from Hive
///   (`LocalStore.getDay`). X axis = hour of day (0..23).
/// • **Semana / Mes** → already-closed `dailyStats/{date}` docs from Firestore.
///   X axis = weekday (1..7) or day-of-month (1..31). One point per day.
class DataAggregator {
  // ── Hoy: per-hour buckets from the live Hive accumulator ──────────────
  /// [day] is the raw Hive map from `LocalStore.getDay(today)`, whose `hourly`
  /// entries hold sum/count accumulators (not yet averaged).
  static AggregatedMetrics todayFromHive(Map<String, dynamic> day, DateTime today) {
    final raw = (day['hourly'] as List?) ?? const [];

    final hr = <DataPoint>[];
    final spo2 = <DataPoint>[];
    final temp = <DataPoint>[];
    final steps = <DataPoint>[];

    int lastCumulative = 0; // running step total, to derive per-hour deltas

    for (var h = 0; h < raw.length && h < 24; h++) {
      final b = Map<String, dynamic>.from(raw[h] as Map);
      final at = DateTime(today.year, today.month, today.day, h);
      final x = h.toDouble();

      final hrCount = (b['hrCount'] as int?) ?? 0;
      if (hrCount > 0) {
        hr.add(DataPoint(x, (b['hrSum'] as num) / hrCount, at));
      }
      final spo2Count = (b['spo2Count'] as int?) ?? 0;
      if (spo2Count > 0) {
        spo2.add(DataPoint(x, (b['spo2Sum'] as num) / spo2Count, at));
      }
      final tempCount = (b['tempCount'] as int?) ?? 0;
      if (tempCount > 0) {
        temp.add(DataPoint(x, (b['tempSum'] as num) / tempCount, at));
      }

      final cumulative = (b['steps'] as int?) ?? 0;
      if (cumulative > 0) {
        final delta = cumulative - lastCumulative;
        if (delta > 0) steps.add(DataPoint(x, delta.toDouble(), at));
        lastCumulative = cumulative;
      }
    }

    return AggregatedMetrics(heartRate: hr, spo2: spo2, temperature: temp, steps: steps);
  }

  // ── Semana / Mes: one point per closed day ────────────────────────────
  static AggregatedMetrics fromDailyStats(
    List<DailyStatsModel> days,
    TimeFrame timeFrame,
    DateTime referenceDate,
  ) {
    if (days.isEmpty) return AggregatedMetrics.empty();

    final hr = <DataPoint>[];
    final spo2 = <DataPoint>[];
    final temp = <DataPoint>[];
    final steps = <DataPoint>[];

    for (final d in days) {
      final date = DateTime.tryParse(d.date);
      if (date == null || !_inWindow(date, timeFrame, referenceDate)) continue;

      final x = (timeFrame == TimeFrame.week ? date.weekday : date.day).toDouble();

      if (d.hr.avg != null && d.hr.avg! > 0) hr.add(DataPoint(x, d.hr.avg!, date));
      if (d.spo2.avg != null && d.spo2.avg! > 0) spo2.add(DataPoint(x, d.spo2.avg!, date));
      if (d.temp.avg != null && d.temp.avg! > 0) temp.add(DataPoint(x, d.temp.avg!, date));
      if (d.steps > 0) steps.add(DataPoint(x, d.steps.toDouble(), date));
    }

    // Charts expect ascending X.
    for (final list in [hr, spo2, temp, steps]) {
      list.sort((a, b) => a.x.compareTo(b.x));
    }

    return AggregatedMetrics(heartRate: hr, spo2: spo2, temperature: temp, steps: steps);
  }

  static bool _inWindow(DateTime date, TimeFrame tf, DateTime ref) {
    switch (tf) {
      case TimeFrame.today:
        return date.year == ref.year && date.month == ref.month && date.day == ref.day;
      case TimeFrame.week:
        final startOfWeek = DateTime(ref.year, ref.month, ref.day)
            .subtract(Duration(days: ref.weekday - 1));
        final d = DateTime(date.year, date.month, date.day);
        return !d.isBefore(startOfWeek) && d.difference(startOfWeek).inDays <= 6;
      case TimeFrame.month:
        return date.year == ref.year && date.month == ref.month;
    }
  }
}
