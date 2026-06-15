import 'dart:async';

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../config/theme.dart';
import '../models/session_model.dart';
import '../services/auth_service.dart';
import '../services/ble_service.dart';
import '../services/firestore_service.dart';
import '../services/pan_tompkins.dart';

/// 30-second spot-check: two modes (ECG or HR/SpO2).
class SpotCheckScreen extends StatefulWidget {
  const SpotCheckScreen({super.key});

  @override
  State<SpotCheckScreen> createState() => _SpotCheckScreenState();
}

enum _Phase { idle, recording, analyzing, done, error }

class _SpotCheckScreenState extends State<SpotCheckScreen> {
  static const int _kSampleRate = 500;
  static const int _kDurationSec = 30;

  final _auth = AuthService();
  final _fs = FirestoreService();

  _Phase _phase = _Phase.idle;
  bool _isEcgMode = true;

  StreamSubscription? _ecgSub;
  StreamSubscription? _telemSub;
  Timer? _countdownTimer;
  int _remainingSec = 0;

  final List<int> _ecgSamples = [];
  final List<int> _hrSamples = [];
  final List<int> _spo2Samples = [];

  PanTompkinsResult? _ecgResult;
  double? _finalHr;
  double? _finalSpo2;
  String? _error;

  @override
  void dispose() {
    _ecgSub?.cancel();
    _telemSub?.cancel();
    _countdownTimer?.cancel();
    super.dispose();
  }

  Future<void> _startEcg() async {
    _isEcgMode = true;
    if (!_checkConnected()) return;

    _startRecordingState();
    final ble = context.read<BleService>();

    _ecgSub = ble.ecgStream.listen((chunk) {
      _ecgSamples.addAll(chunk);
    });

    await ble.startEcgStream();
    _startTimer();
  }

  Future<void> _startHr() async {
    _isEcgMode = false;
    if (!_checkConnected()) return;

    _startRecordingState();
    final ble = context.read<BleService>();

    _telemSub = ble.telemetryStream.listen((t) {
      if (t.heartRate != null && (t.hrQuality ?? 0) >= 50) {
        _hrSamples.add(t.heartRate!);
      }
      if (t.spo2 != null && (t.spo2Quality ?? 0) >= 50) {
        _spo2Samples.add(t.spo2!);
      }
    });

    _startTimer();
  }

  bool _checkConnected() {
    final ble = context.read<BleService>();
    if (!ble.isConnected) {
      setState(() {
        _phase = _Phase.error;
        _error = 'Reloj no conectado.';
      });
      return false;
    }
    return true;
  }

  void _startRecordingState() {
    setState(() {
      _phase = _Phase.recording;
      _remainingSec = _kDurationSec;
      _ecgSamples.clear();
      _hrSamples.clear();
      _spo2Samples.clear();
      _ecgResult = null;
      _finalHr = null;
      _finalSpo2 = null;
      _error = null;
    });
  }

  void _startTimer() {
    _countdownTimer = Timer.periodic(const Duration(seconds: 1), (t) {
      if (!mounted) return;
      setState(() => _remainingSec--);
      if (_remainingSec <= 0) {
        t.cancel();
        _finish();
      }
    });
  }

  Future<void> _finish() async {
    final ble = context.read<BleService>();
    if (_isEcgMode) {
      await ble.stopEcgStream();
      await _ecgSub?.cancel();
      _ecgSub = null;
    } else {
      await _telemSub?.cancel();
      _telemSub = null;
    }

    setState(() => _phase = _Phase.analyzing);

    final uid = _auth.currentUser?.uid;

    if (_isEcgMode) {
      if (_ecgSamples.length < _kSampleRate * 5) {
        setState(() {
          _phase = _Phase.error;
          _error = 'Pocas muestras (${_ecgSamples.length}). ¿El reloj envió ECG?';
        });
        return;
      }

      final samplesD = _ecgSamples.map((s) => s.toDouble()).toList();
      final result = PanTompkins.analyze(samplesD, sampleRate: _kSampleRate.toDouble());

      if (uid != null) {
        try {
          await _persistEcg(uid, samplesD, result);
        } catch (e) {
          setState(() {
            _phase = _Phase.error;
            _error = 'Error guardando: $e';
          });
          return;
        }
      }

      setState(() {
        _ecgResult = result;
        _phase = _Phase.done;
      });
    } else {
      if (_hrSamples.isEmpty && _spo2Samples.isEmpty) {
        setState(() {
          _phase = _Phase.error;
          _error = 'No se obtuvieron muestras de calidad. Ajusta el reloj.';
        });
        return;
      }

      double avgHr = _hrSamples.isNotEmpty ? _hrSamples.reduce((a, b) => a + b) / _hrSamples.length : 0;
      double avgSpo2 = _spo2Samples.isNotEmpty ? _spo2Samples.reduce((a, b) => a + b) / _spo2Samples.length : 0;

      if (uid != null) {
        try {
          final session = SessionModel(
            id: '',
            type: SessionType.spot,
            startTime: DateTime.now().subtract(Duration(seconds: _kDurationSec)),
            endTime: DateTime.now(),
            avgHeartRate: avgHr > 0 ? avgHr : null,
            avgSpO2: avgSpo2 > 0 ? avgSpo2 : null,
            quality: 100, // averaged from high-quality samples
          );
          await _fs.createSession(uid, session);
        } catch (e) {
          setState(() {
            _phase = _Phase.error;
            _error = 'Error guardando: $e';
          });
          return;
        }
      }

      setState(() {
        _finalHr = avgHr;
        _finalSpo2 = avgSpo2;
        _phase = _Phase.done;
      });
    }
  }

