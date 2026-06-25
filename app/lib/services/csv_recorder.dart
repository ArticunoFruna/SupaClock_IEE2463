import 'dart:async';
import 'dart:io';

import 'package:intl/intl.dart';
import 'package:path_provider/path_provider.dart';

import '../models/recording_model.dart';
import 'ble_service.dart';
import 'local_store.dart';

/// Streams BLE data to a local CSV file, persistent in app docs.
/// Modes mirror the columns produced by `tools/supaclock_monitor.py`.
class CsvRecorder {
  final BleService _ble;
  StreamSubscription? _imuSub;
  StreamSubscription? _ecgSub;
  StreamSubscription? _telemSub;

  IOSink? _sink;
  File? _file;
  RecordingType? _mode;
  DateTime? _startedAt;
  int _bytesWritten = 0;
  String? _label; // HAR class label (resting/walking/running/fall)

  // last-seen sensor cache for IMU rows
  SupaClockTelemetry _telCache = SupaClockTelemetry(timestamp: DateTime.now());

  CsvRecorder(this._ble) {
    _telemSub = _ble.telemetryStream.listen((t) => _telCache = t);
  }

  bool get isRecording => _sink != null;
  RecordingType? get mode => _mode;
  DateTime? get startedAt => _startedAt;
  int get bytesWritten => _bytesWritten;

  Future<File> _newFile(RecordingType mode, {String? label}) async {
    final dir = await getApplicationDocumentsDirectory();
    final recDir = Directory('${dir.path}/recordings');
    if (!await recDir.exists()) await recDir.create(recursive: true);
    final ts = DateFormat('yyyyMMdd_HHmmss').format(DateTime.now());
    final prefix = mode == RecordingType.ecgRaw ? 'ecg' : 'imu';
    final labelPart = (label != null && label.isNotEmpty) ? '${label}_' : '';
    return File('${recDir.path}/supaclock_${prefix}_$labelPart$ts.csv');
  }

  /// [label] tags the recording with a HAR class (resting/walking/running/fall).
  /// Embedded both in the filename and as a `label` column so window-builders
  /// (`tools/har/csv_to_windows.py`) can split classes downstream.
  Future<void> startImu({String? label}) async {
    if (isRecording) return;
    _mode = RecordingType.imu;
    _label = label;
    _file = await _newFile(_mode!, label: label);
    _sink = _file!.openWrite();
    _bytesWritten = 0;
    // `label` = etiqueta MANUAL (ground-truth elegida en dev mode).
    // `har_state` = SALIDA del modelo HAR en el reloj (TLV 0x08; -1 = aún sin
    //   inferencia / warmup). Tener ambas permite matriz de confusión en el PC.
    _writeLine(
      'timestamp_ms,ax,ay,az,gx,gy,gz,label,temp_c,steps,bat_mv,bat_soc,hr_bpm,spo2_pct,har_state',
    );
    _startedAt = DateTime.now();

    _imuSub = _ble.imuStream.listen((s) {
      final t = _telCache;
      _writeLine(
        '${s.t.millisecondsSinceEpoch},'
        '${s.ax},${s.ay},${s.az},${s.gx},${s.gy},${s.gz},'
        '${_label ?? ""},'
        '${(t.temperature ?? 0.0).toStringAsFixed(2)},${t.steps ?? 0},'
        '${t.batteryMv ?? 0},${t.batterySoc ?? 0},'
        '${t.heartRate ?? 0},${t.spo2 ?? 0},'
        '${t.harState ?? -1}',
      );
    });
  }

  Future<void> startEcg() async {
    if (isRecording) return;
    _mode = RecordingType.ecgRaw;
    _file = await _newFile(_mode!);
    _sink = _file!.openWrite();
    _bytesWritten = 0;
    _writeLine('timestamp_ms,ecg_raw');
    _startedAt = DateTime.now();

    _ecgSub = _ble.ecgStream.listen((samples) {
      // 10 samples per packet @ 500 Hz → 2 ms/sample, stamped backwards from now
      final endTs = DateTime.now().millisecondsSinceEpoch;
      for (var i = 0; i < samples.length; i++) {
        final ts = endTs - (samples.length - 1 - i) * 2;
        _writeLine('$ts,${samples[i]}');
      }
    });
  }

  Future<File?> stop() async {
    final sink = _sink;
    final file = _file;
    final mode = _mode;
    final started = _startedAt;
    if (sink == null || file == null || mode == null || started == null) {
      return null;
    }

    await _imuSub?.cancel();
    _imuSub = null;
    await _ecgSub?.cancel();
    _ecgSub = null;

    await sink.flush();
    await sink.close();
    _sink = null;
    _file = null;
    _mode = null;
    _label = null;

    // Comprimir el CSV a gzip y borrar el plano. El streaming a CSV se mantiene
    // durante la captura (robusto ante sesiones de 1h+); la compresión ocurre
    // una sola vez al cerrar. pandas/`gzip` en el PC leen el .gz directamente.
    final outFile = await _gzipAndReplace(file);
    final stat = await outFile.stat();
    final id = outFile.path.split('/').last.replaceAll('.csv.gz', '');
    final durationMs = DateTime.now().difference(started).inMilliseconds;

    await LocalStore.saveRecording(id, {
      'id': id,
      'type': mode.name,
      'localPath': outFile.path,
      'sizeBytes': stat.size,
      'durationMs': durationMs,
      'sampleRate': mode == RecordingType.ecgRaw ? 500 : 50,
      'createdAtMs': started.millisecondsSinceEpoch,
      'uploaded': false,
    });

    return outFile;
  }

  /// Gzip-comprime [csv] → `<archivo>.csv.gz` y elimina el original. Si algo
  /// falla, deja el CSV plano intacto y lo devuelve sin comprimir.
  Future<File> _gzipAndReplace(File csv) async {
    try {
      final bytes = await csv.readAsBytes();
      final gz = gzip.encode(bytes);
      final gzPath = '${csv.path}.gz';
      final gzFile = File(gzPath);
      await gzFile.writeAsBytes(gz, flush: true);
      await csv.delete();
      return gzFile;
    } catch (_) {
      return csv; // fallback: conservar el plano si la compresión falla
    }
  }

  void _writeLine(String line) {
    final sink = _sink;
    if (sink == null) return;
    sink.writeln(line);
    _bytesWritten += line.length + 1;
  }

  Future<void> dispose() async {
    await _telemSub?.cancel();
    await _imuSub?.cancel();
    await _ecgSub?.cancel();
    await _sink?.close();
  }
}
