import 'dart:async';
import 'dart:convert';

import 'package:flutter_blue_plus/flutter_blue_plus.dart';

final Guid kServiceUuid = Guid('6e5a0001-5b1c-4f6e-9a3d-2f7d4c0b5a10');
final Guid kCmdUuid = Guid('6e5a0002-5b1c-4f6e-9a3d-2f7d4c0b5a10');
final Guid kEventUuid = Guid('6e5a0003-5b1c-4f6e-9a3d-2f7d4c0b5a10');

/// Thin BLE wrapper: connect to a SmartPack, send command lines, stream event lines.
class BleService {
  BluetoothDevice? _device;
  BluetoothCharacteristic? _cmd;
  StreamSubscription<List<int>>? _eventSub;
  StreamSubscription<BluetoothConnectionState>? _stateSub;
  final _lines = StreamController<String>.broadcast();
  final _connected = StreamController<bool>.broadcast();

  Stream<String> get lines => _lines.stream;
  Stream<bool> get connection => _connected.stream;
  bool get isConnected => _cmd != null;

  /// Scans for devices advertising the SmartPack service.
  Stream<List<ScanResult>> scan({Duration timeout = const Duration(seconds: 8)}) {
    FlutterBluePlus.startScan(withServices: [kServiceUuid], timeout: timeout);
    return FlutterBluePlus.scanResults;
  }

  Future<void> stopScan() => FlutterBluePlus.stopScan();

  Future<void> connect(BluetoothDevice device) async {
    await stopScan();
    await device.connect(autoConnect: false);
    _device = device;
    _stateSub = device.connectionState.listen((s) {
      if (s == BluetoothConnectionState.disconnected) _cleanup();
    });
    try {
      await device.requestMtu(185); // Android only; iOS negotiates itself
    } catch (_) {}

    final services = await device.discoverServices();
    final svc = services.firstWhere((s) => s.uuid == kServiceUuid);
    _cmd = svc.characteristics.firstWhere((c) => c.uuid == kCmdUuid);
    final ev = svc.characteristics.firstWhere((c) => c.uuid == kEventUuid);
    await ev.setNotifyValue(true);
    _eventSub = ev.onValueReceived.listen((data) {
      final s = utf8.decode(data, allowMalformed: true).trim();
      if (s.isNotEmpty) _lines.add(s);
    });
    _connected.add(true);
  }

  Future<void> send(String line) async {
    final c = _cmd;
    if (c == null) throw StateError('Not connected');
    await c.write(utf8.encode(line), withoutResponse: false);
  }

  Future<void> disconnect() async {
    await _device?.disconnect();
    _cleanup();
  }

  void _cleanup() {
    _eventSub?.cancel();
    _stateSub?.cancel();
    _eventSub = _stateSub = null;
    final was = _cmd != null;
    _cmd = null;
    _device = null;
    if (was) _connected.add(false);
  }
}
