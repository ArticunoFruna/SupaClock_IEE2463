import 'package:cloud_firestore/cloud_firestore.dart';

enum RecordingType { imu, ecgRaw, ecgSession }

/// Metadata for a raw stream recording (CSV blob in Firebase Storage).
/// Path: users/{uid}/recordings/{recId}
class RecordingModel {
  final String id;
  final RecordingType type;
  final String storagePath;
  final int sizeBytes;
  final int durationMs;
  final int sampleRate;
  final DateTime createdAt;
  final bool uploaded;
  final String? localPath;

  RecordingModel({
    required this.id,
    required this.type,
    required this.storagePath,
    required this.sizeBytes,
    required this.durationMs,
    required this.sampleRate,
    required this.createdAt,
    this.uploaded = false,
    this.localPath,
  });

  factory RecordingModel.fromFirestore(DocumentSnapshot doc) {
    final data = doc.data() as Map<String, dynamic>;
    return RecordingModel(
      id: doc.id,
      type: RecordingType.values.firstWhere(
        (e) => e.name == data['type'],
        orElse: () => RecordingType.imu,
      ),
      storagePath: data['storagePath'] ?? '',
      sizeBytes: data['sizeBytes'] ?? 0,
      durationMs: data['durationMs'] ?? 0,
      sampleRate: data['sampleRate'] ?? 0,
      createdAt: (data['createdAt'] as Timestamp).toDate(),
      uploaded: data['uploaded'] ?? false,
    );
  }

  Map<String, dynamic> toFirestore() => {
        'type': type.name,
        'storagePath': storagePath,
        'sizeBytes': sizeBytes,
        'durationMs': durationMs,
        'sampleRate': sampleRate,
        'createdAt': Timestamp.fromDate(createdAt),
        'uploaded': uploaded,
      };
}
