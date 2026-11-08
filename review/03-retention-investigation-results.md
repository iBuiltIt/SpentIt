# Retention investigation results — 2026-11-08

Follow-up: the owner authorized step 1 on 2026-11-08. The legacy path is
retired; see [status](../doc/status.md). Rules and other retention changes remain
deferred. Historical failure reproduction uses commit `c1fbc0b`; the current
driver rejects legacy-mode capture instead.

Status at investigation completion: investigation complete for the bounded workloads below; **no storage
implementation changes authorized or made**. Remediation remains incomplete and
requires a separate owner decision. This report qualifies the original
[storage review](01-session-close-storage-growth.md), rather than accepting its
entire proposed refactor as necessary product work.

## Decision summary

1. **Confirmed correctness defect, limited reach:** the legacy staged-balance API
   can create a file that passes verification immediately before successful close,
   then fails Check file, backup creation and unclean-reopen verification after
   close. Current desktop mapping does not generate that staged-balance path;
   its fixed-opening equivalent passed. Resolve the legacy API contract narrowly:
   either retire that unsupported path or make its retained choice and verifier
   consistent with baseline pruning. Do not retain all ordinary draft revisions
   forever to accommodate it. No evidence establishes a failure of current desktop
   fixed-opening imports.
2. **Confirmed ruleset-size-dependent waste:** saving one changed rule retains a
   complete request/snapshot for every rule. Worth a bounded storage-efficiency
   follow-up for larger rule collections; the measurements do not show an urgent
   financial correctness or normal-close responsiveness problem. Preserve retry
   identity and definitions referenced by live rule marks if addressing it.
3. **Other retention gaps are real but not demonstrated urgent product defects:**
   category, income, exclusion and opening edits consume a few KiB after 32 cycles.
   Review Link/Undo cycles keep unused histories, but verification and subsequent
   import Undo passed. Currency history grows, but most retained payload growth
   is the initial transition, not a new full transaction archive on every close.
   Defer a coordinated baseline cleanup unless policy compliance itself is chosen
   as the objective. These results do not justify a six-part storage refactor as
   an immediate product necessity.

## Method and scope

Rules-enabled macOS arm64 Release build at `8d86b9b`, with the existing uncommitted
OFX validation work. No production storage, financial or lifecycle source changed.
The only added executable is
[`tests/storage/retention_measure.cpp`](../tests/storage/retention_measure.cpp),
registered as an optional measurement target, not a passing-regression test.
It intentionally logs verification failures and continues so that subsequent
backup and recovery behavior can be inspected. Exit zero alone is **not** a pass.

Each main workload starts with 1,000 synthetic transactions and performs 32
changes, each followed by successful certification/close and a fresh open.
Rule workloads contain 1, 32 or 128 rules; each save changes only the first rule's
label. Other workloads rename one category, toggle its income flag, toggle one
exclusion, change an opening by one minor unit, alternate CAD/GBP reassignment,
or repeatedly link an imported bank posting to an accepted generated transfer
and undo that link. The transfer cases include a second account. Balance workloads
use a grouped new-account import, with either legacy staged choices or today's
fixed-zero opening. Their main runs change payee text, leaving amounts unchanged.

Measurements at 0, 1, 8 and 32 cycles distinguish physical file size, freelist
bytes, table counts, and logical value bytes. Every main workload's unchanged
second close preserved all those measurements. Full verification was explicitly
requested around sampled closes and after reopening; it was not added to normal
open/close. Main measurements ran serially without concurrent builds/tests.
The initial sandbox invocation was blocked by the application-data lock directory;
executed workloads used approved access with normal locking intact.

Separate zero-edit controls used identical setup. A read-only SQLCipher `dbstat`
analyzer with the source/query in
[`session_storage_pages.c`](../tests/storage/session_storage_pages.c) measured
actual B-tree payload (including indexes) and allocated/unused space. No VACUUM,
padding, storage SQL mutation, private statement or migration was used.

