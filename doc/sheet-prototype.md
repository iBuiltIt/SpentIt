# Synthetic transaction-sheet prototype

Last updated: 2026-10-25

This is a bounded Qt Quick experiment, authorized after the engine and storage experiments. It is not a financial document application. Every record is generated; there is no import, file picker, database connection, or durable save. Closing the process loses session edits. The window labels this explicitly.

## Scope and boundaries

- Persistent toolbar, resizable/hideable sidebar, independently scrollable Accounts/Reports panes with a draggable divider, two synthetic accounts, and account navigation history.
- Virtualized/recycled ledger rows, all six sortable columns, pointer and keyboard selection, multiple selection, and simultaneous read-only allocation expansions.
- One inline editor. Its identity and temporary field strings live outside recycled delegates. Sorting, filtering, or scrolling cannot reassign an edit to another transaction. An editor filtered out of view retains its fields until restored or explicitly resolved through navigation.
- Payee, note, signed total, and a second allocation's category/memo/amount can be edited. Allocation 1 is the designated remainder. A second allocation is added only by explicit checkbox; zero allocations are retained. Editing arbitrary allocation counts/removing allocations and remainder metadata are deferred.
- The existing exact-money and split engine validates final combined edits. Original imported fixture amounts remain accessible. An invalid save leaves the session and editor unchanged; mixed signs require explicit choice. These edits are session operations, not production committed-history commands.
- Save / Discard / Stay protects account navigation, Back/Forward, changing the active editor, and window closing. Failed validation stays in the editor. Busy operations finish before navigation becomes available.
- Per-account committed/projected balance toggles and exact filtered Withdrawal/Deposit totals. The ledger Balance column is explicitly a chronological **projection including drafts** for this experiment; sorting/filtering retains those original balances. Production balance-view policy remains unresolved.
- A bounded, case-insensitive ASCII literal filter across fixture dates, payees, notes, allocation categories and memos. Enter applies; Clear removes it. This does not implement or decide the product query language, QIF mode, Unicode matching, dates, or regex syntax. It restores the previous selected identity and viewport when no result was selected; selecting a result retains that identity when cleared. Full multiselection restoration is deferred.

The settings/categories/rules/report areas are labeled as future work, with no pretend functional buttons. Account creation, currency reassignment, real source evidence/copy actions, transfers, destructive actions, undo/history, document switching, and production persistence are outside this milestone. No personal records should be supplied to it.

`src/app` contains a plain C++ synthetic-session library linked only to the engine. `src/ui` contains the Qt model, QML presentation and executable wiring; it has no SQL or storage dependency. Generation, query computation and saves run in QtConcurrent workers over immutable snapshots. Only one operation is in flight; the worker never accesses UI objects. Results are moved into the UI model. Copying a snapshot on save and recalculating chronological balances on each query are deliberately simple experimental choices, not final storage architecture.

## Prototype conventions, not product decisions

The owner specified approximately 100,000 transactions/account as representative and 200,000 as the stress case, based on long-lived personal accounts. These are measurement workloads, not production record limits. The executable defaults to 100,000 per account and accepts `--rows N`. Generation rejects invalid counts or identities that cannot fit its integer fixture IDs; available memory remains a practical constraint.

The fixtures use two CAD accounts, dates over approximately twenty years, repeated payments, income, split memos, and a final 10% draft subset. They are repetitive synthetic data, not a claim to cover real-world distributions. Same-date ordering uses permanent fixture IDs. All amounts and totals are calculated/formatted in C++, never QML numeric arithmetic. Ascending withdrawal sorting orders displayed magnitudes. View choices persist only during this process.

Keyboard conventions: Up/Down select ledger rows; Shift extends the range; Ctrl/Command-click toggles rows; Enter in the ledger opens its selected row; Enter in a field saves; Cancel abandons that edit. Notes are single-line in this prototype. Ctrl+B toggles the sidebar (Qt maps Ctrl shortcuts to Command on macOS). Detailed production keyboard, accessibility and narrow-window behavior are still open. The experiment uses a 940 × 600 minimum window.

## Build and run

Install a Qt 6.8+ development environment with Core, Gui, Qml, Quick, QuickControls2 and Concurrent; tests also require Qt Test. The verified version and provenance are in [Qt dependency metadata](../third_party/qt/README.md). Engine-only builds do not require Qt. No dependencies are downloaded by CMake.

```sh
cmake -S . -B build/sheet-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DSPENTIT_SHEET_PROTOTYPE=ON \
  -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build/sheet-release
ctest --test-dir build/sheet-release --output-on-failure
build/sheet-release/src/ui/spentit_sheet_prototype --rows 100000
```

For the tested Homebrew split packages, use `-DCMAKE_PREFIX_PATH=/opt/homebrew` and `-DQT_ADDITIONAL_PACKAGES_PREFIX_PATH='/opt/homebrew/opt/qtdeclarative;/opt/homebrew/opt/qtsvg'`. These are local setup examples, not hard-coded build paths. An existing Debug cache initially configured with the qtbase opt prefix emitted Homebrew forwarding-header `#include_next` warnings; Release and sanitizer configurations with the prefix above did not. Vulkan headers were not found, but were not required for the tested macOS renderer.

