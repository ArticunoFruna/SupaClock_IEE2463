import 'package:cloud_firestore/cloud_firestore.dart';

enum SessionType { spot, ecg, continuousDev }

/// Biometric session — spot-check, ECG recording, or developer continuous capture.
/// Path: users/{userId}/sessions/{sessionId}
class SessionModel {
  final String id;
  final SessionType type;
  final DateTime startTime;
  final DateTime? endTime;

  // Spot-check / aggregate summary
  final int? steps;
  final double? avgHeartRate;
  final double? avgSpO2;
  final double? avgTemperature;
  final int? quality; // 0-100, average TLV quality byte

  // ECG-specific
  final EcgMeta? ecg;
  final EcgAnalysis? analysis;

  // Activity classification log (TinyML on the watch — future)
  final List<ActivityEntry> activityLog;

  // Notes / tags
  final String? notes;
  final List<String> tags;

  SessionModel({
    required this.id,
    this.type = SessionType.spot,
    required this.startTime,
    this.endTime,
    this.steps,
    this.avgHeartRate,
    this.avgSpO2,
    this.avgTemperature,
    this.quality,
    this.ecg,
    this.analysis,
    this.activityLog = const [],
    this.notes,
    this.tags = const [],
  });

  factory SessionModel.fromFirestore(DocumentSnapshot doc) {
    final data = doc.data() as Map<String, dynamic>;
    return SessionModel(
      id: doc.id,
      type: SessionType.values.firstWhere(
        (e) => e.name == data['type'],
        orElse: () => SessionType.spot,
      ),
      startTime: (data['startTime'] as Timestamp).toDate(),
      endTime: (data['endTime'] as Timestamp?)?.toDate(),
      steps: data['steps'] as int?,
      avgHeartRate: (data['avgHeartRate'] as num?)?.toDouble(),
      avgSpO2: (data['avgSpO2'] as num?)?.toDouble(),
      avgTemperature: (data['avgTemperature'] as num?)?.toDouble(),
      quality: data['quality'] as int?,
      ecg: data['ecg'] == null ? null : EcgMeta.fromMap(data['ecg']),
      analysis: data['analysis'] == null ? null : EcgAnalysis.fromMap(data['analysis']),
      activityLog: (data['activityLog'] as List<dynamic>?)
              ?.map((e) => ActivityEntry.fromMap(e as Map<String, dynamic>))
              .toList() ??
          const [],
      notes: data['notes'],
      tags: (data['tags'] as List<dynamic>?)?.map((e) => e.toString()).toList() ?? const [],
    );
  }

  Map<String, dynamic> toFirestore() {
    return {
      'type': type.name,
      'startTime': Timestamp.fromDate(startTime),
      if (endTime != null) 'endTime': Timestamp.fromDate(endTime!),
      if (steps != null) 'steps': steps,
      if (avgHeartRate != null) 'avgHeartRate': avgHeartRate,
      if (avgSpO2 != null) 'avgSpO2': avgSpO2,
      if (avgTemperature != null) 'avgTemperature': avgTemperature,
      if (quality != null) 'quality': quality,
      if (ecg != null) 'ecg': ecg!.toMap(),
      if (analysis != null) 'analysis': analysis!.toMap(),
      if (activityLog.isNotEmpty) 'activityLog': activityLog.map((e) => e.toMap()).toList(),
      if (notes != null) 'notes': notes,
      if (tags.isNotEmpty) 'tags': tags,
    };
  }

  Duration get duration => (endTime ?? DateTime.now()).difference(startTime);
}

class EcgMeta {
  final String storagePath; // gs://.../users/{uid}/ecg/{sessionId}.csv.gz
  final int sampleRate;
  final int durationMs;
  final int sampleCount;

  EcgMeta({
    required this.storagePath,
    required this.sampleRate,
    required this.durationMs,
    required this.sampleCount,
  });

  factory EcgMeta.fromMap(Map<String, dynamic> m) => EcgMeta(
        storagePath: m['storagePath'] ?? '',
        sampleRate: m['sampleRate'] ?? 500,
        durationMs: m['durationMs'] ?? 0,
        sampleCount: m['sampleCount'] ?? 0,
      );

  Map<String, dynamic> toMap() => {
        'storagePath': storagePath,
        'sampleRate': sampleRate,
        'durationMs': durationMs,
        'sampleCount': sampleCount,
      };
}

class EcgAnalysis {
  final int rPeakCount;
  final double hrMean; // bpm
  final double? hrvSdnn; // ms
  final double? hrvRmssd; // ms
  final double? rrMeanMs;
  final double qualityScore; // 0-1
  final String? classifierTag;

  EcgAnalysis({
    required this.rPeakCount,
    required this.hrMean,
    this.hrvSdnn,
    this.hrvRmssd,
    this.rrMeanMs,
    this.qualityScore = 1.0,
    this.classifierTag,
  });

  factory EcgAnalysis.fromMap(Map<String, dynamic> m) => EcgAnalysis(
        rPeakCount: m['rPeakCount'] ?? 0,
        hrMean: (m['hrMean'] as num?)?.toDouble() ?? 0.0,
        hrvSdnn: (m['hrvSdnn'] as num?)?.toDouble(),
        hrvRmssd: (m['hrvRmssd'] as num?)?.toDouble(),
        rrMeanMs: (m['rrMeanMs'] as num?)?.toDouble(),
        qualityScore: (m['qualityScore'] as num?)?.toDouble() ?? 0.0,
        classifierTag: m['classifierTag'],
      );

  Map<String, dynamic> toMap() => {
        'rPeakCount': rPeakCount,
        'hrMean': hrMean,
        if (hrvSdnn != null) 'hrvSdnn': hrvSdnn,
        if (hrvRmssd != null) 'hrvRmssd': hrvRmssd,
        if (rrMeanMs != null) 'rrMeanMs': rrMeanMs,
        'qualityScore': qualityScore,
        if (classifierTag != null) 'classifierTag': classifierTag,
      };
}

class ActivityEntry {
  final DateTime timestamp;
  final String activity;

  ActivityEntry({required this.timestamp, required this.activity});

  factory ActivityEntry.fromMap(Map<String, dynamic> map) => ActivityEntry(
        timestamp: (map['timestamp'] as Timestamp).toDate(),
        activity: map['activity'] ?? 'unknown',
      );

  Map<String, dynamic> toMap() => {
        'timestamp': Timestamp.fromDate(timestamp),
        'activity': activity,
      };
}
