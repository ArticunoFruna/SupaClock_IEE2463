import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/ble_service.dart';
import '../config/theme.dart';

/// Pantalla de debugging BLE para validar conexión con el SupaClock (ESP32-C3)
class BleDebugScreen extends StatelessWidget {
  const BleDebugScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return const _BleDebugBody();
  }
}

class _BleDebugBody extends StatelessWidget {
  const _BleDebugBody();

  @override
  Widget build(BuildContext context) {
    final ble = context.watch<BleService>();

    return Scaffold(
      appBar: AppBar(
        title: const Text('Debug BLE'),
        actions: [
          // Indicador de estado de conexión
          Padding(
            padding: const EdgeInsets.only(right: 16),
            child: Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                Container(
                  width: 10,
                  height: 10,
                  decoration: BoxDecoration(
                    shape: BoxShape.circle,
                    color: ble.isConnected ? AppTheme.spo2 : AppTheme.danger,
                    boxShadow: [
                      BoxShadow(
                        color: (ble.isConnected ? AppTheme.spo2 : AppTheme.danger)
                            .withValues(alpha: 0.6),
                        blurRadius: 8,
                      ),
                    ],
                  ),
                ),
                const SizedBox(width: 8),
                Text(
                  ble.isConnected ? 'Conectado' : 'Desconectado',
                  style: TextStyle(
                    fontSize: 13,
                    color: ble.isConnected ? AppTheme.spo2 : AppTheme.textMuted,
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
      body: Column(
        children: [
          // Panel de métricas en vivo
          _buildLiveMetrics(context, ble),
          const Divider(height: 1),
          // Botones de acción
          _buildActionBar(context, ble),
          const Divider(height: 1),
          // Log en vivo (como un terminal)
          Expanded(child: _buildLogConsole(context, ble)),
        ],
      ),
    );
  }

  Widget _buildLiveMetrics(BuildContext context, BleService ble) {
    final data = ble.lastTelemetry;

    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
      child: Row(
        children: [
          _MiniMetric(
            icon: Icons.favorite,
            color: AppTheme.heartRate,
            value: data != null ? '${data.heartRate}' : '--',
            unit: 'bpm',
          ),
          _MiniMetric(
            icon: Icons.water_drop,
            color: AppTheme.spo2,
            value: data != null ? '${data.spo2}' : '--',
            unit: '%',
          ),
          _MiniMetric(
            icon: Icons.thermostat,
            color: AppTheme.temperature,
            value: data?.temperature?.toStringAsFixed(1) ?? '--',
            unit: '°C',
          ),
          _MiniMetric(
            icon: Icons.directions_walk,
            color: AppTheme.steps,
            value: data != null ? '${data.steps}' : '--',
            unit: 'pasos',
          ),
        ],
      ),
    );
  }

  Widget _buildActionBar(BuildContext context, BleService ble) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
      child: Row(
        children: [
          Expanded(
            child: ElevatedButton.icon(
              onPressed: ble.isConnected
                  ? null
                  : () => ble.startScan(),
              icon: Icon(
                ble.isScanning ? Icons.bluetooth_searching : Icons.search,
                size: 20,
              ),
              label: Text(ble.isScanning ? 'Buscando...' : 'Buscar SupaClock'),
              style: ElevatedButton.styleFrom(
                backgroundColor: AppTheme.secondary,
                foregroundColor: Colors.black,
                padding: const EdgeInsets.symmetric(vertical: 14),
              ),
            ),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: OutlinedButton.icon(
              onPressed: ble.isConnected
                  ? () => ble.disconnect()
                  : null,
              icon: const Icon(Icons.bluetooth_disabled, size: 20),
              label: const Text('Desconectar'),
              style: OutlinedButton.styleFrom(
                foregroundColor: AppTheme.danger,
                side: const BorderSide(color: AppTheme.danger),
                padding: const EdgeInsets.symmetric(vertical: 14),
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildLogConsole(BuildContext context, BleService ble) {
    final isDark = Theme.of(context).brightness == Brightness.dark;
    final logs = ble.log;

    return Container(
      color: isDark ? const Color(0xFF0A0E14) : const Color(0xFFF0F2F5),
      child: logs.isEmpty
          ? Center(
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Icon(
                    Icons.terminal,
                    size: 48,
                    color: AppTheme.textMuted.withValues(alpha: 0.4),
                  ),
                  const SizedBox(height: 12),
                  Text(
                    'Consola de Debug BLE',
                    style: TextStyle(
                      color: AppTheme.textMuted,
                      fontSize: 14,
                    ),
                  ),
                  const SizedBox(height: 4),
                  Text(
                    'Presiona "Buscar SupaClock" para iniciar',
                    style: TextStyle(
                      color: AppTheme.textMuted.withValues(alpha: 0.6),
                      fontSize: 12,
                    ),
                  ),
                ],
              ),
            )
          : ListView.builder(
              padding: const EdgeInsets.all(12),
              itemCount: logs.length,
              itemBuilder: (_, i) {
                final line = logs[i];
                // Colorear según tipo de mensaje
                Color textColor;
                if (line.contains('✓') || line.contains('encontrado') || line.contains('Conectad')) {
                  textColor = AppTheme.spo2;
                } else if (line.contains('✗') || line.contains('Error') || line.contains('Desconect')) {
                  textColor = AppTheme.danger;
                } else if (line.contains('←')) {
                  textColor = AppTheme.secondary;
                } else {
                  textColor = isDark ? const Color(0xFF8B949E) : const Color(0xFF57606A);
                }

                return Padding(
                  padding: const EdgeInsets.symmetric(vertical: 1.5),
                  child: Text(
                    line,
                    style: TextStyle(
                      fontFamily: 'monospace',
                      fontSize: 12,
                      color: textColor,
                      height: 1.4,
                    ),
                  ),
                );
              },
            ),
    );
  }
}

class _MiniMetric extends StatelessWidget {
  final IconData icon;
  final Color color;
  final String value;
  final String unit;

  const _MiniMetric({
    required this.icon,
    required this.color,
    required this.value,
    required this.unit,
  });

  @override
  Widget build(BuildContext context) {
    return Expanded(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(icon, color: color, size: 20),
          const SizedBox(height: 4),
          Text(
            value,
            style: TextStyle(
              fontSize: 18,
              fontWeight: FontWeight.w700,
              color: color,
            ),
          ),
          Text(
            unit,
            style: const TextStyle(
              fontSize: 10,
              color: AppTheme.textMuted,
            ),
          ),
        ],
      ),
    );
  }
}
