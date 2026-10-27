# Building and verifying the engine

Last updated: 2026-10-27

## Prerequisites

- CMake 3.21 or newer (preset schema version 3).
- Ninja available on PATH.
- A C++20 compiler and standard library: AppleClang/Clang, GCC, or MSVC. Only the AppleClang version below has been exercised.
- For the optional sanitizer preset: Clang or GCC on a non-Windows platform with AddressSanitizer and UndefinedBehaviorSanitizer runtimes.

On macOS, use an installed Xcode or Command Line Tools compiler. On Windows, run from a developer command prompt with the compiler environment initialized. Engine-only builds have no fetched dependencies, Qt/SQLCipher requirements, or test-framework downloads. The optional storage prototype has separate [dependency/build instructions](storage-prototype.md). Python is not needed to build or test.

## Commands

From the repository root:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug

cmake --preset release
cmake --build --preset release
ctest --preset release

cmake --preset sanitized
cmake --build --preset sanitized
ctest --preset sanitized
```

Each preset uses a separate ignored `build/<preset>/` binary tree, outside the source-code directories. Root in-source configuration is rejected. For output entirely outside the checkout, use `cmake -S . -B /path/to/build -G Ninja -DCMAKE_BUILD_TYPE=Debug`, then `cmake --build /path/to/build` and `ctest --test-dir /path/to/build --output-on-failure`. Machine-specific overrides belong in ignored `CMakeUserPresets.json`.

The build produces the static `spentit_engine` library (alias `SpentIt::engine`) and `spentit_engine_tests`. Engine-only builds have no application executable; the optional [synthetic sheet prototype](sheet-prototype.md) has separate Qt build instructions. `BUILD_TESTING=OFF` disables the test target. Tests use explicit runtime checks, so Release does not disable them via `NDEBUG`.

## Engine verification through historical definitions — 2026-10-25

Host: macOS 27.0.1, arm64; AppleClang 21.0.0; CMake 4.4.4; Ninja 1.13.2.

| Configure / build / test preset | Result |
|---|---|
| `debug` | All three commands succeeded; 9/9 test groups passed |
| `release` | All three commands succeeded; 9/9 test groups passed |
| `sanitized` | All three commands succeeded; 9/9 test groups passed; no ASan/UBSan diagnostics |

No compiler warnings were emitted. Test groups are decimal input, arithmetic, splits, corrections, boundaries, currency catalog, editing, editing boundaries, and retained historical currencies. Boundary coverage includes signed minimum/maximum, overflow during decimal scaling, all 24 permutations of large cancelling terms, and 40,401 small signed operand pairs for addition/subtraction. Editing tests additionally cover 775 small total/entered-amount combinations and extreme signed remainders. Catalog tests exercise every one of the 165 entries, scale enforcement, and representative excluded codes. `python3 tools/generate_currency_catalog.py --check` passed against the pinned SIX snapshot. Python 3 is optional for this offline catalog-maintenance check; normal builds do not require it. Fixtures are synthetic and contain no personal financial records.

Linux, Windows, other compiler versions, and the stated minimum CMake version have not been run. Sanitizer success applies only to these exercised paths. These engine-only runs do not verify persistence, I/O failure, interruption/retry, encryption, or UI. The later optional storage verification is recorded separately below. There is no parser fuzzing campaign or resource benchmark. The decimal parser is linear in the supplied string size and uses constant auxiliary space; a future import layer must bound input sizes.

Local Markdown link targets were checked for existence. Git diff/status checks were unavailable because this workspace has no Git repository metadata.

See [project status](status.md) and [feature 002 decisions](../features/002-accounts-money-splits.md) for implemented scope and unresolved product behavior.

## Historical-definition and storage follow-ups — 2026-10-25

The engine now has nine groups, including historical-definition coverage. Debug, Release and ASan/UBSan configure/build/test runs passed with no warnings or sanitizer diagnostics. `python3 tools/generate_currency_catalog.py --check` verified 165 current choices and 167 retained definitions. `python3 tests/tools/currency_refresh.py` passed its compiled simulated CAD-withdrawal regression.

The optional SQLCipher target adds eight storage groups. Its exact dependency pins, commands, actual results, recovery behavior and platform limits are recorded in [storage prototype verification](storage-prototype.md). The earlier statement about no persistence verification applies to the original engine milestone, not this later optional prototype.

## Synthetic sheet follow-up — 2026-10-25

The optional Qt Quick target and its 13-test Debug/Release/ASan/UBSan results, native scrolling measurements and workload limits are documented in [sheet prototype verification](sheet-prototype.md). These do not add Qt to engine-only builds or establish a production application.

## Optional bounded rules engine

With a development installation of PCRE2 10.49 (8-bit Unicode library) discoverable through pkg-config:

```sh
cmake -S . -B build/rules-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSPENTIT_RULES=ON
cmake --build build/rules-debug
ctest --test-dir build/rules-debug --output-on-failure
```

This builds the ordinary engine/parser tests plus regex, deterministic rule evaluation and pure application-preview tests. It does not build the import store, so it does not save or apply rules and it does not build a rules editor. The core engine alone still requires no PCRE2 dependency. See [dependency provenance](../third_party/pcre2/README.md) and [scope/resource contracts](rules-engine.md). Packaging that dependency and other-platform validation remain unfinished.

Schema 6 Save and Apply are built by adding `SPENTIT_RULES=ON` to the durable-import configuration (`SPENTIT_STORAGE_PROTOTYPE=ON` and `SPENTIT_IMPORT_WORKFLOW=ON`, with the pinned SQLCipher source). That configuration registers the `rulestore.*` tests. The same import configuration with `SPENTIT_RULES=OFF` still creates schema 5 and registers `import.schema_gate`. The standalone `build/rules-debug` tree above does not build the import store or the sheet.

The rules sheet is a separate import-UI tree with both `SPENTIT_IMPORT_UI=ON` and `SPENTIT_RULES=ON`. Do not turn rules on in `build/import-ui-debug`: that tree stays rules-off and still creates schema 5. The prefixes below are the same Homebrew examples as the [import UI build](import-ui.md).

```sh
cmake -S . -B build/import-ui-rules-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/opt/homebrew \
  -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3 \
  -DSPENTIT_STORAGE_PROTOTYPE=ON -DSPENTIT_IMPORT_WORKFLOW=ON \
  -DSPENTIT_IMPORT_UI=ON -DSPENTIT_RULES=ON \
  -DSPENTIT_SQLCIPHER_SOURCE="$PWD/build/dependencies/sqlcipher-build"
