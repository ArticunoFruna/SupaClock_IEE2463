import 'package:cloud_firestore/cloud_firestore.dart';

/// Daily aggregated stats — one doc per day per user.
/// Path: users/{uid}/dailyStats/{YYYY-MM-DD}
class DailyStatsModel {
  final String date; // YYYY-MM-DD
  final int steps;
  final int activeMinutes;
  final HrStats hr;
  final Spo2Stats spo2;
  final TempStats temp;
  final HrZoneMinutes hrZones;
  final DateTime computedAt;
  final int sourceVersion;

  DailyStatsModel({
    required this.date,
    this.steps = 0,
    this.activeMinutes = 0,
    HrStats? hr,
    Spo2Stats? spo2,
    TempStats? temp,
    HrZoneMinutes? hrZones,
    DateTime? computedAt,
    this.sourceVersion = 1,
  })  : hr = hr ?? HrStats.empty(),
        spo2 = spo2 ?? Spo2Stats.empty(),
        temp = temp ?? TempStats.empty(),
        hrZones = hrZones ?? HrZoneMinutes.empty(),
        computedAt = computedAt ?? DateTime.now();

  factory DailyStatsModel.fromFirestore(DocumentSnapshot doc) {
    final data = doc.data() as Map<String, dynamic>;
    return DailyStatsModel(
      date: doc.id,
      steps: data['steps'] ?? 0,
      activeMinutes: data['activeMinutes'] ?? 0,
      hr: HrStats.fromMap(data['hr'] as Map<String, dynamic>?),
      spo2: Spo2Stats.fromMap(data['spo2'] as Map<String, dynamic>?),
      temp: TempStats.fromMap(data['temp'] as Map<String, dynamic>?),
      hrZones: HrZoneMinutes.fromMap(data['hrZoneMinutes'] as Map<String, dynamic>?),
      computedAt: (data['computedAt'] as Timestamp?)?.toDate() ?? DateTime.now(),
      sourceVersion: data['sourceVersion'] ?? 1,
    );
  }

  Map<String, dynamic> toFirestore() => {
        'steps': steps,
        'activeMinutes': activeMinutes,
        'hr': hr.toMap(),
        'spo2': spo2.toMap(),
        'temp': temp.toMap(),
        'hrZoneMinutes': hrZones.toMap(),
        'computedAt': Timestamp.fromDate(computedAt),
        'sourceVersion': sourceVersion,
      };
}

class HrStats {
  final double? avg;
  final int? min;
  final int? max;
  final int? resting;
  final int samples;

  HrStats({this.avg, this.min, this.max, this.resting, this.samples = 0});
  factory HrStats.empty() => HrStats();
  factory HrStats.fromMap(Map<String, dynamic>? m) {
    if (m == null) return HrStats.empty();
    return HrStats(
      avg: (m['avg'] as num?)?.toDouble(),
      min: m['min'] as int?,
      max: m['max'] as int?,
      resting: m['resting'] as int?,
      samples: m['samples'] ?? 0,
    );
  }
  Map<String, dynamic> toMap() => {
        if (avg != null) 'avg': avg,
        if (min != null) 'min': min,
        if (max != null) 'max': max,
        if (resting != null) 'resting': resting,
        'samples': samples,
      };
}

class Spo2Stats {
  final double? avg;
  final int? min;
  final int samples;

  Spo2Stats({this.avg, this.min, this.samples = 0});
  factory Spo2Stats.empty() => Spo2Stats();
  factory Spo2Stats.fromMap(Map<String, dynamic>? m) {
    if (m == null) return Spo2Stats.empty();
    return Spo2Stats(
      avg: (m['avg'] as num?)?.toDouble(),
      min: m['min'] as int?,
      samples: m['samples'] ?? 0,
    );
  }
  Map<String, dynamic> toMap() => {
        if (avg != null) 'avg': avg,
        if (min != null) 'min': min,
        'samples': samples,
      };
}

class TempStats {
  final double? avg;
  final double? min;
  final double? max;
  final int samples;

  TempStats({this.avg, this.min, this.max, this.samples = 0});
  factory TempStats.empty() => TempStats();
  factory TempStats.fromMap(Map<String, dynamic>? m) {
    if (m == null) return TempStats.empty();
    return TempStats(
      avg: (m['avg'] as num?)?.toDouble(),
      min: (m['min'] as num?)?.toDouble(),
      max: (m['max'] as num?)?.toDouble(),
      samples: m['samples'] ?? 0,
    );
  }
  Map<String, dynamic> toMap() => {
        if (avg != null) 'avg': avg,
        if (min != null) 'min': min,
        if (max != null) 'max': max,
        'samples': samples,
      };
}

/// Minutes spent in each HR zone (zone calculation: % of HRmax = 220 - age).
/// z1: 50-60%, z2: 60-70%, z3: 70-80%, z4: 80-90%, z5: 90-100%
class HrZoneMinutes {
  final int z1, z2, z3, z4, z5;
  HrZoneMinutes({this.z1 = 0, this.z2 = 0, this.z3 = 0, this.z4 = 0, this.z5 = 0});
  factory HrZoneMinutes.empty() => HrZoneMinutes();
  factory HrZoneMinutes.fromMap(Map<String, dynamic>? m) {
    if (m == null) return HrZoneMinutes.empty();
    return HrZoneMinutes(
      z1: m['z1'] ?? 0,
      z2: m['z2'] ?? 0,
      z3: m['z3'] ?? 0,
      z4: m['z4'] ?? 0,
      z5: m['z5'] ?? 0,
    );
  }
  Map<String, dynamic> toMap() => {'z1': z1, 'z2': z2, 'z3': z3, 'z4': z4, 'z5': z5};

  int get total => z1 + z2 + z3 + z4 + z5;
}