Use a separate build directory and `-DCMAKE_BUILD_TYPE=Debug` for Debug; additionally `-DSPENTIT_SANITIZERS=ON` for ASan/UBSan. The optional target does not link the SQLCipher prototype or change engine-only builds.

## Verification and measurements

Verification details and final measurements follow below. Linux and Windows UI builds/interactions remain unverified. Qt remains a candidate for the product, selected only for this experiment.

Reproduce the CPU/memory workload using `build/sheet-release/src/ui/spentit_sheet_benchmark 100000` (repeat with `200000`). It measures fixture creation, each of six sorts, three literal filters, and a session save plus refresh. It reports process peak RSS on macOS/Linux; this includes both accounts and old/new snapshots during the save, but excludes Qt UI and database caches. No storage, encryption, disk I/O, rules or QIF parsing is measured.

Run `build/sheet-release/src/ui/spentit_sheet_prototype --rows 100000 --scroll-benchmark` from a desktop session to measure GUI timer and rendered-frame intervals. It scrolls 120 logical pixels every 16 ms for 300 timer events, initiating one payee sort at event 100. This includes model reset and delegate reuse, but is a bounded scripted workload, not proof of stall-free interaction. Native window access is required. `--smoke --screenshot /absolute/path.png` renders a disposable preview; adding `--edit` previews the inline editor. Automated QML/interaction tests use Qt's offscreen platform.

### Actual results — 2026-10-25

macOS 27.0.1 arm64, AppleClang 21.0.0, CMake 4.4.4, Ninja 1.13.2, Qt 6.12.0. Debug, Release and ASan/UBSan builds each passed **13/13 CTest entries**: the existing nine engine groups plus session invariants, model navigation/identity tests, QML smoke, and rendered interaction tests. No sanitizer diagnostics were emitted. Release/sanitizer builds emitted no compiler warnings; the Debug Qt forwarding-header warnings are described above. The unchanged engine-only Debug preset was also configured/built and passed 9/9 tests with the optional sheet target off. Local Markdown link targets passed existence checks. Storage tests were not rerun for this storage-independent change.

The session checks reconcile exact totals, reject malformed/overflowing amounts, preserve original totals and immutable snapshots, verify draft exclusion and chronological balance stability, and exercise final-state validation for simultaneous total/allocation corrections, explicit mixed signs and explicit zero allocations. Model tests cover failed saves, stable identity after sort/filter, multi-selection, search restoration and Save/Discard/Stay including failed-save navigation. Rendered interaction tests exercise pointer double-click, keyboard movement, QML field binding, delegate recycling with an unsaved edit, clicking Save, and dirty-window-close interception. These run offscreen on macOS; they are not cross-platform/manual accessibility certification. The initial ledger and expanded inline-editor renders were visually inspected.

Three serial Release benchmark processes per workload, with no overlapping test/build jobs, produced the ranges below. Each process contains **two accounts** of the stated size. Sort ranges cover six columns across all three processes; filter ranges cover split memo, payee and no-match queries. They include chronological balance recomputation and filtered exact totals. These are worker/use-case costs, not click-to-paint latency.

| Transactions per account | Six sorts, min–max | Three filters, min–max | Save + refresh, min–max | Peak RSS, max |
|---|---|---|---|---|
| 100,000 | 58.60–84.46 ms | 54.43–64.70 ms | 71.71–71.98 ms | 171.56 MiB |
| 200,000 | 123.68–180.62 ms | 116.65–125.78 ms | 148.18–149.24 ms | 341.03 MiB |

Fixture generation took 48.63–59.04 ms and 97.19–98.19 ms respectively. Peak RSS is the non-Qt benchmark process, includes immutable snapshots during save, and is not the desktop application's total memory requirement.

Three separate native-window scrolling runs per workload (300 timer events each) produced:

| Transactions/account | Frame interval p95, range across runs | Largest frame interval | GUI timer p95, range | Largest GUI timer interval | Payee query during scrolling |
|---|---|---|---|---|---|
| 100,000 | 18.02–18.15 ms | 77.19 ms | 17.18–17.92 ms | 74.77 ms | 98–112 ms |
| 200,000 | 18.72–19.50 ms | 34.63 ms | 17.60–18.04 ms | 34.42 ms | 196–201 ms |

The frame samples include initial rendering, scrolling and a sort/model reset. The longest intervals are observable outliers; they were not attributed to a specific cause. The proposed 100 ms query target is not consistently met during native scrolling at 100,000 rows and is exceeded at 200,000. No stall-free claim is made. The GUI remains serviced while worker queries run, but completion/reset still needs frame-budget profiling.

This supports retaining Qt Quick as a candidate, not final adoption. Before the production vertical slice, consider caching chronological balances per immutable account snapshot, sorting precomputed keys/indices, incremental model updates, and avoiding full snapshot copies for a one-row save. Measure those changes with real storage operations and more varied synthetic records, then validate Linux/Windows, keyboard/accessibility, narrow windows and larger monetary displays. A database-backed product cannot inherit the memory/latency numbers of this in-memory experiment.
