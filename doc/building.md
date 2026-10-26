# Building and verifying the engine

Last updated: 2026-10-25

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

Schema 6 Save and Apply are built by adding `SPENTIT_RULES=ON` to the durable-import configuration (`SPENTIT_STORAGE_PROTOTYPE=ON` and `SPENTIT_IMPORT_WORKFLOW=ON`, with the pinned SQLCipher source). That configuration registers the `rulestore.*` tests. The same import configuration with `SPENTIT_RULES=OFF` still creates schema 5 and registers `import.schema_gate`. Neither configuration builds the rules sheet. Debug results for the schema 6 tests are in [project status](status.md). Release and sanitizer runs of those tests are not recorded.
