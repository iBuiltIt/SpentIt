# Durable QIF workflow — headless implementation

Last updated: 2026-10-25

This is a partial implementation of [feature 009](../features/009-durable-import-milestone.md), authorized by the owner's Go. It is an optional synthetic-data experiment, not a production financial-document application. The existing sheet is still session-only. No private exports were opened or copied.

## Implemented boundaries

The engine owns the [bounded banking QIF profile](../features/009-qif-profile.md), UTF-8/calendar/exact-money validation, record diagnostics, source identity comparisons, duplicate-decision validation, draft validation and exact aggregation. The application exposes synchronous commands through `ImportRepository` and provides ledger ordering/filtering/reconciliation independent of Qt. Storage implements those atomic commands with pinned SQLCipher. No UI target links this new store.

The `SPENTIT_IMPORT_WORKFLOW` option adds `spentit_import_store`, `spentit_import_view` and tests; it requires `SPENTIT_STORAGE_PROTOTYPE` for the existing pinned SQLCipher build. Engine-only builds include the parser and its tests without Qt or SQLCipher. No new external dependency was selected.

Each document contains one account with a retained currency definition and an explicit Opening Balance. Setup starts with zero, using the date supplied by the application. Confirming even a zero opening balance is required before first commit. A subsequent opening-balance edit increments any pending batch's revision, so an older commit request cannot silently use a changed starting balance. The first commit freezes the opening balance.

Evidence bytes, parser version and date profile are immutable. Valid source records retain their own bytes, source fields and stable occurrence/sequence identity. Saved interpretations and review decisions append immutable versions and ordered allocations; failed saves leave prior versions intact. Batch state transitions and events share the same physical transaction. Discard retains encrypted evidence and removes the projection. Parser diagnostics are deterministically regenerated from the retained bytes using the pinned profile version; unsupported parser/schema versions are rejected.

Automatic duplicate exclusions require an identical committed source file and matching profile. Comparison is directly byte-for-byte; there is no hash-only identity shortcut. Every record occurrence maps independently to an included committed occurrence, following previously recorded exclusions when necessary. Other imports produce review candidates for the same original date, amount and currency; payee/reference agreement distinguishes exact-source-field evidence from broader candidates. Neither candidate basis automatically excludes a row. A prior occurrence cannot be excluded twice within a new batch. The candidate-existence query is cached per date/amount group during ingestion to avoid repeated scans without merging occurrences.

Commit revalidates every included interpretation and excluded relationship against the expected batch revision. It atomically records the receipt, publishes the whole selected batch, retains prior revisions/history, and freezes opening setup. An operation identity and expected revision identify immutable content; conflicting identity reuse fails. After an uncertain outcome, query that identity or retry the identical command. A callback exists solely for testing interruption before/after durable commit.

The ledger query uses financial date followed by permanent sequence, placing the Opening Balance first on its date. Sorting and literal filtering retain chronological balances. Filtered withdrawals remain signed internally, avoiding negation overflow at the signed minimum; presentation can show their magnitude. The full category-report policy remains deferred. A blocked or unresolved batch has no authoritative projection; requesting its projected ledger fails explicitly rather than returning an apparently complete subtotal.

## Storage and locking

The new format has application ID 1397770577 and schema version 1, distinct from the older storage prototype. There is no migration or reinterpretation of prototype documents. SQLCipher 4.19.0 settings, OpenSSL provider, DELETE journals, EXTRA synchronization, fullfsync, foreign keys, memory-only temporary storage and exclusive backup publication follow the [earlier storage experiment](storage-prototype.md). SQLite's value bound is increased to accommodate the accepted 64 MiB source cap; parser and draft limits still apply separately.

A separate empty `.lock` file carries an exclusive kernel `flock` for the document session. It is not deleted or used as an age-based ownership claim: the kernel releases ownership after process exit. A live holder is never overridden. This is separate from SQLite's durability locks because the macOS filesystem may itself use flock for database locking. Symlink opens and multiply linked document/lock files are rejected. The empty lock file may remain after closing or a failed open; it contains no financial data.

Application-wide instance locking is **not required**. The owner clarified that each running application has one active dataset, selected by a path in Settings, and may close it and open another. A user may own multiple datasets. Exclusive document locking protects a dataset from simultaneous writers; it does not restrict running copies of the application. This target remains POSIX-only; Linux is unverified and Windows is not supported by this experimental storage build.

Backups use the supported encrypted database backup API into a new `.partial` candidate, validate and reopen it, and publish without replacing an existing destination. Restore uses the same API into a new document. Failed candidates and their empty lock files may remain; they are not acknowledged backups. Wrong-password or unsupported-schema errors do not replace the source document. Creation failure may leave an incomplete new file; recovery uses a new destination, never automatic deletion of an existing document.

## Build and test

Prepare the pinned SQLCipher source as described in the storage-prototype guide, then:

```sh
cmake -S . -B build/import-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DSPENTIT_STORAGE_PROTOTYPE=ON -DSPENTIT_IMPORT_WORKFLOW=ON \
  -DSPENTIT_SQLCIPHER_SOURCE="$PWD/build/dependencies/sqlcipher-build"
cmake --build build/import-debug
ctest --test-dir build/import-debug --output-on-failure -j 3
```

