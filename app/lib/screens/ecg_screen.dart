import 'package:flutter/material.dart';
import 'package:fl_chart/fl_chart.dart';
import 'package:intl/intl.dart';
import '../config/theme.dart';
import '../services/auth_service.dart';
import '../services/firestore_service.dart';
import '../models/session_model.dart';

/// Historial de ECG. Lee sesiones con grabación (`session.ecg != null`) desde
/// Firestore y descarga la señal cruda (Blob gzip) de forma perezosa al
/// seleccionar una sesión. Reemplaza la antigua subcolección `ecgReadings`.
class EcgScreen extends StatefulWidget {
  const EcgScreen({super.key});

  @override
  State<EcgScreen> createState() => _EcgScreenState();
}

class _EcgScreenState extends State<EcgScreen> {
  final _authService = AuthService();
  final _firestoreService = FirestoreService();

  List<SessionModel>? _sessions;
  SessionModel? _selected;
  List<double>? _samples; // raw ECG of the selected session
  bool _isLoading = false;
  bool _loadingSamples = false;

  @override
  void initState() {
    super.initState();
    _loadSessions();
  }

  Future<void> _loadSessions() async {
    final uid = _authService.currentUser?.uid;
    if (uid == null) return;

    setState(() => _isLoading = true);
    try {
      final sessions = await _firestoreService.getEcgSessions(uid);
      setState(() {
        _sessions = sessions;
        _selected = sessions.isNotEmpty ? sessions.first : null;
      });
      if (_selected != null) await _loadSamples(_selected!);
    } catch (_) {
      // Silencioso: la UI muestra el placeholder vacío.
    } finally {
      if (mounted) setState(() => _isLoading = false);
    }
  }

  Future<void> _loadSamples(SessionModel s) async {
    final uid = _authService.currentUser?.uid;
    if (uid == null) return;

    setState(() {
      _loadingSamples = true;
      _samples = null;
    });
    try {
      final samples = await _firestoreService.downloadSessionEcg(uid, s.id);
      if (!mounted) return;
      setState(() => _samples = samples);
    } catch (_) {
      if (mounted) setState(() => _samples = const []);
    } finally {
      if (mounted) setState(() => _loadingSamples = false);
    }
  }

  void _select(SessionModel s) {
    if (_selected?.id == s.id) return;
    setState(() => _selected = s);
    _loadSamples(s);
  }

