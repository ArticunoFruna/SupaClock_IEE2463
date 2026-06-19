import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../config/theme.dart';
import '../models/user_model.dart';
import '../providers/settings_provider.dart';
import '../services/auth_service.dart';
import '../services/ble_service.dart';
import '../services/firestore_service.dart';
import '../services/local_store.dart';
import 'ble_debug_screen.dart';
import 'ecg_screen.dart';
import 'login_screen.dart';
import 'settings_screen.dart';
import 'spot_check_screen.dart';
import 'trends_tab.dart';

class DashboardScreen extends StatefulWidget {
  const DashboardScreen({super.key});

  @override
  State<DashboardScreen> createState() => _DashboardScreenState();
}

class _DashboardScreenState extends State<DashboardScreen> {
  final _auth = AuthService();
  final _fs = FirestoreService();
  int _currentIndex = 0;

  @override
  Widget build(BuildContext context) {
    final ble = context.watch<BleService>();
    final settings = context.watch<SettingsProvider>();

    return Scaffold(
      appBar: AppBar(
        title: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Container(
              width: 8,
              height: 8,
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                color: ble.isConnected
                    ? AppTheme.spo2
                    : (kIsWeb
                        ? AppTheme.textMuted
                        : AppTheme.spo2.withValues(alpha: 0.3)),
              ),
            ),
            const SizedBox(width: 8),
            const Text('SupaClock'),
          ],
        ),
        actions: [
          if (!kIsWeb)
            IconButton(
              icon: const Icon(Icons.bluetooth, color: AppTheme.secondary),
              onPressed: () => Navigator.of(context).push(
                MaterialPageRoute(builder: (_) => const BleDebugScreen()),
              ),
              tooltip: 'Conectar reloj',
            ),
          IconButton(
            icon: const Icon(Icons.logout, color: AppTheme.textMuted),
            onPressed: () async {
              final nav = Navigator.of(context);
              await _auth.signOut();
              if (mounted) {
                nav.pushReplacement(
                  MaterialPageRoute(builder: (_) => const LoginScreen()),
                );
              }
            },
          ),
        ],
      ),
      body: Column(
        children: [
          if (!settings.clinicalMode)
            Container(
              width: double.infinity,
              color: AppTheme.warning.withValues(alpha: 0.18),
              padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
              child: const Row(
                children: [
                  Icon(Icons.science_outlined, color: AppTheme.warning, size: 16),
                  SizedBox(width: 6),
                  Expanded(
                    child: Text(
                      'Modo prototipo · datos no clínicos · alertas silenciadas',
                      style: TextStyle(fontSize: 12, color: AppTheme.warning),
                    ),
                  ),
                ],
              ),
            ),
          _statusBanner(ble),
          Expanded(
            child: IndexedStack(
              index: _currentIndex,
              children: [
                _overviewTab(),
                const TrendsTab(),
                const EcgScreen(),
                const SettingsScreen(),
              ],
            ),
          ),
        ],
      ),
      floatingActionButton: _currentIndex == 0 && !kIsWeb
          ? FloatingActionButton.extended(
              icon: const Icon(Icons.favorite),
              label: const Text('Medir ahora'),
              backgroundColor: AppTheme.heartRate,
              onPressed: () => Navigator.of(context).push(
                MaterialPageRoute(builder: (_) => const SpotCheckScreen()),
              ),
            )
          : null,
      bottomNavigationBar: BottomNavigationBar(
        currentIndex: _currentIndex,
        onTap: (i) => setState(() => _currentIndex = i),
        items: const [
          BottomNavigationBarItem(
            icon: Icon(Icons.dashboard_outlined),
            activeIcon: Icon(Icons.dashboard),
            label: 'Resumen',
          ),
          BottomNavigationBarItem(
            icon: Icon(Icons.trending_up),
            label: 'Historial',
          ),
          BottomNavigationBarItem(
            icon: Icon(Icons.monitor_heart_outlined),
            activeIcon: Icon(Icons.monitor_heart),
            label: 'ECG',
          ),
          BottomNavigationBarItem(
            icon: Icon(Icons.settings_outlined),
            activeIcon: Icon(Icons.settings),
            label: 'Ajustes',
          ),
        ],
      ),
    );
  }

  /// Thin status strip: BLE link state on the left, cloud-sync state on the
  /// right. Rebuilds live as the sync queue grows/drains (Hive listenable).
  Widget _statusBanner(BleService ble) {
    final IconData linkIcon;
    final String linkText;
    final Color linkColor;
    if (kIsWeb) {
      linkIcon = Icons.devices;
      linkText = 'App web';
      linkColor = AppTheme.textMuted;
    } else if (ble.isConnected) {
      linkIcon = Icons.bluetooth_connected;
      linkText = 'Reloj conectado';
      linkColor = AppTheme.spo2;
    } else if (ble.isScanning) {
      linkIcon = Icons.bluetooth_searching;
      linkText = 'Buscando reloj…';
      linkColor = AppTheme.secondary;
    } else {
      linkIcon = Icons.bluetooth_disabled;
      linkText = 'Reloj desconectado';
      linkColor = AppTheme.textMuted;
    }

    return ValueListenableBuilder(
      valueListenable: LocalStore.syncListenable(),
      builder: (context, _, _) {
        final pending = LocalStore.pendingSyncCount();
        return Container(
          width: double.infinity,
          color: AppTheme.borderColor.withValues(alpha: 0.25),
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
          child: Row(
            children: [
              Icon(linkIcon, size: 14, color: linkColor),
              const SizedBox(width: 6),
              Text(linkText, style: TextStyle(fontSize: 12, color: linkColor)),
              const Spacer(),
              if (pending > 0) ...[
                const Icon(Icons.sync_problem, size: 14, color: AppTheme.warning),
                const SizedBox(width: 4),
                Text(
                  '$pending sin sincronizar',
                  style: const TextStyle(fontSize: 12, color: AppTheme.warning),
                ),
              ] else ...[
                Icon(Icons.cloud_done_outlined, size: 14, color: AppTheme.textMuted),
                const SizedBox(width: 4),
                const Text('Sincronizado', style: TextStyle(fontSize: 12, color: AppTheme.textMuted)),
              ],
            ],
          ),
        );
      },
    );
  }

  Widget _overviewTab() {
    final uid = _auth.currentUser?.uid;
    if (uid == null) return const SizedBox();
    final ble = context.watch<BleService>();
    final telem = ble.telemetry;

    return StreamBuilder<UserModel>(
      stream: _fs.streamUser(uid),
      builder: (context, snap) {
        final user = snap.data;
        final name = user?.nickname ??
            user?.name ??
            _auth.currentUser?.displayName ??
            'Usuario';

        return SingleChildScrollView(
          padding: const EdgeInsets.all(20),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('Hola, $name',
                  style: Theme.of(context).textTheme.displayMedium),
              const SizedBox(height: 4),
              Text(
                ble.isConnected
                    ? 'Reloj conectado · sincronizando'
                    : 'Conecta tu SupaClock para sincronizar',
                style: Theme.of(context).textTheme.bodyMedium,
              ),
              const SizedBox(height: 24),
              _metricGrid(telem),
              const SizedBox(height: 24),
              _activityCard(),
              const SizedBox(height: 80), // FAB clearance
            ],
          ),
        );
      },
    );
  }

  Widget _metricGrid(SupaClockTelemetry telem) {
    final hr = telem.heartRate;
    final hrQ = telem.hrQuality ?? 0;
    final spo2 = telem.spo2;
    final spo2Q = telem.spo2Quality ?? 0;

    return GridView.count(
      crossAxisCount: 2,
      shrinkWrap: true,
      physics: const NeverScrollableScrollPhysics(),
      crossAxisSpacing: 12,
      mainAxisSpacing: 12,
      childAspectRatio: 1.15,
      children: [
        _MetricCard(
          icon: Icons.favorite,
          color: AppTheme.heartRate,
          label: 'Frecuencia\nCardíaca',
          value: hr != null ? '$hr' : '--',
          unit: 'BPM',
          weak: hr != null && hrQ < 60,
        ),
        _MetricCard(
          icon: Icons.water_drop,
          color: AppTheme.spo2,
          label: 'SpO₂',
          value: spo2 != null ? '$spo2' : '--',
          unit: '%',
          weak: spo2 != null && spo2Q < 60,
        ),
        _MetricCard(
          icon: Icons.thermostat,
          color: AppTheme.temperature,
          label: 'Temperatura',
          value: telem.temperature?.toStringAsFixed(1) ?? '--',
          unit: '°C',
        ),
        _MetricCard(
          icon: Icons.directions_walk,
          color: AppTheme.steps,
          label: 'Pasos',
          value: telem.steps?.toString() ?? '--',
          unit: 'hoy',
        ),
      ],
    );
  }

  Widget _activityCard() {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Container(
                  width: 40,
                  height: 40,
                  decoration: BoxDecoration(
                    borderRadius: BorderRadius.circular(10),
                    color: AppTheme.primary.withValues(alpha: 0.15),
                  ),
                  child: const Icon(Icons.psychology,
                      color: AppTheme.primary, size: 22),
                ),
                const SizedBox(width: 12),
                Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text('Actividad detectada',
                        style: Theme.of(context).textTheme.titleMedium),
                    Text(
                      'Clasificación TinyML — próximamente',
                      style: Theme.of(context)
                          .textTheme
                          .bodyMedium
                          ?.copyWith(fontSize: 12),
                    ),
                  ],
                ),
              ],
            ),
            const SizedBox(height: 16),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceAround,
              children: const [
                _ActivityChip(
                    icon: Icons.airline_seat_recline_normal,
                    label: 'Reposo',
                    active: true),
                _ActivityChip(
                    icon: Icons.directions_walk, label: 'Caminar', active: false),
                _ActivityChip(
                    icon: Icons.directions_run, label: 'Correr', active: false),
                _ActivityChip(
                    icon: Icons.warning_amber,
                    label: 'Caída',
                    active: false,
                    danger: true),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

class _MetricCard extends StatelessWidget {
  final IconData icon;
  final Color color;
  final String label;
  final String value;
  final String unit;
  final bool weak;

  const _MetricCard({
    required this.icon,
    required this.color,
    required this.label,
    required this.value,
    required this.unit,
    this.weak = false,
  });

  @override
  Widget build(BuildContext context) {
    final disp = weak ? AppTheme.textMuted : color;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Row(
              children: [
                Container(
                  width: 36,
                  height: 36,
                  decoration: BoxDecoration(
                    borderRadius: BorderRadius.circular(8),
                    color: disp.withValues(alpha: 0.15),
                  ),
                  child: Icon(icon, color: disp, size: 20),
                ),
                if (weak) const Spacer(),
                if (weak)
                  Container(
                    padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
                    decoration: BoxDecoration(
                      borderRadius: BorderRadius.circular(8),
                      color: AppTheme.warning.withValues(alpha: 0.18),
                    ),
                    child: const Text(
                      'señal débil',
                      style: TextStyle(
                          fontSize: 9, color: AppTheme.warning, fontWeight: FontWeight.w600),
                    ),
                  ),
              ],
            ),
            FittedBox(
              fit: BoxFit.scaleDown,
              alignment: Alignment.centerLeft,
              child: RichText(
                text: TextSpan(children: [
                  TextSpan(
                    text: value,
                    style: TextStyle(
                      fontSize: 24,
                      fontWeight: FontWeight.w700,
                      color: disp,
                    ),
                  ),
                  TextSpan(
                    text: ' $unit',
                    style: const TextStyle(fontSize: 12, color: AppTheme.textMuted),
                  ),
                ]),
              ),
            ),
            Text(
              label,
              style: const TextStyle(fontSize: 11, color: AppTheme.textSecondary),
              maxLines: 2,
            ),
          ],
        ),
      ),
    );
  }
}

class _ActivityChip extends StatelessWidget {
  final IconData icon;
  final String label;
  final bool active;
  final bool danger;
  const _ActivityChip({
    required this.icon,
    required this.label,
    required this.active,
    this.danger = false,
  });

  @override
  Widget build(BuildContext context) {
    final color =
        danger ? AppTheme.danger : (active ? AppTheme.primary : AppTheme.textMuted);
    return Column(
      children: [
        Container(
          width: 44,
          height: 44,
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(12),
            color: active ? color.withValues(alpha: 0.15) : Colors.transparent,
            border: Border.all(
              color: active ? color : AppTheme.borderColor,
              width: active ? 2 : 1,
            ),
          ),
          child: Icon(icon, color: color, size: 22),
        ),
        const SizedBox(height: 4),
        Text(
          label,
          style: TextStyle(
            fontSize: 10,
            color: active ? color : AppTheme.textMuted,
            fontWeight: active ? FontWeight.w600 : FontWeight.w400,
          ),
        ),
      ],
    );
  }
}
