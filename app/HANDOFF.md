# SupaClock Flutter App — Handoff

Context for the next agent picking up this work. Self-contained: read this and you have the full picture.

---

## 1. What this app is

Companion app for the **SupaClock** wearable (ESP32-C3, custom firmware in `../src/` and `../lib/ble_telemetry/`). It pairs over BLE, ingests biometric telemetry (HR, SpO₂, temp, IMU, ECG) and persists it to Firebase. Two-faced UI:

- **End-user view**: dashboard, "Measure now" spot-check (HR + SpO₂ + 30s ECG with Pan-Tompkins analysis), trends, alerts.
- **Developer Mode** (hidden, unlocked by tap-7 on the version label in Settings): live IMU/ECG waveforms, REC to local CSV, raw command console, recordings list — port of `../tools/supaclock_monitor.py`.

UI is in **Spanish**.

---

## 2. Critical architectural decisions (don't reverse without asking)

1. **No Cloud Functions** — the user explicitly didn't want them (billing concerns). Everything runs client-side: Pan-Tompkins, daily aggregation, alert evaluation. Don't add Functions without checking.
2. **Local-first**: Hive holds today's accumulator + sync queue + recordings metadata. Firestore is the synced mirror. Raw waveforms (CSV) go to **Firebase Storage**, never to Firestore.
3. **Two profiles, one binary**: clinical mode toggle + dev mode hidden behind tap-to-unlock. The user wants the final user-facing app clean — don't let dev features leak into the default UI.
4. **BLE protocol**: real firmware uses **`0xFF01-0xFF04`** with TLV records on `0xFF02`. Authoritative source is `../lib/ble_telemetry/ble_telemetry.h`. The pre-existing `BleService` was talking to a *test* firmware (UUIDs `0001-0003`, no TLV) — I rewrote it. Don't revert.
5. **Clinical mode is OFF by default** because the hardware is still on protoboard — alerts would fire constantly with bad contact. Quality byte from TLVs gates samples (≥60 to count). When the user mounts the hardware definitively, they'll flip the toggle. Don't auto-enable.

---

## 3. File map

### New / rewritten in this session

```
app/lib/
├── main.dart                                    REWRITE — Hive init, telemetry collector, lifecycle hook
├── config/theme.dart                            tweak — added AppTheme.warning
├── models/
│   ├── user_model.dart                          REWRITE — adds age/sex/thresholds/hrMax
│   ├── session_model.dart                      REWRITE — type:spot|ecg|continuousDev, EcgMeta, EcgAnalysis
│   ├── daily_stats_model.dart                  NEW
│   ├── alert_model.dart                        NEW
│   └── recording_model.dart                    NEW
├── providers/
│   └── settings_provider.dart                  REWRITE — clinicalMode, devModeUnlocked, version-tap counter
├── services/
│   ├── ble_service.dart                        REWRITE — FF01-FF04 + TLV parser, separate streams
│   ├── firestore_service.dart                  REWRITE — new schema (users/sessions/dailyStats/alerts/recordings)
│   ├── storage_service.dart                    NEW — gzip CSV → Firebase Storage
│   ├── local_store.dart                        NEW — Hive boxes
│   ├── pan_tompkins.dart                       NEW — pure-Dart QRS detector + HRV
│   ├── telemetry_collector.dart                NEW — BLE → Hive bucket + alert evaluator
│   ├── daily_rollup_service.dart               NEW — closes prior day, writes dailyStats doc
│   ├── notifications_service.dart              NEW — flutter_local_notifications wrapper
│   └── csv_recorder.dart                       NEW — IMU/ECG → local CSV (dev mode REC)
└── screens/
    ├── dashboard_screen.dart                    REWRITE — clinical banner, FAB "Medir ahora", quality badges
    ├── settings_screen.dart                     REWRITE — profile, clinical toggle, thresholds, dev unlock
    ├── spot_check_screen.dart                   NEW — 30s ECG + Pan-Tompkins + persist
    └── dev_mode_screen.dart                     NEW — 4 tabs: IMU/ECG/Console/Recordings
```

