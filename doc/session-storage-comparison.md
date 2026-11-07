# Session-storage correction: measured comparison

Date: 2026-11-07. State: **lifecycle and storage acceptance met**.

## Commits and approach

- `f8a1f4ba344e201586cd023f457c6a3a8604e254` introduced the failed
  session-pruning implementation, including permanent `snapshot_facts`, allocation
  tombstones, compact command receipts, session markers and close-time pruning.
- `cf8f1a5` is its immediate parent and the comparison baseline. It retains ordinary
  history. Its own change is an unrelated manual-transaction test wait.
- `0edf528` and `68b73f5` are earlier Undo/audit performance changes. Preserve those,
  `9fa2166`'s Settings/navigation changes, and unrelated earlier work.

Correcting `f8a1f4b` in place is cleaner than reverting it wholesale: its atomic
close boundary, failed-close handling and retry foundation remain relevant. The
permanent facts and historical-replay architecture must be replaced. The correction replaces the permanent-revision layer while preserving the useful
atomic close and retry foundation. No revert, reset or Git-history rewrite was used. An isolated `git archive cf8f1a5` supplied the comparison build.

Inspection of the failed implementation found:

- `session_snapshot.inc` writes a fact for every draft/committed revision.
  `session_prune.inc` marks all facts as baseline but never deletes them when
  their ordinary payload expires. It also leaves ordinary ledger operations.
- `verify_committed` still replays ledger monetary continuity using those facts.
  Currency attribution in `account_money.inc`, historical duplicate stamps and
  provenance checks also depend on old ordering/operation records.
- `connect` still hashes/audits clean files under the old cache policy. `certify`
  publishes the session boundary without an audit, but does not complete the
  accepted opening policy.
- Cross-session import rollback consumes transfer and Review restoration inputs;
  these must be distinguished from ordinary history before pruning their parents.

Deleting only the new table, hiding history, or bypassing the verifier would not
meet the corrected contract. No unavoidable required storage tradeoff has been
established. The measured overhead is a defect to remove, not an accepted tradeoff.

## Reproducible synthetic workloads

`tests/storage/session_storage_measure.cpp` builds as
`spentit_session_storage_measure`. The same driver source was compiled against
both revisions, using macOS Release, rules enabled, the same pinned SQLCipher
source and OpenSSL installation. Baseline format: 58; failed format: 62; corrected format: 64.

Inputs: 1,000 QIF rows per source, each CAD -1.00, one allocation, a fixed valid
date, and distinct synthetic payee/reference values. Zero opening balance and no
private files. Each workload uses a fresh database and independent output directory.

- `import`: import 1,000 uncommitted rows, verify, successfully close, reopen,
  verify the CAD -1,000.00 balance, and close again.
- `edits`: import and accept 1,000 rows; perform eight payee edits to one accepted
  row in each of four sessions, verifying and closing after each session. Reopen
  and check the unchanged balance. Original implementation retains all 32 edits.
- `undo`: import 1,000 rows and close; reopen, import another 1,000 distinct rows,
  check CAD -2,000.00, and close; reopen and Undo the second import, check CAD
  -1,000.00, verify, close, reopen, verify and close.

All six workload runs exited successfully. These checks establish the measured
workloads' exact balances and simple cross-session source Undo; they do not replace
transfer-restoration, conflict, fault-injection or general financial regression tests.

Run each executable with a fresh directory:

```sh
spentit_session_storage_measure OUTPUT_DIRECTORY import 1000
spentit_session_storage_measure OUTPUT_DIRECTORY edits 1000
spentit_session_storage_measure OUTPUT_DIRECTORY undo 1000
```

For the baseline, extract `cf8f1a5` into a separate directory, copy the driver there,
and add the same executable/link target from `tests/storage/CMakeLists.txt`.
Configure Release with `SPENTIT_STORAGE_PROTOTYPE`, `SPENTIT_IMPORT_WORKFLOW` and
`SPENTIT_RULES` enabled and the reviewed `SPENTIT_SQLCIPHER_SOURCE` path. Build both
executables against the same dependency/toolchain settings. Production code in the
comparison checkout is otherwise unchanged. The first sandboxed runs were refused
by the application-data lock boundary; subsequent runs used approved access to
that directory. Locking was not bypassed.

## Physical and retained-page results

Bytes, measured after successful close; page size 4,096. Live allocated bytes are
`(page_count - freelist_count) * page_size`, including partially unused B-tree
pages. Free bytes are wholly reusable pages, not retained financial records.

| Workload point | Before file | Failed file | Before free bytes | Failed free bytes | Before live allocated | Failed live allocated |
|---|---:|---:|---:|---:|---:|---:|
| Import-only, reopened | 1,556,480 | 1,773,568 | 0 | 8,192 | 1,556,480 | 1,765,376 |
| Accepted, before edits | 1,851,392 | 2,174,976 | 0 | 20,480 | 1,851,392 | 2,154,496 |
| After 32 edits / four closes | 1,855,488 | 2,174,976 | 0 | 16,384 | 1,855,488 | 2,158,592 |
| Two imports, before Undo | 2,146,304 | 2,510,848 | 0 | 8,192 | 2,146,304 | 2,502,656 |
| After cross-session import Undo | 2,146,304 | 2,510,848 | 573,440 | 729,088 | 1,572,864 | 1,781,760 |

The failed implementation increases physical size by 13.9% for import-only, 17.2%
after repeated edits, and 17.0% for the import-Undo workload. These are synthetic
results, not reproduction of the owner's approximately 23% observation.

## Actual B-tree payload and unused space

