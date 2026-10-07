import 'package:flutter_test/flutter_test.dart';
import 'package:smartpack_app/protocol.dart';

void main() {
  test('tag events', () {
    final e = parseEvent('IN 1 04a1b2c3') as TagEvent;
    expect((e.kind, e.comp, e.uid), ('IN', 1, '04A1B2C3'));
    expect((parseEvent('MISS 2 DEADBEEF') as TagEvent).kind, 'MISS');
  });

  test('reed and verify', () {
    expect((parseEvent('CLOSED 2') as ReedEvent).closed, true);
    expect((parseEvent('OPEN 1') as ReedEvent).closed, false);
    final v = parseEvent('VERIFY 1 MISSING 2') as VerifyEvent;
    expect((v.comp, v.result, v.missing), (1, 'MISSING', 2));
  });

  test('replies', () {
    expect((parseEvent('ITEM 1 AABBCCDD 1') as ItemEvent).inside, true);
    final s = parseEvent('STATE 1 CLOSED 3 2') as StateEvent;
    expect((s.closed, s.items, s.inside), (true, 3, 2));
    expect((parseEvent('OK LIST') as Ok).cmd, 'LIST');
    expect((parseEvent('ERR NOT_FOUND') as Err).reason, 'NOT_FOUND');
    expect(parseEvent('ENROLL_TIMEOUT'), isA<EnrollTimeout>());
  });

  test('garbage is ignored', () {
    expect(parseEvent(''), isNull);
    expect(parseEvent('IN x y'), isNull);
    expect(parseEvent('WAT'), isNull);
  });
}