### Untouched (still using legacy patterns — candidates for cleanup)

- `screens/ecg_screen.dart` — still reads `EcgReadingModel` subcollection (legacy). New code writes ECG via `SessionModel.ecg` + Storage instead. Should migrate to read from sessions where `ecg != null`.
- `screens/trends_tab.dart` + `utils/data_aggregator.dart` — still aggregate from `sessions`. Works (now sees spot-check sessions) but should be rewritten to consume `dailyStats/{date}` for week/month, with sessions only for the "today/intraday" view.
- `screens/ble_debug_screen.dart`, `screens/login_screen.dart` — minor, unchanged.

### Firebase config

- `../firebase/firestore.rules` — REWRITTEN with type/range validation per collection.
- `../firebase/storage.rules` — NEW, owner-only access, 50 MB cap per blob.
- `../firebase/firebase.json` — added `"storage": {...}` block.
- **Not deployed yet** — run `firebase deploy --only firestore:rules,storage` from `../firebase/`.

---

## 4. Firestore + Storage schema

```
users/{uid}
  ├ name, nickname, age, sex, weight, height, createdAt, lastActive
  ├ wearableSettings: { clockFormat, syncIntervalMinutes, powerMode }
  ├ thresholds: { hrHigh, hrLow, spo2Low, tempHigh, batteryLow, stabilitySeconds }
  │
  ├─ sessions/{sessionId}
  │     ├ type: 'spot' | 'ecg' | 'continuousDev'
  │     ├ startTime, endTime, steps, avgHeartRate, avgSpO2, avgTemperature, quality
  │     ├ ecg: { storagePath, sampleRate, durationMs, sampleCount }
  │     ├ analysis: { rPeakCount, hrMean, hrvSdnn, hrvRmssd, rrMeanMs, qualityScore, classifierTag }
  │     ├ activityLog: [], notes, tags
  │     └─ ecgReadings/{rid}                     (legacy subcollection, still readable)
  │
  ├─ dailyStats/{YYYY-MM-DD}
  │     ├ steps, activeMinutes, computedAt, sourceVersion
  │     ├ hr: { avg, min, max, resting, samples }
  │     ├ spo2: { avg, min, samples }
  │     ├ temp: { avg, min, max, samples }
  │     └ hrZoneMinutes: { z1, z2, z3, z4, z5 }
  │
  ├─ alerts/{alertId}
  │     ├ type, value, threshold, triggeredAt, ackedAt, sessionRef, note
  │
  └─ recordings/{recId}                           (dev mode raw streams)
        ├ type: 'imu' | 'ecgRaw' | 'ecgSession'
        ├ storagePath, sizeBytes, durationMs, sampleRate, createdAt, uploaded
```

**Storage paths**:
```
users/{uid}/ecg/{sessionId}.csv.gz
users/{uid}/imu/{recId}.csv.gz
users/{uid}/exports/{sessionId}.pdf            (planned, not implemented)
```

CSV format mirrors `tools/supaclock_monitor.py` exactly (same headers) so the same Python tooling can ingest both.

---

## 5. BLE protocol cheat-sheet

Authoritative source: `../lib/ble_telemetry/ble_telemetry.h`.

| UUID    | Direction | Format | Notes |
|---------|-----------|--------|-------|
| 0xFF01  | notify    | 12 B raw `int16 ax,ay,az,gx,gy,gz` | 50/25/12.5 Hz depending on power mode |
| 0xFF02  | notify    | header (6 B) + TLV records | Aggregated HR/SpO2/Temp/Bat/Steps |
| 0xFF03  | notify    | 20 B = 10 × `int16` ECG samples | 500 Hz when streaming |
| 0xFF04  | write     | 1 B command | `0x01` start ECG, `0x00` stop ECG |

TLV header (always little-endian): `u32 boot_ts_ms; u8 power_mode; u8 payload_len`. Then records: `u8 type; u8 len; data...`.