A separate read-only analyzer, `tests/storage/session_storage_pages.c`, was built
from the same pinned SQLCipher amalgamation with `SQLITE_ENABLE_DBSTAT_VTAB`.
It opens only the synthetic driver files with their fixed test password. Production
build flags and database contents are unchanged. Build it with the production
SQLCipher codec, memory-temp, thread, extra-init/shutdown and OpenSSL settings plus
the DBSTAT definition; invoke it with the closed synthetic database path.

DBSTAT payload includes actual serialized table and index records, including
schema records. Unused bytes are space inside allocated B-tree pages. Encryption
reserve, cell pointers and page headers account for the remaining allocated space.
Freelist pages are accounted separately above.

| Final workload | Before payload | Failed payload | Before B-tree unused | Failed B-tree unused |
|---|---:|---:|---:|---:|
| Import-only | 602,151 | 733,549 | 847,483 | 904,294 |
| Repeated edits | 822,085 | 1,035,757 | 867,147 | 926,278 |
| Import Undo | 602,202 | 733,600 | 863,429 | 920,240 |

Import-only adds 131,398 bytes of actual serialized payload. In that workload,
`snapshot_facts` and its primary-key index occupy 26 pages (106,496 bytes), with
82,612 payload bytes. `allocation_origins` and its index occupy 16 pages (65,536
bytes), with 41,612 payload bytes. Thus the regression is not merely a physical
file failing to shrink after deletion.

After 32 edits, the original has 1,032 entry revisions and 33 ledger operations.
The failed implementation has 1,001 entry revisions, **2,032 snapshot facts**,
33 ledger operations, 1,000 allocation-origin rows and 32 command completions.
Ordinary revision facts and monetary operations still grow despite payload pruning.

No VACUUM, padding, artificial growth of the comparator, file rewriting, encryption
changes or deletion of required financial dependencies was used.

## Corrected implementation: final measurements

The same driver, inputs, dependencies and settings were used for corrected format
64. All three primary workloads completed exact-balance checks and explicit verification,
including reopening and checking the result after cross-session import Undo.

| Workload point | Original file | Corrected file | Corrected free bytes | Original live bytes | Corrected live bytes |
|---|---:|---:|---:|---:|---:|
| Import-only, reopened | 1,556,480 | 1,503,232 | 16,384 | 1,556,480 | 1,486,848 |
| Accepted, before edits | 1,851,392 | 1,777,664 | 32,768 | 1,851,392 | 1,744,896 |
| After 32 edits / four closes | 1,855,488 | 1,777,664 | 32,768 | 1,855,488 | 1,744,896 |
| Two imports, before Undo | 2,146,304 | 2,039,808 | 20,480 | 2,146,304 | 2,019,328 |
| After cross-session import Undo | 2,146,304 | 2,039,808 | 544,768 | 1,572,864 | 1,495,040 |

| Final workload | Original payload | Corrected payload | Original B-tree unused | Corrected B-tree unused |
|---|---:|---:|---:|---:|
| Import-only | 602,151 | 569,698 | 847,483 | 831,355 |
| Repeated edits | 822,085 | 760,219 | 867,147 | 857,204 |
| Import Undo | 602,202 | 569,752 | 863,429 | 839,298 |

Physical file size is lower by **3.4%, 4.2%, and 5.0%** respectively. Actual retained
serialized payload is lower by **5.4%, 7.5%, and 5.4%**. All measured points meet
both physical and live-retention acceptance. Free pages are not counted as a
payload saving or disguised as retained records. No shrinking operation was used.

After every edit-session close the corrected file remains 1,777,664 bytes with
1,001 entry revisions, one import-publication ledger operation and 1,000 draft
origins. The only per-command growth is the compact completion receipt count
(8, 16, 24, 32). No snapshot facts or allocation tombstones exist. The original
retains 1,032 entry revisions and 33 ledger operations after the identical edits.

Savings come from removing permanent revision facts and ordinary operation history,
and storing composite-key payload tables WITHOUT ROWID instead of duplicating each
key in a separate index. Current payload metadata contains baseline/currency fields;
these do not survive independently after a payload expires.

## Additional retained-transfer import Undo comparison

The `linked` mode uses 1,000 ordinary rows plus an explicit transfer in the first
import, accepts it, and closes. A second account receives 1,000 ordinary rows plus
the matching bank posting. Review Link matches that posting to the accepted
transfer. After another close/reopen, Undo last import removes the later import
and restores the earlier generated relationship. Both account balances are checked
before/after Undo and after reopening; full verification runs at each stage.
The identical extended driver was rebuilt against `cf8f1a5` and the correction.

| Point | Original file | Corrected file | Original live bytes | Corrected live bytes |
|---|---:|---:|---:|---:|
| Accepted first import | 1,851,392 | 1,777,664 | 1,851,392 | 1,744,896 |
| Matched second import, closed | 2,519,040 | 2,375,680 | 2,519,040 | 2,355,200 |
| Undo completed and reopened | 2,519,040 | 2,375,680 | 2,445,312 | 2,281,472 |

Final reusable freelist bytes: **73,728 original / 94,208 corrected**. Final actual
payload: **1,314,653 / 1,216,681**; B-tree unused space: **892,503 / 884,741**.
Physical storage is 5.7% lower and retained payload 7.5% lower. Required source,
acceptance and relationship restoration inputs are included in these totals.

## Verification and limits

See [status](status.md) for executed lifecycle, financial, retry, import restoration,
failure and sanitizer checks. Storage results above are actual macOS Release
synthetic measurements, not a reconstruction of the owner's private 65/79 MB file.
They do not promise identical percentages for all possible data distributions.
SQLite retains reusable pages, so files do not necessarily shrink at close.
Compact request IDs/digests grow with completed commands by design; transaction
snapshots and ordinary monetary summaries do not grow with expired revisions.

Linux/Windows and native interactive platform behavior are not established by
these measurements. Manual prose and screenshots remain unchanged.
