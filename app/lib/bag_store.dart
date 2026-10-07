import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'ble_service.dart';
import 'models.dart';
import 'protocol.dart';

/// Result of a closure check, shown to the user as an alert.
class MissingAlert {
  final String compartment;
  final List<String> itemNames;
  const MissingAlert(this.compartment, this.itemNames);
}

/// App state. The phone owns names; the bag owns tag UIDs + IN/OUT state per compartment.
class BagStore extends ChangeNotifier {
  BagStore(this.ble) {
    _lineSub = ble.lines.listen(_onLine);
    _connSub = ble.connection.listen(_onConnection);
  }

  final BleService ble;
  late final StreamSubscription<String> _lineSub;
  late final StreamSubscription<bool> _connSub;

  final List<Compartment> compartments =
      List.generate(kCompartments, (i) => Compartment(name: 'Compartment ${i + 1}'));
  bool connected = false;
  String? lastMessage; // transient info/error for a snackbar
  MissingAlert? alert; // set when a closure check finds missing items

  // Enrol flow
  int? _enrollComp;
  String? _enrollName;
  bool get enrolling => _enrollComp != null;

  // Closure check accumulators: MISS lines arrive before the VERIFY summary.
  final Map<int, List<String>> _missBuf = {};
  // LIST replies: ITEM lines collected until the matching `OK LIST`.
  final List<int> _listQueue = [];
  final Map<int, Map<String, bool>> _listBuf = {};

  static const _prefsKey = 'smartpack_v1';

  // ---------- persistence ----------
  Future<void> load() async {
    final p = await SharedPreferences.getInstance();
    final raw = p.getString(_prefsKey);
    if (raw == null) return;
    final list = jsonDecode(raw) as List;
    for (var i = 0; i < kCompartments && i < list.length; i++) {
      compartments[i] = Compartment.fromJson(list[i] as Map<String, dynamic>);
    }
    notifyListeners();
  }

  Future<void> _save() async {
    final p = await SharedPreferences.getInstance();
    await p.setString(_prefsKey, jsonEncode(compartments.map((c) => c.toJson()).toList()));
  }

  void _changed() {
    notifyListeners();
    _save();
  }

  // ---------- user actions ----------
  Future<void> _send(String line) async {
    try {
      await ble.send(line);
    } catch (e) {
      lastMessage = 'Bag not connected';
      notifyListeners();
    }
  }

  void renameCompartment(int index, String name) {
    compartments[index].name = name;
    _changed();
  }

  /// Starts scan-to-enroll: the bag adds the next tag it sees to [index].
  Future<void> startEnroll(int index, String itemName) async {
    _enrollComp = index + 1;
    _enrollName = itemName;
    notifyListeners();
    await _send('ENROLL ${index + 1}');
  }

  Future<void> cancelEnroll() async {
    _enrollComp = null;
    _enrollName = null;
    notifyListeners();
    await _send('CANCEL');
  }

  Future<void> removeItem(int index, BagItem item) async {
    compartments[index].items.remove(item);
    _changed();
    await _send('DEL ${index + 1} ${item.uid}');
  }

  /// Manual correction if the app/bag disagree about an item being inside.
  Future<void> setInside(int index, BagItem item, bool inside) async {
    item.inside = inside;
    _changed();
    await _send('SET ${index + 1} ${item.uid} ${inside ? 1 : 0}');
  }

  void clearAlert() {
    alert = null;
    notifyListeners();
  }

  void clearMessage() => lastMessage = null;

  // ---------- connection / sync ----------
  void _onConnection(bool up) {
    connected = up;
    notifyListeners();
    if (up) _syncFromBag();
  }

  /// Reconcile after connecting: push items the bag lacks, then pull the bag's state.
  Future<void> _syncFromBag() async {
    for (var c = 0; c < kCompartments; c++) {
      _listQueue.add(c + 1);
      await _send('LIST ${c + 1}');
    }
    await _send('STATUS');
  }

