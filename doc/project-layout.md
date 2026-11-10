# Project layout and dependency boundaries

Last updated: 2026-10-29

State: selected organizational direction; the standalone `spentit_engine` library, optional `spentit_storage_prototype`, optional synthetic app/UI libraries and sheet executable, the headless durable-import experiment, and their tests now exist. The user prefers `src/`. The structure below reserves responsibilities, not a requirement to create empty directories or speculative abstractions.

## Planned layout

```text
SpentIt/
├── AGENTS.md
├── CMakeLists.txt           # Root build entry, when implementation starts
├── CMakePresets.json        # Shared portable build/test configurations
├── src/
│   ├── engine/             # Financial model and deterministic business logic
│   ├── storage/            # SQLCipher persistence and storage recovery
│   │   └── migrations/     # Only after the owner-announced release gate
│   ├── app/                # Use cases, document lifecycle, executable wiring
│   └── ui/                 # Qt adapters, models, QML, and presentation assets
├── tests/
│   ├── engine/
│   ├── storage/
│   ├── app/
│   ├── ui/
│   ├── integration/        # Cross-boundary workflows and recovery checks
│   └── fixtures/           # Synthetic or deliberately sanitized inputs
├── cmake/                  # Small project-specific build helpers
├── third_party/            # Dependency inventory, notices, patches, vendored code
├── tools/                  # Developer scripts with concrete uses
├── packaging/              # Platform installation/distribution definitions
├── doc/                    # Actual status and project-wide decisions/guidance
└── features/               # Feature design and acceptance criteria
```

The root CMake files, `cmake/`, `src/engine/`, and `tests/engine/` now exist alongside the documentation. `third_party/iso4217/` holds the pinned currency reference snapshot and provenance; `tools/` contains its offline generator/checker. `SpentIt::engine` exposes only its own headers and the C++20 requirement, with no UI, storage, or application dependencies. The test executable links this library. See [building and verification](building.md) for the validated toolchain. Introduce remaining directories only with actual work. Root README and license files can be added when their contents/terms are ready; no license is selected by this layout.

`src/storage/` and `tests/storage/` hold the SQLCipher document store and its failure tests. The storage target depends on the engine and privately links SQLCipher. Engine-only builds do not fetch or link storage. `third_party/sqlcipher/` contains the dependency pin and notices. Downloaded sources stay in ignored `build/dependencies/`. `src/app/` holds application commands. `src/ui/` holds the Qt adapter and QML. There is no migration layer. See [building](building.md).

## Responsibilities

### Engine

Own money/currency types, accounts, transactions, allocations, financial validation, QIF parsing, OFX-family statement parsing, duplicate-matching logic, rule evaluation, and reporting semantics. It must be testable without opening a window or a real database.

The engine may use appropriate non-UI libraries such as PCRE2. It must not depend on QML, Qt GUI/Widgets/Quick, concrete SQLCipher code, or application startup. Prefer standard C++ for core domain types; any Qt Core dependency needs a concrete justification rather than entering accidentally through UI types.

Group related functionality into subdirectories such as `money/`, `import/`, `rules/`, and `reports/` as it grows. Do not create a library for every subdirectory or put financial logic in a generic `utils/` bucket.

### Storage

Own SQLCipher connections, SQL, schema/migrations, source/draft/history persistence, transactional write primitives, backup/restore, and storage-specific failure handling. Map stored values to engine types without leaking database handles into the UI.

Database constraints complement engine validation. Storage performs the physical atomic transaction; application use cases define the complete logical operation that must be atomic. For example, committing an import must not become separate UI-triggered saves for transaction rows, transfer counterpart state, and history. [Dataset-wide acceptance](import-commit-discussion.md) accepts imported transactions and generated counterparts together; legacy per-account transfer events remain an internal history representation.

Reporting may use SQL aggregation for efficiency. Its filters, currency rules, allocation semantics, and drill-down contract remain defined outside ad hoc SQL/UI code, and integration tests must prove equivalence to those semantics.

### Application

Own use cases such as open document, import/review, commit batch, edit transaction, undo, and generate report. Coordinate engine operations with persistence, cancellation, and worker scheduling. Keep use cases callable without UI objects.

Define narrow persistence interfaces at the boundary where needed; do not design a generic repository framework before there are actual operations to support. Compose concrete storage and UI at the executable entry point. Keep that wiring separate from independently testable use-case code even if both live under `src/app/`.

### UI

Own QML views/components, C++ Qt view models/adapters, selection, navigation, formatting, accessibility, and user-facing progress/errors. Keep QML and C++ presentation code together under this boundary, with subfolders introduced as useful.

The UI submits commands and displays results. It does not execute SQL, recalculate authoritative money in JavaScript, or directly save individual pieces of a financial operation. It may perform convenience validation, but engine/application validation remains authoritative.

## Dependency direction

- Engine depends on selected foundational libraries, not app, UI, or storage.
- Application use cases depend on the engine and narrow persistence contracts, not concrete storage or UI.
- Storage implements those contracts and uses engine types, not UI.
- UI calls application interfaces and consumes presentation-safe values.
- Executable wiring assembles the concrete implementations.

Build targets should enforce these boundaries. A physical folder is not sufficient isolation. The exact number/names of CMake targets and location of boundary-interface headers can emerge with the first vertical slice; avoid circular dependencies.

## Third-party code and build output

Use `third_party/` consistently rather than competing `external/`, `vendor/`, and `3rd-party/` roots. It is for intentional dependency metadata/notices and, only where justified, checked-in upstream source or patches. It is not a destination for arbitrary SDK installers or developer-specific binaries.

