# SmartPack BLE protocol (draft v0)

Device name: `SmartPack`. Compartments are numbered **1..2**. UIDs are upper-case hex (4/7/10 bytes).
The same text lines work over USB serial (115200) for bench testing.

| Role | UUID |
|---|---|
| Service | `6e5a0001-5b1c-4f6e-9a3d-2f7d4c0b5a10` |
| Command (app → bag, write) | `6e5a0002-5b1c-4f6e-9a3d-2f7d4c0b5a10` |
| Event (bag → app, notify) | `6e5a0003-5b1c-4f6e-9a3d-2f7d4c0b5a10` |

The bag requests MTU 185; the app should request a large MTU too (one line per notification, max ~30 bytes).

## Commands (one line per write)

| Command | Effect |
|---|---|
| `ENROLL <c>` | Next tag scanned (on either reader) is added to compartment `c`. 30 s timeout. |
| `CANCEL` | Leave enroll mode |
| `ADD <c> <UID>` | Assign a tag (e.g. from a previous enroll / manual entry) |
| `DEL <c> <UID>` | Unassign a tag |
| `CLEAR <c>` | Remove all items of compartment |
| `SET <c> <UID> <0\|1>` | Force item state: 0 = outside, 1 = inside (fix drift) |
| `LIST <c>` | Emits `ITEM` lines |
| `STATUS` | Emits `STATE` lines |

Replies: `OK <cmd>` or `ERR <BAD_COMMAND\|ADD_FAILED\|NOT_FOUND>`.

## Events

| Event | Meaning |
|---|---|
| `READY` | Boot finished |
| `IN <c> <UID>` / `OUT <c> <UID>` | Scan toggled the item inside / outside |
| `UNKNOWN <c> <UID>` | Tag not assigned to that compartment |
| `IGNORED_CLOSED <c> <UID>` | Scan while compartment closed (ignored) |
| `OPEN <c>` / `CLOSED <c>` | Reed switch changed (debounced) |
| `MISS <c> <UID>` ... then `VERIFY <c> MISSING <n>` | On closure, items not inside |
| `VERIFY <c> OK 0` / `VERIFY <c> EMPTY 0` | All present / nothing assigned |
| `ENROLLED <c> <UID>` / `ENROLL_DUP <c> <UID>` / `ENROLL_TIMEOUT` | Enrol results |
| `ITEM <c> <UID> <0\|1>` / `STATE <c> <OPEN\|CLOSED> <items> <inside>` | Replies to LIST / STATUS |

## Behaviour

* Each scan of an assigned tag **toggles** it: 1st = IN, 2nd = OUT, 3rd = IN ...
* Newly added items start OUTSIDE; the first scan puts them IN.
* Scans only count while the compartment is open (`SCAN_ONLY_WHEN_OPEN` in `src/main.cpp`).
* On every OPEN→CLOSED edge the compartment is verified: all assigned items must be inside.
* Item list and IN/OUT state are stored in NVS and survive reboot.
