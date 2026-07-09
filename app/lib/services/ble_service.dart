import 'dart:async';
import 'dart:io' show Platform;

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:shared_preferences/shared_preferences.dart';

/// SupaClock BLE protocol — matches firmware in `lib/ble_telemetry/`.
///
/// Service: 0xFF00
/// Characteristics:
///   0xFF01  IMU 6-DOF        — 12 B notify (int16 ax,ay,az,gx,gy,gz)
///   0xFF02  Aggregate TLV    — header + TLV records (HR/SpO2/Temp/Bat/Steps...)
///   0xFF03  ECG samples      — 20 B notify (10 × int16, 500 Hz stream)
///   0xFF04  Commands (write) — 1 B  (0x00 stop ECG, 0x01 start ECG)
class SupaClockUuids {
  // Match BLE_UUID16_DECLARE — short IDs, expanded with the SIG base.
  static String svc(int id) => '0000${id.toRadixString(16).padLeft(4, '0')}-0000-1000-8000-00805f9b34fb';

  static final telemetryService = Guid(svc(0xFF00));
  static final imuChar          = Guid(svc(0xFF01));
  static final aggChar          = Guid(svc(0xFF02));
  static final ecgChar          = Guid(svc(0xFF03));
  static final cmdChar          = Guid(svc(0xFF04));
}

class TlvTypes {
  static const int hr = 0x01;
  static const int spo2 = 0x02;
  static const int temp = 0x03;
  static const int bat = 0x04;
  static const int steps = 0x05;
  static const int modeEvt = 0x06;
  static const int spotResult = 0x07;
  static const int harState = 0x08; // salida del modelo HAR (1 byte: estado consolidado)
  static const int pedDbg = 0x10; // debug pedómetro (quitar tras calibrar)
}

/// Opcodes del canal 0xFF04. Deben coincidir con `lib/ble_telemetry/ble_cmd.h`.
class SupaClockCmd {
  static const int stopEcg   = 0x00;
  static const int startEcg  = 0x01;
  static const int syncTime  = 0x02; // payload: u32 unix_ts LE
  static const int unpairAll = 0x03;
  static const int reqBat    = 0x04;
}

/// Estados del modelo HAR (TLV 0x08). Tolerante a valores fuera de rango:
/// el firmware advierte que podría añadirse una 5ª clase (har_state = 4) sin
/// romper el protocolo, así que cualquier valor desconocido cae en [unknown].
enum HarState {
  resting(0, 'Reposo'),
  walking(1, 'Caminar'),
  running(2, 'Correr'),
  stairs(3, 'Escaleras'),
  unknown(-1, 'Desconocido');

  const HarState(this.code, this.label);
  final int code;
  final String label;

  static HarState fromCode(int? code) {
    if (code == null) return HarState.unknown;
    for (final s in HarState.values) {
      if (s.code == code) return s;
    }
    return HarState.unknown;
  }
}

class SupaClockTelemetry {
  final int? heartRate;
  final int? hrQuality;
  final int? spo2;
  final int? spo2Quality;
  final double? temperature;
  final int? steps;
  final int? batteryMv;
  final int? batterySoc;
  final int? powerMode;
  final int? harState; // 0–3; null antes del primer TLV 0x08 (~20 s tras boot)
  final DateTime timestamp;

  SupaClockTelemetry({
    this.heartRate,
    this.hrQuality,
    this.spo2,
    this.spo2Quality,
    this.temperature,
    this.steps,
    this.batteryMv,
    this.batterySoc,
    this.powerMode,
    this.harState,
    required this.timestamp,
  });

  /// Backwards-compat fields (non-null) for legacy widgets.
  int get heartRateOr0 => heartRate ?? 0;
  int get spo2Or0 => spo2 ?? 0;
  double get temperatureOr0 => temperature ?? 0.0;
  int get stepsOr0 => steps ?? 0;

  SupaClockTelemetry merge(Map<String, dynamic> updates) {
    return SupaClockTelemetry(
      heartRate: updates['hr_bpm'] ?? heartRate,
      hrQuality: updates['hr_quality'] ?? hrQuality,
      spo2: updates['spo2_pct'] ?? spo2,
      spo2Quality: updates['spo2_quality'] ?? spo2Quality,
      temperature: updates['temp_c'] ?? temperature,
      steps: updates['steps'] ?? steps,
      batteryMv: updates['bat_mv'] ?? batteryMv,
      batterySoc: updates['bat_soc'] ?? batterySoc,
      powerMode: updates['power_mode'] ?? powerMode,
      harState: updates['har_state'] ?? harState,
      timestamp: DateTime.now(),
    );
  }