This measures growth in the current implementation, **not** a new matched run
against `cf8f1a5`. Existing comparison acceptance is neither disproved nor broadened.
Inspection confirms that complete ruleset-save copies and append-only exclusion
history already existed in `cf8f1a5`; these are not established new regressions.
This is a bounded synthetic investigation, not proof of all workloads or platforms.

## Measured growth after 32 changes/closes

All values below are **additional bytes relative to the same workload's setup**.
B-tree payload includes table and index record payload, not unused page space.

| Workload | Physical file growth | B-tree payload growth | Full verification |
|---|---:|---:|---|
| One rule, 32 saves | 0 | 8,827 | Pass |
| 32 rules, 32 saves | 159,744 | 140,794 | Pass |
| 128 rules, 32 saves | 679,936 | 561,782 | Pass |
| Category rename | 0 | 5,044 | Pass |
| Income toggle | 0 | 4,646 | Pass |
| Link-exclusion toggle | 0 | 2,742 | Pass |
| Opening correction | 0 | 5,982 | Pass |
| Currency reassignment, 1,000 entries | 593,920 | 247,195 | Pass |
| Currency reassignment, plus transfer | 598,016 | 249,748 | Pass |
| Review Link followed by Undo | 0 | 38,904* | Pass; subsequent import Undo also passed |
| Legacy balance-choice toggle, unchanged amounts | 0 | 1,373 | Pass |
| Current fixed opening, repeated payee edits | 0 | 332 | Pass |

*The Link/Undo B-tree comparison is taken after both the 32-cycle run and its
zero-cycle control additionally undo the later import. The row-count/value-byte
CSV records the pre-import-Undo checkpoints instead. This distinction avoids
attributing the later import itself to growth. Physical file growth was zero
both before and after that final import Undo.

Raw results:

- [Checkpoint sizes and row counts](retention-measurements-2026-11-08.csv).
- [B-tree page/payload accounting and controls](retention-pages-2026-11-08.csv).
- [Verification, mutation/close timings and failure observations](retention-verification-2026-11-08.txt).

### What the growth means

**Rules:** the 128-rule case ends with 4,224 request rows and 4,224 snapshot rows
(33 saves including setup), although only one rule changed and no rules were
applied. This demonstrates retained copies that are not required by transaction
rule marks in this fixture. The extra physical size is 664 KiB over 32 saves,
roughly 21 KiB per save for this particular short-text ruleset. Longer expressions
and templates would change that figure. Explicit verification was approximately
292 ms at setup and 369 ms at the last checkpoint. These are individual observations,
not repeated latency estimates. Normal final close was approximately 11 ms.

**Small configuration histories:** their rows accumulate as reported, but this
fixture had enough free space that 32 changes did not enlarge the file. That is
not evidence of bounded retention: payload grew by 2.7–6.0 KB including indexes.
There is no observed verification failure in these modes.

**Currency:** on the unlinked fixture, the first change adds 262,144 physical
bytes; by cycle 8 the increase is 573,440 bytes. Cycles 8 through 32 add only
20,480 more physical bytes. Entry revisions remain at 2,000 after the first
change, and amount revisions remain at 2,000. This is not 1,000 newly retained
transaction revisions per successive currency change. With one transfer, amount
rows grow from 2,003 after the first change to 2,034 after the 32nd: a small
per-change transfer remainder remains. The review's append-only account-command
observation is true, but attributing the entire initial increase to unbounded
historical replay would overstate it. Current currency and exact values passed
full verification throughout.

**Review Link/Undo:** all 32 answer/Undo pairs and 32 link-history rows survive
close, alongside compact receipts. Only one transfer version remains. The test
then undoes the later import and checks that the earlier generated counterpart
and exact destination balance of CAD 10.00 remain; verification passes before and
after that close. A separate one-row/32-cycle repetition also creates a verified
backup successfully. A final one-row/32-cycle repetition additionally saves a new
active match, closes/reopens, then undoes the later import. It restores the earlier
generated relationship with the exact CAD 10.00 destination balance and passes
verification before/after close. No import-Undo breakage was demonstrated by these
cycles.

