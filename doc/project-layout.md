# Project layout and dependency boundaries

| Location | Responsibility |
| --- | --- |
| `src/engine/` | Exact money, currency, accounts, allocations, financial validation, statement parsing, matching and reporting semantics. `SpentIt::engine` is standard C++20 and independently testable. |
| `src/app/` | Plain application contracts and use cases. Owns the scope of each complete operation. Start with [ImportRepository](../src/app/include/spentit/import_repository.hpp) and [DocumentGateway](../src/app/include/spentit/document_gateway.hpp). |
| `src/storage/` | SQLCipher connections and transactions, migrations, durable outcomes, and retained source, draft, relationship and history state. Start with [ImportStore](../src/storage/import_store.cpp) and the [document gateway implementation](../src/storage/document_gateway.cpp). |
| `src/ui/` | Qt models, QML, presentation and interaction. The serial worker submits complete repository commands. |
| `src/diagnostics/` | Bounded read-only observation and serialization. Storage and UI produce observations through narrow adapters. |
| `tests/` | Engine, storage, app, UI and diagnostics tests, with synthetic fixtures under `tests/fixtures/`. |
| `tools/`, `cmake/`, `packaging/` | Developer tools, build support and platform distribution. |
| `third_party/` | Pinned dependency provenance and notices, and intentionally vendored code or reference data. |
| `doc/`, `manual/` | Agent contracts and the bundled user manual. |

## Dependency direction

- Engine logic stays independent of Qt and concrete storage. Optional rules use
  the separately linked `spentit_regex` PCRE2 adapter, which also serves search.
- Application contracts use engine types and narrow persistence interfaces.
  Concrete storage implements those interfaces. The application use case defines
  the full operation, and storage commits it in one transaction.
- The Qt model links the application view layer. The production application
  target `spentit_import_experiment` assembles the UI and concrete gateway.
  UI never executes SQL, computes authoritative money, or sequences individual
  saves to implement one financial operation. Database validation remains
  authoritative. Keep heavy work on the worker and publish finished snapshots.
- Reporting SQL must reconcile with the core filter and allocation semantics.
  Rules preview and durable Save and apply use the same matcher. The bounded
  caseless category matcher is shared by the view and storage layers.
- Diagnostics owns no financial commands. Its IPC thread reads published
  summaries and never traverses QML or queries the repository. See
  [diagnostics](application-diagnostics.md).

## Build and data placement

Build out of source. Ignored `build/` trees hold generated objects and
downloaded dependencies. Pin and verify third-party inputs and record local
patches. Keep machine-specific presets and SDK installs out of Git.
Documents and backups live in user-selected or platform-appropriate runtime
locations. Tests use isolated temp directories and synthetic evidence.

Use [building](building.md) for the rules-enabled application. The optional
`SPENTIT_SHEET_PROTOTYPE` target is a synthetic, storage-free sheet.
