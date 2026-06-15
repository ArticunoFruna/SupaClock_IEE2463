import 'package:flutter/material.dart';
import 'package:fl_chart/fl_chart.dart';
import 'package:intl/intl.dart';
import '../models/session_model.dart';
import '../services/firestore_service.dart';
import '../services/auth_service.dart';
import '../utils/data_aggregator.dart';
import '../config/theme.dart';

class TrendsTab extends StatefulWidget {
  const TrendsTab({super.key});

  @override
  State<TrendsTab> createState() => _TrendsTabState();
}

class _TrendsTabState extends State<TrendsTab> {
  final _authService = AuthService();
  final _firestoreService = FirestoreService();

  TimeFrame _timeFrame = TimeFrame.today;

  @override
  Widget build(BuildContext context) {
    final uid = _authService.currentUser?.uid;
    if (uid == null) return const Center(child: Text('No autenticado'));

    return SafeArea(
      child: Column(
        children: [
          _buildSegmentedControl(),
          const SizedBox(height: 16),
          Expanded(
            child: StreamBuilder<List<SessionModel>>(
              // For week and month, we should ideally fetch more than the top 20 limit of streamSessions.
              // We'll fetch a stream of the last 100 sessions to ensure we get a week's data
              stream: _firestoreService.streamSessions(uid), 
              builder: (context, snapshot) {
                if (snapshot.connectionState == ConnectionState.waiting) {
                  return const Center(child: CircularProgressIndicator());
                }

                final sessions = snapshot.data ?? [];
                if (sessions.isEmpty) {
                  return const Center(
                    child: Text('No hay suficientes datos para generar gráficos.'),
                  );
                }

                // Process aggregated metrics based on selected TimeFrame
                final metrics = DataAggregator.process(sessions, _timeFrame, DateTime.now());

                return RefreshIndicator(
                  onRefresh: () async {},
                  child: ListView(
                    padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
                    children: [
                      _buildLineChartCard('Frecuencia Cardíaca (BPM)', metrics.heartRate, AppTheme.heartRate, 40, 200),
                      const SizedBox(height: 16),
                      _buildLineChartCard('SpO2 (%)', metrics.spo2, AppTheme.spo2, 80, 100),
                      const SizedBox(height: 16),
                      _buildLineChartCard('Temperatura (°C)', metrics.temperature, AppTheme.temperature, 30, 42),
                      const SizedBox(height: 16),
                      _buildBarChartCard('Pasos', metrics.steps, AppTheme.steps),
                      const SizedBox(height: 40),
                    ],
                  ),
                );
              },
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildSegmentedControl() {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      child: SegmentedButton<TimeFrame>(
        segments: const [
          ButtonSegment(
            value: TimeFrame.today,
            label: Text('Hoy'),
            icon: Icon(Icons.today),
          ),
          ButtonSegment(
            value: TimeFrame.week,
            label: Text('Semana'),
            icon: Icon(Icons.calendar_view_week),
          ),
          ButtonSegment(
            value: TimeFrame.month,
            label: Text('Mes'),
            icon: Icon(Icons.calendar_month),
          ),
        ],
        selected: {_timeFrame},
        onSelectionChanged: (Set<TimeFrame> newSelection) {
          setState(() {
            _timeFrame = newSelection.first;
          });
        },
      ),
    );
  }

  Widget _buildLineChartCard(String title, List<DataPoint> data, Color color, double minY, double maxY) {
    return Card(
      elevation: 0,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(16),
        side: const BorderSide(color: AppTheme.borderColor),
      ),
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(title, style: Theme.of(context).textTheme.titleMedium),
            const SizedBox(height: 24),
            SizedBox(
              height: 200,
              child: data.isEmpty
                  ? const Center(child: Text('Sin datos en este periodo'))
                  : LineChart(
                      LineChartData(
                        minY: minY,
                        maxY: maxY,
                        lineTouchData: LineTouchData(
                          touchTooltipData: LineTouchTooltipData(
                            getTooltipItems: (touchedSpots) {
                              return touchedSpots.map((spot) {
                                final point = data[spot.spotIndex];
                                final timeStr = _formatTooltipTime(point.originalTime);
                                return LineTooltipItem(
                                  '${spot.y.toStringAsFixed(1)}\n$timeStr',
                                  const TextStyle(color: Colors.white, fontWeight: FontWeight.bold),
                                );
                              }).toList();
                            },
                          ),
                        ),
                        gridData: const FlGridData(show: false),
                        titlesData: FlTitlesData(
                          rightTitles: const AxisTitles(sideTitles: SideTitles(showTitles: false)),
                          topTitles: const AxisTitles(sideTitles: SideTitles(showTitles: false)),
                          leftTitles: AxisTitles(
                            sideTitles: SideTitles(
                              showTitles: true,
                              reservedSize: 40,
                              getTitlesWidget: (value, meta) {
                                return Text(value.toInt().toString(), style: const TextStyle(fontSize: 10));
                              },
                            ),
                          ),
                          bottomTitles: AxisTitles(
                            sideTitles: SideTitles(
                              showTitles: true,
                              getTitlesWidget: (value, meta) {
                                return Padding(
                                  padding: const EdgeInsets.only(top: 8),
                                  child: Text(_formatXAxis(value), style: const TextStyle(fontSize: 10)),
                                );
                              },
                            ),
                          ),
                        ),
                        borderData: FlBorderData(show: false),
                        lineBarsData: [
                          LineChartBarData(
                            spots: data.map((d) => FlSpot(d.x, d.y)).toList(),
                            isCurved: true,
                            color: color,
                            barWidth: 3,
                            isStrokeCapRound: true,
                            dotData: FlDotData(show: _timeFrame != TimeFrame.today), // For today, too many dots
                            belowBarData: BarAreaData(
                              show: true,
                              color: color.withValues(alpha: 0.1),
                            ),
                          ),
                        ],
                      ),
                    ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildBarChartCard(String title, List<DataPoint> data, Color color) {
    return Card(
      elevation: 0,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(16),
        side: const BorderSide(color: AppTheme.borderColor),
      ),
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(title, style: Theme.of(context).textTheme.titleMedium),
            const SizedBox(height: 24),
            SizedBox(
              height: 200,
              child: data.isEmpty
                  ? const Center(child: Text('Sin datos en este periodo'))
                  : BarChart(
                      BarChartData(
                        barTouchData: BarTouchData(
                          touchTooltipData: BarTouchTooltipData(
                            getTooltipItem: (group, groupIndex, rod, rodIndex) {
                              final point = data[groupIndex];
                              return BarTooltipItem(
                                '${rod.toY.toInt()} pasos\n${_formatTooltipTime(point.originalTime)}',
                                const TextStyle(color: Colors.white, fontWeight: FontWeight.bold),
                              );
                            },
                          ),
                        ),
                        titlesData: FlTitlesData(
                          rightTitles: const AxisTitles(sideTitles: SideTitles(showTitles: false)),
                          topTitles: const AxisTitles(sideTitles: SideTitles(showTitles: false)),
                          bottomTitles: AxisTitles(
                            sideTitles: SideTitles(
                              showTitles: true,
                              getTitlesWidget: (value, meta) {
                                return Padding(
                                  padding: const EdgeInsets.only(top: 8),
                                  child: Text(_formatXAxis(value), style: const TextStyle(fontSize: 10)),
                                );
                              },
                            ),
                          ),
                        ),
                        borderData: FlBorderData(show: false),
                        barGroups: data.map((d) {
                          return BarChartGroupData(
                            x: d.x.toInt(),
                            barRods: [
                              BarChartRodData(
                                toY: d.y,
                                color: color,
                                width: 12,
                                borderRadius: BorderRadius.circular(4),
                              ),
                            ],
                          );
                        }).toList(),
                      ),
                    ),
            ),
          ],
        ),
      ),
    );
  }

  // Formatting helpers based on the selected timeframe

  String _formatXAxis(double x) {
    switch (_timeFrame) {
      case TimeFrame.today:
        // x = hour (e.g. 14.5)
        final hourInt = x.toInt();
        return '$hourInt:00';
      case TimeFrame.week:
        // x = weekday number (1-7)
        const days = ['L', 'M', 'X', 'J', 'V', 'S', 'D'];
        final idx = x.toInt() - 1;
        if (idx >= 0 && idx < 7) return days[idx];
        return '';
      case TimeFrame.month:
        // x = day of month
        if (x.toInt() % 5 == 1) return x.toInt().toString(); // Show every 5 days
        return '';
    }
  }

  String _formatTooltipTime(DateTime time) {
    switch (_timeFrame) {
      case TimeFrame.today:
        return DateFormat('HH:mm').format(time); // HH:MM
      case TimeFrame.week:
        return DateFormat('EEEE').format(time); // "lunes"
      case TimeFrame.month:
        return DateFormat('d MMM').format(time); // "15 mar"
    }
  }
}
