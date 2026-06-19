import 'package:flutter/foundation.dart';
import 'package:hive_flutter/hive_flutter.dart';

/// Local persistence — Hive-backed. All raw boxes are typed-loosely (Map)
/// to keep schema migrations cheap.
///
/// Boxes:
///  - daily_pending : current-day rolling accumulator (in-memory, persisted)
///  - sync_queue    : FIFO of events that didn't make it to Firestore yet
///  - recordings    : metadata of CSV files saved locally (+ upload status)
///  - app_meta      : misc. app state (lastRollupDate, devModeUnlockedAt, etc.)
class LocalStore {
  static const _dailyBox = 'daily_pending';
  static const _syncBox = 'sync_queue';
  static const _recBox = 'recordings';
  static const _metaBox = 'app_meta';

  static Future<void> init() async {
    await Hive.initFlutter();
    await Future.wait([
      Hive.openBox(_dailyBox),
      Hive.openBox(_syncBox),
      Hive.openBox(_recBox),
      Hive.openBox(_metaBox),
    ]);
  }

  // ── Daily pending ─────────────────────────────────────────────────
  /// Per-date entry. Key = 'YYYY-MM-DD'.
  /// Value = Map: { steps, hrSamples:[int], spo2Samples:[int], tempSamples:[double],
  ///                hrZoneSeconds:[5], lastTouchEpoch }
  static Box get _daily => Hive.box(_dailyBox);

  static Map<String, dynamic> getDay(String dateKey) {
    final raw = _daily.get(dateKey);
    if (raw == null) {
      return {
        'steps': 0,
        'hrSamples': <int>[],
        'spo2Samples': <int>[],
        'tempSamples': <double>[],
        'hrZoneSeconds': <int>[0, 0, 0, 0, 0],
        'hourly': emptyHourly(),
        'lastTouchEpoch': 0,
      };
    }
    final m = Map<String, dynamic>.from(raw as Map);
    // Backfill for days persisted before the hourly buckets existed.
    m['hourly'] ??= emptyHourly();
    return m;
  }

  /// 24 per-hour accumulator buckets (sum + count per metric). Stored as
  /// primitive maps so Hive can persist them.
  static List<Map<String, dynamic>> emptyHourly() => List.generate(
        24,
        (_) => <String, dynamic>{
          'hrSum': 0,
          'hrCount': 0,
          'spo2Sum': 0,
          'spo2Count': 0,
          'tempSum': 0.0,
          'tempCount': 0,
          'steps': 0,
        },
      );

  static Future<void> putDay(String dateKey, Map<String, dynamic> data) async {
    await _daily.put(dateKey, data);
  }

  static Future<void> clearDay(String dateKey) async => _daily.delete(dateKey);

  static List<String> daysWithData() => _daily.keys.cast<String>().toList()..sort();

  // ── Sync queue ────────────────────────────────────────────────────
  static Box get _sync => Hive.box(_syncBox);

  /// Enqueue a payload to be retried later.
  /// kind: 'session' | 'dailyStats' | 'alert'
  static Future<void> enqueue(String kind, Map<String, dynamic> payload) async {
    await _sync.add({
      'kind': kind,
      'payload': payload,
      'enqueuedAt': DateTime.now().millisecondsSinceEpoch,
    });
  }

  static List<MapEntry<dynamic, Map>> pendingSync() =>
      _sync.toMap().entries.map((e) => MapEntry(e.key, e.value as Map)).toList();

  /// Number of writes still waiting to reach Firestore.
  static int pendingSyncCount() => _sync.length;

  /// Rebuilds UI (e.g. the status banner) when the queue grows or drains.
  static ValueListenable<Box> syncListenable() => _sync.listenable();

  static Future<void> dequeue(dynamic key) async => _sync.delete(key);

  // ── Recordings ─────────────────────────────────────────────────────
  static Box get _rec => Hive.box(_recBox);

  static Future<void> saveRecording(String id, Map<String, dynamic> meta) async {
    await _rec.put(id, meta);
  }

  static Map<String, dynamic>? getRecording(String id) {
    final v = _rec.get(id);
    return v == null ? null : Map<String, dynamic>.from(v as Map);
  }

  static List<Map<String, dynamic>> listRecordings() => _rec.values
      .map((v) => Map<String, dynamic>.from(v as Map))
      .toList()
    ..sort((a, b) => (b['createdAtMs'] ?? 0).compareTo(a['createdAtMs'] ?? 0));

  static Future<void> deleteRecording(String id) async => _rec.delete(id);

  // ── Meta ──────────────────────────────────────────────────────────
  static Box get _meta => Hive.box(_metaBox);

  static T? getMeta<T>(String key) => _meta.get(key) as T?;
  static Future<void> putMeta(String key, dynamic value) async => _meta.put(key, value);
}