## Reproduced legacy balance failure

Minimal reproduction uses one transaction and ordinary public repository commands;
no SQL edits or malformed data are involved:

1. Prepare a grouped import into a newly created account with reported balance
   100.00 and one -1.00 transaction. Leave `use_reported_opening` absent, invoking
   the legacy staged-review API. Review its balance, verify and close.
2. Reopen, correct the transaction to -1.01, review the balance as a closing
   balance, verify and close. Verification still passes.
3. Reopen, correct the transaction to -1.02, review the balance as an opening
   balance. Verification **passes immediately before close**.
4. Successful close prunes the intermediate -1.01 draft version. Its saved balance
   event still exists. Verification immediately after close fails with
   `Reviewed balance change does not replay`.
5. A clean reopen succeeds under the accepted clean-marker policy, and the current
   balance remains exactly -1.02. Check file and backup creation fail with the
   same error. Leaving an opened session uncertified, then reopening, also fails
   the required recovery verification. This is an unclean-marker simulation, not
   a process-kill/fault-injection experiment.

The earlier three-edit repetition reproduced the same failure. The two-edit
repetition establishes the exact before/after-close boundary. No changed amount
or lost transaction was observed; the defect makes an internally inconsistent
history fail checks and can prevent recovery activation.

### Current desktop reachability

`ImportMain.qml` initializes every mapping with `useReportedOpening:false` and
updates that value from its opening selector. `ImportModel` forwards the explicit
boolean into `ImportAccountMapping`. For such accounts `review_import_balance`
refuses later staged choices with “Opening was chosen during account creation;
use Correct opening to change it”.

The identical two amount edits using that fixed-zero mapping passed verification
before and after each close, clean reopening, backup creation and unclean-reopen
verification; the exact current balance was -1.02. A three-edit fixed-opening
repetition also passed. The 1,000-row/32-payee-edit fixed-opening run retained only
one balance event. Legacy API tests in `account_tests.cpp` still exercise the
absent-option path, and low-level callers can invoke it, but no normal current
QML route to that legacy setup was found.

Recommendation: treat this as a concrete internal supported-path correctness gap,
with limited demonstrated desktop exposure. Decide whether the legacy path should
exist before designing its repair. It does **not** justify rewriting all six
retention domains or changing the accepted clean-marker policy.

## Reproduction

Build the measurement target in the existing rules-enabled Release tree:

```sh
cmake --build build/import-ui-rules-release --target spentit_retention_measure -j 2
```

Pass a fresh output directory, mode, transaction count, rule count and cycle count:

```sh
build/import-ui-rules-release/tests/storage/spentit_retention_measure /tmp/retention-rules rules 1000 128 32
build/import-ui-rules-release/tests/storage/spentit_retention_measure /tmp/retention-categories categories 1000 0 32
build/import-ui-rules-release/tests/storage/spentit_retention_measure /tmp/retention-legacy balance-amount 1 0 2
build/import-ui-rules-release/tests/storage/spentit_retention_measure /tmp/retention-fixed fixed-amount 1 0 2
```

Other modes: `income`, `exclusions`, `opening`, `currency`, `currency-linked`,
`links`, `balance`, `fixed`. Use zero cycles in a separate fresh directory for
controls. Synthetic password is `synthetic-storage-comparison`; never use this
utility with private financial data. The final driver also attempts a checked
backup for each workload and creates a final active match before the link-mode
import Undo. The first main run predates those additional observations. Later
boundary/control/link runs executed the backup check, and the final active-match
run executed the stronger Undo check, as recorded in the logs. Backups were
verified during creation; no separate restore workflow was exercised here.

No crash/disk-full injection, native UI timing, sanitizer rerun, Linux/Windows
measurement or private-dataset extrapolation is claimed. The correctness failure
is left unfixed deliberately under the owner's investigation-only authorization.

Changed-document relative file links and `git diff --check` passed. The final
measurement target rebuilt without warnings.
