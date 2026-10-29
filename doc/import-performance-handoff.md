# Import matching and performance handoff

Last updated: 2026-10-29

Current prerequisite (2026-10-29): complete the [diagnostics implementation plan](../features/012-application-diagnostics.md) before resuming these priorities. The owner subsequently gave Go for all seven stages; see [implementation and verification](application-diagnostics.md), including the unresolved ordinary-workload overhead target. This Go does not authorize resuming performance optimization or repeated-transfer matching. Screenshots are excluded. This supersedes the next-work ordering below, while retaining the performance targets and accepted repeated-transfer behavior. A possible MCP adapter is separate future scope.

Update 2026-10-29: the owner reversed the order and gave Go for performance first. See [implementation and measurement boundaries](import-ui-performance.md#application-performance-work--2026-10-29) and [current status](status.md). Repeated-transfer matching still waits for its own Go. The original handoff below preserves the accepted behavior and initial measurements.

Owner-requested handoff for the next session. The two priorities below are agreed; neither is implemented by this documentation change. Resume with an explicit **Go** before coding. Preserve the existing working tree and dataset; recreating the dataset does not fix these implementation problems. Do not access or retain private financial files for benchmarking; use synthetic equivalents.

## 1. Preserve repeated transfers with one-to-one links

Accepted behavior: legitimate repeated transfers between the same accounts on the same date must not remain blocked merely because their amounts repeat. Banks can impose a per-transfer limit, requiring several separate transfers. Every withdrawal must link to exactly one deposit, and each endpoint may be used once only. Preserve every source transaction, amount and evidence record; do not merge repeated occurrences into one movement or create extra postings.

The current resolver requires mutually unique candidates. It refuses repeated candidates rather than linking several withdrawals to one deposit. The UI's imported “Transfer destination” label identifies source intent, not a completed relationship. The owner confirmed that the repeated occurrences in the reported case are real separate transfers.

For equivalent reciprocal groups under the confirmed account mapping, with identical dates, identical currency definitions, exactly opposite amounts and equal eligible occurrence counts, pair the available entries one-to-one. Skip already-linked endpoints. A stable order may choose among otherwise equivalent occurrences; it must not claim that source evidence identifies a unique historical pair. Unequal remaining counts or meaningful differences still require review. Do not broaden date matching, infer currency conversion, discard repeats or bypass existing eligibility checks.

Implementation starting points: `resolve_group_transfers` and blocker analysis in `src/storage/import_group_store.inc`; transfer lifecycle/history in `src/storage/transfer_store.inc`; model/UI in `src/ui/import_model.cpp` and `src/ui/ImportMain.qml`. Update diagnostics so a newly resolvable repeated group is not still described as inherently ambiguous. Apply the behavior both during preparation and through **Resolve mapped transfers** for existing drafts.

Required synthetic verification: repeated balanced groups, already-linked endpoints and reruns, unequal counts, differing dates/currencies/amounts, excluded or stale rows, split allocations, preservation of occurrence counts and balances, independent account commit in either order, reopen, Abort, atomic failure/cancellation and retry. Keep existing per-account commit and history guarantees.

This accepted extension supersedes the mutual-unique-only restriction for equivalent repeated groups in [feature 008](../features/008-transfer-matching-and-lifecycle.md). The existing [implementation contract](qif-account-imports.md#reusing-confirmed-mappings-for-transfer-pairs--accepted-2026-10-28) describes the code before this extension.

## 2. Make application performance usable

Performance is a show-stopper, not optional polish. Owner-reported measurements for the same export:

- 116,621 lines, 18,974 transactions, six accounts; diagnostic reported zero errors.
- `spentit_qif_diagnose`: index 10.5498 ms; parse/report 32.593 ms.
- App **Prepare all accounts**: 4 minutes 39 seconds.
- Switching accounts and deleting a displayed opening: about 12 seconds each.
- App opening/loading: 56 seconds, approximately a minute.
- An earlier attempted commit took about 26 seconds before reporting unresolved transfers.

These are owner measurements, not independently reproduced app benchmarks. The diagnostic parses only; it does not persist drafts, verify a dataset or construct the UI. That distinction explains different work, but does not make multi-minute preparation or minute-long opening acceptable.

Profile a comparable synthetic multi-account workload, including repeated transfers, prepared drafts and link history. Measure application startup separately from dataset opening, group preparation, account switching, status/blocker refresh and commit. Record before/after wall times and the actual dominant costs. Previously recorded synthetic group timings already showed expensive materialization and status reload; remeasure the current build rather than treating older results as current.

Investigation starting points: repeated `import_group`/`import_groups` calls and full-source parsing; per-record `load_row` queries; repeated transfer/history and balance reconstruction; verification on opening; and model refreshes that rebuild all account/group metadata for a local selection. These are leads, not a confirmed profile. Prefer shared snapshots, bulk reads and appropriate indexing/caching with correct invalidation where evidence supports them. Keep heavy work off the UI thread, but do not mistake asynchronous waiting for a speed improvement.

Aim for dataset opening in a few seconds and account switching below one second at this workload; these are targets, not achieved results or promises. Preparation must improve many-fold. Do not weaken encryption, integrity verification, exact arithmetic, transaction atomicity, recovery or evidence retention to reach targets. Verify warm/cold paths and post-edit invalidation, and report remaining bottlenecks honestly.

## Resume and validation

Read [project status](status.md), [QIF account imports](qif-account-imports.md), and [diagnostic usage/privacy](qif-diagnostics.md). Existing schema-30/29 drafts are the current baseline. The earlier performance checkpoint is committed; preserve any subsequent working-tree changes and keep private diagnostic artifacts out of commits.

Local application: `./build/SpentIt`. Build tree: `build/import-ui-rules-release`. Relevant existing tests include `accounts.groups`, `accounts.validation`, storage transfer tests and offscreen mapped-transfer/blocker UI tests. `spentit_account_tests group_benchmark` provides a synthetic group workload. Select additional recovery and sanitizer checks according to the changed paths. Record measured results in status; do not claim native macOS, Linux or Windows validation from offscreen tests.

This handoff changes documentation only; no application rebuild or runtime verification is implied.


Diagnostics implementation update (2026-10-29): the owner authorized all seven stages of [feature 012](../features/012-application-diagnostics.md), excluding screenshots/MCP, with the existing documentation changes preserved. See [the runtime contract and verification record](application-diagnostics.md). This authorization does not resume performance optimization or repeated-transfer matching.