cmake --build build/import-ui-rules-debug
ctest --test-dir build/import-ui-rules-debug --output-on-failure -R '^(import.rules_ui|import.category_ui|import.qml)$'
```

That rules-on Debug invocation passed `import.rules_ui`, `import.category_ui`, and `import.qml`. The rules-off Debug tree `build/import-ui-debug` then passed `import.ui`, `import.navigation`, `import.recovery_ui`, `import.committed_ui`, `import.category_ui`, and `import.qml`. Times are in [project status](status.md). Release and sanitizer runs of schema 6 and of the sheet are not recorded.

## Search and bulk follow-up — 2026-10-26

Import-workflow builds now require pinned PCRE2 10.49 for search, even when `SPENTIT_RULES=OFF`. Engine-only builds remain dependency-free. The search checkpoint introduced schema 7 for atomic bulk changes. Current formats are described in the [account follow-up](#account-scoped-follow-up--2026-10-26); this pre-release phase has no migration obligation.

Use the import-UI configuration above with binary directories `build/search-debug`, `build/search-release`, and `build/search-sanitized`. Select `CMAKE_BUILD_TYPE=Debug`, `Release`, and `Debug` respectively; set `SPENTIT_SANITIZERS=ON` for the last. Keep all the same storage/import/UI/rules flags and dependency paths. `cmake --build <directory> -j 3` builds each tree. The new registered checks are `import.search`, `rulestore.bulk`, `rulestore.bulk_io`, and `rulestore.bulk_crashes`; import UI tests also cover selection, source search, bulk previews and recovery.

The current-format verification ran 60 selected cases using:

```sh
ctest --test-dir build/search-release --output-on-failure -j 2 \
  -E '^(storage[.]|import[.](io|crashes|backup_failures|backup_crashes)$|committed[.](io|crashes)$|category[.](io|crashes)$)'
```

The same selection was run in the sanitizer tree. The excluded older fault sweeps had already passed in the full 72-case schema-6 baseline; the new bulk and rules fault sweeps are included. Do not describe this as a full 76-case schema-7 suite. Final targeted rebuilds/reruns and the corrected sanitizer-test issue are recorded in [project status](status.md).

`build/search-release/src/app/spentit_search_benchmark 100000` (or `200000`) checks and times synthetic in-memory application queries, including copy/sort/filter/totals. It excludes database/source-file I/O and Qt rendering. See [results and limits](status.md).

## Account-scoped follow-up — 2026-10-26

Use the same rules-enabled Debug, Release and ASan/UBSan configurations above. Current format behavior and limitations are in [accounts](accounts.md); actual results are in [status](status.md#account-scoped-durable-workflow--2026-10-26). The rules-off build uses schema 9, while rules-enabled builds use schema 8. Neither opens earlier experimental layouts.

The selected functional run was:

```sh
ctest --test-dir build/import-ui-rules-debug \
  -E '(\.io|\.crashes|_io|_crashes|backup_failures|backup_crashes|storage\.)' \
  --output-on-failure -j 3