The dependency acquisition mechanism remains undecided. Pin dependency versions/revisions and verify downloaded artifacts as appropriate; record provenance, license, and local patches. Do not fetch mutable latest branches or commit entire Qt installations. Keep downloaded/build-generated dependency trees in ignored build/cache directories. Document a reproducible setup when implementation begins.

Use out-of-source builds, normally an ignored `build/` directory with per-configuration subdirectories, or a directory outside the checkout. Do not generate object files, executables, or downloaded dependencies inside `src/`. Local preset overrides and developer paths must not enter shared configuration.

Application documents/backups belong in user-selected or platform-appropriate runtime locations, never a repository `data/` directory. Tests use isolated temporary directories and synthetic fixtures, never a developer's financial document.

## What can emerge later

Exact class names, file splits, private/public header organization, target granularity, asset subfolders, test framework, dependency manager, and platform packaging details can follow demonstrated needs. Establish the safety-relevant dependency direction now; avoid building abstractions solely to fill this tree.

## Optional sheet target

`SPENTIT_SHEET_PROTOTYPE=ON` builds a synthetic sheet that links the engine and
Qt, not storage. It is not the application. The application is
`SPENTIT_IMPORT_UI`.

## Import store

`SPENTIT_IMPORT_WORKFLOW` adds `spentit_import_store` and `spentit_import_view`, requiring the existing optional SQLCipher dependency setup. Engine parsing/review validation remain in `spentit_engine`. `src/app/include/spentit/import_repository.hpp` defines complete synchronous atomic command contracts without Qt or SQL. Storage implements that boundary; the plain application ledger-view target links only the engine. The existing UI remains storage-independent. See [actual scope and verification](durable-import.md).

## Durable import UI boundary

With `SPENTIT_IMPORT_UI=ON`, `spentit_import_model` links the application ledger target and Qt Core/Concurrent, not concrete storage. The plain `DocumentGateway` contract supplies lifecycle/backup and the existing repository commands; its storage implementation owns the credential convention. `spentit_import_experiment` composes the adapter with the Qt model and QML. A private serial worker owns document operations; the UI has no SQL. The synthetic sheet targets remain separately optional. See [UI implementation and limits](import-ui.md).


## Bounded category and rules extensions

The category types/name validation remain in the standard-C++ engine; SQLCipher catalog/history and assignment metadata remain behind the application repository interface, with Qt editor/picker code in the UI. Optional `SPENTIT_RULES` adds `SpentIt::rules`, privately linked to pinned-version PCRE2, while the core `SpentIt::engine` keeps no third-party software dependency. The plain `spentit_rule_preview` application target links that rules library and reconstructs financially unchanged proposals without SQL or Qt. When rules and the import workflow are both enabled, the import store privately links `SpentIt::rules` and `spentit_rule_preview` so it can run that same matcher inside Save and Apply. The UI still must not construct a ruleset or execute SQL. PCRE2 stays off `SpentIt::engine`. When `SPENTIT_IMPORT_UI` and `SPENTIT_RULES` are both on, the import UI includes the rules page, and that page calls worker commands. See [categories](categories.md), [rules engine](rules-engine.md), and [durable rules](durable-rules.md).

## Search and bulk ledger boundary — 2026-10-26

`spentit_regex` owns the reusable bounded PCRE2 adapter; both optional rules and the import workflow link it. `spentit_import_view` privately links that adapter for the independently testable query parser/evaluator, with no SQL or Qt dependency. `SpentIt::engine` keeps its dependency-free boundary. The import worker loads immutable import evidence through the repository and publishes completed query snapshots. QIF and OFX-family occurrence bytes both remain available to that read path. Schema-7 group receipts and bulk writes stay behind complete `ImportRepository` commands; QML does not loop over single-record writes or calculate authoritative totals. See [search/ledger contracts](search-ledger.md).

## Account-scoped commands — 2026-10-26

Account enumeration, creation, durable creation-outcome lookup and session selection are exposed through `ImportRepository`. The existing import worker owns selection; storage scopes batches, money and financial commands and verifies all accounts. QML contains no SQL or authoritative money calculations. See [account behavior and limits](accounts.md).

## Creation preferences and independent categories — 2026-10-28

The dependency-free engine validates date patterns/calendar dates and colon-category syntax. The application import-view target owns bounded Unicode caseless category comparison through the existing PCRE2 adapter. Storage links that application matcher to enforce the same uniqueness policy and owns immutable setup preferences plus atomic starter-category history. The serial UI worker converts between stored ISO dates and the dataset display format; QML neither parses financial import dates nor owns initialization transactions.


## Optional application diagnostics — 2026-10-29

`src/diagnostics/` owns the standard-C++ typed observation contract, bounded collector and serialization. It depends on neither Qt nor SQLCipher and is not linked into the engine. `SPENTIT_DIAGNOSTICS` gates runtime enablement; its disabled sink remains available to the storage/UI adapters. Executable wiring explicitly enables it before constructing the application model. Storage observes existing work through process-local operation scope; it never asks the UI for state or performs a diagnostic query against financial data.

The QML adapter publishes an explicit semantic allowlist. The optional Qt local IPC server runs on its own thread and reads published summaries; it never traverses QML or calls the repository. The CLI has only status, bounded events and observation waits. See [the diagnostics contract and verification](application-diagnostics.md). Diagnostics-specific tests are under `tests/diagnostics/`; existing synthetic UI/storage tests retain their independent financial assertions.