  /// Estado HAR como enum (mapea null → [HarState.unknown]).
  HarState get activity => HarState.fromCode(harState);
}

class ImuSample {
  final int ax, ay, az, gx, gy, gz;
  final DateTime t;
  ImuSample(this.ax, this.ay, this.az, this.gx, this.gy, this.gz, this.t);
}

class BleService extends ChangeNotifier {
  static const String _kPrefRemoteId = 'ble_remote_id';

  BluetoothDevice? _device;
  BluetoothConnectionState _state = BluetoothConnectionState.disconnected;
  StreamSubscription? _connectionSub;
  StreamSubscription? _scanSub;

  String? _remoteId; // MAC/deviceId persistido tras el primer pairing.
  Timer? _reconnectTimer;
  int _reconnectAttempts = 0;
  static const List<int> _backoffSecs = [1, 2, 4, 8, 16];

  final List<String> _log = [];
  SupaClockTelemetry _telemetry = SupaClockTelemetry(timestamp: DateTime.now());

  BluetoothCharacteristic? _imuChar;
  BluetoothCharacteristic? _aggChar;
  BluetoothCharacteristic? _ecgChar;
  BluetoothCharacteristic? _cmdChar;

  final List<StreamSubscription> _notifSubs = [];

  // Streams for live consumers (dev mode, ECG screen, etc.).
  final _imuController = StreamController<ImuSample>.broadcast();
  Stream<ImuSample> get imuStream => _imuController.stream;

  final _ecgController = StreamController<List<int>>.broadcast();
  Stream<List<int>> get ecgStream => _ecgController.stream;

  final _telemetryController = StreamController<SupaClockTelemetry>.broadcast();
  Stream<SupaClockTelemetry> get telemetryStream => _telemetryController.stream;

  // Spot-check completion stream.
  final _spotController = StreamController<SpotResult>.broadcast();
  Stream<SpotResult> get spotStream => _spotController.stream;

  // ── Getters ──
  BluetoothConnectionState get connectionState => _state;
  bool get isConnected => _state == BluetoothConnectionState.connected;
  bool get isScanning => FlutterBluePlus.isScanningNow;
  SupaClockTelemetry get telemetry => _telemetry;
  SupaClockTelemetry? get lastTelemetry => _telemetry;
  List<String> get log => List.unmodifiable(_log);
  BluetoothDevice? get device => _device;
  String? get remoteId => _remoteId;
  bool get hasBond => _remoteId != null;

  void _addLog(String msg) {
    final ts = DateTime.now().toIso8601String().substring(11, 19);
    _log.insert(0, '[$ts] $msg');
    if (_log.length > 200) _log.removeLast();
    notifyListeners();
  }

  // ── Scan / connect ────────────────────────────────────────────────
  /// Carga el remoteId persistido (si hay) e intenta reconectar en caliente.
  /// Se llama desde main.dart al startup. Devuelve true si conectó.
  Future<bool> tryAutoReconnect() async {
    final prefs = await SharedPreferences.getInstance();
    _remoteId = prefs.getString(_kPrefRemoteId);
    if (_remoteId == null) {
      _addLog('Sin bond previo: se requiere pairing manual');
      notifyListeners();
      return false;
    }
    _addLog('Bond guardado: $_remoteId, intentando auto-reconnect...');
    try {
      final device = BluetoothDevice.fromId(_remoteId!);
      await device.connect(
        license: License.free,
        autoConnect: true,
        timeout: const Duration(seconds: 8),
      );
      _device = device;
      _wireConnectionListener(device);
      await Future.delayed(const Duration(milliseconds: 500));
      await _afterConnect(device);
      return true;
    } catch (e) {
      _addLog('Auto-reconnect falló: $e');
      return false;
    }
  }

