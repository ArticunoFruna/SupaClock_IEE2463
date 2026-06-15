import 'package:flutter/foundation.dart';
import 'package:flutter_local_notifications/flutter_local_notifications.dart';

import '../models/alert_model.dart';

/// Local-only notification surface (no FCM yet — needs no Cloud Function).
class NotificationsService {
  static final _plugin = FlutterLocalNotificationsPlugin();
  static const _channelId = 'supaclock_alerts';
  static const _channelName = 'Alertas SupaClock';
  static bool _initialized = false;

  Future<void> init() async {
    if (_initialized) return;
    const androidInit = AndroidInitializationSettings('@mipmap/ic_launcher');
    const init = InitializationSettings(android: androidInit);
    try {
      await _plugin.initialize(init);

      // Create the notification channel (Android 8+).
      final android = _plugin.resolvePlatformSpecificImplementation<
          AndroidFlutterLocalNotificationsPlugin>();
      await android?.createNotificationChannel(
        const AndroidNotificationChannel(
          _channelId,
          _channelName,
          description: 'Notificaciones de salud y estado del dispositivo',
          importance: Importance.high,
        ),
      );

      // Android 13+ requires runtime permission.
      await android?.requestNotificationsPermission();
      _initialized = true;
    } catch (e) {
      debugPrint('Notifications init failed: $e');
    }
  }

  Future<void> showAlert(AlertType type, double value) async {
    if (!_initialized) await init();

    final (title, body) = _format(type, value);
    final id = type.index * 1000 +
        (DateTime.now().millisecondsSinceEpoch % 1000);

    try {
      await _plugin.show(
        id,
        title,
        body,
        const NotificationDetails(
          android: AndroidNotificationDetails(
            _channelId,
            _channelName,
            importance: Importance.high,
            priority: Priority.high,
          ),
        ),
      );
    } catch (e) {
      debugPrint('Show notification failed: $e');
    }
  }

  (String, String) _format(AlertType type, double value) {
    switch (type) {
      case AlertType.hrHigh:
        return ('Frecuencia cardíaca elevada', '${value.toInt()} bpm sostenido');
      case AlertType.hrLow:
        return ('Frecuencia cardíaca baja', '${value.toInt()} bpm sostenido');
      case AlertType.spo2Low:
        return ('SpO₂ bajo', '${value.toInt()}% — revisa el sensor');
      case AlertType.tempHigh:
        return ('Temperatura elevada', '${value.toStringAsFixed(1)} °C');
      case AlertType.batteryLow:
        return ('Batería baja', 'Reloj al ${value.toInt()}%');
      case AlertType.deviceLost:
        return ('Reloj desconectado', 'Sin sincronización reciente');
      case AlertType.fallDetected:
        return ('Posible caída detectada', 'Toca para confirmar que estás bien');
    }
  }
}
