# Replacement Undo implementation and verification

Date: 2026-11-09. State: **implemented; verification complete with the limits below**.

The owner's implementation Go includes the replacement design's explicit scope
choices. The design-session stop is historical. No additional Go is required for
this work. The rejected generic row-capture architecture and its storage-budget
increases stay retired. The Change History layout in
[the handoff](change-history-handoff.md) is implemented; see
[status](status.md). Manual publication remains outside this work.

## Implemented behavior

The repository and application use one durable newest-first action chain, with the
existing document session and mutation revision as the token authority. Working
experimental formats are **70 with rules / 69 without**; older formats are rejected
without migration. Financial Undo buttons and shortcuts use the same head and show
its action, transaction count and affected accounts before confirmation. Text-field
Undo remains native. Saving a dirty editor changes the token and requires a fresh
Undo invocation; it cannot silently reverse the just-saved edit. Surviving ledger
selection and viewport position are preserved. Independent rule/report/category
definition drafts remain untouched.

Typed adapters cover transaction creation/edit/repair/selection/deletion, Review,
transfer assignment and matching, rule application, opening/currency correction,
acceptance and import. Existing transaction snapshots and domain restoration logic
supply inverse values. Consumed, prior and restored revision identities establish
lineage; matching values alone do not authorize overwriting newer work. A two-sided
accepted transfer restores both account contributions inside the same atomic Undo.
Standalone legacy correction/Undo APIs obey the common head. Semantic no-ops add no
step: editor paths retain compact retry receipts; unchanged standalone corrections
and bulk requests refuse before writing.

Import creates one action root, with no ordinary edit members for new rows.
Preparation/category decisions block unrelated saved commands and successful close.
Interrupted capture is cleaned up; cancelled IDs are terminal. Completed import
rollback retains created accounts, categories, aliases and explicit opening choices.
Older-import removal and account Discard require an empty chain. Successful close
expires ordinary history; crashes, failed/cancelled close and open-session backups
retain recovery. Compact source-removal/cancellation receipts resolve lost replies.

Save and apply saves the definition and applies transactions in one transaction.
Its Undo reverses the application and retains the saved definition. Compatible
settings and definition saves preserve the chain while invalidating previews;
account archive/deletion and category dependencies remain guarded.

## Verification record

All data below are synthetic. No private statements were read, copied or modified.
The rules-enabled Release application builds on macOS. The replacement coordinator
and repository integration passed with rules enabled, rules disabled, and ASan/UBSan.
Repository scenarios cover repeated pending/accepted edits, legacy routes, imports
between edits, repeated acceptance, currencies, generated-source relationships,
initial category decisions, first repair, retained setup and composed rule saves.
Forged restoration membership is rejected during recovery.

The expanded acceptance Undo sweep passed injected write/sync failures and abrupt
process exits for ordinary transactions and both accepted sides of a manual
transfer. The composed Save and apply sweep passed the same boundary exploration,
checking atomic definition/application results and retained definition after Undo.
The older-import Undo fault sweep passed, including linked imports and exact retry
after lost acknowledgement. Existing acceptance, review-policy, committed editing,
session pruning, close-marker, backup and corruption regressions also passed.
The full import UI suite passes. The final category/account-lifecycle/report group
passed 21/21, and the rule/Review/report group passed 22/22 (including bulk I/O).
Editor, selected-edit and linked-deletion I/O/crash sweeps passed, including exact
retry and before/after durable-boundary outcomes. The common Undo command is
registered in diagnostics and reports a saved Undo honestly if screen refresh
fails. Collector, transport, QML and common-action integration checks passed 5/5.
The Help suite passes after its fixtures were updated for explicit Undo confirmation,
the existing automatic survivor policy, and the existing scoped report checklists.
The final Review sweep passed all eight modes (1,055.59 seconds), covering injected
write/sync failures, exact retries, restored backups, encrypted storage and process
interruptions before/after durable commit. Its paired Help run also passed
(110.42 seconds).

