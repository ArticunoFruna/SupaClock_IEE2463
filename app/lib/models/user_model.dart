import 'package:cloud_firestore/cloud_firestore.dart';

/// User profile model — Firestore: users/{userId}
class UserModel {
  final String uid;
  final String name;
  final String? nickname;
  final int? age;
  final String? sex; // 'm' / 'f' / 'x'
  final double? weight; // kg
  final double? height; // cm
  final DateTime createdAt;
  final DateTime? lastActive;

  // Wearable hardware settings (synced TO the watch)
  final Map<String, dynamic>? wearableSettings;

  // Per-user thresholds for alerts (in app)
  final AlertThresholds thresholds;

  UserModel({
    required this.uid,
    required this.name,
    this.nickname,
    this.age,
    this.sex,
    this.weight,
    this.height,
    required this.createdAt,
    this.lastActive,
    this.wearableSettings,
    AlertThresholds? thresholds,
  }) : thresholds = thresholds ?? AlertThresholds.defaults();

  factory UserModel.fromFirestore(DocumentSnapshot doc) {
    final data = doc.data() as Map<String, dynamic>;
    return UserModel(
      uid: doc.id,
      name: data['name'] ?? '',
      nickname: data['nickname'],
      age: data['age'],
      sex: data['sex'],
      weight: (data['weight'] as num?)?.toDouble(),
      height: (data['height'] as num?)?.toDouble(),
      createdAt: (data['createdAt'] as Timestamp?)?.toDate() ?? DateTime.now(),
      lastActive: (data['lastActive'] as Timestamp?)?.toDate(),
      wearableSettings: data['wearableSettings'] as Map<String, dynamic>?,
      thresholds: AlertThresholds.fromMap(data['thresholds'] as Map<String, dynamic>?),
    );
  }

  Map<String, dynamic> toFirestore() => {
        'name': name,
        if (nickname != null) 'nickname': nickname,
        if (age != null) 'age': age,
        if (sex != null) 'sex': sex,
        if (weight != null) 'weight': weight,
        if (height != null) 'height': height,
        'createdAt': Timestamp.fromDate(createdAt),
        if (lastActive != null) 'lastActive': Timestamp.fromDate(lastActive!),
        if (wearableSettings != null) 'wearableSettings': wearableSettings,
        'thresholds': thresholds.toMap(),
      };

  UserModel copyWith({
    String? name,
    String? nickname,
    int? age,
    String? sex,
    double? weight,
    double? height,
    Map<String, dynamic>? wearableSettings,
    AlertThresholds? thresholds,
  }) {
    return UserModel(
      uid: uid,
      name: name ?? this.name,
      nickname: nickname ?? this.nickname,
      age: age ?? this.age,
      sex: sex ?? this.sex,
      weight: weight ?? this.weight,
      height: height ?? this.height,
      createdAt: createdAt,
      lastActive: lastActive,
      wearableSettings: wearableSettings ?? this.wearableSettings,
      thresholds: thresholds ?? this.thresholds,
    );
  }

  /// HRmax estimation — Tanaka formula (208 - 0.7·age) is more accurate than 220-age.
  int get hrMax => age != null ? (208 - 0.7 * age!).round() : 190;
}

class AlertThresholds {
  final int hrHigh; // bpm
  final int hrLow;
  final int spo2Low; // %
  final double tempHigh; // °C
  final int batteryLow; // %
  final int stabilitySeconds; // sustained N s before triggering

  AlertThresholds({
    required this.hrHigh,
    required this.hrLow,
    required this.spo2Low,
    required this.tempHigh,
    required this.batteryLow,
    required this.stabilitySeconds,
  });

  factory AlertThresholds.defaults() => AlertThresholds(
        hrHigh: 130,
        hrLow: 45,
        spo2Low: 90,
        tempHigh: 38.0,
        batteryLow: 15,
        stabilitySeconds: 60,
      );

  factory AlertThresholds.fromMap(Map<String, dynamic>? m) {
    if (m == null) return AlertThresholds.defaults();
    final d = AlertThresholds.defaults();
    return AlertThresholds(
      hrHigh: m['hrHigh'] ?? d.hrHigh,
      hrLow: m['hrLow'] ?? d.hrLow,
      spo2Low: m['spo2Low'] ?? d.spo2Low,
      tempHigh: (m['tempHigh'] as num?)?.toDouble() ?? d.tempHigh,
      batteryLow: m['batteryLow'] ?? d.batteryLow,
      stabilitySeconds: m['stabilitySeconds'] ?? d.stabilitySeconds,
    );
  }

  Map<String, dynamic> toMap() => {
        'hrHigh': hrHigh,
        'hrLow': hrLow,
        'spo2Low': spo2Low,
        'tempHigh': tempHigh,
        'batteryLow': batteryLow,
        'stabilitySeconds': stabilitySeconds,
      };

  AlertThresholds copyWith({
    int? hrHigh,
    int? hrLow,
    int? spo2Low,
    double? tempHigh,
    int? batteryLow,
    int? stabilitySeconds,
  }) =>
      AlertThresholds(
        hrHigh: hrHigh ?? this.hrHigh,
        hrLow: hrLow ?? this.hrLow,
        spo2Low: spo2Low ?? this.spo2Low,
        tempHigh: tempHigh ?? this.tempHigh,
        batteryLow: batteryLow ?? this.batteryLow,
        stabilitySeconds: stabilitySeconds ?? this.stabilitySeconds,
      );
}