| TLV type | Len | Payload |
|----------|-----|---------|
| 0x01 HR  | 4 | `u16 delta_ms; u8 bpm; u8 quality` |
| 0x02 SpO₂| 4 | `u16 delta_ms; u8 pct; u8 quality` |
| 0x03 Temp| 4 | `u16 delta_ms; i16 temp_x100` |
| 0x04 Bat | 5 | `u16 delta_ms; u16 mv; u8 soc` |
| 0x05 Steps|4 | `u32 total_steps` |
| 0x06 ModeEvt|1| `u8 new_mode` (0=SPORT 1=NORM 2=SAVER) |
| 0x07 SpotResult|6| `u8 bpm; u8 spo2; u16 dur_ms; u8 quality; u8 aborted` |

Parsed in `services/ble_service.dart::_onAggData`. The `quality` byte is **the gate** for sample acceptance and alert evaluation (≥60 = trustworthy).

---

## 6. Quality gate + alert pipeline (important)

`services/telemetry_collector.dart` is the alert engine:

1. Subscribes to `BleService.telemetryStream`.
2. For each TLV update:
   - If sample passes the quality gate (`q ≥ 60`), append to today's Hive bucket and accumulate HR-zone seconds.
   - If `clinicalMode == true`, run `_evaluateAlerts`:
     - Each threshold has a stability window (default 60 s) — only fires after the bad value persists.
     - On fire: writes `alerts/{id}` to Firestore + shows local notification.
     - Battery-low fires once per percent crossing.
3. If `clinicalMode == false`: stability timers are reset, alerts never fire, but bucket still fills.

When changing alert logic, keep this invariant: **a brief glitch (one bad sample) must never trigger an alert**.

---

## 7. Daily rollup

`services/daily_rollup_service.dart` runs:

- On app start (after wiring telemetry).
- On app `resumed` lifecycle event.

It iterates Hive's `daily_pending` keys; for each key that's **not today**, it aggregates:

- HR: avg/min/max/resting (resting = median of bottom 5 % of samples).
- SpO₂: avg/min.
- Temp: avg/min/max.
- HR zones: convert seconds → minutes per zone.
- Active minutes = sum of zone minutes.

Then writes `dailyStats/{date}.set(...)` and clears the Hive entry. If the write fails, it goes to `sync_queue` for retry. **No retry loop is wired yet** — see §10 TODO.

Today's day stays in Hive until the next launch after midnight. Trends should read it from Hive for "live today" view (not implemented).

---

## 8. Build & run

```bash
cd app
flutter pub get
flutter analyze                       # should be 0 issues
flutter build apk --debug
adb install -r build/app/outputs/flutter-apk/app-debug.apk
```

Caveat: `flutter install` may pick up an older `app-release.apk` if both exist. Always specify the debug path explicitly until release signing is configured.

**Android requires** core library desugaring (configured in `android/app/build.gradle.kts`):

```kotlin
compileOptions {
    isCoreLibraryDesugaringEnabled = true
}
dependencies {
    coreLibraryDesugaring("com.android.tools:desugar_jdk_libs:2.1.4")
}
```

This was needed for `flutter_local_notifications`. Don't remove.

---

## 9. Hidden / non-obvious behaviours

- **Tap-7 dev unlock**: tap "v1.0.0+1" at the bottom of Settings 7 times. The counter is in `SettingsProvider.registerVersionTap()`. After unlock, Settings shows a "Developer" section and dev mode persists across restarts. "Lock developer mode" hides it again.
- **Quality badge on Dashboard**: HR/SpO₂ cards show "señal débil" + grey color when `quality < 60`. Logic in `dashboard_screen.dart::_metricGrid`.
- **Spot-check timing**: 30 s @ 500 Hz = 15 000 samples. The screen shows a sample counter — if it stays at 0, the firmware is not streaming ECG (check `0xFF04` write went through).
- **Watch-side SpotResult TLV (0x07)**: when the wearable does its own spot measurement, it sends a single TLV with bpm/spo2/quality. The app surfaces it via `BleService.spotStream` — the spot-check screen subscribes but the TLV is optional (the app does its own Pan-Tompkins on the raw ECG anyway).
- **No FCM**: notifications are local-only via `flutter_local_notifications`. If the user wants cross-device push later, that's where it'll go — not Cloud Functions.
- **Hive type erasure**: boxes store `Map`, not typed Hive objects. Done deliberately to avoid the codegen step. Be careful with `int` vs `double` when reading (see `daily_rollup_service.dart::_aggregate`).

