# 011 — Committed editing, removal, and undo

Accepted next direction, 2026-11-08: [unified session action Undo and
implementation handoff](../doc/action-undo-design.md). One strict file-wide action
chain includes acceptance, full current-session import reversal and undoable
removal of eligible older imports. Design/review is complete; the owner approved
the reviewed design and granted fresh-session implementation Go on 2026-11-08.
The older mechanisms below describe current code, not the
new target. Final History layout follows this design; no new runtime behaviour is
claimed.

Implementation Go, 2026-11-07: [session-only history and a close-time
baseline](../doc/session-history-baseline.md). The owner requests ordinary history
to expire on successful close while originals and eligible import Undo persist.
The session boundary, compact completion receipts, dependency-based snapshot
pruning and baseline-aware verifier are implemented. See [status](../doc/status.md)
for executed verification and platform limits.
Descriptions of indefinitely available ordinary Undo below are superseded by the
accepted contract; retained dependency records are not ordinary Undo eligibility.

Current follow-up (owner Go, 2026-11-03): the
[transaction-origin contract](../doc/transaction-origin-handoff.md)
supersedes origin-specific category/transfer restrictions below for saved ledger
entries. Manual and imported postings share transfer assignment, explicit Link,
accepted editing/history and independent-counterpart preservation. Formats are
50 with rules / 49 without. Rule matching policy is unchanged. See
[verification](../doc/status.md).

The [2026-11-01 ledger editing contract](../doc/ledger-editing.md) adds direct Enter
acceptance and stacked session Undo for ordinary draft and committed edits.
Committed history remains durable and its strict history Undo remains available
when eligible after restart. This supersedes the old editor opt-in/Apply
presentation below; other dataset mutations end the session edit Undo sequence.


