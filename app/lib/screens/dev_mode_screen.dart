import 'dart:async';
import 'dart:io';

import 'package:fl_chart/fl_chart.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:share_plus/share_plus.dart';

import '../config/theme.dart';
import '../models/recording_model.dart';
import '../services/ble_service.dart';
import '../services/csv_recorder.dart';
import '../services/local_store.dart';

/// Developer Mode — port of `tools/supaclock_monitor.py`.
class DevModeScreen extends StatefulWidget {
  const DevModeScreen({super.key});

  @override
  State<DevModeScreen> createState() => _DevModeScreenState();
}

class _DevModeScreenState extends State<DevModeScreen>
    with SingleTickerProviderStateMixin {
  late TabController _tabs;
  late CsvRecorder _recorder;
  StreamSubscription? _imuSub;
  StreamSubscription? _ecgSub;

  static const int _kImuPts = 200;
  static const int _kEcgPts = 1500;
  final List<double> _ax = List.filled(_kImuPts, 0);
  final List<double> _ay = List.filled(_kImuPts, 0);
  final List<double> _az = List.filled(_kImuPts, 0);
  final List<double> _gx = List.filled(_kImuPts, 0);
  final List<double> _gy = List.filled(_kImuPts, 0);
  final List<double> _gz = List.filled(_kImuPts, 0);
  final List<double> _ecg = List.filled(_kEcgPts, 0);
  bool _ecgMode = false;
  Timer? _redrawTimer;

  // Etiqueta de actividad para grabaciones IMU del HAR.
  static const List<String> _harLabels = ['resting', 'walking', 'running', 'stairs'];
  String _harLabel = 'resting';

  final TextEditingController _cmdController = TextEditingController(text: '0x01');

  @override
  void initState() {
    super.initState();
    _tabs = TabController(length: 4, vsync: this);
    final ble = context.read<BleService>();
    _recorder = CsvRecorder(ble);

    _imuSub = ble.imuStream.listen((s) {
      _shift(_ax, s.ax.toDouble());
      _shift(_ay, s.ay.toDouble());
      _shift(_az, s.az.toDouble());
      _shift(_gx, s.gx.toDouble());
      _shift(_gy, s.gy.toDouble());
      _shift(_gz, s.gz.toDouble());
    });

    _ecgSub = ble.ecgStream.listen((chunk) {
      for (final v in chunk) {
        _shift(_ecg, v.toDouble());
      }
    });

    _redrawTimer = Timer.periodic(const Duration(milliseconds: 66), (_) {
      if (mounted) setState(() {});
    });
  }

  void _shift(List<double> buf, double v) {
    for (var i = 0; i < buf.length - 1; i++) {
      buf[i] = buf[i + 1];
    }
    buf[buf.length - 1] = v;
  }

  @override
  void dispose() {
    _imuSub?.cancel();
    _ecgSub?.cancel();
    _redrawTimer?.cancel();
    _cmdController.dispose();
    _recorder.dispose();
    _tabs.dispose();
    super.dispose();
  }

  Future<void> _toggleEcg() async {
    final ble = context.read<BleService>();
    if (_ecgMode) {
      await ble.stopEcgStream();
    } else {
      await ble.startEcgStream();
    }
    setState(() => _ecgMode = !_ecgMode);
  }

  /// Pregunta el nº real de pasos dados en la grabación (ground-truth).
  /// Devuelve null si el usuario omite (p. ej. grabaciones de reposo).
  Future<int?> _askSteps() async {
    final ctrl = TextEditingController();
    final result = await showDialog<int>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Pasos reales'),
        content: TextField(
          controller: ctrl,
          autofocus: true,
          keyboardType: TextInputType.number,
          decoration: const InputDecoration(
            labelText: '¿Cuántos pasos diste? (contados)',
            hintText: 'ej. 50 · vacío = omitir',
          ),
          onSubmitted: (v) => Navigator.pop(ctx, int.tryParse(v.trim())),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, null),
            child: const Text('Omitir'),
          ),
          ElevatedButton(
            onPressed: () => Navigator.pop(ctx, int.tryParse(ctrl.text.trim())),
            child: const Text('Guardar'),
          ),
        ],
      ),
    );
    ctrl.dispose();
    return result;
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Developer Mode'),
        backgroundColor: Colors.deepPurple,
        bottom: TabBar(
          controller: _tabs,
          isScrollable: true,
          tabs: const [
            Tab(text: 'IMU'),
            Tab(text: 'ECG'),
            Tab(text: 'Console'),
            Tab(text: 'Recordings'),
          ],
        ),
      ),
      body: TabBarView(
        controller: _tabs,
        children: [
          _imuTab(),
          _ecgTab(),
          _consoleTab(),
          _recordingsTab(),
        ],
      ),
    );
  }

  Widget _imuTab() {
    final telem = context.watch<BleService>().telemetry;
    return SingleChildScrollView(
      padding: const EdgeInsets.all(12),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: [
              _chip('Temp', '${(telem.temperature ?? 0).toStringAsFixed(2)} °C'),
              _chip('Bat', '${telem.batterySoc ?? 0}% / ${telem.batteryMv ?? 0}mV'),
              _chip('Pasos', '${telem.steps ?? 0}'),
              _chip('HR', '${telem.heartRate ?? 0} bpm (q ${telem.hrQuality ?? 0})'),
              _chip('SpO₂', '${telem.spo2 ?? 0}% (q ${telem.spo2Quality ?? 0})'),
              _chip('Mode', '${telem.powerMode ?? "?"}'),
              _chip('HAR', telem.harState == null
                  ? 'calculando…'
                  : '${telem.activity.label} (${telem.harState})'),
            ],
          ),
          const SizedBox(height: 12),
          _wavePanel('Acelerómetro', [
            _line(_ax, Colors.redAccent),
            _line(_ay, Colors.greenAccent),
            _line(_az, Colors.blueAccent),
          ]),
          const SizedBox(height: 12),
          _wavePanel('Giroscopio', [
            _line(_gx, Colors.pinkAccent),
            _line(_gy, Colors.purpleAccent),
            _line(_gz, Colors.lightBlueAccent),
          ]),
          const SizedBox(height: 16),
          // Etiqueta de actividad para entrenamiento del HAR CNN-1D.
          // Se incrusta en el nombre del archivo y en la columna `label` de
          // cada fila → tools/har/csv_to_windows.py la usa para ventanear.
          Row(
            children: [
              const Text('Actividad:', style: TextStyle(fontWeight: FontWeight.w600)),
              const SizedBox(width: 8),
              Expanded(
                child: DropdownButtonFormField<String>(
                  value: _harLabel,
                  isDense: true,
                  decoration: const InputDecoration(
                    border: OutlineInputBorder(),
                    contentPadding: EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                  ),
                  items: _harLabels
                      .map((l) => DropdownMenuItem(value: l, child: Text(l)))
                      .toList(),
                  onChanged: _recorder.isRecording
                      ? null
                      : (v) => setState(() => _harLabel = v ?? 'resting'),
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),
          ElevatedButton.icon(
            icon: Icon(
              _recorder.isRecording && _recorder.mode == RecordingType.imu
                  ? Icons.stop
                  : Icons.fiber_manual_record,
              color: Colors.white,
            ),
            label: Text(
                _recorder.isRecording && _recorder.mode == RecordingType.imu
                    ? 'Detener IMU'
                    : 'Grabar IMU CSV ($_harLabel)'),
            style: ElevatedButton.styleFrom(
              backgroundColor: Colors.red,
              foregroundColor: Colors.white,
              minimumSize: const Size(double.infinity, 50),
            ),
            onPressed: () async {
              if (_recorder.isRecording) {
                var f = await _recorder.stop();
                setState(() {});
                if (f != null && mounted) {
                  // Ground-truth de pasos para calibrar el pedómetro.
                  final steps = await _askSteps();
                  if (steps != null) {
                    f = await _recorder.tagGroundTruthSteps(f, steps);
                    setState(() {});
                  }
                  if (mounted) {
                    ScaffoldMessenger.of(context).showSnackBar(
                      SnackBar(content: Text('Guardado: ${f.path.split('/').last}')),
                    );
                  }
                }
              } else {
                await _recorder.startImu(label: _harLabel);
                setState(() {});
              }
            },
          ),
        ],
      ),
    );
  }

  Widget _ecgTab() {
    return Padding(
      padding: const EdgeInsets.all(12),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Expanded(
                child: ElevatedButton.icon(
                  icon: Icon(_ecgMode ? Icons.stop : Icons.play_arrow),
                  label: Text(_ecgMode ? 'Detener stream' : 'Iniciar ECG'),
                  style: ElevatedButton.styleFrom(
                    backgroundColor: _ecgMode ? Colors.red : Colors.green,
                    foregroundColor: Colors.white,
                  ),
                  onPressed: _toggleEcg,
                ),
              ),
              const SizedBox(width: 8),
              Expanded(
                child: ElevatedButton.icon(
                  icon: Icon(_recorder.isRecording &&
                          _recorder.mode == RecordingType.ecgRaw
                      ? Icons.stop
                      : Icons.fiber_manual_record),
                  label: Text(_recorder.isRecording &&
                          _recorder.mode == RecordingType.ecgRaw
                      ? 'Detener REC'
                      : 'REC ECG'),
                  style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.red,
                    foregroundColor: Colors.white,
                  ),
                  onPressed: () async {
                    if (_recorder.isRecording) {
                      await _recorder.stop();
                    } else {
                      await _recorder.startEcg();
                    }
                    setState(() {});
                  },
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),
          Expanded(
            child: _wavePanel('ECG raw', [_line(_ecg, Colors.greenAccent)],
                fixedHeight: 360),
          ),
        ],
      ),
    );
  }

  Widget _consoleTab() {
    final ble = context.watch<BleService>();
    return Column(
      children: [
        Padding(
          padding: const EdgeInsets.all(12),
          child: Row(children: [
            Expanded(
              child: TextField(
                controller: _cmdController,
                decoration: const InputDecoration(
                  labelText: 'Byte (ej. 0x01, 0, 255)',
                  border: OutlineInputBorder(),
                ),
              ),
            ),
            const SizedBox(width: 8),
            ElevatedButton(
              onPressed: () async {
                final raw = _cmdController.text.trim();
                final v = raw.startsWith('0x')
                    ? int.tryParse(raw.substring(2), radix: 16)
                    : int.tryParse(raw);
                if (v == null || v < 0 || v > 255) return;
                await ble.sendCommand(v);
                if (!mounted) return;
                ScaffoldMessenger.of(context).showSnackBar(
                  SnackBar(content: Text('Enviado 0x${v.toRadixString(16)}')),
                );
              },
              child: const Text('Enviar'),
            ),
          ]),
        ),
        const Divider(),
        Expanded(
          child: ListView.builder(
            padding: const EdgeInsets.symmetric(horizontal: 12),
            itemCount: ble.log.length,
            itemBuilder: (_, i) => Text(
              ble.log[i],
              style: const TextStyle(fontFamily: 'monospace', fontSize: 11),
            ),
          ),
        ),
      ],
    );
  }

  Widget _recordingsTab() {
    final recs = LocalStore.listRecordings();
    if (recs.isEmpty) {
      return const Center(child: Text('Sin grabaciones todavía.'));
    }
    return ListView.builder(
      padding: const EdgeInsets.all(12),
      itemCount: recs.length,
      itemBuilder: (_, i) {
        final r = recs[i];
        final type = r['type'] ?? 'imu';
        final size = (r['sizeBytes'] ?? 0) as int;
        final dur = ((r['durationMs'] ?? 0) as int) ~/ 1000;
        return Card(
          child: ListTile(
            leading: Icon(
              type == 'ecgRaw' ? Icons.monitor_heart : Icons.show_chart,
              color: AppTheme.textMuted,
            ),
            title: Text(r['id'] ?? '—'),
            // Dev-mode recordings stay on-device only (ML dataset). Share to
            // pull them off via the OS sheet; nothing goes to the cloud.
            subtitle: Text('${(size / 1024).toStringAsFixed(1)} KB · ${dur}s · local'),
            trailing: Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                IconButton(
                  icon: const Icon(Icons.share),
                  onPressed: () => _shareLocal(r),
                ),
                IconButton(
                  icon: const Icon(Icons.delete_outline),
                  onPressed: () => _deleteRec(r),
                ),
              ],
            ),
          ),
        );
      },
    );
  }

  Future<void> _shareLocal(Map<String, dynamic> r) async {
    final path = r['localPath'] as String?;
    if (path == null) return;
    await Share.shareXFiles([XFile(path)]);
  }

  Future<void> _deleteRec(Map<String, dynamic> r) async {
    final id = r['id'];
    await LocalStore.deleteRecording(id);
    final path = r['localPath'] as String?;
    if (path != null) {
      try {
        await File(path).delete();
      } catch (_) {}
    }
    setState(() {});
  }

  Widget _chip(String label, String value) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
      decoration: BoxDecoration(
        color: AppTheme.borderColor.withValues(alpha: 0.4),
        borderRadius: BorderRadius.circular(8),
      ),
      child: Text(
        '$label: $value',
        style: const TextStyle(fontSize: 11, fontFamily: 'monospace'),
      ),
    );
  }

  Widget _wavePanel(String title, List<LineChartBarData> bars,
      {double fixedHeight = 160}) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(title, style: const TextStyle(fontWeight: FontWeight.w600)),
            const SizedBox(height: 8),
            SizedBox(
              height: fixedHeight,
              child: LineChart(
                LineChartData(
                  lineBarsData: bars,
                  borderData: FlBorderData(show: false),
                  titlesData: const FlTitlesData(show: false),
                  gridData: const FlGridData(show: false),
                  lineTouchData: const LineTouchData(enabled: false),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  LineChartBarData _line(List<double> data, Color color) {
    return LineChartBarData(
      spots: [for (var i = 0; i < data.length; i++) FlSpot(i.toDouble(), data[i])],
      color: color,
      barWidth: 1.5,
      dotData: const FlDotData(show: false),
      isCurved: false,
    );
  }
}
