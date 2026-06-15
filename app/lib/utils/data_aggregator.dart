import '../models/session_model.dart';

enum TimeFrame { today, week, month }

class DataPoint {
  final double x;
  final double y;
  final DateTime originalTime;

  DataPoint(this.x, this.y, this.originalTime);
}

class AggregatedMetrics {
  final List<DataPoint> heartRate;
  final List<DataPoint> spo2;
  final List<DataPoint> temperature;
  final List<DataPoint> steps;

  AggregatedMetrics({
    required this.heartRate,
    required this.spo2,
    required this.temperature,
    required this.steps,
  });
}

class DataAggregator {
  /// Procesa una lista de sesiones crudas para generar los puntos de gráfica 
  /// dependiendo del TimeFrame seleccionado.
  static AggregatedMetrics process(List<SessionModel> sessions, TimeFrame timeFrame, DateTime referenceDate) {
    if (sessions.isEmpty) {
      return AggregatedMetrics(heartRate: [], spo2: [], temperature: [], steps: []);
    }

    switch (timeFrame) {
      case TimeFrame.today:
        return _processToday(sessions, referenceDate);
      case TimeFrame.week:
        return _processWeek(sessions, referenceDate);
      case TimeFrame.month:
        return _processMonth(sessions, referenceDate);
    }
  }

  /// Hoy: Muestra la data granular. El eje X es la hora del día en formato decimal.
  /// Ej: 14:30 -> 14.5
  static AggregatedMetrics _processToday(List<SessionModel> sessions, DateTime today) {
    final filtered = sessions.where((s) {
      return s.startTime.year == today.year &&
          s.startTime.month == today.month &&
          s.startTime.day == today.day;
    }).toList();
    
    // Sort chronologically for line charts
    filtered.sort((a, b) => a.startTime.compareTo(b.startTime));

    final hr = <DataPoint>[];
    final spo2 = <DataPoint>[];
    final temp = <DataPoint>[];
    final steps = <DataPoint>[];

    for (var s in filtered) {
      // Convert time to a decimal hour, e.g., 14:30 => 14.5
      final x = s.startTime.hour + (s.startTime.minute / 60.0);
      
      if (s.avgHeartRate != null && s.avgHeartRate! > 0) {
        hr.add(DataPoint(x, s.avgHeartRate!, s.startTime));
      }
      if (s.avgSpO2 != null && s.avgSpO2! > 0) {
        spo2.add(DataPoint(x, s.avgSpO2!, s.startTime));
      }
      if (s.avgTemperature != null && s.avgTemperature! > 0) {
        temp.add(DataPoint(x, s.avgTemperature!, s.startTime));
      }
      // Steps might be cumulative in a session, we plot each session's steps
      final stepsVal = s.steps ?? 0;
      if (stepsVal > 0) {
        steps.add(DataPoint(x, stepsVal.toDouble(), s.startTime));
      }
    }

    return AggregatedMetrics(heartRate: hr, spo2: spo2, temperature: temp, steps: steps);
  }

  /// Semana: Agrupa por día de la semana (Lunes=1, Domingo=7)
  static AggregatedMetrics _processWeek(List<SessionModel> sessions, DateTime referenceDate) {
    // Determine the start of the week (Monday)
    final startOfWeek = referenceDate.subtract(Duration(days: referenceDate.weekday - 1));
    startOfWeek.copyWith(hour: 0, minute: 0, second: 0, millisecond: 0, microsecond: 0);

    final buckets = <int, List<SessionModel>>{};
    for (int i = 1; i <= 7; i++) {
      buckets[i] = [];
    }

    for (var s in sessions) {
      if (s.startTime.isAfter(startOfWeek) || s.startTime.isAtSameMomentAs(startOfWeek)) {
        if (s.startTime.difference(startOfWeek).inDays <= 6) {
           buckets[s.startTime.weekday]?.add(s);
        }
      }
    }

    return _aggregateBuckets(buckets);
  }

  /// Mes: Agrupa por día del mes (1 al 31)
  static AggregatedMetrics _processMonth(List<SessionModel> sessions, DateTime referenceDate) {
    final buckets = <int, List<SessionModel>>{};
    final daysInMonth = DateTime(referenceDate.year, referenceDate.month + 1, 0).day;
    
    for (int i = 1; i <= daysInMonth; i++) {
      buckets[i] = [];
    }

    for (var s in sessions) {
      if (s.startTime.year == referenceDate.year && s.startTime.month == referenceDate.month) {
        buckets[s.startTime.day]?.add(s);
      }
    }

    return _aggregateBuckets(buckets);
  }

  static AggregatedMetrics _aggregateBuckets(Map<int, List<SessionModel>> buckets) {
    final hr = <DataPoint>[];
    final spo2 = <DataPoint>[];
    final temp = <DataPoint>[];
    final steps = <DataPoint>[];

    final sortedKeys = buckets.keys.toList()..sort();

    for (var key in sortedKeys) {
      final list = buckets[key]!;
      if (list.isEmpty) continue;

      double sumHr = 0, sumSpo2 = 0, sumTemp = 0;
      int countHr = 0, countSpo2 = 0, countTemp = 0;
      int totalSteps = 0;

      // Dummy date for the tooltip since it represents an entire day aggregate
      DateTime reprDate = list.first.startTime;

      for (var s in list) {
        if (s.avgHeartRate != null && s.avgHeartRate! > 0) {
          sumHr += s.avgHeartRate!;
          countHr++;
        }
        if (s.avgSpO2 != null && s.avgSpO2! > 0) {
          sumSpo2 += s.avgSpO2!;
          countSpo2++;
        }
        if (s.avgTemperature != null && s.avgTemperature! > 0) {
          sumTemp += s.avgTemperature!;
          countTemp++;
        }
        totalSteps += s.steps ?? 0;
      }

      final xVal = key.toDouble();
      if (countHr > 0) hr.add(DataPoint(xVal, sumHr / countHr, reprDate));
      if (countSpo2 > 0) spo2.add(DataPoint(xVal, sumSpo2 / countSpo2, reprDate));
      if (countTemp > 0) temp.add(DataPoint(xVal, sumTemp / countTemp, reprDate));
      if (totalSteps > 0) steps.add(DataPoint(xVal, totalSteps.toDouble(), reprDate));
    }

    return AggregatedMetrics(heartRate: hr, spo2: spo2, temperature: temp, steps: steps);
  }
}