---

## 10. Known TODOs / what to do next

In rough priority order. The user has been driving feature-by-feature; check with them before bundling.

1. **Deploy security rules**. The new schema **will fail writes** until rules are deployed:
   ```bash
   cd ../firebase
   firebase deploy --only firestore:rules,storage
   ```
2. **Migrate Trends** (`trends_tab.dart` + `data_aggregator.dart`) to read from `dailyStats/{date}` for week/month. Today view should read Hive's `daily_pending` for live numbers. Currently it works on sessions and will only show spot-check data.
3. **Migrate ECG screen** to list `sessions where type=='ecg'` and lazy-load the CSV from Storage. The legacy `ecgReadings` subcollection should be deprecated.
4. **PDF export** for spot-check sessions (`pdf` + `printing` packages). Required by the user's plan ("ideal para mostrar al médico"). Upload to `users/{uid}/exports/`.
5. **3D orientation cube** in Dev Mode IMU tab. User asked for it (port of `supaclock_monitor.py` GLBoxItem). Easiest: `vector_math` (already in deps) + a CustomPainter, or pull in `flutter_cube`.
6. **Sync queue retry**. `LocalStore.enqueue('alert' / 'dailyStats:date', ...)` enqueues failed writes but nothing drains the queue. Add a periodic flush (e.g. on connectivity change, or every app resume).
7. **Daily rollup test on real day boundary**: I haven't been able to verify the cross-midnight flow. Worth manually flipping the device clock or unit-testing `_aggregate`.
8. **Resting HR algorithm**: current implementation is "median of bottom 5% of samples". Could be improved by gating on motion (low IMU activity windows only). User-facing visibility low; can wait.
9. **Sleep tracking**: user asked for it as WIP — a card on Dashboard with "próximamente" badge would be enough placeholder. Not started.
10. **Watch-side commands**: app sends only 0x00/0x01 (ECG start/stop). Firmware accepts more (mode change, find-watch buzz?) — protocol is the user's call. Don't invent commands.
11. **TinyML activity classifier**: the dashboard has an "Actividad detectada" card with hardcoded "Reposo" active. The TLV protocol doesn't carry classifier output yet. Not in scope.

---

## 11. User preferences I've inferred

- Spanish UI everywhere (don't translate to English).
- Concise responses, terse prompts. Don't summarize what the diff already shows.
- Wants real implementation, not stubs — but does NOT want speculative features beyond what's discussed. When in doubt, ask before adding.
- Cares about cost — Firestore quotas, free tiers, no Cloud Functions. Don't add any paid services.
- Hardware is on protoboard, will be mounted later. Treat sensor data as suspect by default.
- Respects iterative work: ship one thing well, then the next.

---

## 12. Quick sanity check after picking this up

```bash
flutter pub get && flutter analyze
```

If `analyze` reports 0 issues, the codebase is consistent. If it gripes about `flutter_local_notifications`, check Java desugaring config (§8). If it gripes about `share_plus` API, the version is pinned to v10.x — `Share.shareXFiles([XFile(...)])`, not `SharePlus.instance.share(...)` (that's v11+).

Run the app, log in, open Settings → tap version 7×, confirm Dev Mode appears. Connect a SupaClock, watch the IMU tab in Dev Mode for live waveforms — that proves the BLE rewrite works end-to-end.

Good luck.
