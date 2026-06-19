import 'package:flutter/foundation.dart';

import '../models/alert_model.dart';
import '../models/daily_stats_model.dart';
import 'firestore_service.dart';
import 'local_store.dart';

/// Drains the offline write queue (`LocalStore.sync_queue`) into Firestore.
///
/// Items are enqueued by [TelemetryCollector] (alerts) and
/// [DailyRollupService] (daily rollups) when a live write fails. Nothing else
/// read this queue before — so offline writes were stranded forever.
///
/// Replays are **idempotent where it matters**: dailyStats is an upsert keyed
/// by date, so a re-run overwrites cleanly. Alerts use `.add()` (new doc each
/// time), but an item is dequeued only after its write succeeds, keeping the
/// duplicate window to a single in-flight request.
///
/// Cloud Firestore already retries plain writes via its own offline cache; this
/// queue is the app-level safety net for failures that escape that cache
/// (rule rejection, transient errors). Draining on app-resume is enough — no
/// need for a connectivity package.
class SyncService {
  final FirestoreService _fs;
  bool _draining = false;

  SyncService(this._fs);

  /// Flush every pending item for [uid]. Stops early on the first failure
  /// (most likely still-offline) and leaves the rest queued for the next run.
  Future<void> drain(String uid) async {
    if (_draining) return; // re-entrancy guard (resume can fire rapidly)
    _draining = true;
    try {
      final pending = LocalStore.pendingSync();
      for (final entry in pending) {
        final key = entry.key;
        final item = entry.value;
        final kind = item['kind'] as String? ?? '';
        final payload = item['payload'] == null
            ? <String, dynamic>{}
            : Map<String, dynamic>.from(item['payload'] as Map);

        try {
          final handled = await _replay(uid, kind, payload);
          if (handled) {
            await LocalStore.dequeue(key);
          } else {
            // Unknown kind — drop it so it can't poison the queue forever.
            debugPrint('SyncService: dropping unknown kind "$kind"');
            await LocalStore.dequeue(key);
          }
        } catch (e) {
          // Likely offline / transient — keep this item and bail; the next
          // resume will retry from here.
          debugPrint('SyncService: drain stopped at "$kind": $e');
          break;
        }
      }
    } finally {
      _draining = false;
    }
  }

  /// Returns true if the kind was recognised and the write was attempted.
  Future<bool> _replay(String uid, String kind, Map<String, dynamic> payload) async {
    switch (kind) {
      case 'alert':
        await _fs.createAlert(uid, AlertModel.fromJson(payload));
        return true;
      case 'dailyStats':
        await _fs.upsertDailyStats(uid, DailyStatsModel.fromJson(payload));
        return true;
      default:
        return false;
    }
  }
}