  Future<void> startScan() async {
    _addLog('Escaneando dispositivos BLE...');
    await FlutterBluePlus.stopScan();

    _scanSub?.cancel();
    _scanSub = FlutterBluePlus.scanResults.listen((results) {
      for (final r in results) {
        if (r.device.platformName.contains('SupaClock')) {
          _addLog('SupaClock encontrado, RSSI ${r.rssi} dBm');
          FlutterBluePlus.stopScan();
          connectToDevice(r.device);
          break;
        }
      }
    });

    await FlutterBluePlus.startScan(
      // Filtra por servicio primario del SupaClock para descartar ruido.
      withServices: [SupaClockUuids.telemetryService],
      timeout: const Duration(seconds: 10),
      androidUsesFineLocation: false,
    );
    notifyListeners();
  }

  Future<void> connectToDevice(BluetoothDevice device) async {
    _device = device;
    _addLog('Conectando a ${device.platformName}...');

    try {
      await device.connect(
        license: License.free,
        autoConnect: false,
        timeout: const Duration(seconds: 10),
      );
    } catch (e) {
      _addLog('Error al conectar: $e');
      return;
    }

    _wireConnectionListener(device);

    await Future.delayed(const Duration(milliseconds: 500));
    await _afterConnect(device);
  }

  void _wireConnectionListener(BluetoothDevice device) {
    _connectionSub?.cancel();
    _connectionSub = device.connectionState.listen((s) {
      _state = s;
      _addLog('Estado: ${s.name}');
      if (s == BluetoothConnectionState.connected) {
        _reconnectAttempts = 0;
        _reconnectTimer?.cancel();
      } else if (s == BluetoothConnectionState.disconnected) {
        for (final sub in _notifSubs) {
          sub.cancel();
        }
        _notifSubs.clear();
        // Sólo reintenta si hay bond guardado (Android reconectará vía autoConnect).
        if (_remoteId != null) _scheduleReconnect();
      }
      notifyListeners();
    });
  }

  /// Post-connect: crea bond en Android, negocia MTU alto y persiste remoteId.
  Future<void> _afterConnect(BluetoothDevice device) async {
    // 1. Bond persistente: dispara el prompt Just Works en Android.
    if (Platform.isAndroid) {
      try {
        await device.createBond(timeout: 15);
        _addLog('Bond creado / ya existía con ${device.remoteId}');
      } catch (e) {
        _addLog('createBond: $e (puede que ya estuviera bonded)');
      }
    }

    // 2. MTU alto para TLVs largos y ECG.
    try {
      final mtu = await device.requestMtu(247);
      _addLog('MTU negociado = $mtu');
    } catch (e) {
      _addLog('requestMtu falló: $e');
    }

    // 3. Persistir el deviceId para futuros arranques.
    _remoteId = device.remoteId.str;
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_kPrefRemoteId, _remoteId!);

