// Parser for the text event lines sent by the bag (see ../docs/PROTOCOL.md).
// Pure Dart: no Flutter/BLE imports, so it is unit-testable.

sealed class BagEvent {
  const BagEvent();
}

class Ready extends BagEvent {
  const Ready();
}

/// IN / OUT / UNKNOWN / IGNORED_CLOSED / ENROLLED / ENROLL_DUP / MISS
class TagEvent extends BagEvent {
  final String kind;
  final int comp; // 1-based, as on the wire
  final String uid;
  const TagEvent(this.kind, this.comp, this.uid);
}

/// OPEN / CLOSED
class ReedEvent extends BagEvent {
  final int comp;
  final bool closed;
  const ReedEvent(this.comp, this.closed);
}

/// VERIFY <c> OK|EMPTY|MISSING <n>; preceded by `n` MISS lines when missing.
class VerifyEvent extends BagEvent {
  final int comp;
  final String result; // OK, EMPTY, MISSING
  final int missing;
  const VerifyEvent(this.comp, this.result, this.missing);
}

/// ITEM <c> <uid> <0|1> (reply to LIST)
class ItemEvent extends BagEvent {
  final int comp;
  final String uid;
  final bool inside;
  const ItemEvent(this.comp, this.uid, this.inside);
}

/// STATE <c> OPEN|CLOSED <items> <inside> (reply to STATUS)
class StateEvent extends BagEvent {
  final int comp;
  final bool closed;
  final int items;
  final int inside;
  const StateEvent(this.comp, this.closed, this.items, this.inside);
}

class Ok extends BagEvent {
  final String cmd;
  const Ok(this.cmd);
}

class Err extends BagEvent {
  final String reason;
  const Err(this.reason);
}

class EnrollTimeout extends BagEvent {
  const EnrollTimeout();
}

const _tagKinds = {
  'IN', 'OUT', 'UNKNOWN', 'IGNORED_CLOSED', 'ENROLLED', 'ENROLL_DUP', 'MISS'
};

/// Returns null for lines it does not understand.
BagEvent? parseEvent(String line) {
  final t = line.trim().split(RegExp(r'\s+'));
  if (t.isEmpty || t.first.isEmpty) return null;
  int? n(int i) => i < t.length ? int.tryParse(t[i]) : null;

  final k = t[0];
  if (k == 'READY') return const Ready();
  if (k == 'ENROLL_TIMEOUT') return const EnrollTimeout();
  if (_tagKinds.contains(k) && t.length == 3 && n(1) != null) {
    return TagEvent(k, n(1)!, t[2].toUpperCase());
  }
  if ((k == 'OPEN' || k == 'CLOSED') && t.length == 2 && n(1) != null) {
    return ReedEvent(n(1)!, k == 'CLOSED');
  }
  if (k == 'VERIFY' && t.length == 4 && n(1) != null && n(3) != null) {
    return VerifyEvent(n(1)!, t[2], n(3)!);
  }
  if (k == 'ITEM' && t.length == 4 && n(1) != null) {
    return ItemEvent(n(1)!, t[2].toUpperCase(), t[3] == '1');
  }
  if (k == 'STATE' && t.length == 5 && n(1) != null && n(3) != null && n(4) != null) {
    return StateEvent(n(1)!, t[2] == 'CLOSED', n(3)!, n(4)!);
  }
  if (k == 'OK' && t.length >= 2) return Ok(t[1]);
  if (k == 'ERR' && t.length >= 2) return Err(t[1]);
  return null;
}
