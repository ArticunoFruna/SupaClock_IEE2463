import 'dart:typed_data';
import 'package:archive/archive.dart';
import 'package:cloud_firestore/cloud_firestore.dart';
import '../models/user_model.dart';
import '../models/session_model.dart';
import '../models/daily_stats_model.dart';
import '../models/alert_model.dart';
import '../models/ecg_reading_model.dart';

/// Firestore service for the SupaClock schema:
///
/// users/{uid}
///   ├ profile fields, thresholds, wearableSettings
///   ├ sessions/{sessionId}              — spot, ecg, continuousDev
///   │     └ ecgReadings/{readingId}     — legacy / chunk subcollection (still supported)
///   ├ dailyStats/{YYYY-MM-DD}           — one daily rollup doc
///   ├ alerts/{alertId}                  — fired thresholds
///   └ recordings/{recId}                — raw CSV metadata (dev mode)
class FirestoreService {
  final FirebaseFirestore _db = FirebaseFirestore.instance;

  CollectionReference<Map<String, dynamic>> _users() => _db.collection('users');
  DocumentReference<Map<String, dynamic>> _user(String uid) => _users().doc(uid);

  // ═════════════════════════════════════════════════════════════════
  //                            User Profile
  // ═════════════════════════════════════════════════════════════════
  Future<UserModel> getOrCreateUser(String uid, {String? name}) async {
    final doc = await _user(uid).get();
    if (doc.exists) return UserModel.fromFirestore(doc);

    final user = UserModel(
      uid: uid,
      name: name ?? 'Usuario',
      createdAt: DateTime.now(),
    );
    await _user(uid).set(user.toFirestore());
    return user;
  }

  Future<void> updateUser(UserModel user) async {
    await _user(user.uid).set(user.toFirestore(), SetOptions(merge: true));
  }

  Stream<UserModel> streamUser(String uid) =>
      _user(uid).snapshots().map((d) => UserModel.fromFirestore(d));

  // ═════════════════════════════════════════════════════════════════
  //                              Sessions
  // ═════════════════════════════════════════════════════════════════
  CollectionReference<Map<String, dynamic>> _sessions(String uid) =>
      _user(uid).collection('sessions');

  Future<String> createSession(String uid, SessionModel session) async {
    final doc = await _sessions(uid).add(session.toFirestore());
    return doc.id;
  }

  Future<void> updateSession(String uid, String sessionId, Map<String, dynamic> patch) async {
    await _sessions(uid).doc(sessionId).set(patch, SetOptions(merge: true));
  }

  /// Raw 30s ECG → gzip Blob inside the session doc's `blobs/csv_gz`.
  /// This is the ONLY raw waveform we keep in the cloud; everything else is
  /// edge-summarised. Stays well under Firestore's 1 MB/doc limit (~40-70 KB).
  Future<String> uploadSessionCsvGz(String uid, String sessionId, String csvBody) async {
    final gz = GZipEncoder().encode(Uint8List.fromList(csvBody.codeUnits));
    await _sessions(uid).doc(sessionId).collection('blobs').doc('csv_gz').set({
      'data': Blob(Uint8List.fromList(gz)),
      'timestamp': FieldValue.serverTimestamp(),
    });
    return 'firestore:sessions/$sessionId/blobs/csv_gz';
  }

  Future<List<SessionModel>> getSessions(String uid, {int limit = 50}) async {
    final snap = await _sessions(uid)
        .orderBy('startTime', descending: true)
        .limit(limit)
        .get();
    return snap.docs.map(SessionModel.fromFirestore).toList();
  }

  Stream<List<SessionModel>> streamSessions(String uid, {int limit = 20}) =>
      _sessions(uid)
          .orderBy('startTime', descending: true)
          .limit(limit)
          .snapshots()
          .map((s) => s.docs.map(SessionModel.fromFirestore).toList());

  Future<SessionModel?> getSession(String uid, String sessionId) async {
    final doc = await _sessions(uid).doc(sessionId).get();
    if (!doc.exists) return null;
    return SessionModel.fromFirestore(doc);
  }

  /// Recent sessions that carry an ECG recording (`ecg != null`), newest first.
  /// ECG spot sessions are stored with `type == 'spot'`, so we filter client
  /// side on the presence of the ecg meta rather than by type.
  Future<List<SessionModel>> getEcgSessions(String uid, {int scan = 40}) async {
    final all = await getSessions(uid, limit: scan);
    return all.where((s) => s.ecg != null).toList();
  }

