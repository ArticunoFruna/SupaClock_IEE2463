import 'dart:async';

import 'package:firebase_core/firebase_core.dart';
import 'package:flutter/material.dart';
import 'package:intl/intl.dart';
import 'package:provider/provider.dart';

import 'config/theme.dart';
import 'firebase_options.dart';
import 'models/user_model.dart';
import 'providers/settings_provider.dart';
import 'screens/dashboard_screen.dart';
import 'screens/login_screen.dart';
import 'services/auth_service.dart';
import 'services/ble_service.dart';
import 'services/daily_rollup_service.dart';
import 'services/firestore_service.dart';
import 'services/local_store.dart';
import 'services/notifications_service.dart';
import 'services/telemetry_collector.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  await Firebase.initializeApp(
    options: DefaultFirebaseOptions.currentPlatform,
  );
  await LocalStore.init();

  runApp(
    MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => SettingsProvider()),
        ChangeNotifierProvider(create: (_) => BleService()),
      ],
      child: const SupaClockApp(),
    ),
  );
}

class SupaClockApp extends StatefulWidget {
  const SupaClockApp({super.key});

  @override
  State<SupaClockApp> createState() => _SupaClockAppState();
}

class _SupaClockAppState extends State<SupaClockApp> with WidgetsBindingObserver {
  final _auth = AuthService();
  final _fs = FirestoreService();
  final _notif = NotificationsService();
  late final DailyRollupService _rollup;

  TelemetryCollector? _collector;
  StreamSubscription<UserModel>? _userSub;

  bool _wired = false;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    _rollup = DailyRollupService(_fs);
    _notif.init();
    // Wire up telemetry once first frame is rendered (need providers in tree).
    WidgetsBinding.instance.addPostFrameCallback((_) => _wireUp());
  }

  void _wireUp() {
    if (_wired) return;
    _wired = true;

    final ble = context.read<BleService>();
    _collector = TelemetryCollector(ble, _fs, _auth, _notif)..start();

    final uid = _auth.currentUser?.uid;
    if (uid != null) {
      _userSub = _fs.streamUser(uid).listen((u) {
        // clinicalMode is propagated separately from build() via updateContext.
        _collector?.updateContext(user: u);
      });
      _runRollup(uid);
    }
  }

  Future<void> _runRollup(String uid) async {
    try {
      await _rollup.runRollupIfNeeded(uid);
      final today = DateFormat('yyyy-MM-dd').format(DateTime.now());
      if (!mounted) return;
      // ignore: use_build_context_synchronously
      await context.read<SettingsProvider>().setLastRollupDate(today);
    } catch (e) {
      debugPrint('Rollup failed: $e');
    }
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state == AppLifecycleState.resumed) {
      final uid = _auth.currentUser?.uid;
      if (uid != null) _runRollup(uid);
    }
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    _userSub?.cancel();
    _collector?.stop();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final settings = context.watch<SettingsProvider>();

    // Keep collector's view of clinicalMode + user fresh.
    _collector?.updateContext(clinicalMode: settings.clinicalMode);

    return MaterialApp(
      title: 'SupaClock',
      debugShowCheckedModeBanner: false,
      theme: AppTheme.lightTheme,
      darkTheme: AppTheme.darkTheme,
      themeMode: settings.themeMode,
      home: _auth.isLoggedIn ? const DashboardScreen() : const LoginScreen(),
    );
  }
}
