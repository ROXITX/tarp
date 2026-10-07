# SmartPack app (Flutter)

Phone-only app: owns item/compartment **names**; the bag owns tag UIDs and IN/OUT state.
Talks to the bag over BLE using the text protocol in [`../docs/PROTOCOL.md`](../docs/PROTOCOL.md).

## First-time setup

Platform folders are not committed; generate them, then add Bluetooth permissions:

```bash
cd app
flutter create --project-name smartpack_app --platforms android,ios .
flutter pub get
flutter test
flutter run
```

**Android** (`android/app/src/main/AndroidManifest.xml`, inside `<manifest>`):
```xml
<uses-permission android:name="android.permission.BLUETOOTH_SCAN" android:usesPermissionFlags="neverForLocation"/>
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT"/>
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" android:maxSdkVersion="30"/>
<uses-permission android:name="android.permission.BLUETOOTH" android:maxSdkVersion="30"/>
<uses-permission android:name="android.permission.BLUETOOTH_ADMIN" android:maxSdkVersion="30"/>
```
Set `minSdkVersion 21` or higher in `android/app/build.gradle`.

**iOS** (`ios/Runner/Info.plist`): add `NSBluetoothAlwaysUsageDescription`.

## How it works

| File | Role |
|---|---|
| `lib/protocol.dart` | Parses bag event lines (pure Dart, tested in `test/protocol_test.dart`) |
| `lib/ble_service.dart` | Scan, connect, write commands, stream event lines |
| `lib/bag_store.dart` | App state, persistence (shared_preferences), enroll flow, sync, missing-item alert |
| `lib/home_screen.dart` | Two compartment cards, add/remove items, live IN/OUT, connect sheet |

* **Add item:** tap *Add item*, type a name, tap the tag on the reader. App sends `ENROLL n`, bag answers `ENROLLED n UID`, app stores UID + name.
* **On connect:** app sends `LIST 1`, `LIST 2`, `STATUS`; adds back items the bag lost, adopts the bag's IN/OUT state, and shows tags it doesn't know as "Unnamed tag XXXX".
* **On closure:** bag sends `MISS` lines then `VERIFY n MISSING k`; app shows a dialog naming each missing item.