  /// Downloads and gunzips a session's raw ECG Blob into its sample list.
  /// Mirrors the writer in [uploadSessionCsvGz] (CSV: `timestamp_ms,ecg_raw`).
  Future<List<double>> downloadSessionEcg(String uid, String sessionId) async {
    final doc =
        await _sessions(uid).doc(sessionId).collection('blobs').doc('csv_gz').get();
    final blob = doc.data()?['data'];
    if (blob is! Blob) return const [];

    final bytes = GZipDecoder().decodeBytes(blob.bytes);
    final csv = String.fromCharCodes(bytes);

    final out = <double>[];
    final lines = csv.split('\n');
    for (var i = 1; i < lines.length; i++) {
      // skip header
      final line = lines[i].trim();
      if (line.isEmpty) continue;
      final parts = line.split(',');
      if (parts.length < 2) continue;
      final v = double.tryParse(parts[1]);
      if (v != null) out.add(v);
    }
    return out;
  }

  // ═════════════════════════════════════════════════════════════════
  //                           Daily Stats
  // ═════════════════════════════════════════════════════════════════
  CollectionReference<Map<String, dynamic>> _daily(String uid) =>
      _user(uid).collection('dailyStats');

  Future<void> upsertDailyStats(String uid, DailyStatsModel stats) async {
    await _daily(uid).doc(stats.date).set(stats.toFirestore(), SetOptions(merge: true));
  }

  Future<DailyStatsModel?> getDailyStats(String uid, String date) async {
    final doc = await _daily(uid).doc(date).get();
    if (!doc.exists) return null;
    return DailyStatsModel.fromFirestore(doc);
  }

  Stream<DailyStatsModel?> streamDailyStats(String uid, String date) =>
      _daily(uid).doc(date).snapshots().map(
            (d) => d.exists ? DailyStatsModel.fromFirestore(d) : null,
          );

  /// Last [days] days of dailyStats, oldest first.
  Future<List<DailyStatsModel>> getRecentDailyStats(String uid, {int days = 30}) async {
    final snap = await _daily(uid)
        .orderBy(FieldPath.documentId, descending: true)
        .limit(days)
        .get();
    final list = snap.docs.map(DailyStatsModel.fromFirestore).toList();
    return list.reversed.toList();
  }

  // ═════════════════════════════════════════════════════════════════
  //                              Alerts
  // ═════════════════════════════════════════════════════════════════
  CollectionReference<Map<String, dynamic>> _alerts(String uid) =>
      _user(uid).collection('alerts');

  Future<String> createAlert(String uid, AlertModel alert) async {
    final doc = await _alerts(uid).add(alert.toFirestore());
    return doc.id;
  }

  Future<void> ackAlert(String uid, String alertId) async {
    await _alerts(uid).doc(alertId).update({
      'ackedAt': Timestamp.fromDate(DateTime.now()),
    });
  }

  Stream<List<AlertModel>> streamUnackedAlerts(String uid) =>
      _alerts(uid)
          .where('ackedAt', isNull: true)
          .orderBy('triggeredAt', descending: true)
          .limit(20)
          .snapshots()
          .map((s) => s.docs.map(AlertModel.fromFirestore).toList());

  // ═════════════════════════════════════════════════════════════════
  //                        ECG Readings (legacy)
  // ═════════════════════════════════════════════════════════════════
  /// Kept for backwards-compat with the existing EcgScreen. New code should
  /// use SessionModel.ecg / SessionModel.analysis instead.
  Future<String> uploadEcgReading(
    String userId,
    String sessionId,
    EcgReadingModel reading,
  ) async {
    final doc = await _sessions(userId)
        .doc(sessionId)
        .collection('ecgReadings')
        .add(reading.toFirestore());
    return doc.id;
  }

  Future<List<EcgReadingModel>> getEcgReadings(String userId, String sessionId) async {
    final snap = await _sessions(userId)
        .doc(sessionId)
        .collection('ecgReadings')
        .orderBy('timestamp', descending: true)
        .get();
    return snap.docs.map(EcgReadingModel.fromFirestore).toList();
  }
}
