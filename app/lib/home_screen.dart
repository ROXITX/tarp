import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:provider/provider.dart';

import 'bag_store.dart';
import 'ble_service.dart';
import 'models.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  bool _alertOpen = false;

  @override
  Widget build(BuildContext context) {
    final store = context.watch<BagStore>();

    // Surface transient messages and missing-item alerts after this frame.
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!mounted) return;
      final msg = store.lastMessage;
      if (msg != null) {
        store.clearMessage();
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(msg)));
      }
      final alert = store.alert;
      if (alert != null && !_alertOpen) {
        _alertOpen = true;
        showDialog<void>(
          context: context,
          builder: (_) => AlertDialog(
            icon: const Icon(Icons.warning_amber_rounded, color: Colors.red, size: 40),
            title: Text('Missing in ${alert.compartment}'),
            content: Text(alert.itemNames.map((n) => '• $n').join('\n')),
            actions: [TextButton(onPressed: () => Navigator.pop(context), child: const Text('OK'))],
          ),
        ).then((_) {
          _alertOpen = false;
          store.clearAlert();
        });
      }
    });

    return Scaffold(
      appBar: AppBar(
        title: const Text('SmartPack'),
        actions: [
          IconButton(
            tooltip: store.connected ? 'Connected' : 'Connect to bag',
            icon: Icon(store.connected ? Icons.bluetooth_connected : Icons.bluetooth_searching),
            onPressed: () => store.connected ? context.read<BleService>().disconnect() : _pickDevice(context),
          ),
        ],
      ),
      body: ListView(
        padding: const EdgeInsets.all(12),
        children: [
          for (var i = 0; i < kCompartments; i++) _CompartmentCard(index: i),
        ],
      ),
    );
  }

  Future<void> _pickDevice(BuildContext context) async {
    final ble = context.read<BleService>();
    final device = await showModalBottomSheet<BluetoothDevice>(
      context: context,
      builder: (_) => _ScanSheet(ble: ble),
    );
    await ble.stopScan();
    if (device == null || !context.mounted) return;
    try {
      await ble.connect(device);
    } catch (e) {
      if (context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('Connect failed: $e')));
      }
    }
  }
}

class _ScanSheet extends StatelessWidget {
  const _ScanSheet({required this.ble});
  final BleService ble;

  @override
  Widget build(BuildContext context) => SafeArea(
        child: StreamBuilder<List<ScanResult>>(
          stream: ble.scan(),
          builder: (context, snap) {
            final results = snap.data ?? const [];
            if (results.isEmpty) {
              return const SizedBox(height: 160, child: Center(child: Text('Scanning for SmartPack...')));
            }
            return ListView(
              shrinkWrap: true,
              children: [
                for (final r in results)
                  ListTile(
                    leading: const Icon(Icons.backpack),
                    title: Text(r.device.platformName.isEmpty ? 'SmartPack' : r.device.platformName),
                    subtitle: Text(r.device.remoteId.str),
                    onTap: () => Navigator.pop(context, r.device),
                  ),
              ],
            );
          },
        ),
      );
}

class _CompartmentCard extends StatelessWidget {
  const _CompartmentCard({required this.index});
  final int index;

  @override
  Widget build(BuildContext context) {
    final store = context.watch<BagStore>();
    final c = store.compartments[index];
    return Card(
      margin: const EdgeInsets.only(bottom: 12),
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
          Row(children: [
            Expanded(
              child: GestureDetector(
                onTap: () => _rename(context, store),
                child: Text(c.name, style: Theme.of(context).textTheme.titleLarge),
              ),
            ),
            Chip(
              avatar: Icon(c.closed ? Icons.lock : Icons.lock_open, size: 16),
              label: Text(c.closed ? 'Closed' : 'Open'),
            ),
          ]),
          Text('${c.insideCount} of ${c.items.length} inside'),
          const Divider(),
          if (c.items.isEmpty) const Padding(padding: EdgeInsets.all(8), child: Text('No items yet')),
          for (final item in c.items)
            Dismissible(
              key: ValueKey('${index}_${item.uid}'),
              direction: DismissDirection.endToStart,
              background: Container(color: Colors.red, alignment: Alignment.centerRight, padding: const EdgeInsets.only(right: 16), child: const Icon(Icons.delete, color: Colors.white)),
              onDismissed: (_) => store.removeItem(index, item),
              child: ListTile(
                dense: true,
                leading: Icon(item.inside ? Icons.check_circle : Icons.radio_button_unchecked,
                    color: item.inside ? Colors.green : Colors.grey),
                title: Text(item.name),
                subtitle: Text(item.uid),
                trailing: Switch(value: item.inside, onChanged: (v) => store.setInside(index, item, v)),
              ),
            ),
          Align(
            alignment: Alignment.centerRight,
            child: store.enrolling
                ? TextButton.icon(
                    onPressed: store.cancelEnroll,
                    icon: const Icon(Icons.nfc),
                    label: const Text('Tap the tag on the reader... (cancel)'))
                : FilledButton.icon(
                    onPressed: store.connected ? () => _addItem(context, store) : null,
                    icon: const Icon(Icons.add),
                    label: const Text('Add item')),
          ),
        ]),
      ),
    );
  }

  Future<void> _addItem(BuildContext context, BagStore store) async {
    final name = await _askText(context, 'Item name', 'e.g. Wallet');
    if (name != null && name.isNotEmpty) await store.startEnroll(index, name);
  }

  Future<void> _rename(BuildContext context, BagStore store) async {
    final name = await _askText(context, 'Compartment name', store.compartments[index].name);
    if (name != null && name.isNotEmpty) store.renameCompartment(index, name);
  }

  Future<String?> _askText(BuildContext context, String title, String hint) {
    final ctl = TextEditingController();
    return showDialog<String>(
      context: context,
      builder: (_) => AlertDialog(
        title: Text(title),
        content: TextField(controller: ctl, autofocus: true, decoration: InputDecoration(hintText: hint)),
        actions: [
          TextButton(onPressed: () => Navigator.pop(context), child: const Text('Cancel')),
          FilledButton(onPressed: () => Navigator.pop(context, ctl.text.trim()), child: const Text('OK')),
        ],
      ),
    );
  }
}
