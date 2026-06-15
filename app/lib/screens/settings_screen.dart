import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../config/theme.dart';
import '../models/user_model.dart';
import '../providers/settings_provider.dart';
import '../services/auth_service.dart';
import '../services/firestore_service.dart';
import 'dev_mode_screen.dart';

class SettingsScreen extends StatefulWidget {
  const SettingsScreen({super.key});

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  final _auth = AuthService();
  final _fs = FirestoreService();

  UserModel? _user;
  bool _loading = true;

  final _nicknameCtrl = TextEditingController();
  final _ageCtrl = TextEditingController();
  final _weightCtrl = TextEditingController();
  final _heightCtrl = TextEditingController();
  String _sex = 'x';

  // Wearable
  String _clockFormat = '24h';
  int _syncIntervalMin = 60;
  String _powerMode = 'full';

  // Thresholds (mirror UserModel.thresholds)
  late int _hrHigh, _hrLow, _spo2Low, _batteryLow, _stabilitySec;
  late double _tempHigh;

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    final uid = _auth.currentUser?.uid;
    if (uid == null) return;
    final u = await _fs.getOrCreateUser(uid);
    if (!mounted) return;
    setState(() {
      _user = u;
      _nicknameCtrl.text = u.nickname ?? '';
      _ageCtrl.text = u.age?.toString() ?? '';
      _weightCtrl.text = u.weight?.toString() ?? '';
      _heightCtrl.text = u.height?.toString() ?? '';
      _sex = u.sex ?? 'x';
      _clockFormat = u.wearableSettings?['clockFormat'] ?? '24h';
      _syncIntervalMin = u.wearableSettings?['syncIntervalMinutes'] ?? 60;
      _powerMode = u.wearableSettings?['powerMode'] ?? 'full';
      _hrHigh = u.thresholds.hrHigh;
      _hrLow = u.thresholds.hrLow;
      _spo2Low = u.thresholds.spo2Low;
      _tempHigh = u.thresholds.tempHigh;
      _batteryLow = u.thresholds.batteryLow;
      _stabilitySec = u.thresholds.stabilitySeconds;
      _loading = false;
    });
  }

  Future<void> _save() async {
    final u = _user;
    if (u == null) return;
    final updated = u.copyWith(
      nickname:
          _nicknameCtrl.text.trim().isEmpty ? null : _nicknameCtrl.text.trim(),
      age: int.tryParse(_ageCtrl.text),
      weight: double.tryParse(_weightCtrl.text),
      height: double.tryParse(_heightCtrl.text),
      sex: _sex,
      wearableSettings: {
        'clockFormat': _clockFormat,
        'syncIntervalMinutes': _syncIntervalMin,
        'powerMode': _powerMode,
      },
      thresholds: u.thresholds.copyWith(
        hrHigh: _hrHigh,
        hrLow: _hrLow,
        spo2Low: _spo2Low,
        tempHigh: _tempHigh,
        batteryLow: _batteryLow,
        stabilitySeconds: _stabilitySec,
      ),
    );
    await _fs.updateUser(updated);
    if (!mounted) return;
    setState(() => _user = updated);
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text('Ajustes guardados')),
    );
  }

  @override
  void dispose() {
    _nicknameCtrl.dispose();
    _ageCtrl.dispose();
    _weightCtrl.dispose();
    _heightCtrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    if (_loading) {
      return const Scaffold(
        body: Center(child: CircularProgressIndicator(color: AppTheme.primary)),
      );
    }
    final settings = context.watch<SettingsProvider>();

    return Scaffold(
      appBar: AppBar(title: const Text('Configuración')),
      body: ListView(
        padding: const EdgeInsets.all(20),
        children: [
          _section('Perfil'),
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                children: [
                  TextField(
                    controller: _nicknameCtrl,
                    decoration: const InputDecoration(
                      labelText: 'Apodo',
                      prefixIcon: Icon(Icons.person),
                    ),
                  ),
                  const SizedBox(height: 12),
                  Row(
                    children: [
                      Expanded(
                        child: TextField(
                          controller: _ageCtrl,
                          keyboardType: TextInputType.number,
                          decoration: const InputDecoration(
                            labelText: 'Edad',
                            prefixIcon: Icon(Icons.cake),
                          ),
                        ),
                      ),
                      const SizedBox(width: 12),
                      Expanded(
                        child: DropdownButtonFormField<String>(
                          isExpanded: true,
                          initialValue: _sex,
                          decoration: const InputDecoration(
                            labelText: 'Sexo',
                            prefixIcon: Icon(Icons.wc),
                          ),
                          items: const [
                            DropdownMenuItem(value: 'm', child: Text('Masculino')),
                            DropdownMenuItem(value: 'f', child: Text('Femenino')),
                            DropdownMenuItem(value: 'x', child: Text('Sin especificar')),
                          ],
                          onChanged: (v) => setState(() => _sex = v ?? 'x'),
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 12),
                  Row(
                    children: [
                      Expanded(
                        child: TextField(
                          controller: _weightCtrl,
                          keyboardType: TextInputType.number,
                          decoration: const InputDecoration(
                            labelText: 'Peso (kg)',
                            prefixIcon: Icon(Icons.monitor_weight),
                          ),
                        ),
                      ),
                      const SizedBox(width: 12),
                      Expanded(
                        child: TextField(
                          controller: _heightCtrl,
                          keyboardType: TextInputType.number,
                          decoration: const InputDecoration(
                            labelText: 'Altura (cm)',
                            prefixIcon: Icon(Icons.height),
                          ),
                        ),
                      ),
                    ],
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 24),
          _section('Aplicación'),
          Card(
            child: Column(
              children: [
                ListTile(
                  leading: const Icon(Icons.medical_services_outlined),
                  title: const Text('Modo clínico'),
                  subtitle: Text(settings.clinicalMode
                      ? 'Alertas activas — hardware validado'
                      : 'Alertas silenciadas — hardware en prototipo'),
                  trailing: Switch(
                    value: settings.clinicalMode,
                    onChanged: settings.setClinicalMode,
                  ),
                ),
                const Divider(height: 1),
                Padding(
                  padding: const EdgeInsets.fromLTRB(16, 8, 16, 16),
                  child: DropdownButtonFormField<ThemeMode>(
                    initialValue: settings.themeMode,
                    decoration: const InputDecoration(
                      labelText: 'Tema',
                      prefixIcon: Icon(Icons.brightness_6),
                    ),
                    items: const [
                      DropdownMenuItem(
                          value: ThemeMode.system, child: Text('Sistema')),
                      DropdownMenuItem(
                          value: ThemeMode.light, child: Text('Claro')),
                      DropdownMenuItem(
                          value: ThemeMode.dark, child: Text('Oscuro')),
                    ],
                    onChanged: (m) => m == null ? null : settings.setThemeMode(m),
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(height: 24),
          _section('Reloj'),
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                children: [
                  DropdownButtonFormField<String>(
                    initialValue: _clockFormat,
                    decoration: const InputDecoration(
                      labelText: 'Formato de hora',
                      prefixIcon: Icon(Icons.access_time),
                    ),
                    items: const [
                      DropdownMenuItem(value: '12h', child: Text('12 horas')),
                      DropdownMenuItem(value: '24h', child: Text('24 horas')),
                    ],
                    onChanged: (v) => setState(() => _clockFormat = v ?? '24h'),
                  ),
                  const SizedBox(height: 12),
                  DropdownButtonFormField<int>(
                    initialValue: _syncIntervalMin,
                    decoration: const InputDecoration(
                      labelText: 'Intervalo BLE',
                      prefixIcon: Icon(Icons.sync),
                    ),
                    items: const [
                      DropdownMenuItem(value: 15, child: Text('15 min')),
                      DropdownMenuItem(value: 30, child: Text('30 min')),
                      DropdownMenuItem(value: 60, child: Text('1 hora')),
                      DropdownMenuItem(value: 120, child: Text('2 horas')),
                    ],
                    onChanged: (v) => setState(() => _syncIntervalMin = v ?? 60),
                  ),
                  const SizedBox(height: 12),
                  DropdownButtonFormField<String>(
                    initialValue: _powerMode,
                    decoration: const InputDecoration(
                      labelText: 'Modo de energía',
                      prefixIcon: Icon(Icons.battery_charging_full),
                    ),
                    items: const [
                      DropdownMenuItem(value: 'full', child: Text('Rendimiento')),
                      DropdownMenuItem(value: 'bmi_only', child: Text('Estándar')),
                      DropdownMenuItem(value: 'eco', child: Text('Ahorro')),
                    ],
                    onChanged: (v) => setState(() => _powerMode = v ?? 'full'),
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 24),
          _section('Alertas'),
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                children: [
                  _slider('FC alta (bpm)', _hrHigh.toDouble(), 100, 200,
                      (v) => setState(() => _hrHigh = v.round())),
                  _slider('FC baja (bpm)', _hrLow.toDouble(), 30, 80,
                      (v) => setState(() => _hrLow = v.round())),
                  _slider('SpO₂ bajo (%)', _spo2Low.toDouble(), 80, 95,
                      (v) => setState(() => _spo2Low = v.round())),
                  _slider('Temp alta (°C)', _tempHigh, 37.0, 40.0,
                      (v) => setState(() => _tempHigh = v),
                      step: 0.1, valueLabel: _tempHigh.toStringAsFixed(1)),
                  _slider('Batería baja (%)', _batteryLow.toDouble(), 5, 30,
                      (v) => setState(() => _batteryLow = v.round())),
                  _slider('Estabilidad (s)', _stabilitySec.toDouble(), 15, 180,
                      (v) => setState(() => _stabilitySec = v.round())),
                ],
              ),
            ),
          ),
          const SizedBox(height: 24),
          if (settings.devModeUnlocked) ...[
            _section('Developer'),
            Card(
              child: Column(
                children: [
                  SwitchListTile(
                    secondary: const Icon(Icons.bug_report),
                    title: const Text('Mostrar Developer Mode'),
                    subtitle: const Text('Acceso a IMU/ECG live, REC, consola'),
                    value: settings.devModeEnabled,
                    onChanged: settings.setDevModeEnabled,
                  ),
                  if (settings.devModeEnabled)
                    ListTile(
                      leading: const Icon(Icons.science),
                      title: const Text('Abrir Developer Mode'),
                      onTap: () => Navigator.of(context).push(
                        MaterialPageRoute(builder: (_) => const DevModeScreen()),
                      ),
                    ),
                  ListTile(
                    leading: const Icon(Icons.lock_outline, color: AppTheme.danger),
                    title: const Text('Bloquear Developer Mode'),
                    onTap: settings.lockDevMode,
                  ),
                ],
              ),
            ),
            const SizedBox(height: 24),
          ],
          ElevatedButton.icon(
            onPressed: _save,
            icon: const Icon(Icons.save),
            label: const Text('Guardar configuración'),
            style: ElevatedButton.styleFrom(
                minimumSize: const Size(double.infinity, 50)),
          ),
          const SizedBox(height: 24),
          _aboutTile(settings),
          const SizedBox(height: 24),
        ],
      ),
    );
  }

  Widget _section(String title) => Padding(
        padding: const EdgeInsets.only(bottom: 12, left: 4),
        child: Text(
          title,
          style: Theme.of(context).textTheme.titleLarge?.copyWith(
                color: Theme.of(context).colorScheme.primary,
              ),
        ),
      );

  Widget _slider(
    String label,
    double value,
    double min,
    double max,
    ValueChanged<double> onChanged, {
    double step = 1,
    String? valueLabel,
  }) {
    final divisions = ((max - min) / step).round();
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 6),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              Text(label),
              Text(valueLabel ?? value.toStringAsFixed(0),
                  style: const TextStyle(fontWeight: FontWeight.w600)),
            ],
          ),
          Slider(
            value: value.clamp(min, max),
            min: min,
            max: max,
            divisions: divisions,
            onChanged: onChanged,
          ),
        ],
      ),
    );
  }

  Widget _aboutTile(SettingsProvider settings) {
    final remaining = settings.devUnlockTapsRemaining;
    return Center(
      child: GestureDetector(
        onTap: () async {
          final unlocked = await settings.registerVersionTap();
          if (unlocked && mounted) {
            ScaffoldMessenger.of(context).showSnackBar(
              const SnackBar(content: Text('🛠 Developer mode desbloqueado')),
            );
          }
        },
        child: Padding(
          padding: const EdgeInsets.all(12),
          child: Column(
            children: [
              const Text('SupaClock', style: TextStyle(fontWeight: FontWeight.w600)),
              const SizedBox(height: 4),
              Text(
                'v1.0.0+1',
                style: const TextStyle(color: AppTheme.textMuted),
              ),
              if (!settings.devModeUnlocked && remaining < 7)
                Padding(
                  padding: const EdgeInsets.only(top: 4),
                  child: Text(
                    '$remaining toques restantes',
                    style: const TextStyle(fontSize: 11, color: AppTheme.textMuted),
                  ),
                ),
            ],
          ),
        ),
      ),
    );
  }
}
