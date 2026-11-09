# File exchange verification — 2026-11-09

All files and transactions used by these checks are synthetic. No private input
files or manual screenshots were read or changed. The [contract](../../doc/file-exchange.md)
and [status](../../doc/status.md) describe behavior and limits. Initial development
runs exposed fixture mistakes (Income set during category creation, invalid template
syntax, a transfer already resolved automatically, QObject-only lookup of visual
controls) and a result-dialog width binding loop; these were corrected before the
passing runs below. Sandbox-only execution cannot create the platform document
locks; these tests ran with access to SpentIt's ordinary application-data lock directory.

## Final build and suite

```sh
cmake --build build/import-ui-rules-release --target spentit_file_exchange_tests spentit_import_experiment -j4
ctest --test-dir build/import-ui-rules-release -R '^import.file_exchange$' --output-on-failure
ctest --test-dir build/exchange-rules-off -R '^import.file_exchange$' --output-on-failure
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/exchange-sanitized -R '^import.file_exchange$' --output-on-failure
QT_QPA_PLATFORM=cocoa build/import-ui-rules-release/src/ui/spentit_file_exchange_tests menuAndDialogs
```

The additional trees use Ninja, `SPENTIT_IMPORT_UI`, `SPENTIT_IMPORT_WORKFLOW` and
`SPENTIT_STORAGE_PROTOTYPE` enabled, and the same pinned SQLCipher source as the
normal build. `exchange-rules-off` is Release with `SPENTIT_RULES=OFF`;
`exchange-sanitized` is Debug with rules and `SPENTIT_SANITIZERS=ON`.
No ASan/UBSan diagnostics occurred. Leak detection was disabled for Qt GUI testing;
this is not a leak-sanitizer claim.

- [Release](release.log): 1/1, 6.59 s.
- [Rules disabled](rules-off.log): 1/1, 6.15 s.
- [ASan/UBSan](sanitized.log): 1/1, 12.80 s.
- [Native macOS](native.log): menu/dialog smoke, all three Qt test results pass.

The final suite includes normal QIF group reimport with explicit reported-opening
choices and category decisions, exact account balance/count reconciliation with both
transfer sides, and archived-account export with no selected account restored.
The JSON checks use actual SQLCipher-backed catalogs, all-or-none failure outcomes,
first/middle/last injected I/O and disk-full failure points, before/after-commit
process exits, restored-backup verification and matching/conflicting retries.
All imported catalogs are checked with the existing repository verifier.

## Nearby regression checkpoints

```sh
ctest --test-dir build/import-ui-rules-release --output-on-failure -j4 \
  -R '^(import.file_exchange|import.reports|import.reports_ui|import.rules_ui|storage.action_repository|rulestore.definitions|category.catalog|import.backup_cancellation)$'
ctest --test-dir build/import-ui-rules-release --output-on-failure -j3 \
  -R '^import\.(reports_ui|rules_ui|category_ui)$'
```

[Eight regressions](regressions.log) passed in 35.28 s; the subsequent
[three shared-QML UI suites](ui-regressions.log) passed in 31.29 s. Later changes were
bounded to the exchange dialog cancellation wiring, archived-account enumeration,
and the corresponding exchange tests; the final exchange suites above cover them.
The full repository suite was not run. Linux and Windows are unverified, as are
interactive native file-picker selection/overwrite flows and cross-application QIF
compatibility. Native QIF/definition dialog screenshots were inspected from temporary
synthetic captures; these were not added to the manual.


## Export failure follow-up — 2026-11-09

Rebuild `spentit_file_exchange_tests` in each existing build directory, then run:

```sh
ctest --test-dir build/import-ui-rules-release -R '^import.file_exchange$' --output-on-failure
ctest --test-dir build/exchange-rules-off -R '^import.file_exchange$' --output-on-failure
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/exchange-sanitized -R '^import.file_exchange$' --output-on-failure
QT_QPA_PLATFORM=cocoa build/import-ui-rules-release/src/ui/spentit_file_exchange_tests menuAndDialogs
```

Rules-enabled and disabled suites passed in 6.78 s and 6.30 s. ASan/UBSan passed
in 13.89 s with Qt leak detection disabled and no sanitizer diagnostics. Cocoa passed
3 Qt test results in 4.38 s. The menu test deliberately uses Qt's fallback picker,
entering filenames with/without an extension, accepting them, and verifying files
and inline busy/error feedback. This is not OS-native save-panel automation.
It imports synthetic tab/embedded-CR text before exporting two checked accounts.
The text regression independently checks original/current field preservation,
CRLF record endings, split memos, exact money, unchanged Undo, Unicode controls,
invalid UTF-8 and normalized account-name collisions. No private data was read.
The initial added picker test omitted the import destination and therefore had no
transaction to export; specifying that destination fixed the test setup.