    // 4. Discover + subscribe.
    await _discoverAndSubscribe(device);
  }

  void _scheduleReconnect() {
    _reconnectTimer?.cancel();
    if (_reconnectAttempts >= _backoffSecs.length) {
      _addLog('Max reintentos alcanzado ($_reconnectAttempts). Reconectar manualmente.');
      return;
    }
    final secs = _backoffSecs[_reconnectAttempts];
    _reconnectAttempts++;
    _addLog('Reintento $_reconnectAttempts en ${secs}s...');
    _reconnectTimer = Timer(Duration(seconds: secs), () async {
      if (_state == BluetoothConnectionState.connected) return;
      if (_remoteId == null) return;
      try {
        final device = BluetoothDevice.fromId(_remoteId!);
        await device.connect(
          license: License.free,
          autoConnect: true,
          timeout: const Duration(seconds: 8),
        );
        _device = device;
        _wireConnectionListener(device);
        await Future.delayed(const Duration(milliseconds: 500));
        await _afterConnect(device);
      } catch (e) {
        _addLog('Reintento fallido: $e');
        _scheduleReconnect();
      }
    });
  }

  /// Manda UNPAIR_ALL al reloj, quita bond en Android y borra el remoteId.
  Future<void> unpair() async {
    _addLog('Desemparejando...');
    try {
      await sendCommand(SupaClockCmd.unpairAll);
    } catch (_) {}
    try {
      if (Platform.isAndroid) {
        await _device?.removeBond();
      }
    } catch (e) {
      _addLog('removeBond: $e');
    }
    try {
      await _device?.disconnect();
    } catch (_) {}
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove(_kPrefRemoteId);
    _remoteId = null;
    _device = null;
    _state = BluetoothConnectionState.disconnected;
    _reconnectTimer?.cancel();
    _reconnectAttempts = 0;
    notifyListeners();
  }

  /// Manda el timestamp actual (unix ts en segundos, u32 LE) al reloj.
  Future<void> syncTime() async {
    final ts = DateTime.now().millisecondsSinceEpoch ~/ 1000;
    final bd = ByteData(4)..setUint32(0, ts, Endian.little);
    final payload = <int>[
      SupaClockCmd.syncTime,
      4,
      bd.getUint8(0),
      bd.getUint8(1),
      bd.getUint8(2),
      bd.getUint8(3),
    ];
    final chr = _cmdChar;
    if (chr == null) {
      _addLog('CMD sync_time: característica no disponible');
      return;
    }
    try {
      await chr.write(payload, withoutResponse: true);
      _addLog('Sync time enviado (ts=$ts)');
    } catch (e) {
      _addLog('Error enviando sync_time: $e');
    }
  }

  Future<void> _discoverAndSubscribe(BluetoothDevice device) async {
    _addLog('Descubriendo servicios GATT...');
    final services = await device.discoverServices();

    for (final svc in services) {
      for (final chr in svc.characteristics) {
        final shortId = _shortUuid(chr.uuid);
        if (shortId == 0xFF01) _imuChar = chr;
        if (shortId == 0xFF02) _aggChar = chr;
        if (shortId == 0xFF03) _ecgChar = chr;
        if (shortId == 0xFF04) _cmdChar = chr;
      }
    }

    if (_imuChar != null) await _subscribe(_imuChar!, _onImuData);
    if (_aggChar != null) await _subscribe(_aggChar!, _onAggData);
    if (_ecgChar != null) await _subscribe(_ecgChar!, _onEcgData);

    _addLog('Subscripción completa (IMU=${_imuChar != null} AGG=${_aggChar != null} ECG=${_ecgChar != null})');
  }

  Future<void> _subscribe(
    BluetoothCharacteristic chr,
    void Function(Uint8List) handler,
  ) async {
    if (!chr.properties.notify) return;
    try {
      await chr.setNotifyValue(true);
      _notifSubs.add(chr.onValueReceived.listen(
        (v) => handler(Uint8List.fromList(v)),
      ));
    } catch (e) {
      _addLog('Error suscribiendo a ${chr.uuid}: $e');
    }
  }

  /// Returns the 16-bit short UUID for a Guid that uses the SIG base, or -1.
  int _shortUuid(Guid g) {
    final s = g.toString().toLowerCase().replaceAll('-', '');
    // Try 4-char form ("ff01") first
    if (s.length == 4) return int.tryParse(s, radix: 16) ?? -1;
    // 32-char form: take chars 4..8 ("0000XXXX-...-...")
    if (s.length == 32) {
      return int.tryParse(s.substring(4, 8), radix: 16) ?? -1;
    }
    return -1;
  }

  // ── Notification handlers ─────────────────────────────────────────
  void _onImuData(Uint8List data) {
    if (data.length != 12) return;
    final bd = ByteData.sublistView(data);
    final s = ImuSample(
      bd.getInt16(0, Endian.little),
      bd.getInt16(2, Endian.little),
      bd.getInt16(4, Endian.little),
      bd.getInt16(6, Endian.little),
      bd.getInt16(8, Endian.little),
      bd.getInt16(10, Endian.little),
      DateTime.now(),
    );
    _imuController.add(s);
  }

  void _onEcgData(Uint8List data) {
    if (data.length != 20) return;
    final bd = ByteData.sublistView(data);
    final samples = List<int>.generate(10, (i) => bd.getInt16(i * 2, Endian.little));
    _ecgController.add(samples);
  }

  void _onAggData(Uint8List data) {
    if (data.length < 6) return;
    final bd = ByteData.sublistView(data);
    final powerMode = bd.getUint8(4);
    final payloadLen = bd.getUint8(5);
    var off = 6;
    final endOff = (6 + payloadLen).clamp(0, data.length);

    final updates = <String, dynamic>{'power_mode': powerMode};

    while (off + 2 <= endOff) {
      final type = data[off];
      final len = data[off + 1];
      off += 2;
      if (off + len > endOff) break;
      final payload = ByteData.sublistView(data, off, off + len);
      off += len;

      switch (type) {
        case TlvTypes.hr:
          if (len == 4) {
            updates['hr_bpm'] = payload.getUint8(2);
            updates['hr_quality'] = payload.getUint8(3);
          }
          break;
        case TlvTypes.spo2:
          if (len == 4) {
            updates['spo2_pct'] = payload.getUint8(2);
            updates['spo2_quality'] = payload.getUint8(3);
          }
          break;
        case TlvTypes.temp:
          if (len == 4) {
            updates['temp_c'] = payload.getInt16(2, Endian.little) / 100.0;
          }
          break;
        case TlvTypes.bat:
          if (len == 5) {
            updates['bat_mv'] = payload.getUint16(2, Endian.little);
            updates['bat_soc'] = payload.getUint8(4);
          }
          break;
        case TlvTypes.steps:
          if (len == 4) {
            updates['steps'] = payload.getUint32(0, Endian.little);
          }
          break;
        case TlvTypes.harState:
          if (len == 1) {
            updates['har_state'] = payload.getUint8(0);
          }
          break;
        case TlvTypes.pedDbg:
          if (len == 13) {
            final amp = payload.getUint16(0, Endian.little);
            final prom = payload.getUint16(2, Endian.little) / 10.0;
            final ratio = payload.getUint8(4) / 100.0;
            final pkHz = payload.getUint8(5) / 10.0;
            final cad = payload.getUint8(6) / 10.0;
            final gate = payload.getUint8(7);
            final cons = payload.getUint8(8);
            final steps = payload.getUint32(9, Endian.little);
            _addLog('PED amp=$amp prom=${prom.toStringAsFixed(1)} '
                'ratio=${ratio.toStringAsFixed(2)} pkHz=${pkHz.toStringAsFixed(1)} '
                'cad=${cad.toStringAsFixed(1)} gate=$gate cons=$cons pasos=$steps');
          }
          break;
        case TlvTypes.spotResult:
          if (len == 6) {
            final bpm = payload.getUint8(0);
            final spo2 = payload.getUint8(1);
            final durMs = payload.getUint16(2, Endian.little);
            final quality = payload.getUint8(4);
            final aborted = payload.getUint8(5) != 0;
            _spotController.add(SpotResult(
              bpm: bpm,
              spo2: spo2,
              durationMs: durMs,
              quality: quality,
              aborted: aborted,
              completedAt: DateTime.now(),
            ));
            if (!aborted) {
              updates['hr_bpm'] = bpm;
              updates['spo2_pct'] = spo2;
            }
          }
          break;
      }
    }

    _telemetry = _telemetry.merge(updates);
    _telemetryController.add(_telemetry);
    notifyListeners();
  }

  // ── Commands ──────────────────────────────────────────────────────
  Future<void> sendCommand(int cmdByte) async {
    final chr = _cmdChar;
    if (chr == null) {
      _addLog('CMD: característica no disponible');
      return;
    }
    try {
      await chr.write([cmdByte], withoutResponse: true);
    } catch (e) {
      _addLog('Error enviando CMD 0x${cmdByte.toRadixString(16)}: $e');
    }
  }

  Future<void> startEcgStream() => sendCommand(0x01);
  Future<void> stopEcgStream() => sendCommand(0x00);

  Future<void> disconnect() async {
    _addLog('Desconectando...');
    _reconnectTimer?.cancel();
    _reconnectAttempts = _backoffSecs.length; // desactiva el auto-reconnect
    await _device?.disconnect();
    _device = null;
    _state = BluetoothConnectionState.disconnected;
    notifyListeners();
  }

  @override
  void dispose() {
    _reconnectTimer?.cancel();
    _connectionSub?.cancel();
    _scanSub?.cancel();
    for (final s in _notifSubs) {
      s.cancel();
    }
    _imuController.close();
    _ecgController.close();
    _telemetryController.close();
    _spotController.close();
    _device?.disconnect();
    super.dispose();
  }
}

class SpotResult {
  final int bpm;
  final int spo2;
  final int durationMs;
  final int quality;
  final bool aborted;
  final DateTime completedAt;
  SpotResult({
    required this.bpm,
    required this.spo2,
    required this.durationMs,
    required this.quality,
    required this.aborted,
    required this.completedAt,
  });
}
