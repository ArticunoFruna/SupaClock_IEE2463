import 'package:flutter/material.dart';
import 'package:fl_chart/fl_chart.dart';
import '../config/theme.dart';
import '../services/auth_service.dart';
import '../services/firestore_service.dart';
import '../models/ecg_reading_model.dart';

class EcgScreen extends StatefulWidget {
  const EcgScreen({super.key});

  @override
  State<EcgScreen> createState() => _EcgScreenState();
}

class _EcgScreenState extends State<EcgScreen> {
  final _authService = AuthService();
  final _firestoreService = FirestoreService();
  List<EcgReadingModel>? _readings;
  EcgReadingModel? _selectedReading;
  bool _isLoading = false;

  @override
  void initState() {
    super.initState();
    _loadReadings();
  }

  Future<void> _loadReadings() async {
    final uid = _authService.currentUser?.uid;
    if (uid == null) return;

    setState(() => _isLoading = true);

    try {
      // Get the most recent session's ECG readings
      final sessions = await _firestoreService.getSessions(uid, limit: 5);
      List<EcgReadingModel> allReadings = [];

      for (final session in sessions) {
        final readings =
            await _firestoreService.getEcgReadings(uid, session.id);
        allReadings.addAll(readings);
      }

      setState(() {
        _readings = allReadings;
        if (allReadings.isNotEmpty) {
          _selectedReading = allReadings.first;
        }
      });
    } catch (e) {
      // Handle gracefully
    } finally {
      setState(() => _isLoading = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return SingleChildScrollView(
      padding: const EdgeInsets.all(20),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Title
          Text(
            'Electrocardiograma',
            style: Theme.of(context).textTheme.displayMedium,
          ),
          const SizedBox(height: 4),
          Text(
            'Señal ECG procesada con Pan-Tompkins',
            style: Theme.of(context).textTheme.bodyMedium,
          ),
          const SizedBox(height: 24),

          // ECG Chart
          _buildEcgChart(),
          const SizedBox(height: 24),

          // Results
          _buildResults(),
          const SizedBox(height: 24),

          // History
          _buildHistory(),
        ],
      ),
    );
  }

  Widget _buildEcgChart() {
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
                      child: const Icon(
                        Icons.monitor_heart,
                        color: AppTheme.ecg,
                        size: 20,
                      ),
                    ),
                    const SizedBox(width: 12),
                    Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          'Señal ECG',
                          style: Theme.of(context).textTheme.titleMedium,
                        ),
                        Text(
                          _selectedReading != null
                              ? '${_selectedReading!.durationSeconds.toStringAsFixed(0)}s @ ${_selectedReading!.samplingRate.toInt()} Hz'
                              : 'Sin datos',
                          style: Theme.of(context)
                              .textTheme
                              .bodyMedium
                              ?.copyWith(fontSize: 12),
                        ),
                      ],
                    ),
                  ],
                ),
                if (_selectedReading?.isProcessed == true)
                  Container(
                    padding:
                        const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
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

            // Chart
            SizedBox(
              height: 200,
              child: _selectedReading != null && _selectedReading!.rawData.isNotEmpty
                  ? _buildLineChart(_selectedReading!)
                  : _buildPlaceholderChart(),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildLineChart(EcgReadingModel reading) {
    // Downsample for performance (show every Nth point)
    final data = reading.rawData;
    final step = (data.length / 500).ceil().clamp(1, data.length);

    final spots = <FlSpot>[];
    for (int i = 0; i < data.length; i += step) {
      spots.add(FlSpot(
        i / reading.samplingRate,
        data[i],
      ));
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
          leftTitles: const AxisTitles(
            sideTitles: SideTitles(showTitles: false),
          ),
          rightTitles: const AxisTitles(
            sideTitles: SideTitles(showTitles: false),
          ),
          topTitles: const AxisTitles(
            sideTitles: SideTitles(showTitles: false),
          ),
          bottomTitles: AxisTitles(
            sideTitles: SideTitles(
              showTitles: true,
              reservedSize: 22,
              interval: 5,
              getTitlesWidget: (value, meta) {
                return Text(
                  '${value.toInt()}s',
                  style: const TextStyle(
                    color: AppTheme.textMuted,
                    fontSize: 10,
                  ),
                );
              },
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
            belowBarData: BarAreaData(
              show: true,
              color: AppTheme.ecg.withValues(alpha: 0.08),
            ),
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
          Icon(
            Icons.show_chart,
            size: 48,
            color: AppTheme.textMuted.withValues(alpha: 0.5),
          ),
          const SizedBox(height: 12),
          Text(
            'Sin grabaciones ECG',
            style: TextStyle(
              color: AppTheme.textMuted,
              fontSize: 14,
            ),
          ),
          const SizedBox(height: 4),
          Text(
            'Realiza una medición desde el wearable',
            style: TextStyle(
              color: AppTheme.textMuted.withValues(alpha: 0.7),
              fontSize: 12,
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildResults() {
    final reading = _selectedReading;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'Resultados Pan-Tompkins',
              style: Theme.of(context).textTheme.titleMedium,
            ),
            const SizedBox(height: 16),
            Row(
              children: [
                _ResultTile(
                  label: 'BPM',
                  value: reading?.processedBPM?.toStringAsFixed(0) ?? '--',
                  color: AppTheme.heartRate,
                  icon: Icons.favorite,
                ),
                const SizedBox(width: 12),
                _ResultTile(
                  label: 'HRV (SDNN)',
                  value: reading?.hrv != null
                      ? '${reading!.hrv!.toStringAsFixed(1)} ms'
                      : '--',
                  color: AppTheme.secondary,
                  icon: Icons.timeline,
                ),
                const SizedBox(width: 12),
                _ResultTile(
                  label: 'Picos R',
                  value: reading?.rPeaks?.length.toString() ?? '--',
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
      return const Center(
        child: CircularProgressIndicator(color: AppTheme.primary),
      );
    }

    if (_readings == null || _readings!.isEmpty) {
      return const SizedBox.shrink();
    }

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          'Historial ECG',
          style: Theme.of(context).textTheme.titleMedium,
        ),
        const SizedBox(height: 12),
        ...(_readings!.map((r) => Card(
              margin: const EdgeInsets.only(bottom: 8),
              child: ListTile(
                selected: _selectedReading?.id == r.id,
                selectedTileColor: AppTheme.primary.withValues(alpha: 0.08),
                leading: Icon(
                  r.isProcessed ? Icons.check_circle : Icons.pending,
                  color: r.isProcessed ? AppTheme.spo2 : AppTheme.textMuted,
                ),
                title: Text(
                  r.isProcessed
                      ? '${r.processedBPM?.toStringAsFixed(0)} BPM'
                      : 'Procesando...',
                  style: const TextStyle(color: AppTheme.textPrimary),
                ),
                subtitle: Text(
                  '${r.rawData.length} samples • ${r.durationSeconds.toStringAsFixed(0)}s',
                  style: const TextStyle(color: AppTheme.textSecondary, fontSize: 12),
                ),
                onTap: () => setState(() => _selectedReading = r),
              ),
            ))),
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
              style: TextStyle(
                fontSize: 18,
                fontWeight: FontWeight.w700,
                color: color,
              ),
            ),
            const SizedBox(height: 2),
            Text(
              label,
              style: const TextStyle(
                fontSize: 10,
                color: AppTheme.textSecondary,
              ),
              textAlign: TextAlign.center,
            ),
          ],
        ),
      ),
    );
  }
}
