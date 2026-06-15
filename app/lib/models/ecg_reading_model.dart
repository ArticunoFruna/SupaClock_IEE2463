import 'package:cloud_firestore/cloud_firestore.dart';

/// ECG reading model matching Firestore schema:
/// users/{userId}/sessions/{sessionId}/ecgReadings/{readingId}
class EcgReadingModel {
  final String id;
  final DateTime timestamp;
  final List<double> rawData; // 3000 samples (100Hz × 30s)
  final double? processedBPM; // Computed by Cloud Function
  final double? hrv; // Heart Rate Variability (SDNN)
  final List<int>? rPeaks; // R-peak indices detected
  final List<double>? rrIntervals; // R-R intervals in ms
  final double samplingRate;
  final String processingStatus; // "pending", "completed", "error"

  EcgReadingModel({
    required this.id,
    required this.timestamp,
    required this.rawData,
    this.processedBPM,
    this.hrv,
    this.rPeaks,
    this.rrIntervals,
    this.samplingRate = 100.0,
    this.processingStatus = 'pending',
  });

  factory EcgReadingModel.fromFirestore(DocumentSnapshot doc) {
    final data = doc.data() as Map<String, dynamic>;
    return EcgReadingModel(
      id: doc.id,
      timestamp: (data['timestamp'] as Timestamp).toDate(),
      rawData: (data['rawData'] as List<dynamic>?)
              ?.map((e) => (e as num).toDouble())
              .toList() ??
          [],
      processedBPM: (data['processedBPM'] as num?)?.toDouble(),
      hrv: (data['hrv'] as num?)?.toDouble(),
      rPeaks: (data['rPeaks'] as List<dynamic>?)
          ?.map((e) => (e as num).toInt())
          .toList(),
      rrIntervals: (data['rrIntervals'] as List<dynamic>?)
          ?.map((e) => (e as num).toDouble())
          .toList(),
      samplingRate: (data['samplingRate'] as num?)?.toDouble() ?? 100.0,
      processingStatus: data['processingStatus'] ?? 'pending',
    );
  }

  Map<String, dynamic> toFirestore() {
    return {
      'timestamp': Timestamp.fromDate(timestamp),
      'rawData': rawData,
      'samplingRate': samplingRate,
      'processingStatus': processingStatus,
      if (processedBPM != null) 'processedBPM': processedBPM,
      if (hrv != null) 'hrv': hrv,
      if (rPeaks != null) 'rPeaks': rPeaks,
      if (rrIntervals != null) 'rrIntervals': rrIntervals,
    };
  }

  bool get isProcessed => processingStatus == 'completed';

  /// Duration of the recording in seconds
  double get durationSeconds => rawData.length / samplingRate;
}
