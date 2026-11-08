# Ruleset-save retention — 2026-11-08

Status: implemented and verified under the bounded ruleset-save Go. The owner
explicitly left all other retention work deferred: category histories, undone
Review links, exclusions and account/opening/currency history. Those items remain
incomplete and require a separate discussion and Go.
The [contract](../features/004-rules-and-explanations.md#ruleset-save-retention--owner-go-2026-11-08)
requires current rules, referenced definitions, retry identity, recovery and import
Undo to survive. The paired storage measurements below justify retaining this bounded change;
final verification is recorded in status.

## Paired measurements

macOS arm64 Release, 1,000 synthetic accepted transactions, 1/32/128 rules,
32 saves changing only the first rule's label. Each save is followed by successful
close and reopen. Independent zero-edit controls use identical setup. Runs were
serial, without concurrent builds/tests, using the same SQLCipher dependency and
`tests/storage/retention_measure.cpp` driver.

Before: the preserved executable from `fa630d1`, format 66. After: the ruleset-retention implementation, format 68. The [raw logs and page accounting](rule-retention-measurements/summary.csv)
include all 12 runs. Raw command output is retained as `.txt` alongside the CSV files. Full verification before/after sampled closes, reopen and
backup creation passed throughout. Measurements report main-file bytes and actual
B-tree payload (including indexes and schema), separately from logical field sizes.
There is no VACUUM, padding, weakened durability, private data or migration.

| Rules | Before file growth | After file growth | Before B-tree payload growth | After B-tree payload growth |
|---|---:|---:|---:|---:|
| 1 | 0 | 0 | 8,827 | 4,362 |
| 32 | 159,744 | 0 | 140,794 | 4,519 |
| 128 | 679,936 | 0 | 561,782 | 4,998 |

The 128-rule final file is **1,748,992 bytes**, versus **2,428,928** before:
**679,936 bytes (664 KiB) saved**. The 32-rule case saves 156 KiB. Neither has
physical growth over its own setup. The one-rule case has no physical saving,
but about half the retained payload growth. Its setup payload is 161 bytes larger
because the compact receipt/schema overhead exceeds the tiny initial request;
its final payload is 4,304 bytes smaller. This is an efficiency improvement for
larger collections, not evidence of an urgent financial correctness problem.

After 33 saves including setup, the 128-rule case retains 128 definitions,
128 baseline members and zero full requests, versus 160 definitions and 4,224
members in each of the request/snapshot tables before. It retains 33 compact
completion digests plus existing operation/order identities. Growth therefore
tracks compact command identities and necessary definitions, not 128 repeated
copies per save. Referenced older definitions are separately tested; the storage
workload intentionally does not apply rules.

One additional unchanged close left all reported sizes/counts unchanged. File free
pages are reusable rather than physically truncated: 32,768 bytes in both one-/
32-rule after cases, 45,056 in the 128-rule after case. Exact totals and zero-edit
control values are in the summary CSV.

## Original storage acceptance

The unchanged `session_storage_measure.cpp` driver was also rerun against the
existing isolated `cf8f1a5` build and this change, with 1,000 rows. Driver sources
were byte-identical. These size checks ran alongside independent tests; no timing
claim is made. Every workload completed its internal verification, exact-balance
and Undo assertions. [Raw totals](rule-retention-measurements/session-summary.csv):

| Workload, final point | `cf8f1a5` file | After file | `cf8f1a5` B-tree payload | After B-tree payload |
|---|---:|---:|---:|---:|
| Import-only | 1,556,480 | 1,503,232 | 602,151 | 569,896 |
| 32 edits / four closes | 1,855,488 | 1,777,664 | 822,085 | 760,417 |
| Cross-session import Undo | 2,146,304 | 2,039,808 | 602,202 | 569,950 |
| Linked import / Undo | 2,519,040 | 2,375,680 | 1,314,653 | 1,216,879 |

Both physical size and retained payload remain below the accepted pre-change
baseline in all four cases. The schema adds 198 payload bytes relative to the
previous corrected acceptance results; physical sizes are unchanged.

## Bounded implementation

- Retain complete requests and ordered snapshots during the current session.
- Successful close keeps one current baseline, including compact deleted-rule
  members, and removes superseded snapshots/full requests.
- Retain definitions referenced by heads, the baseline and all retained draft/entry
  field/allocation marks. This uses the existing transaction dependency pruning.
- Record canonical request digests; exact expired retries report already completed,
  and changed-content reuse refuses without writing.
- Seed verification from the retained baseline, then verify only subsequent save
  transitions. Keep independent structural/provenance and request-digest checks.

The production change is concentrated in rule save/verification and session
pruning, with one snapshot schema field. No new retention framework, normal-open
scan or full-file rewrite was introduced. Formats 68/67 reject older data unchanged.

## Verification and acceptance

Accepted: the savings justify the bounded implementation. All 21 selected Release
checks passed, including the two UI suites, save I/O/crashes and the 121-position
rules-close fault sweep. The final retained-definition/deleted-rule-capacity
regression passed in Release and ASan/UBSan; rules-disabled checks passed 2/2.
Full results, final-rebuild coverage and platform limits are in [status](../doc/status.md).
Manual text/screenshots and other retention cleanup remain deferred.

To repeat one workload, build `spentit_retention_measure` and run:

```sh
build/import-ui-rules-release/tests/storage/spentit_retention_measure /tmp/rules-after-128 rules 1000 128 32
```

Use distinct output directories for each variant/rule count/control. Repeat with
1 and 32 rules, and with zero cycles. Build the same driver against `fa630d1` for
the before comparison. The read-only `tests/storage/session_storage_pages.c`
analyzer requires a separately built SQLCipher with `SQLITE_ENABLE_DBSTAT_VTAB`;
production flags stay unchanged. Do not treat the driver's exit code alone as
success: inspect every `VERIFY`/`BACKUP` result for `FAIL`.
