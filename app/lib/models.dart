const int kCompartments = 2;

class BagItem {
  final String uid;
  String name;
  bool inside;
  BagItem({required this.uid, required this.name, this.inside = false});

  Map<String, dynamic> toJson() => {'uid': uid, 'name': name, 'inside': inside};
  factory BagItem.fromJson(Map<String, dynamic> j) =>
      BagItem(uid: j['uid'] as String, name: j['name'] as String, inside: j['inside'] as bool? ?? false);
}

class Compartment {
  String name;
  final List<BagItem> items;
  bool closed;
  Compartment({required this.name, List<BagItem>? items, this.closed = false}) : items = items ?? [];

  int get insideCount => items.where((i) => i.inside).length;

  BagItem? byUid(String uid) {
    for (final i in items) {
      if (i.uid == uid) return i;
    }
    return null;
  }

  Map<String, dynamic> toJson() => {'name': name, 'items': items.map((i) => i.toJson()).toList()};
  factory Compartment.fromJson(Map<String, dynamic> j) => Compartment(
        name: j['name'] as String,
        items: (j['items'] as List).map((e) => BagItem.fromJson(e as Map<String, dynamic>)).toList(),
      );
}
