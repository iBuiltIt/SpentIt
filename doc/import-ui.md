# Application UI

Last updated: 2026-11-09

The application UI is `./build/SpentIt`. Build it from [building.md](building.md).

Ledger editing is [ledger-editing.md](ledger-editing.md). Import, Review,
rules, reports, categories, and Help are the feature documents. Settings is in
the lower-left sidebar only.

Backup shows its phases on the worker. Cancel is available before exclusive
publication. A cancelled candidate is not a valid backup. Success follows
directory synchronization and candidate cleanup. Individual SQLite calls and
filesystem synchronization are not interruptible.