  Future<void> _persistEcg(String uid, List<double> samples, PanTompkinsResult res) async {
    final start = DateTime.now().subtract(Duration(seconds: _kDurationSec));
    final session = SessionModel(
      id: '',
      type: SessionType.spot,
      startTime: start,
      endTime: DateTime.now(),
      avgHeartRate: res.bpmMean > 0 ? res.bpmMean : null,
      quality: (res.qualityScore * 100).round(),
      analysis: EcgAnalysis(
        rPeakCount: res.rPeakIndices.length,
        hrMean: res.bpmMean,
        hrvSdnn: res.sdnnMs,
        hrvRmssd: res.rmssdMs,
        rrMeanMs: res.rrIntervalsMs.isEmpty
            ? null
            : res.rrIntervalsMs.reduce((a, b) => a + b) / res.rrIntervalsMs.length,
        qualityScore: res.qualityScore,
      ),
    );

    final sessionId = await _fs.createSession(uid, session);

    final csv = StringBuffer('timestamp_ms,ecg_raw\n');
    final endTs = DateTime.now().millisecondsSinceEpoch;
    for (var i = 0; i < samples.length; i++) {
      final ts = endTs - (samples.length - 1 - i) * 2;
      csv.write('$ts,${samples[i].toStringAsFixed(0)}\n');
    }
    
    // New Blob approach instead of Firebase Storage
    final storagePath = await _fs.uploadSessionCsvGz(uid, sessionId, csv.toString());

    await _fs.updateSession(uid, sessionId, {
      'ecg': EcgMeta(
        storagePath: storagePath,
        sampleRate: _kSampleRate,
        durationMs: _kDurationSec * 1000,
        sampleCount: samples.length,
      ).toMap(),
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Medir ahora')),
      body: SafeArea(
        child: Padding(
          padding: const EdgeInsets.all(20),
          child: _buildBody(),
        ),
      ),
    );
  }

  Widget _buildBody() {
    switch (_phase) {
      case _Phase.idle:
        return _buildIdle();
      case _Phase.recording:
        return _buildRecording();
      case _Phase.analyzing:
        return const Center(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              CircularProgressIndicator(color: AppTheme.primary),
              SizedBox(height: 16),
              Text('Guardando resultados...'),
            ],
          ),
        );
      case _Phase.done:
        return _isEcgMode ? _buildDoneEcg() : _buildDoneHr();
      case _Phase.error:
        return _buildError();
    }
  }

  Widget _buildIdle() {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const Text(
          'Elige el tipo de medición',
          style: TextStyle(fontSize: 22, fontWeight: FontWeight.w700),
        ),
        const SizedBox(height: 8),
        const Text(
          'Mantén el reloj en contacto firme con la piel y permanece en reposo.',
          style: TextStyle(fontSize: 14, color: AppTheme.textSecondary),
        ),
        const SizedBox(height: 32),
        _OptionCard(
          title: 'ECG Completo',
          subtitle: '30 segundos · Frecuencia Cardíaca · VFC · Calidad de señal',
          icon: Icons.monitor_heart,
          color: AppTheme.primary,
          onTap: _startEcg,
        ),
        const SizedBox(height: 16),
        _OptionCard(
          title: 'Signos Vitales',
          subtitle: '30 segundos · Frecuencia Cardíaca · SpO₂',
          icon: Icons.favorite,
          color: AppTheme.heartRate,
          onTap: _startHr,
        ),
      ],
    );
  }

  Widget _buildRecording() {
    final progress = 1.0 - (_remainingSec / _kDurationSec);
    return Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          SizedBox(
            width: 180,
            height: 180,
            child: Stack(
              alignment: Alignment.center,
              children: [
                SizedBox(
                  width: 180,
                  height: 180,
                  child: CircularProgressIndicator(
                    value: progress,
                    strokeWidth: 10,
                    color: _isEcgMode ? AppTheme.primary : AppTheme.heartRate,
                    backgroundColor: AppTheme.borderColor,
                  ),
                ),
                Text(
                  '${_remainingSec}s',
                  style: const TextStyle(fontSize: 42, fontWeight: FontWeight.w700),
                ),
              ],
            ),
          ),
          const SizedBox(height: 24),
          if (_isEcgMode)
            Text('Muestras ECG: ${_ecgSamples.length}')
          else
            Text('Muestras FC: ${_hrSamples.length} · SpO₂: ${_spo2Samples.length}'),
          const SizedBox(height: 8),
          const Text('Manten contacto firme — quédate quieto/a'),
        ],
      ),
    );
  }

  Widget _buildDoneEcg() {
    final res = _ecgResult;
    if (res == null) return _buildError();
    return SingleChildScrollView(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Text(
            'Resultados ECG',
            style: TextStyle(fontSize: 22, fontWeight: FontWeight.w700),
          ),
          const SizedBox(height: 16),
          _MetricBig('FC', res.bpmMean.toStringAsFixed(0), 'bpm', AppTheme.heartRate),
          _MetricBig('Calidad', (res.qualityScore * 100).toStringAsFixed(0), '%', AppTheme.primary),
          if (res.sdnnMs != null)
            _MetricBig('HRV (SDNN)', res.sdnnMs!.toStringAsFixed(1), 'ms', AppTheme.secondary),
          if (res.rmssdMs != null)
            _MetricBig('RMSSD', res.rmssdMs!.toStringAsFixed(1), 'ms', AppTheme.secondary),
          _MetricBig('Picos R', '${res.rPeakIndices.length}', 'detectados', AppTheme.textPrimary),
          const SizedBox(height: 24),
          SizedBox(
            width: double.infinity,
            child: OutlinedButton(
              onPressed: () => Navigator.of(context).pop(),
              child: const Text('Volver'),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildDoneHr() {
    return SingleChildScrollView(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Text(
            'Resultados de Signos Vitales',
            style: TextStyle(fontSize: 22, fontWeight: FontWeight.w700),
          ),
          const SizedBox(height: 16),
          _MetricBig(
            'FC',
            _finalHr != null && _finalHr! > 0 ? _finalHr!.toStringAsFixed(0) : '--',
            'bpm',
            AppTheme.heartRate,
          ),
          _MetricBig(
            'SpO₂',
            _finalSpo2 != null && _finalSpo2! > 0 ? _finalSpo2!.toStringAsFixed(1) : '--',
            '%',
            AppTheme.spo2,
          ),
          const SizedBox(height: 24),
          SizedBox(
            width: double.infinity,
            child: OutlinedButton(
              onPressed: () => Navigator.of(context).pop(),
              child: const Text('Volver'),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildError() {
    return Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          const Icon(Icons.error_outline, size: 64, color: AppTheme.danger),
          const SizedBox(height: 16),
          Text(_error ?? 'Error desconocido', textAlign: TextAlign.center),
          const SizedBox(height: 24),
          OutlinedButton(
            onPressed: () => setState(() => _phase = _Phase.idle),
            child: const Text('Reintentar'),
          ),
        ],
      ),
    );
  }
}

class _OptionCard extends StatelessWidget {
  final String title;
  final String subtitle;
  final IconData icon;
  final Color color;
  final VoidCallback onTap;

  const _OptionCard({
    required this.title,
    required this.subtitle,
    required this.icon,
    required this.color,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    return InkWell(
      onTap: onTap,
      borderRadius: BorderRadius.circular(16),
      child: Container(
        padding: const EdgeInsets.all(20),
        decoration: BoxDecoration(
          border: Border.all(color: AppTheme.borderColor),
          borderRadius: BorderRadius.circular(16),
        ),
        child: Row(
          children: [
            Container(
              width: 56,
              height: 56,
              decoration: BoxDecoration(
                color: color.withValues(alpha: 0.15),
                shape: BoxShape.circle,
              ),
              child: Icon(icon, color: color, size: 28),
            ),
            const SizedBox(width: 16),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(title, style: const TextStyle(fontSize: 18, fontWeight: FontWeight.w600)),
                  const SizedBox(height: 4),
                  Text(subtitle, style: const TextStyle(fontSize: 13, color: AppTheme.textSecondary)),
                ],
              ),
            ),
            const Icon(Icons.chevron_right, color: AppTheme.textMuted),
          ],
        ),
      ),
    );
  }
}

class _MetricBig extends StatelessWidget {
  final String label;
  final String value;
  final String unit;
  final Color color;
  const _MetricBig(this.label, this.value, this.unit, this.color);

  @override
  Widget build(BuildContext context) {
    return Card(
      margin: const EdgeInsets.only(bottom: 12),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Row(
          children: [
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(label, style: const TextStyle(color: AppTheme.textSecondary)),
                  const SizedBox(height: 4),
                  Row(
                    crossAxisAlignment: CrossAxisAlignment.baseline,
                    textBaseline: TextBaseline.alphabetic,
                    children: [
                      Text(
                        value,
                        style: TextStyle(
                          fontSize: 28,
                          fontWeight: FontWeight.w700,
                          color: color,
                        ),
                      ),
                      const SizedBox(width: 6),
                      Text(unit, style: const TextStyle(color: AppTheme.textMuted)),
                    ],
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}
