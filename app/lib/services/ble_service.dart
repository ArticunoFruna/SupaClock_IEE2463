import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

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
      timestamp: DateTime.now(),
    );
  }
}

class ImuSample {
  final int ax, ay, az, gx, gy, gz;
  final DateTime t;
  ImuSample(this.ax, this.ay, this.az, this.gx, this.gy, this.gz, this.t);
}

class BleService extends ChangeNotifier {
  BluetoothDevice? _device;
  BluetoothConnectionState _state = BluetoothConnectionState.disconnected;
  StreamSubscription? _connectionSub;
  StreamSubscription? _scanSub;

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

  void _addLog(String msg) {
    final ts = DateTime.now().toIso8601String().substring(11, 19);
    _log.insert(0, '[$ts] $msg');
    if (_log.length > 200) _log.removeLast();
    notifyListeners();
  }

  // ── Scan / connect ────────────────────────────────────────────────
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

    _connectionSub?.cancel();
    _connectionSub = device.connectionState.listen((s) {
      _state = s;
      _addLog('Estado: ${s.name}');
      if (s == BluetoothConnectionState.disconnected) {
        _device = null;
        for (final sub in _notifSubs) {
          sub.cancel();
        }
        _notifSubs.clear();
      }
      notifyListeners();
    });

    await Future.delayed(const Duration(milliseconds: 500));
    await _discoverAndSubscribe(device);
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
    await _device?.disconnect();
    _device = null;
    _state = BluetoothConnectionState.disconnected;
    notifyListeners();
  }

  @override
  void dispose() {
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
