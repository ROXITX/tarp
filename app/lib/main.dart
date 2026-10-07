import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import 'bag_store.dart';
import 'ble_service.dart';
import 'home_screen.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  final ble = BleService();
  final store = BagStore(ble);
  await store.load();
  runApp(MultiProvider(
    providers: [
      Provider<BleService>.value(value: ble),
      ChangeNotifierProvider<BagStore>.value(value: store),
    ],
    child: const SmartPackApp(),
  ));
}

class SmartPackApp extends StatelessWidget {
  const SmartPackApp({super.key});

  @override
  Widget build(BuildContext context) => MaterialApp(
        title: 'SmartPack',
        theme: ThemeData(colorSchemeSeed: Colors.teal, useMaterial3: true),
        home: const HomeScreen(),
      );
}