Current implementation (2026-10-29): the [accepted import/commit handoff](../doc/import-commit-discussion.md) supersedes earlier per-account commit, separate publication and compulsory permanent import-evidence retention below. Commit accepts all new transactions dataset-wide; accepted transactions remain editable; Forget removes exclusively owned import material. The owner settled the transaction decisions and gave Go on 2026-10-29, preserving existing account/category creation and opening-review behavior. The complete agreed acceptance/rollback model is implemented, including multiple pending imports, strict overlap matching, partial-error acceptance, source-owned cleanup, warning overrides and quit prompting. See the handoff and [verification record](../doc/status.md#dataset-acceptance-storage-refactor--2026-10-29). Older per-account descriptions below are historical; the broader feature remains active for its separately deferred scope.

Status: **active**  
Last updated: 2026-11-08  
Implementation: ordinary committed storage/UI operations are implemented per account in the [current multi-account experiment](../doc/accounts.md); schema 4 was the original one-account checkpoint. **The owner accepted the ordinary correction/removal/strict-undo policy on 2026-10-25.** Bounded verification is recorded in the [implementation report](../doc/committed-changes.md) and [project status](../doc/status.md), including configuration-specific results and limits. Allocation-level Merge and unlink are implemented in [feature 008](008-transfer-matching-and-lifecycle.md#accepted-transfer-management-decisions--2026-10-27). The [2026-10-27 committed transfer edits](#accepted-committed-transfer-editing--2026-10-27) are implemented for the locked cases. Moving a committed row and rewriting source links remain open, so this feature remains active.

Accepted and implemented follow-up, 2026-10-26: committed-only bulk category/removal operations exclude Opening Balance and reject draft/mixed selections. Category changes affect every allocation while retaining amounts and memos. Exact previews, durable group receipts, atomic Apply and strict group Undo are documented in [search and ledger](../doc/search-ledger.md).

## Duplicate Merge history follow-up — accepted direction, 2026-10-30

The [Merge/Link decision](005-transaction-sheet-navigation.md#accepted-merge-and-link-interaction--2026-10-30) requires same-account duplicate Merge to keep the sole committed transaction automatically, without a survivor prompt. With no committed transactions or more than one, the user chooses the survivor. Retention of non-survivors' source representations, one active posting, and persistent history with safe reversal remain required. Same-account duplicate Merge and safe stacked Undo are now implemented in the [Review workspace](../doc/review-sheet.md), separately from transfer Link/repair. See the contract and status for batch Merge, source recognition and verification limits.

The merge must preserve survivor fields, splits and relationships by default, and must not erase later work. Exact eligibility for accepted/pending combinations, conflict resolution, atomic reassociation/retirement receipts and Undo dependencies after new source attachments remain design work. Existing strict Undo principles apply; this is not authorization for force-undo or financial implementation. The owner authorized documentation only.

## Purpose and accepted constraints

The owner authorized coordinated roadmap work, with questions reserved for concrete missing information or material decisions. The owner accepted correction of current interpretations, removal from current balances with retained history, strict dependency-aware Undo, separate actual refunds, and explicit removed-record reimport review. This policy separates correction of the application's interpretation from a new movement of money so that a convenient Undo command cannot fabricate bank activity.

Accepted constraints come from [documents and history](001-documents-storage-security.md), [exact money and splits](002-accounts-money-splits.md), [source evidence and reimport](003-qif-import-review.md), and [transfer lifecycle](008-transfer-matching-and-lifecycle.md):

- Preserve immutable imported evidence separately from editable interpretations.
- Retain recoverable committed-change history; never overwrite later work silently.
- Keep exact amounts, currencies, allocation sums and original imported totals valid.
- Apply the complete chosen operation, history and relationship changes atomically; resolve uncertain outcomes by durable operation identity.
- Preserve independent counterpart evidence and explicit per-account commit boundaries.
- Do not silently resurrect or suppress a removed transaction when new evidence arrives.

The application now provides typed committed-entry/history queries, read-only change previews, atomic correction/removal/Undo commands, and durable outcome lookup. The optional UI exposes explicit committed editing, before/after review, removed-entry discovery and strict Undo. Immutable original import snapshots and receipts remain separate from current committed interpretations. See the [bounded implementation and recovery behavior](../doc/committed-changes.md).

## Accepted first scope

Start with ordinary committed transactions in one account, without transfer relationships. Exclude opening-balance changes, currency reassignment, rule reapplication, evidence-association repair and transfer correction from this first implementation. Those operations have separate financial dependencies; exposing ordinary editing must not enable them accidentally.

Offer three distinct actions:

| Action | Accepted meaning | Current balance effect |
|---|---|---|
| Correct transaction | Publish a new editable interpretation of the same transaction: date, payee, memo, categories, allocations or effective total. | Replace the previous effective total with the corrected total. A date correction also changes chronological placement. |
| Remove erroneous transaction | Mark the same transaction inactive in the current ledger, retaining its evidence and all versions. | Remove its effective amount from current balances and reports. Do not create an opposite-signed bank entry. |
| Undo change | Append a compensating application-history operation that restores the immediately preceding interpretation/activity state, if dependency checks permit. | Restore that prior state; this is not a new dated movement of money. |

An actual bank refund, charge reversal or returned payment is a separate transaction with its own financial date and amount, usually imported from its bank evidence. It must not be manufactured by Undo or Remove. A future manual-transaction workflow can record a real movement without import evidence, but is outside this first scope. A relation between a purchase and refund, and its reporting treatment, remain separate design questions.

Avoid a generic user action named Reverse in this initial UI: it can mean either undoing an interpretation or recording actual bank activity. The history may call an operation a reversal internally, but the visible action explains exactly which state it restores.

## Implemented bounded correction and removal workflow

1. Editing starts from a committed transaction identity and expected current revision. Keystrokes remain an editor buffer; Cancel/Discard leaves the committed transaction unchanged.
2. Show a preview of the old and proposed date, effective amount, allocations and balance impact, including original imported values where they differ. Category-only corrections show zero account-balance change while identifying changed allocations.
3. An explicit **Apply correction** or **Remove transaction** action publishes the change and history. These actions do not use the pending import's Commit button or implicitly commit unrelated drafts.
4. Validate current revision, complete allocations, money definitions and dependencies at the storage boundary. A stale preview fails with refresh/review; it is not silently rebased.
5. Success follows durable completion. Retrying the same operation and same content returns its recorded outcome; reuse with different content fails. A failed or uncertain acknowledgement resolves the stored outcome before offering retry.

Committed-change history records retain operation identity, transaction identity, prior/result revisions, full before/after interpretation, active/removed state, source associations and relevant dependency revisions. The schema-4 contract below specifies this retained information; it is not permission to copy sensitive fields into logs. History is encrypted with the document.

The current ledger uses the latest active interpretation; future current-state reports must use the same policy. Thus correcting a historical date or amount changes current historical-period totals; removal excludes the amount from those current views. Previously recorded commit receipts and operation snapshots retain their original values and must be labeled as results at the time of that operation. A historical as-of reporting feature is not implied.

## Accepted undo eligibility and examples

Undo applies only when the transaction and all affected relationships still match the exact revisions produced by the operation being undone. The first version offers no selective field merge and no force-undo option. Unrelated edits to other transactions do not block undo. Newer changes to the same transaction, evidence associations or affected relationships do block it, even when their visible values happen to match.

When blocked, explain the newer dependency and offer inspection of history; the user can make an explicit new correction against the current state. Do not automatically undo a chain of later operations. This strict first version may refuse an old undo even after another action has restored similar values; safety takes priority over guessing equivalence.

| Synthetic case | Accepted outcome |
|---|---|
| Opening CAD 100.00, committed purchase -29.31; correct it to -30.00 with allocations -0.14 and -29.86. | Current balance changes from 70.69 to 70.00. Original -29.31 and the earlier commit receipt remain retained. Undo, if still eligible, restores 70.69. |
| The same purchase was an erroneous extra transaction; remove it. | Current balance becomes 100.00. The removed row, evidence and history remain inspectable. No +29.31 bank transaction is created. Eligible Undo restores the existing row and 70.69. |
| A real +29.31 refund arrives next month. | Keep the original purchase and import the refund as a separate occurrence/transaction. Its own financial date remains intact. Refund reporting policy is not decided here. |
| Correct a payee, then change that transaction's allocations; try to undo the payee operation. | Refuse the older undo because a newer transaction revision exists. Do not erase the allocation correction. A new explicit payee correction can preserve it. |
| Correct transaction A, then independently correct B; undo A's correction. | Permit if A and its dependencies are unchanged. B remains untouched. |
| Remove one of two legitimate identical purchases, then reimport a file containing both. | Preserve occurrence multiplicity. Surface the removed occurrence for explicit review; do not restore or exclude both purchases by a shared fingerprint. |

## Implemented bounded reimport review

Keep removed transaction identities and source associations available to matching. Match against immutable source facts, not the edited interpretation. A candidate pointing to a removed transaction blocks automatic disposition, including the normally eligible byte-identical-file path for that occurrence.

The initial implementation keeps the existing explicit review choices:

- **Keep removed:** the existing Exclude as duplicate choice, clearly labeled for a removed target. Retain the incoming evidence and explicit decision without a monetary posting.
- **Include as a separate transaction:** retain a distinct identity and occurrence for a legitimate repeated movement. Preserve the decision and match basis.
- **Inspect removal history:** navigate to the target's retained history. Eligible Undo of the removal is a separate explicit committed-change command. Import preview, matching, discarding or committing an exclusion never restores the target automatically.

Direct Restore and combined restore-plus-import commands are outside the first implementation. When newer dependencies prevent Undo, explain the block; do not offer force-undo or substitute a newly posted transaction as an automatic restoration. A future explicit restoration workflow can be designed separately.

The existing exclusion relationship provides the bounded QIF evidence link. This does not claim the complete cross-format association model in [feature 003](003-qif-import-review.md). The command/revision rules below specify how pending review remains separate from committed changes.

## Transfer boundary and future proposal

The first implementation rejected committed mutation when a live transfer link was present. That restriction was the starting point. The [accepted committed transfer edits](#accepted-committed-transfer-editing--2026-10-27) now cover the locked cases. An ordinary non-transfer command still refuses a live linked entry. After the link has ended, the unresolved imported counterpart is an ordinary committed row.

The accepted distinction remains:

- Removing an origin may remove its purely generated, uncommitted counterpart atomically, with history and protection/link updates.
- Independently evidenced counterparts retain their transaction and evidence when the origin link is removed; make the unresolved relationship visible.
- A committed destination must never be silently removed or changed by an origin action. Explicit per-account commitment still applies.

This document does not authorize cascading committed deletions or reinterpret transfer principal as ordinary category spending.

## Accepted committed transfer editing — 2026-10-27

The owner locked these answers for point 6. They keep the 2026-10-25 rule: Undo does not un-commit an import, does not create an opposite payment, and has no force-undo. A later edit on either affected side blocks Undo. The same block applies to a newer transfer relationship or a newer source association. The entry's own publication association does not block Undo of a later correction.

- Merge of a purely generated committed duplicate with an independent row of the same amount: one confirmed Merge inactivates the generated row like an ordinary removal. It does not post an offsetting payment. The independent row is linked. The survivor's amount and date stay. The generated row's account balance drops by that amount. The other account is unchanged. Evidence and history stay. The preview names both accounts. A generated row that has its own import evidence is not removed. A Merge that changes no committed money still applies immediately.
- Category rewrite or removal of a committed transfer origin, while the counterpart is already committed: change only the edited side. The committed counterpart's amount and balance stay. The link ends. The counterpart stays visible and unresolved. A purely generated counterpart that is still an uncommitted draft is removed in the same operation. A committed counterpart is not deleted.
- Amount correction of one side of a both-committed link: change only that side and end the link. The other amount stays. Both amounts remain and are no longer one movement. There is no fee and no rate. A date-only correction keeps the link and does not change the other side's date or amount. Linking unequal amounts is still refused. A link that still has attached or proposed evidence refuses the correction, including a payee-only edit.
- Settled account and source-association corrections: do not move a committed row to another account and do not rewrite source-occurrence links. Those links stay immutable. History records them. Wrong-account posting, cross-format evidence-link repair, and explicit same-currency account moves of a committed row stay open and are not done.

Bulk category or removal of a transfer-linked row is refused; those rows are corrected one at a time. Committed rule reapplication remains point 7. The preview of a linked correction states whether the other account's date and amount stay and whether the link remains or ends. A money-changing Merge states the selected account's balance change and that the other account is unchanged. Cancel writes nothing. Identical operation-id retry does not apply twice. A conflicting reuse of that id fails. New documents stay schema 20 with rules and 21 without. These commands did not add a ledger kind.

## Owner decision — accepted 2026-10-25

The owner explicitly accepted the recommended bundle:

> For ordinary committed transactions, correct or remove the current ledger interpretation while keeping all evidence and history. Removing an erroneous entry changes current historical balances; it does not add a dated offsetting payment. Actual refunds remain separate transactions. Undo appends history and is refused if that transaction or a dependent relationship has changed since. Reimports matching removed entries require explicit review. Begin without transfers, opening-balance changes or currency reassignment.

No further owner approval is needed for routine implementation choices that preserve this policy. The locked committed transfer edits are settled above. Opening-balance and currency corrections were point 4. Source-association repair, moving a committed row, and committed rule reapplication remain separate gates. The engineering contract below passed independent review and is implemented for the bounded experimental scope. It does not settle the remaining open policies.

## Verification obligations and evidence

- Exact correction/removal/undo balance effects, date ordering and allocation reconciliation; original source amounts/bytes and earlier receipts unchanged.
- Mixed-sign allocations, overflow, invalid precision and currency mismatch rejection without state changes.
- Expected-revision conflicts, newer-relationship conflicts, unrelated-edit independence and rejected force/partial undo.
- Removed-occurrence multiplicity and review choices, including identical files, overlapping exports and edited original interpretations.
- Disk-full/write/sync failures, abrupt exits before/after durable completion, lost acknowledgements and idempotent/conflicting retries for every new command.
- Restart and restored backup retain active/removed state, evidence, operation outcomes, histories and undo eligibility.
- UI Save/Discard/Stay and document switching preserve buffers/isolation; projected balances incorporate the changed committed base exactly once while leaving unrelated pending work intact.
- Account moves of a committed row, source-link rewrites, and other financially unsupported transfer paths fail in domain/storage validation, not just by hiding buttons.

Independent storage tests exercise these bounded behaviors, including interleaved publication/change receipts, exact money, stale review and Undo dependencies, genuine legacy schemas, restored backups, authenticated corruption, and failure/crash retries. UI tests cover guarded editing, retained buffers, explicit previews, removal/history/Undo, and known/unknown durable outcomes. Configuration-specific executed results and limitations are maintained in the [implementation report](../doc/committed-changes.md) and [project status](../doc/status.md); this list is not a claim of exhaustive failure coverage. Destructive migration and silently rewriting experimental documents remain prohibited.

The independent committed-storage suite includes 14 authenticated corruption cases and sweeps 49 write/sync positions for each of correction, removal and Undo: 147 injected I/O failures and 147 abrupt exits, plus six explicit before/after durable-boundary exits. These are synthetic tested paths, not physical power-loss guarantees or exhaustive filesystem coverage. Linux/Windows, native accessibility, and representative large-workload performance for schema 4 remain unverified. No private financial inputs were used.

## Implemented schema-4 engineering contract

### Format compatibility and identity

This workflow was introduced in schema 4 and extended by the schema-5 [category extension](../doc/categories.md). Current new documents use 40/39 (rules on/off); earlier experimental schemas are rejected unchanged without migration. At the historical schema-4 checkpoint, the new committed-change commands explicitly rejected schemas 1–3. Do not rewrite their schemas or pretend that setting a version number migrates their contents. Backup preserves the actual source format. Older binaries reject schema 4 before interpretation.

An included committed transaction keeps its existing `(batch, ordinal)` occurrence identity and permanent account-entry sequence. New monetary postings are still created only by committing included import rows. Correcting/removing/undoing one uses that same identity and sequence. Date corrections change chronological ordering by date while retaining the sequence tie-breaker. An excluded incoming occurrence remains an evidence/review record, not an independently editable committed transaction.

Original `records`, import `versions`, allocations, batch events and original import receipts remain immutable. The new current-state overlay must never edit those tables to implement a committed correction.

Keep the existing `row(occurrence)` repository API as the original import/review interpretation. Use `committed_entry(occurrence)` for the current committed overlay; silently changing `row()` would corrupt duplicate matching assumptions and historical receipt verification. The UI explicitly chooses which view it needs.

### Immutable entry snapshots and operation timeline

Implemented relational responsibilities:

- `entry_revisions`: immutable complete committed interpretations keyed by occurrence and entry revision; include active/removed state, financial date, payee, memo, effective total and ordered allocations. Revision 0 is the exact included interpretation published by the original import commit. Every subsequent revision has exactly one producing committed-change operation. Original amount and currency identity remain bound to immutable source/account data.
- `entry_heads`: current entry-revision pointer for each included committed occurrence. This is a transactional index over immutable history, not authoritative evidence by itself. Removal advances the head to an inactive snapshot with its prior interpretation retained.
- `ledger_operations`: a single immutable ordered financial timeline containing both import commits and committed correction/removal/undo operations. Record operation ID, kind, permanent sequence, exact account balance before/change/after, and the corresponding immutable receipt or entry-revision transition. IDs are unique across this timeline; conflicting reuse fails.
- `history_order`: immutable common event ordering for every batch event and financial timeline operation. Each batch event `(batch, revision)` and each financial operation ID receives exactly one monotonic order entry in the same transaction as its source record. This lets validation reconstruct pending review references as they existed when a correction/Undo occurred; financial operations alone cannot order draft decisions.
- Committed-change operation detail: target occurrence, before/after entry revisions, full command payload and expected token, exact dependency stamps, and optional `undo_of` operation ID. Snapshots referenced by the operation must be immutable and complete. Persist sufficient data to compare retry content exactly; a new command cannot reuse an ID with changed payload or expectations.

Names and physical normalization are implementation choices, but these invariants are mandatory. All writes belonging to one command occur in one physical transaction, including snapshots, allocations, head updates, timeline rows, original import receipts where applicable, and history.

The operation-ID namespace is global across import publication and committed correction/removal/undo. An ID used for an import cannot be reused for a change, or vice versa, even if a narrower per-table lookup finds no receipt. Check the global timeline identity before retry resolution, then compare the original operation kind and complete payload/expectations. A conflicting reuse fails without writes.

The timeline is necessary: the old verifier chains only import receipts. If an import establishes balance 70.69, a correction changes it to 70.00, and a later import adds 50.00, that later import receipt must retain `before=70.00`, `change=50.00`, `after=120.00`. It cannot be reconciled by pretending the correction never occurred. Schema-4 validation replays import and change operations together; earlier schemas retain their existing receipt-chain verification.

An import commit creates revision-0 snapshots for its included rows and appends one import timeline operation in the same transaction as its existing receipt/batch transition. Exclusions create no entry snapshot or monetary amount. Subsequent ledger queries read current active heads, while receipts continue describing immutable historical publication results.

Within that one transaction, order the batch's commit event before its import financial-operation entry in `history_order`. Replaying this adjacent pair treats it as one atomic publication: the commit event advances dependency/lifecycle state, and the financial entry publishes the included revision-0 snapshots and balance receipt. No command or preview can observe the intermediate state. Validation must require the paired import operation and reject an orphan commit event or mismatched pair rather than treating the event alone as durable monetary publication.

### Expected state, dependencies and preview freshness

Use two distinct notions of freshness:

1. **Entry/dependency token:** target occurrence, current entry revision, and an exact ordered list of dependency stamps. Each stamp identifies a batch that has ever explicitly referenced that target through retained duplicate-decision history, together with that batch's current revision and lifecycle state. Search immutable decision history, not only the latest disposition: deciding away from a reference must not erase the dependency's existence. Include pending, committed and discarded referencing batches. A new reference, review edit, commit or discard changes the token. In this bounded design even an unrelated row edit within a referencing batch invalidates the token conservatively; unrelated batches do not.
2. **Preview account sequence:** the latest global financial timeline sequence when the before/delta/after preview was computed. If another financial operation changes this sequence, refresh the preview before applying. This does not change Undo eligibility when the target/dependencies themselves are untouched.

Compare dependency lists exactly rather than using a lossy count or unchecked hash. Their ordering must be deterministic. A live transfer link is not an implicit bulk dependency: linked rows are corrected one at a time, and Undo also refuses a newer transfer relationship or a newer source association. Source-occurrence links themselves are not rewritten.

An operation stores the entry/dependency token of its resulting state. Undo eligibility requires the current entry revision to equal that operation's result revision and the dependency list to remain identical. A later operation on the same entry blocks the older Undo even if it restores equal values. A newer referencing-batch review/discard also blocks it. An unrelated transaction change does not block eligibility, but can require a refreshed balance preview.

### Application APIs

The typed application boundary separates these command responsibilities:

| API | Contract |
|---|---|
| `committed_entry(occurrence)` | Returns current revision/activity, complete current interpretation, original imported interpretation/evidence references, stable sequence and current dependency token. Reject excluded/noncommitted occurrences. |
| `committed_history(occurrence)` | Returns immutable ordered publication/change records with before/after snapshots, operation IDs and current Undo eligibility/block reason. Removed entries remain queryable. |
| `committed_changes(before_sequence, limit)` | Returns a bounded newest-first page of correction/removal/undo summaries, excluding ordinary import publications. Each summary contains operation ID/kind/sequence, occurrence identity, display date/payee and current active/removed state. An exclusive sequence cursor provides stable pagination without scanning every import row. Removed entries remain discoverable even when absent from the ledger; selecting a summary loads that entry's history. Enforce a positive bounded page size. |
| `preview_committed_change(request)` | Read-only. Request is Correct with a complete proposed interpretation, Remove, or Undo with its target operation ID. Validate ordinary-entry scope, all financial invariants and eligibility; return before/proposed snapshots, exact account before/delta/after and the complete expected token plus account sequence. No financial write or import-state transition occurs. |
| `apply_committed_change(operation_id, request, expected_token, preview_sequence)` | Recompute and validate the same request atomically against current entry, dependencies and account sequence. Append resulting revision/history/timeline and update head. Return immutable change receipt. Identical retries return the original receipt before testing current state; conflicting ID reuse fails. |
| `committed_change_outcome(operation_id)` | Transaction-free durable lookup for uncertain acknowledgement. If rollback has not closed a transaction or the lookup fails, quarantine/close the session and require reopen; never report success from uncommitted reads. |

The change receipt includes operation ID/kind, occurrence, prior/result revisions and activity, before/change/after account balance, and `undo_of` when present. Store exact signed delta; do not require a potentially overflowing intermediate negation. Reuse the exact-sum arithmetic contract to validate representable account totals and deltas. A delta outside the supported money range is an explicit error even if the separate before/after balances are representable; no truncation or floating-point fallback.

Correct and Remove require an active ordinary committed entry. Correction requires the complete validated interpretation and preserves original source amount/currency. Remove produces the same interpretation with inactive state. Undo may restore an inactive entry to active when undoing its eligible removal. It restores the operation's before snapshot as a new revision and records `undo_of`; it never deletes the operation being undone. Undo of an Undo may be offered as another eligible history action using the same checks; there is no independent redo stack or force/partial merge.

The initial import-publication history entry is inspectable but is not Undo-eligible through this API. Undo applies only to committed correction/removal/undo operations; reversing an entire import batch is not part of this scope. A single erroneous included entry can instead be explicitly removed.

Opening balance is frozen for this milestone. Transfer-bearing categories and unsupported constructs remain rejected by domain/storage validation. Category text remains the existing bounded profile's text; this feature does not introduce category lifecycle or rules.

### Reimport review and dependencies

Candidate generation continues using immutable original source facts, including removed entries. Corrected payees/dates/totals never substitute for those source facts. The byte-identical fast path resolves each source occurrence to its existing original included target. If that target is currently removed, create an unresolved decision instead of automatic exclusion; other occurrence mappings remain independent. Preserve existing one-to-one multiplicity constraints.

For schema 4, an exclusion review stores the target's current entry revision and activity at decision time, in addition to its occurrence identity and existing match basis. An exclusion against an inactive target is the explicit **Keep removed** choice. An exclusion against an active target is the ordinary duplicate choice. No separate monetary state is inferred from the button label.

Ordinary draft Save copies the existing exclusion target stamp unchanged. Only an explicit Decide/Reconfirm action refreshes it to the target's current revision/activity. Record the decision-origin batch revision, or an equivalent immutable link to that review action, so validation can distinguish a copied stale stamp from a new review. A later field edit must not silently reconfirm a stale duplicate decision. Automatic byte-identical decisions have a distinct initial review origin at the ingest/materialize revision; they are still invalidated by subsequent target changes.

At import commit, revalidate each exclusion's target revision/activity and matching evidence. If the target changed after review, reject the whole import commit with a re-review diagnostic; do not silently reinterpret Keep removed after a restoration, or an ordinary duplicate after removal. Reconfirming a decision appends a draft revision using the current target state. Include-as-new remains explicit and creates a separate entry only at the usual whole-batch commit.

A stale exclusion is incomplete review: expose its stale-review flag, withhold the authoritative projected balance/ledger and require explicit reconfirmation. Reopening a saved stale decision remains valid and reviewable; it must not be treated as document corruption or silently refreshed.

Pending review decisions never mutate target interpretations. However, their retained references participate in the dependency token: review, decide-away, commit and discard are newer dependent work for Undo purposes. When a committed correction/removal changes a target that an existing pending exclusion references, that exclusion becomes stale and must be reconfirmed before committing its batch. Its source evidence and saved draft remain intact. This is an explicit review conflict, not a silent lost update or automatic posting.

Existing historical exclusion decisions remain intelligible if their target later changes/removes: historical validation checks their target revision/activity as recorded at the time, rather than requiring current activity to remain unchanged forever. Current candidate generation resolves through retained original included occurrence identity, never through a new duplicate posting.

### Verification and recovery contract

Schema-4 open verification must independently replay the global timeline from the frozen opening balance, validating each original import snapshot and each correction/removal/undo transition. Track reconstructed entry heads/activity while replaying; check exact operation before/change/after amounts and that every snapshot, head and change operation has exactly the required references. Finish by comparing reconstructed heads and exact active-entry sum with current ledger queries. Replaying is required; simply summing mutable heads does not prove history consistency.

Validate every stored interpretation against its immutable original source amount/currency and exact allocations, including inactive/historical snapshots. Verify monotonic per-entry revisions, unique producing operations, immutable original import receipts, and Undo's recorded before/result/dependency references. The live dependency check governs whether a future Undo is allowed; historical operation validity must be checked against the dependency state at its position in history, using retained batch events/revisions. The common `history_order` provides this provenance: replay or query the latest batch revision before the operation's order, and only duplicate-decision revisions whose own batch event precedes that order. Derive each batch's lifecycle state from its initial capture/ingest, materialize, commit and discard events. Validate the stored dependency list against that historical state. Every source batch event and financial operation must have exactly one corresponding order entry, and no orphan order entry is valid. A mutable present-day stamp alone is insufficient. Physical insertion ordering may use narrow triggers or explicit helpers, but all cross-references must be checked before durable completion and on reopen.

Writes use the existing durable transaction/error discipline. Do not allow mid-write cancellation after applying a committed-change command begins; show progress and resolve its outcome. Test failures/crashes and retry on both sides of the durable boundary for correction, removal and Undo, alongside ordinary import commits interleaved with each kind of change. New schema-4 backups/restores must preserve the entire timeline, entry heads/snapshots, dependencies, review-target revisions and outcome IDs. No schema migration is included.

## Independent work alongside implementation

After cancellation/navigation, the coordinator can continue accepted QIF resource-boundary and recovery tests, native keyboard/accessibility checks, parser fuzzing where tooling permits, and bounded performance work without weakening verification. The implemented history/receipt inspector preserves the distinction between original publication and later changes. Backup recovery UX can guide users through validated separate-copy restoration using existing commands. Build/dependency packaging preparation and available-platform compilation can proceed without choosing a final license or publishing a release.

Category/rules/report proposals and synthetic examples can also be prepared in parallel. Their unsettled precedence, historical-category behavior and refund/income policies must remain visibly proposed; visual editor prototypes do not settle those financial contracts.
