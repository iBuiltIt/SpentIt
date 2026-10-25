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

New documents have application ID 1397770577 and schema version 2 (schema 1 remains readable without migration), distinct from the older storage prototype. There is no migration or reinterpretation of prototype documents. SQLCipher 4.19.0 settings, OpenSSL provider, DELETE journals, EXTRA synchronization, fullfsync, foreign keys, memory-only temporary storage and exclusive backup publication follow the [earlier storage experiment](storage-prototype.md). SQLite's value bound is increased to accommodate the accepted 64 MiB source cap; parser and draft limits still apply separately.

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

- The optional [durable UI](import-ui.md) now implements setup/unlocking, bounded file reading, source previews, duplicate selection, durable draft editing, opening confirmation, receipts, close/reopen and backup/copy. Inline editing and parsed source-field copying are implemented. Simultaneous read-only split expansions are now implemented; full navigation/selection behavior remains. Bounded native large-UI measurements are now in the [performance report](import-ui-performance.md).
- The UI gateway implements default-on optional protection and a public raw key with password-domain separation. Installation-independent SQLCipher recovery is tested; older direct-headless credential APIs remain unchanged. See the [credential convention](import-ui.md#portable-credential-convention-v1).
- Settings stores the selected dataset path. All operations use one worker and switching is disabled while busy. Mid-operation cancellation and broader recovery UX remain incomplete.
- Bounded date and split-total repair is implemented in schema 2, as described below. Other malformed records remain blocked for discard/reimport. There is no partial commit or migration of older datasets.
- Bounded UI/restart/backup tests now exist; accessibility and supported-platform validation remain incomplete. Large-workload measurements in this document cover only headless commands; the [durable UI report](import-ui-performance.md) records separate native measurements.
- Broader QIF dialects, QFX/ASO/QBO/OFX, rules, transfers, category reports, committed editing/reversal, password changes, migrations and final architecture/license decisions remain outside this milestone.

Fault injection covers the enumerated synthetic paths, not physical power removal, every filesystem, all error combinations, or a universal absence of leaks. The import wrapper now has its own backup crash and publication-boundary tests; see below. The older prototype's backup tests remain separate.

## Backup interruption recovery

Backup publication links the validated encrypted `.partial` file to a new destination, syncs the directory, removes the temporary name, then syncs again. Success is returned only after those steps. An interruption or failure can leave an incomplete candidate, a validated candidate, or a published destination whose durability has not been acknowledged. Do not assume that the mere presence of the destination means success.

Between link and cleanup, the candidate and destination name the same inode. Both are intentionally rejected by normal document opening because they have multiple hard links. Preserve them. With the backup operation stopped, copy one retained file to a **new, separate file** outside the checkout, then open that copy with the compatible application and its original credentials for full integrity and financial verification. Do not copy a live source database, remove links automatically, overwrite an existing backup, or weaken hard-link validation. If the copy does not validate, retain the files and use the previous verified backup or create a fresh backup from the verified source into a new destination. A leftover `.partial` can prevent retry at the same destination; use a new destination.

The new tests inject exceptions or process exits before publication, before the first directory sync, before cleanup, before the final directory sync, and after completion but before caller acknowledgement. Pending and committed snapshots are covered. They check unchanged source/prior-backup bytes, exact evidence and balances, commit receipt/retry identity, validation of a separate recovery copy, successful retry to a new destination, and synthetic marker scans. These boundary exceptions model failed filesystem steps; they do not force the OS itself to return every possible link/unlink/fsync error or model physical power loss.

Verification on 2026-10-25, on the same macOS arm64 toolchain recorded above: the pre-change Debug suite passed 33/33 entries. After adding the backup observer and tests, Debug, Release and ASan/UBSan each passed the five affected entries (`import.backup_failures`, `import.backup_crashes`, `import.backup_publication`, `import.ui`, `import.qml`). Each configuration exercised 120 SQLite write/sync failure positions, 120 abrupt-exit positions and 20 publication-boundary cases. The failure sweep now requires each injected fault to fire and the operation to report failure. No compiler warnings or sanitizer diagnostics were emitted. That verification stage registered 35 entries; the repair verification below supersedes this count.

## Bounded record repair

Schema 2 permits an explicit first saved interpretation for records whose only errors are malformed dates and/or mismatched split totals. The original `T` amount must have parsed exactly. Missing fields, invalid amounts, transfer/class categories, unknown fields, malformed split structure, encoding and file-level errors remain unsupported. The parser retains its first diagnostic and independently checks every later error before enabling repair. Evidence metadata is not a validated transaction.

The existing atomic `save` command inserts the immutable record and its first validated interpretation, allocations, unresolved duplicate disposition and save-history event in one transaction. The permanent ledger sequence is allocated at that first successful save. The source amount, original date (empty when invalid), payee, reference and exact source bytes remain immutable. Subsequent edits append revisions. A first repair cannot overwrite an existing revision or change the original amount; it may explicitly correct the effective total with exactly matching allocations.

The batch becomes structurally unblocked only when all records have validated interpretations and there is no file-level diagnostic. Each newly repaired record still requires explicit duplicate review, even when no candidates exist. Candidate queries use original amounts and dates, never corrected totals/dates. If either original date is unavailable, same-original-amount occurrences are uncertain candidates with an explicit explanation; only a user-selected occurrence can be excluded. Occurrence multiplicity and whole-batch commit checks still apply. Reimporting identical malformed bytes does not silently reproduce repairs or exclusions.

Verification reparses immutable evidence, validates all saved revisions, checks the first repair's save event and unresolved disposition, reconciles blocked state and validates original fields. Committed snapshots, receipts and backup copies retain the same repair history. A failed save leaves either the prior state or the complete saved repair, and revision checks reject stale retries rather than overwrite newer work.

Schema 1 remains readable with its prior editing/commit commands. It cannot accept a first repair, is not upgraded implicitly, and backup preserves its schema. Create a separate schema-2 experimental dataset to exercise repair. Schema-2 files retain the existing application ID, parser profile and credential conventions; older binaries reject their version before interpreting repaired records. This is not a production migration mechanism.

Repair verification on 2026-10-25: Debug, Release and ASan/UBSan each built and passed **36/36 CTest entries** on the recorded macOS arm64 toolchain. After final UI-label/screenshot and unknown-original-date candidate assertions, the three affected entries (`import.repairs`, `import.ui`, `import.qml`) were rebuilt and passed again in all configurations. No compiler warnings or sanitizer diagnostics were emitted in the successful final builds/runs. Initial compilation found a missing local flag declaration; it was corrected before these results. A test invocation against the earlier binary was superseded by the rebuilt runs.

New coverage includes invalid-date and split-total repair, original-total preservation, invalid/stale save rejection, later unsupported fields masked by the first diagnostic, file-level blockers, required duplicate decisions, uncertain original-date matching in both import orders, saved-repair restart, pending/committed backup restoration, receipt retries, and refusal to repair a schema-1 fixture without changing its file. The repair-save operation was added to the write/sync fault and process-exit sweeps (57 positions each in Debug and ASan/UBSan), plus explicit exits immediately before/after its durable commit. The rendered QML repair editor was inspected. Native pointer/accessibility review, Linux/Windows and physical power loss remain unverified. No private inputs were accessed.