```

Equivalent Release and sanitizer invocations used their respective build directories. The new account failure sweeps are `accounts.io` and `accounts.crashes`; `accounts.basics` and `accounts.validation` cover isolation/replay and corruption. They are not substitutes for platform validation or the full existing fault suite. The final affected account/UI invocation selected:

```sh
ctest --test-dir build/import-ui-rules-release \
  -R '^(accounts\.|import.(ui|navigation|recovery_ui|committed_ui|category_ui|rules_ui|qml|benchmark_smoke)$)' \
  --output-on-failure -j 3
```

## Durable transfer follow-up — 2026-10-26

Current import formats are schema 12 with `SPENTIT_RULES=ON`, and schema 13 without rules. Existing configured import/UI build directories remain usable; rebuilding reconfigures the new engine and storage test groups. No migration is supplied for earlier experimental datasets.

The transfer storage executable registers `transferstore.basics`, `transferstore.imports`, `transferstore.splits`, `transferstore.manual_evidence`, `transferstore.validation`, `transferstore.io` and `transferstore.crashes`. Run these alongside the existing account/import, committed-change, category/rules, UI and navigation checks after changing storage. The I/O and crash groups sweep ten complete operations, rather than individual SQL statements. `import.ui` includes transfer lifecycle, unknown-outcome and counterpart-navigation cases. Use `SPENTIT_TRANSFERS_SCREENSHOT=/absolute/output.png` with its `transferWorkflow` QtTest function for a synthetic offscreen rendering check.

Actual configuration results and limitations are recorded in [project status](status.md#durable-transfer-integration--2026-10-26). Test registration and example commands are not evidence of a passing run.

## QIF destination follow-up — 2026-10-26

Rebuild the existing import UI trees for parser `bank-v2` and schemas 12/13. `transferstore.qif` adds destination-resolution checks; existing transfer fault sweeps use bracketed evidence. The parser, rules preview, import errors/repair and UI suites also cover the extension. See [scope](qif-transfers.md) and [actual results](status.md#qif-transfer-destinations--2026-10-26).

## Whole-source QIF follow-up — 2026-10-26

Point 1 rules-enabled import builds used experimental schema 16; rules-off builds used 17. Those formats are now rejected without migration. The statement-import section below is the current writer. The existing `build/import-ui-rules-debug`, `build/import-ui-rules-release`, `build/import-ui-rules-sanitized` and `build/import-ui-debug` configurations remain usable after rebuilding. These paths name local generated build trees, not committed artifacts.

`accounts.groups` covers the grouped-source workflow. `accounts.group_io` and `accounts.group_crashes` sweep seven operations, including staged-opening publication through both account and transfer commits; their 1800-second test budget retains all measured positions. `import.ui` includes the mapping/review/Abort flow, cancellation, acknowledgement recovery, setup-currency defaults and mapping-delegate recreation. Environment variables `SPENTIT_GROUP_MAPPING_SCREENSHOT` and `SPENTIT_GROUP_REVIEW_SCREENSHOT` optionally save synthetic offscreen renders during its `multiAccountImport` slot. See [current verification](status.md#whole-source-qif-account-review--2026-10-26) for actual results rather than assuming a configured build passed.

## Current-format hardening — 2026-10-26

The [hardening guide](hardening.md) documents the coverage-guided QIF target, pinned optional libFuzzer runtime and synthetic boundary-corpus generator. It also distinguishes current combined-recovery, accessibility and native workload checks from earlier prototype results. Point 2 excludes Linux/Windows validation at the owner’s request.

## Statement imports — 2026-10-27

Current rules-enabled import builds use experimental schema 20; rules-off builds use 21. Schemas 18 and 19, and every other unexpected `user_version`, are rejected with `Unsupported experimental format; no migration was attempted`. The point-3 sentence that named schemas 18 and 19 describes that checkpoint. Account-command verification is in [status](status.md#point-4-account-management--2026-10-27). Rebuild the existing `build/import-ui-rules-debug`, `build/import-ui-rules-release`, `build/import-ui-rules-sanitized`, and rules-off `build/import-ui-debug` trees after this checkpoint. `statement.parser` covers the engine grammar, including the synthetic ASO credit-card fixture. `statement.review` and `statement.faults` cover associations, refusal, backup restore, and the new commit write path. `statement.review` includes the `aso` label, shared source tables, and a second import of the same FITID. The optional `spentit_statement_fuzzer` target follows the existing libFuzzer option and is not part of the default import-ui caches. See [verification](status.md#point-3-statement-imports--2026-10-27).