  @override
  Widget build(BuildContext context) {
    return RefreshIndicator(
      onRefresh: _loadSessions,
      child: SingleChildScrollView(
        physics: const AlwaysScrollableScrollPhysics(),
        padding: const EdgeInsets.all(20),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('Electrocardiograma', style: Theme.of(context).textTheme.displayMedium),
            const SizedBox(height: 4),
            Text(
              'Señal ECG procesada con Pan-Tompkins',
              style: Theme.of(context).textTheme.bodyMedium,
            ),
            const SizedBox(height: 24),
            _buildEcgChart(),
            const SizedBox(height: 24),
            _buildResults(),
            const SizedBox(height: 24),
            _buildHistory(),
          ],
        ),
      ),
    );
  }

  Widget _buildEcgChart() {
    final s = _selected;
    final rate = s?.ecg?.sampleRate ?? 500;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Row(
                  children: [
                    Container(
                      width: 36,
                      height: 36,
                      decoration: BoxDecoration(
                        borderRadius: BorderRadius.circular(8),
                        color: AppTheme.ecg.withValues(alpha: 0.15),
                      ),
                      child: const Icon(Icons.monitor_heart, color: AppTheme.ecg, size: 20),
                    ),
                    const SizedBox(width: 12),
                    Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text('Señal ECG', style: Theme.of(context).textTheme.titleMedium),
                        Text(
                          s?.ecg != null
                              ? '${(s!.ecg!.durationMs / 1000).toStringAsFixed(0)}s @ ${s.ecg!.sampleRate} Hz'
                              : 'Sin datos',
                          style: Theme.of(context).textTheme.bodyMedium?.copyWith(fontSize: 12),
                        ),
                      ],
                    ),
                  ],
                ),
                if (s?.analysis != null)
                  Container(
                    padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
                    decoration: BoxDecoration(
                      borderRadius: BorderRadius.circular(20),
                      color: AppTheme.spo2.withValues(alpha: 0.15),
                    ),
                    child: const Text(
                      '✓ Procesado',
                      style: TextStyle(
                        color: AppTheme.spo2,
                        fontSize: 12,
                        fontWeight: FontWeight.w600,
                      ),
                    ),
                  ),
              ],
            ),
            const SizedBox(height: 20),
            SizedBox(
              height: 200,
              child: _loadingSamples
                  ? const Center(child: CircularProgressIndicator(color: AppTheme.primary))
                  : (_samples != null && _samples!.isNotEmpty)
                      ? _buildLineChart(_samples!, rate)
                      : _buildPlaceholderChart(),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildLineChart(List<double> data, int samplingRate) {
    // Downsample for performance (show ~500 points max).
    final step = (data.length / 500).ceil().clamp(1, data.length);
    final spots = <FlSpot>[];
    for (int i = 0; i < data.length; i += step) {
      spots.add(FlSpot(i / samplingRate, data[i]));
    }

    return LineChart(
      LineChartData(
        gridData: FlGridData(
          show: true,
          drawVerticalLine: false,
          horizontalInterval: 0.5,
          getDrawingHorizontalLine: (value) => FlLine(
            color: AppTheme.borderColor.withValues(alpha: 0.3),
            strokeWidth: 0.5,
          ),
        ),
        titlesData: FlTitlesData(
          show: true,
          leftTitles: const AxisTitles(sideTitles: SideTitles(showTitles: false)),
          rightTitles: const AxisTitles(sideTitles: SideTitles(showTitles: false)),
          topTitles: const AxisTitles(sideTitles: SideTitles(showTitles: false)),
          bottomTitles: AxisTitles(
            sideTitles: SideTitles(
              showTitles: true,
              reservedSize: 22,
              interval: 5,
              getTitlesWidget: (value, meta) => Text(
                '${value.toInt()}s',
                style: const TextStyle(color: AppTheme.textMuted, fontSize: 10),
              ),
            ),
          ),
        ),
        borderData: FlBorderData(show: false),
        lineBarsData: [
          LineChartBarData(
            spots: spots,
            isCurved: true,
            curveSmoothness: 0.15,
            color: AppTheme.ecg,
            barWidth: 1.5,
            dotData: const FlDotData(show: false),
            belowBarData: BarAreaData(show: true, color: AppTheme.ecg.withValues(alpha: 0.08)),
          ),
        ],
        lineTouchData: const LineTouchData(enabled: false),
      ),
    );
  }

  Widget _buildPlaceholderChart() {
    return Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Icon(Icons.show_chart, size: 48, color: AppTheme.textMuted.withValues(alpha: 0.5)),
          const SizedBox(height: 12),
          Text('Sin grabaciones ECG',
              style: TextStyle(color: AppTheme.textMuted, fontSize: 14)),
          const SizedBox(height: 4),
          Text(
            'Realiza una medición desde "Medir ahora"',
            style: TextStyle(color: AppTheme.textMuted.withValues(alpha: 0.7), fontSize: 12),
          ),
        ],
      ),
    );
  }

  Widget _buildResults() {
    final a = _selected?.analysis;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('Resultados Pan-Tompkins', style: Theme.of(context).textTheme.titleMedium),
            const SizedBox(height: 16),
            Row(
              children: [
                _ResultTile(
                  label: 'BPM',
                  value: (a != null && a.hrMean > 0) ? a.hrMean.toStringAsFixed(0) : '--',
                  color: AppTheme.heartRate,
                  icon: Icons.favorite,
                ),
                const SizedBox(width: 12),
                _ResultTile(
                  label: 'HRV (SDNN)',
                  value: a?.hrvSdnn != null ? '${a!.hrvSdnn!.toStringAsFixed(1)} ms' : '--',
                  color: AppTheme.secondary,
                  icon: Icons.timeline,
                ),
                const SizedBox(width: 12),
                _ResultTile(
                  label: 'Picos R',
                  value: a?.rPeakCount.toString() ?? '--',
                  color: AppTheme.primary,
                  icon: Icons.stacked_line_chart,
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildHistory() {
    if (_isLoading) {
      return const Center(child: CircularProgressIndicator(color: AppTheme.primary));
    }
    final sessions = _sessions;
    if (sessions == null || sessions.isEmpty) return const SizedBox.shrink();

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text('Historial ECG', style: Theme.of(context).textTheme.titleMedium),
        const SizedBox(height: 12),
        ...sessions.map((s) {
          final a = s.analysis;
          final processed = a != null;
          return Card(
            margin: const EdgeInsets.only(bottom: 8),
            child: ListTile(
              selected: _selected?.id == s.id,
              selectedTileColor: AppTheme.primary.withValues(alpha: 0.08),
              leading: Icon(
                processed ? Icons.check_circle : Icons.pending,
                color: processed ? AppTheme.spo2 : AppTheme.textMuted,
              ),
              title: Text(
                processed && a.hrMean > 0
                    ? '${a.hrMean.toStringAsFixed(0)} BPM'
                    : 'Sin análisis',
                style: const TextStyle(color: AppTheme.textPrimary),
              ),
              subtitle: Text(
                '${DateFormat('d MMM HH:mm').format(s.startTime)} • '
                '${s.ecg?.sampleCount ?? 0} muestras',
                style: const TextStyle(color: AppTheme.textSecondary, fontSize: 12),
              ),
              onTap: () => _select(s),
            ),
          );
        }),
      ],
    );
  }
}

class _ResultTile extends StatelessWidget {
  final String label;
  final String value;
  final Color color;
  final IconData icon;

  const _ResultTile({
    required this.label,
    required this.value,
    required this.color,
    required this.icon,
  });

  @override
  Widget build(BuildContext context) {
    return Expanded(
      child: Container(
        padding: const EdgeInsets.all(16),
        decoration: BoxDecoration(
          borderRadius: BorderRadius.circular(12),
          color: color.withValues(alpha: 0.08),
          border: Border.all(color: color.withValues(alpha: 0.2)),
        ),
        child: Column(
          children: [
            Icon(icon, color: color, size: 20),
            const SizedBox(height: 8),
            Text(
              value,
              style: TextStyle(fontSize: 18, fontWeight: FontWeight.w700, color: color),
            ),
            const SizedBox(height: 2),
            Text(
              label,
              style: const TextStyle(fontSize: 10, color: AppTheme.textSecondary),
              textAlign: TextAlign.center,
            ),
          ],
        ),
      ),
    );
  }
}
