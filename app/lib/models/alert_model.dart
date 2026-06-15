import 'package:cloud_firestore/cloud_firestore.dart';

enum AlertType {
  hrHigh,
  hrLow,
  spo2Low,
  tempHigh,
  batteryLow,
  deviceLost,
  fallDetected,
}

class AlertModel {
  final String id;
  final AlertType type;
  final double value;
  final double threshold;
  final DateTime triggeredAt;
  final DateTime? ackedAt;
  final String? sessionRef;
  final String? note;

  AlertModel({
    required this.id,
    required this.type,
    required this.value,
    required this.threshold,
    required this.triggeredAt,
    this.ackedAt,
    this.sessionRef,
    this.note,
  });

  factory AlertModel.fromFirestore(DocumentSnapshot doc) {
    final data = doc.data() as Map<String, dynamic>;
    return AlertModel(
      id: doc.id,
      type: AlertType.values.firstWhere(
        (e) => e.name == data['type'],
        orElse: () => AlertType.hrHigh,
      ),
      value: (data['value'] as num).toDouble(),
      threshold: (data['threshold'] as num).toDouble(),
      triggeredAt: (data['triggeredAt'] as Timestamp).toDate(),
      ackedAt: (data['ackedAt'] as Timestamp?)?.toDate(),
      sessionRef: data['sessionRef'],
      note: data['note'],
    );
  }

  Map<String, dynamic> toFirestore() => {
        'type': type.name,
        'value': value,
        'threshold': threshold,
        'triggeredAt': Timestamp.fromDate(triggeredAt),
        if (ackedAt != null) 'ackedAt': Timestamp.fromDate(ackedAt!),
        if (sessionRef != null) 'sessionRef': sessionRef,
        if (note != null) 'note': note,
      };

  bool get isAcked => ackedAt != null;
}
