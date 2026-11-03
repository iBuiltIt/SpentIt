# Next session: transaction origin and ordinary operations

Last updated: 2026-11-03
Status: audit and proposed implementation handoff; broader cleanup not implemented.

## Owner intent and authorization

The owner asks why manual origin should matter downstream: a saved transaction
should be manipulable like any other transaction, except where an operation
actually requires imported information. The owner requested this handoff and a
commit so a fresh session can take up the cleanup.

The category-menu defect below received explicit implementation Go and is fixed.
The broader six-part cleanup was proposed, then deferred to the next session.
Do not interpret this document as an already implemented feature or a settled
rule-matching policy. At the start of the next session, summarize the proposed
transaction/category/transfer scope and obtain the owner's implementation Go
under AGENTS.md. Rule matching against current rather than original values remains
an explicit, separate decision. Read-only investigation can continue beforehand.

## Starting checkpoint

- Current experimental formats are 48 with rules / 47 without.
- The system light/dark palette change is committed as `35fa0c7`.
- This handoff accompanies the tested manual category-menu fix. See
  [status](status.md#manual-transaction-context-menu-categories--2026-11-03) and
  [manual transaction contract](manual-transactions.md#right-click-category-assignment--fix-authorized-2026-11-03).
- Launch the rebuilt rules-enabled Release app with `./build/SpentIt`; rebuild
  using the README. Do not open private documents for routine tests.
- Reports remain deferred and unimplemented. This work does not authorize reports,
  publication, licensing changes or general UI redesign.

## What was found

### Fixed: ordinary category assignment required imported origin

The ledger context menu routes ordinary category assignment through
`release_transfer_batch` in `src/ui/import_model.cpp`, then
`ImportStore::assign_transfer_batch` in `src/storage/review_sheet_store.inc`.
Its unlinked ordinary-category branch rejected `!fact.imported`, producing
`Invalid category assignment` for manual ledger rows.

The bounded fix allows either origin and verifies the request's imported/manual
flags against the stored transaction's optional original imported amount.
Generated rows, stale revisions, invalid allocation/category/account facts and
inappropriate relationship changes remain refused. Existing atomic snapshots and
Undo are reused. This is a correctness fix, not the proposed API cleanup.

Passing tests cover manual category assignment through the menu, Uncategorized,
mixed manual/imported and accepted/new selections, reopen, group Undo, exact retry,
cancellation, sampled I/O faults and commit-boundary interruption. See status for
configurations and limitations; do not describe sampled faults as an exhaustive
sweep.

### Transfer assignment has an accepted-state gap for both origins

A temporary synthetic storage probe exercised the same repository operation as
account selection in the category menu. Results:

| Saved transaction | Assign a destination account |
| --- | --- |
| New manual | Succeeded |
| New imported | Succeeded |
| Accepted manual | Rejected: `Invalid transfer relationship` |
| Accepted imported | Rejected: `Invalid transfer relationship` |

After each attempt, verification passed, origin balance remained -1000 minor
units, and the successful/refused cases had one/zero links respectively. This
was a storage probe, not native UI reproduction or exhaustive transfer testing.
An initial probe failed at document creation because its temporary path was not
canonical; canonicalizing the temporary directory resolved the harness error.

Reproduce with synthetic fixtures, not an external attachment or private dataset:

1. Create a CAD Checking account and a second CAD Savings account, with zero
   openings. Use a canonical temporary directory.
2. Create one manual -10.00 transaction dated 2026-01-01, or ingest an ordinary
   QIF transaction with the same date/amount. Test each origin in a fresh document.
3. For the accepted variants, call `accept_all` before assigning the account.
4. Construct a `TransferRequest`: action `retarget`, mode 1, unique link/kept IDs,
   current transfer sequence and batch revision, source allocation identity,
   current amount/date/state, and origin flags derived as the UI does. Supply a
   generated destination endpoint in Savings and the CAD currency definition.
5. Call `assign_transfer_batch`, inspect both ledgers/relationships, and verify.

Useful starting points: `read_transfer_choice` in `src/ui/import_model.cpp`,
`retarget` and `generate` in `src/engine/transfer.cpp`, and `manage_views` plus
apply/preview/history replay in `src/storage/transfer_store.inc`. In particular,
inspect the draft-state link constructed when assigning an account to an existing
accepted transaction; do not assume a manual-only fix solves this.

### Two manual transactions cannot currently be linked

`ordered_origin` in `src/engine/transfer.cpp` requires an imported destination
and rejects a second manually entered side. Link validation also forbids a manual
destination. This is confirmed by code inspection, not a new end-to-end test.
It is an existing bounded transfer policy, not an inherent monetary requirement.
Changing it must address relationship representation, history replay and cleanup,
not just remove the guard.

### The word manual covers two different representations

Saved manual ledger entries now live in the ordinary transaction store using
`manual-v1` creation records. Older manual transfers can instead have their origin
represented inside transfer state (`~transfer-...-origin`). Transfer facts,
`local_side`/`counts_locally`, release paths and history replay still distinguish
manual/imported/generated representations.

Some transfer validators return early for manual/generated facts instead of
loading an ordinary stored ledger row. The ordinary category command also carries
transfer-specific origin flags. These are architectural coupling to investigate,
not proof that every such branch is defective. Preserve accounting of old local
transfer records until their representation is deliberately replaced.

### Rules intentionally treat origins differently today

`src/app/rule_preview.cpp` uses original imported payee/memo for imported records,
but current payee/memo for manual records. In `src/engine/rules.cpp`, split-phase
rules skip allocations without a source ordinal, including manually created
splits. Manual field ownership protects payee/memo from rule replacement.

These behaviors follow the current manual/rule contracts. They are not silently
included in the category bug fix. No additional blanket origin restriction was
found in the ordinary edit, balance and search paths inspected; that statement is
not exhaustive verification of every operation or combination.

## Proposed cleanup, in order

1. **Ordinary category command:** identify transaction and allocation, expected
   revision and desired category. Storage loads authoritative facts. Do not require
   callers to label origin. Share validation with ordinary editor changes while
   preserving mixed-selection atomicity, Review decisions, viewport and group Undo.
2. **Uniform transfer endpoints:** allow manual/manual, manual/imported and
   imported/imported pairs, new or accepted, under the applicable existing exact
   amount/currency and explicit linking policies. Fix accepted account assignment.
   Preserve distinctions between explicit Link and bounded Review suggestions;
   do not widen automatic matching.
3. **Explicit counterpart ownership:** distinguish user-created independent
   transactions from disposable generated counterparts. Removing/changing one
   side must preserve independent or accepted counterparts according to the
   existing lifecycle rules. Do not equate lack of imported evidence with permission
   to delete a user's transaction. Avoid counting a saved manual ledger row again
   as a local transfer posting.
4. **Shared editing and history:** make payee, memo, date, amount, splits, category
   and deletion obey the same rules regardless of origin. Relationship, accepted
   history and revision constraints still apply. Persist all affected rows, links,
   history and Undo atomically; preserve retry and unknown-outcome handling.
5. **Rule policy, only after a separate decision:** recommended ordinary matching
   inputs are current transaction/split values for both origins. Explicitly
   imported-value conditions would require an original. This would change existing
   imported-rule semantics; settle what happens to existing rule definitions and
   previews before coding. Preserve Uncategorized gating, explicit categories,
   split-first ordering, selected-rule ordering, manual text protection and the
   rule prohibition on creating transfers. Do not invent source fields for manual
   entries or assume new rule UI is already authorized.
6. **Narrow source-dependent capabilities:** keep Show/Restore original, reimport
   recognition, import-scoped rollback and source-specific matching tied to actual
   source associations. A manual transaction may acquire an imported original
   through explicit Merge; creation origin and present capabilities are different.

Prefer small reviewable stages, but do not leave financial commands partially
converted. If a representation/schema change is needed, follow the pre-release
no-migration policy: bump formats and reject old experimental data unchanged.
Do not bump merely for cosmetic/API changes, and do not silently reinterpret an
old journal under new semantics.

## Verification and delivery

Create reusable synthetic tests for the cross-product of manual/imported,
new/accepted, single/split, linked/unlinked, and single/mixed selections. Include:

- Category and Uncategorized, transfer assignment and account retargeting, both
  manual endpoints, amount/date/category correction, removal and group Undo.
- Positive/negative/zero amounts where eligible, mixed-sign balanced splits,
  explicit allocation selection, cross-currency rejection and stale snapshots.
- Generated versus independent counterpart preservation, later imported evidence,
  Merge/reimport recognition, import undo, and no double counting in either ledger.
- Reopen and full verification/history replay after each meaningful transition;
  group Undo after reopen and refusal after incompatible later changes.
- Exact retry, changed retry rejection, cancellation after partial in-transaction
  work, I/O/disk-full failures and exits before/after durable commit.
- UI entry points with selection and viewport retention; rules-on/off Release and
  appropriate ASan/UBSan coverage. State native/platform limits explicitly.

Start from `tests/storage/manual_transaction_tests.cpp`,
`tests/storage/review_sheet_tests.cpp`, the transfer engine/store suites and
`tests/ui/import_navigation_tests.cpp`. Keep financial logic out of QML. Update
status and the affected contracts with actual results, then rebuild `build/SpentIt`.

Read alongside [manual transactions](manual-transactions.md),
[ledger editing](ledger-editing.md), [Review](review-sheet.md),
[import undo](import-undo.md), [transfers](../features/008-transfer-matching-and-lifecycle.md),
[categories](../features/010-categories-and-selection.md),
[committed editing](../features/011-committed-editing-and-reversal.md), and the
[current rules contract](../features/004-rules-and-explanations.md).