  Future<void> _finishList(int comp) async {
    final c = compartments[comp - 1];
    final onBag = _listBuf.remove(comp) ?? {};
    for (final it in c.items) {
      final inside = onBag[it.uid];
      if (inside == null) {
        await _send('ADD $comp ${it.uid}'); // phone knows it, bag lost it
      } else {
        it.inside = inside;
      }
    }
    for (final e in onBag.entries) {
      if (c.byUid(e.key) == null) {
        final short = e.key.length > 4 ? e.key.substring(e.key.length - 4) : e.key;
        c.items.add(BagItem(uid: e.key, name: 'Unnamed tag $short', inside: e.value));
      }
    }
    _changed();
  }

  // ---------- events from the bag ----------
  void _onLine(String line) {
    final ev = parseEvent(line);
    if (ev == null) return;
    switch (ev) {
      case Ready():
        break;
      case TagEvent():
        _onTag(ev);
      case ReedEvent():
        if (_valid(ev.comp)) {
          compartments[ev.comp - 1].closed = ev.closed;
          notifyListeners();
        }
      case VerifyEvent():
        _onVerify(ev);
      case ItemEvent():
        if (_valid(ev.comp)) (_listBuf[ev.comp] ??= {})[ev.uid] = ev.inside;
      case StateEvent():
        if (_valid(ev.comp)) {
          compartments[ev.comp - 1].closed = ev.closed;
          notifyListeners();
        }
      case Ok():
        if (ev.cmd == 'LIST' && _listQueue.isNotEmpty) _finishList(_listQueue.removeAt(0));
      case Err():
        lastMessage = 'Bag error: ${ev.reason}';
        if (enrolling) {
          _enrollComp = null;
          _enrollName = null;
        }
        notifyListeners();
      case EnrollTimeout():
        _enrollComp = null;
        _enrollName = null;
        lastMessage = 'No tag scanned - try again';
        notifyListeners();
    }
  }

  bool _valid(int comp) => comp >= 1 && comp <= kCompartments;

  void _onTag(TagEvent e) {
    if (!_valid(e.comp)) return;
    final c = compartments[e.comp - 1];
    switch (e.kind) {
      case 'IN':
      case 'OUT':
        final item = c.byUid(e.uid);
        if (item != null) {
          item.inside = e.kind == 'IN';
          _changed();
        }
      case 'UNKNOWN':
        lastMessage = 'Unknown tag ${e.uid} on ${c.name}';
        notifyListeners();
      case 'ENROLLED':
        if (_enrollComp == e.comp) {
          c.items.add(BagItem(uid: e.uid, name: _enrollName ?? 'Item', inside: false));
          _enrollComp = null;
          _enrollName = null;
          lastMessage = 'Added. Scan it once more to put it IN.';
          _changed();
        }
      case 'ENROLL_DUP':
        _enrollComp = null;
        _enrollName = null;
        lastMessage = 'That tag is already in ${c.name}';
        notifyListeners();
      case 'MISS':
        (_missBuf[e.comp] ??= []).add(e.uid);
    }
  }

  void _onVerify(VerifyEvent v) {
    if (!_valid(v.comp)) return;
    final c = compartments[v.comp - 1];
    final uids = _missBuf.remove(v.comp) ?? const <String>[];
    if (v.result == 'MISSING') {
      final names = uids.map((u) => c.byUid(u)?.name ?? 'Unknown tag $u').toList();
      alert = MissingAlert(c.name, names);
      HapticFeedback.heavyImpact();
    } else if (v.result == 'OK') {
      lastMessage = '${c.name}: all items present';
    } else {
      lastMessage = '${c.name}: no items assigned';
    }
    notifyListeners();
  }

  @override
  void dispose() {
    _lineSub.cancel();
    _connSub.cancel();
    super.dispose();
  }
}