Use separate Release and Debug+`SPENTIT_SANITIZERS=ON` directories. A compiled, synthetic benchmark is available as `tests/storage/spentit_import_tests benchmark 100000` within the build directory; repeat with 200000. It measures ingestion, commit and ledger loading, not GUI responsiveness or total application startup.

`SPENTIT_QIF_FUZZER=ON` builds a libFuzzer harness when a compatible Clang runtime and sanitizers are available. The local AppleClang installation lacks `libclang_rt.fuzzer_osx.a`; the initial link attempt failed, and configuration now rejects that unsupported combination explicitly. Coverage-guided fuzzing was therefore not run. Deterministic 10,000-case parser mutation tests are separate and do run under the normal sanitizer suite.

## Verification record

Tests use isolated temporary directories and synthetic data. No milestone-wide completion claim follows from these headless checks.

Actual verification on 2026-10-25: macOS 27.0.1 arm64, AppleClang 21.0.0, pinned SQLCipher 4.19.0 and OpenSSL 3.5.9. The final Debug, Release and ASan/UBSan configurations each built and passed **31/31 CTest entries** (13 engine/parser, eight original storage, nine durable-import storage and one application ledger test). Final builds emitted no compiler warnings, and no sanitizer diagnostics appeared. Test-log scans found none of the new synthetic private-data/password markers.

The new tests cover exact draft persistence, original/effective totals, occurrence multiplicity, broader possible matches, exclusion overrides, unresolved-review blocking, stale revisions, explicit opening confirmation, commit identity conflicts, retained discarded evidence, unsupported transfers, malformed authenticated state, wrong passwords, exclusive document access, restored backups and ledger reconciliation.

| New operation | Observed write/sync fault positions |
|---|---:|
| Ingest evidence and initial drafts | 69 |
| Save a draft revision | 41 |
| Discard a batch | 25 |
| Commit a batch | 41 |
| Create/validate backup candidate | 120 |

For the first four operations, each observed position was tested both with an abrupt subprocess exit and with one injected failure (alternating disk-full/write-I/O; intercepted syncs return sync-I/O). Commit also has explicit immediately-before/immediately-after-durable-boundary exit tests. Backup positions received the failure-injection sweep, with prior-backup byte comparison and restored reopening; abrupt exits in the new backup wrapper were not swept. These are actual bounded paths, not exhaustive combinations. Private markers were checked in relevant temporary files before recovery where applicable.

Successful single-process Release measurements on macOS arm64 (one run per workload, no concurrent builds/tests during the measurements):

| Account entries | Ingest | Commit | Full ledger load |
|---|---:|---:|---:|
| 100,000 | 23,643 ms | 1,414 ms | 1,593 ms |
| 200,000 | 47,655 ms | 2,819 ms | 3,191 ms |

The synthetic workload uses same-date, equal-amount records with distinct references. It exercises retained occurrence multiplicity and a large candidate group; it is not a varied bank dataset. These figures include SQLCipher operations and exclude UI rendering, reopening/full verification, source-file reading, backup and memory measurement. They are baselines, not a responsiveness claim. Earlier exploratory runs were interrupted without completed measurements; inspecting the hot paths led to per-group candidate caching and a separate evidence table so metadata queries no longer traverse large source blobs. Prepared-statement reuse, batched ledger loading and further profiling remain worthwhile before UI performance claims.

The existing storage-independent sheet Debug build also passed 17/17 entries after the engine additions. Coverage-guided fuzzing remains unavailable as described above; the deterministic mutation suite is not a substitute for an extended fuzz campaign.

## Remaining work

- Writable Qt integration: bounded file reading, create/unlock/import/review interfaces, source/diagnostic presentation, duplicate selection, inline editing, Save / Discard / Stay, opening confirmation, receipts, and async document/session isolation.
- Optional-password setup, reserved public-key automatic unlocking and installation-independent recovery verification (see [accepted document behavior](../features/001-documents-storage-security.md#accepted-setup-unlock-and-recovery-behavior--2026-10-25)). The current API still requires an explicit password.
- Settings dataset-path selection, close/switch/cancellation coordination, worker lifetime and user-facing recovery. The synchronous repository must be used serially on a worker.
- Interpretation repair for malformed records. Valid records can be edited; blocked imports currently support evidence inspection and discard/reimport, not per-record diagnostic resolution. They cannot be partially committed.
- End-to-end UI and restart/backup interactions, accessibility and supported-platform validation. Large-workload measurements here cover only headless commands.
- Broader QIF dialects, QFX/ASO/QBO/OFX, rules, transfers, category reports, committed editing/reversal, password changes, migrations and final architecture/license decisions remain outside this milestone.

Fault injection covers the enumerated synthetic paths, not physical power removal, every filesystem, all error combinations, or a universal absence of leaks. Backup filesystem publication/directory-sync errors and abrupt exits during the new backup wrapper remain separately unverified; the older prototype's backup crash tests do not certify this wrapper.