The initial verification found four legacy account fixtures failing on both the
replacement and unchanged b4b65df. In the owner-authorized 2026-11-09 follow-up,
`accounts.basics`, `accounts.validation`, `accounts.combined`, and `accounts.groups`
were updated and all passed on macOS with rules enabled and disabled. Production
sources and guards were unchanged. Fixtures now provide mandatory rule categories,
resolve initial import choices explicitly, apply rules only to Uncategorized rows,
use fresh IDs after capture cleanup, and retain completed import-created accounts.
They use current revision tokens and successful close/reopen before older removal.
Corruption fixtures assert that their SQL actually changes rows and recovery rejects
the inconsistent state. Refusal assertions still cover payee-only rules, forced
application, premature acceptance, archived-category assignment and active-chain
Discard. The combined workflow includes interruption before/after the category
decision as well as subsequent repair, acceptance, correction and Undo.

Verification commands used `ctest --test-dir BUILD --output-on-failure -j4` with
`-R '^accounts\.(basics|validation|combined|groups)$'` for the rules-disabled build
(`build/category-rules-off`, 4/4 passed in 30.61 seconds). The rules-enabled run
(`build/import-ui-rules-release`) additionally selected `combined_io`: validation,
groups, combined and combined_io passed in 11.95, 17.68, 40.56 and 51.79 seconds.
After correcting the new payee-only rejection assertion to recognize the engine's
validation error, its focused `accounts.basics` rerun passed in 1.37 seconds.
The shared combined I/O scenario retains first/middle/last injected write failures,
recovery, exact balances and restored-backup checks. This follow-up did not rerun
the full repository suite and does not claim it is all green.

Native Cocoa synthetic tests passed dirty-target revalidation, stale-token refusal,
native text Undo, linked deletion Undo and immediate editor behavior (5 Qt test
results including initialization/cleanup). Offscreen Categories, Rules, committed editing, recovery and
navigation suites passed after adapting their explicit confirmation/session-boundary
fixtures. Linux and Windows builds/interactions remain unverified. This work does
not claim to fix the separately recorded native Rules crash.

## Storage evidence

[Raw measurements and per-table DBSTAT results](../review/action-undo-replacement/)
record identical synthetic workloads using the same driver on `cf8f1a5`, unchanged
`b4b65df`, and the replacement. No VACUUM, padding, reduced durability or private
inputs were used. Latencies were collected under concurrent verification load and
are not isolated performance benchmarks. Journal peaks are 2 ms sampled file sizes,
not exact VFS high-water guarantees.

The accumulated workload starts with 19,000 accepted transactions, categorizes them
with a rule, then performs four sessions of 25-row imports, acceptance and a small
edit, with successful close/reopen and exact balance checks at each checkpoint.

| Final accumulated storage, bytes | `cf8f1a5` | `b4b65df` | Replacement |
| --- | ---: | ---: | ---: |
| Main file | 32,268,288 | 31,977,472 | 31,850,496 |
| Live B-tree pages | 32,268,288 | 23,437,312 | 23,310,336 |
| B-tree payload | 24,718,313 | 18,263,445 | 18,155,220 |
| Sampled simultaneous database/journal peak | 34,312,888 | 43,797,504 | 43,691,048 |

The replacement is below both comparators for retained physical/payload storage.
The accumulated temporary peak is above the older pre-baseline implementation and
slightly below unchanged production: successful-close pruning already has this
journal cost. It is reported separately, not concealed by freed-page accounting.
All 19,000 legitimate rule-edit members expire at close; later small sessions do
not accumulate an ordinary history archive.

The separate 50,000-row import-only probe produces **one root, zero ordinary Undo
members, zero repair members, and zero entry revisions**. After close/reopen its
main file is 32,587,776 bytes versus 35,164,160 for `cf8f1a5` and 33,157,120 for
unchanged production. The root expires and the compact source receipt remains.
The 50,000-row sampled simultaneous peaks are 35,838,656 bytes for the replacement,
35,333,712 for cf8f1a5 and 36,387,480 for b4b65df. As with the accumulated case,
close-time pruning has a temporary cost versus the older history-retaining baseline.

All nine identical 1,000-row edit/import-removal/linked-transfer runs passed. The
replacement stays below cf8f1a5 for main-file and retained payload size. Compared
with b4b65df, fixed schema/coordinator pages add 28,672–40,960 live bytes at this
small scale; the repeated-edit payload adds 897 bytes, while both removal workloads
retain less payload. The raw evidence reports these differences and their peaks;
the larger import and accumulated cases do not hide the small-workload fixed cost.

The retired experiment's source copies and measurements remain under
`review/action-undo-prototype/`; they are historical and are not replacement results.
Manual work remains in MAN-017 for the separately authorized batch update.
