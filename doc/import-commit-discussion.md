# Dataset-wide acceptance of new transactions

The [opening/ledger cleanup and interrupted-import rollback](ui-import-cleanup.md)
is implemented under the owner's 2026-10-31 Go on experimental formats **40/39**.
It adds import routing, Review repair/Commit, account-context Discard and automatic
cleanup of incomplete preparation. Completed uncommitted imports remain durable.
See [current verification and limits](status.md); older Resume and toolbar
descriptions below are historical.

Last updated: 2026-10-31

## First priority: correct matching in the common internal representation — 2026-10-30

The owner put this correction **ahead of the unified Review sheet**. Matching common internal source facts, independent of import language, and automatically recognizing sufficiently certain matches are existing accepted requirements, not a new feature request. The completed acceptance/storage work and strict bank-ID slice do not establish that those matching requirements are fulfilled. The owner subsequently gave Go for the same-account re-import correction below; broader cross-account transfer matching remains separate.

### Accepted same-account correction — 2026-10-30

The owner distinguished re-import recognition from cross-account transfer matching and gave Go for **re-import recognition only**. The starting rule is a unique match on original resolved account, currency, exact amount, calendar date and payee. Different posting dates between a bank and credit-card account belong to the separate transfer workflow, which this change does not broaden.

The bounded implementation uses exact parsed payee text (no case folding, whitespace normalization or fuzzy matching), requires nonempty date/payee and valid original money, and requires exactly one incoming occurrence and one existing ledger identity for that key. Repeated keys in a changed export remain reviewable, rather than choosing an occurrence by order. Existing identical-source occurrence matching remains available. Original memo, category and reference differences do not gate this rule; bank identifiers retain their stricter protection. Both sides with bank IDs must pass the existing strict bank-ID rule; conflicting retained IDs cannot be bypassed through an ID-less association. Removed targets and malformed original records cannot auto-match. Matches use saved original facts, preserving later date/payee/amount/category corrections. Accepted and uncommitted targets are eligible within the selected account.

Associations persist through the existing atomic import/decision machinery, so already-present rows contribute no extra posting and duplicate-only imports require no acceptance. Unresolved candidates retain the existing inclusive balance behavior. The existing pending duplicate decision can be changed to Include; this slice does not add the proposed ledger badge, paired inspection, or a new historied unlink command for completed imports. Those interactions remain design work with the unified Review sheet. Verification and limits are recorded in [status](status.md#first-priority-duplicate-matching-correction--2026-10-30).

### Confirmed pre-correction implementation gap

Code inspection and owner-authorized live debugging confirmed that `ImportStore::populate` automatically excludes QIF occurrences through whole-source byte equality; otherwise it flags candidates for review. The separate automatic bank-ID path requires OFX-family sources in `same_bank_transaction`. An incoming QIF occurrence can therefore agree with a committed occurrence on original date, exact amount and payee and still remain unresolved. `current_ledger` and `balance_for` include unresolved incoming rows, so the possible duplicate appears and contributes to the balance again. QIF-to-QIF candidate lookup also requires agreeing dates when both dates are available; a date difference can leave an occurrence classified as new. Equal amounts alone do not prove that two such occurrences are the same transaction.

The hooks confirmed successful import capture/preparation and delivery, not transaction identity. Read-only inspection established the decision behavior above. No private source paths, records, values or identifying details belong in this handoff or regression fixtures. At that investigation checkpoint, no runtime correction or new regression tests had been performed. The session also exposed a separate diagnostics limitation: `ImportMain.qml` still references the removed `projected` control in its UI sampler, preventing semantic UI observations; worker hooks remain usable.

### Original correction and broader design obligations

- Match parsed common internal source facts within the resolved account against accepted and uncommitted transactions. Original facts remain distinct from later display edits/categorization. A parser/format label must not itself gate automatic recognition; whole-file equality may optimize matching but must not be its only QIF path.
- Recognize sufficiently certain matches automatically while preserving legitimate repeated occurrences, exact money/currency boundaries, conflicting identifiers, ambiguous candidates and removed-target handling. Define and review the confidence and occurrence-consumption rules explicitly; do not replace the missing implementation with unrestricted date/amount exclusion.
- The owner favours visible, reversible duplicate links: one monetary posting with attached imported occurrences, a clear ledger indication, paired inspection, and a way to break a wrong match and keep the transactions separate. This is the preferred design direction; exact criteria, normalization, per-import multiplicity, balance treatment of unresolved candidates, search of attached evidence, and accepted-history/Undo behavior still need a concrete design. Do not describe every detail suggested in chat as settled.
- Use independently synthetic regressions for equivalent QIF files with changed surrounding content, cross-format equivalents, original-versus-edited fields, exact matching candidates, distinct identifiers, ambiguous/date-shifted candidates, repeated identical purchases and repeated imports. Verify visible match inspection/unlinking, exact ledger/balance reconciliation, reopen, ownership cleanup, atomicity and retries around durable boundaries for the chosen design.

The [unified Review sheet](#next-implementation-item-unified-review-sheet--2026-10-29) follows this correction and should consume its final match/review states. Reports and unrelated work remain deferred.

## Merge evidence and repeat-import follow-up — 2026-10-30

**Accepted direction; subsequently implemented under the Review Go.** See [implementation and verification](review-sheet.md). The owner prefers explicit review of uncertain pairs rather than requiring a new automatic memo-based rule. The Review sheet should show candidate transactions together. Same-account **Merge** (preferred Alt+M) uses a selected group of two or more rows. Exactly one committed transaction is automatically kept without a survivor prompt; if none or more than one is committed, ask the user to choose the survivor. Cross-account **Link** (preferred Alt+L) retains both independent transfer postings and has no survivor. The [feature 005 decision](../features/005-transaction-sheet-navigation.md#accepted-merge-and-link-interaction--2026-10-30) is the authoritative interaction, rationale, open-design and verification record; it includes the special generated-transfer placeholder repair case.

Retain the original source representations of merged-away postings as evidence associated with the survivor, plus the merge history required for inspection and safe Undo. A later sufficiently certain match against either original representation should resolve to that survivor, without another duplicate question. This retains evidence supporting a retained transaction under the ownership model; it does not require an unrelated permanent archive or preserve a second balance contribution.

The Review implementation closes the inspected gap in `ImportStore::populate`: ID-less exact-key recognition now searches attached source occurrences, deduplicates survivor identities and preserves the existing occurrence/conflict guards. Same-account group Merge, explicit survivor choice and grouped Undo are implemented. See the [verification record](review-sheet.md) for coverage and limits.

## Next implementation item: unified Review sheet — 2026-10-29

Implementation update (2026-10-30): the owner subsequently gave Go. The [Review implementation record](review-sheet.md) supersedes the earlier documentation-only status below and records the saved decisions, Merge/Link, difference categorisation and safe Undo boundaries.

Priority update (2026-10-30): this item follows the matching correction above. The previously selected next item is a **dataset-wide Review sheet**, grouped by account, as specified in [feature 005](../features/005-transaction-sheet-navigation.md#unified-review-sheet--next-implementation-item-2026-10-29). After import, any outstanding issue makes a **Review** button available on the normal account screen. Users open it when ready and work systematically through all affected transactions under account-name separators. Commit also offers entry to this same sheet. There must be one place to find outstanding review work, not a workflow that requires trying Commit to discover it.

This item includes investigating the owner's reported unresolved-count flip when inspecting a record and making counts consistent across review entry points. The former selected-import summary differed in scope from dataset-wide Commit warnings; the exact reported sequence remains unverified. The implemented question cards now have a single dataset-wide question count and clearly scope the remaining import outcome summary. Automatic matching policy is not broadened by this decision.

**Implemented following the separate owner Go on 2026-10-30.** Reports and unrelated work remain deferred.

## Owner direction and authority

The owner settled the four transaction decisions below and gave **Go for implementation on 2026-10-29**. That authorization remains in effect for this model; it does not authorize reports or unrelated changes. The owner subsequently confirmed: preserve existing account/category creation and opening-review behavior while making account rollback remove transaction work only. Do not invent a new creation or opening-cleanup policy.

This direction supersedes earlier requirements for per-account import commit, separate draft-to-committed publication, and compulsory permanent original-import evidence retention. Older feature and implementation documents describe the existing system where they conflict with this handoff; they must not silently reintroduce those requirements. Exact arithmetic, encryption, atomic durable writes, occurrence multiplicity and correction of accepted transactions remain required.

## Current implementation checkpoint

The accepted model is implemented: independent acceptance references, multiple pending imports, strict matching against pending/accepted targets, duplicate-only completion, partial-error acceptance, dataset-wide Commit, source-owned account rollback and physical cleanup. The UI offers warning navigation/Ignore and quit-time Yes/No/Cancel. See [current verification](status.md#dataset-acceptance-storage-refactor--2026-10-29). The gap descriptions below are the historical pre-refactor baseline, not remaining implementation requirements.

Initial accepted revisions reference persisted draft revisions and allocation metadata; they do not copy monetary values and splits into a second baseline. Later corrections retain their ordinary immutable history. Rollback removes transient source memberships and collects unowned records, source files and relationship/review history in one transaction. Existing accounts, confirmed aliases, reviewed opening proposals and staged category names survive transaction rollback. A retained source can keep the matching facts it depends on even after the original import relinquishes ownership.

Explicitly confirming a whole-record bank match to an already accepted generated transfer records source ownership immediately. It adds no financial transaction or acceptance obligation, preserves the accepted transaction's date/payee/amount, and supplies original bank facts for later overlap matching. Forgetting that source leaves the accepted generated transaction. Split records with other allocations continue through their existing review/acceptance path. The transfer sheet shows the retained bank match; the journal presents one match action.

Rollback previews use a persistent mutation generation so a stale preview cannot become valid again after reopening. The latest rollback receipt resolves a lost acknowledgement; older receipts are not retained as a forgotten-import archive. Reusing a stale preview after later operations is refused. Current-format atomicity and verification remain required; previous experimental formats are rejected without migration.

## Common matching correction — owner Go, 2026-10-31

The owner reaffirmed: every format parses into common internal facts, then the
same matching policy applies. An audit found that the earlier exact re-import
correction left a legacy format-dependent possible-duplicate date exception in
both engine code and storage SQL. Tests explicitly preserved that inconsistency.
This was incomplete policy consolidation above a shared record representation.

The authorized correction makes ordinary amount-based suggestions same-day for
all formats, makes the engine authoritative for import and Review candidates,
and tests cross-format parity. Bank-ID evidence can still expose contradictory
facts for review, but format labels do not establish identity or eligibility.
See the [current contract](review-sheet.md#common-duplicate-policy--owner-go-2026-10-31)
and [verification](status.md#common-duplicate-matching-policy--2026-10-31).
Transfer matching is explicitly outside this change.

## Immediate next priority: repeated imports and format-independent matching — 2026-10-29

The owner rejected three observed behaviors: a duplicate-only reimport still requiring Commit, Import statement being disabled while an import is uncommitted, and approximately 141 duplicate warnings after exporting the same transactions in another format (ASO followed by what the owner believes was OFX). The reported count and second format are owner observations, not a verified inspection of private files. No private source was accessed for this investigation.

These are the next implementation priorities within the accepted transaction model, ahead of reports or unrelated work. Do not fix only button enablement or turn every candidate into an automatic exclusion. The owner requested this handoff and a commit; this documentation update itself does not implement the remaining storage changes.

### Settled requirements

- **Keep importing before Commit.** Existing uncommitted transactions are not a reason to disable Import statement or reject another source for the same account. Multiple imports accumulate saved work. Normal protection against concurrent operations and writes to read-only documents remains distinct from this obsolete pending-batch restriction.
- **Commit accepts transactions, not import bookkeeping.** A successful reimport that adds no transactions and changes no transactions creates no new acceptance obligation. Complete any necessary source associations durably during import. Show a plain outcome such as “141 already present; 0 added” rather than making the user commit an empty batch. If earlier imports left genuinely new transactions, Commit remains available for those transactions; a duplicate-only import must neither accept nor discard that earlier work.
- **Match in the common internal representation.** Choosing Microsoft Money, Quicken or another supported bank export option must not itself trigger warnings or change transaction identity. All supported parsers feed common date, exact money/currency, account and identifying fields. Match within the resolved destination account against both accepted and uncommitted transactions, including eligible generated transfer counterparts through the transfer workflow. Preserve original matching facts when the user changes display fields or categorization.
- **Recognize sufficiently certain matches automatically.** A byte-identical source may be an optimization, but must not be the only automatic-match path. A different file/export label containing the same identifiable transactions should produce an ordinary already-present result, not one blocking warning per row. Actual conflicts and ambiguous identity remain reviewable.
- **Preserve multiplicity and ownership.** Two legitimate identical-looking payments remain two transactions. Associate source occurrences one-to-one with existing occurrences; do not collapse rows by date/amount alone. Reimports must respect retained-source ownership and account rollback, and must not resurrect a deliberately removed transaction without the existing explicit review policy.
- **Keep failures separate.** “Nothing added” is not success if some input records failed or remain unresolved. Report added, already-present, unresolved and failed outcomes distinctly; do not hide errors or silently skip records. Import completion and transaction acceptance are separate states.

### Historical implementation gaps — resolved by this refactor

Inspection of [the UI](../src/ui/ImportMain.qml) found Import statement gated by `!s.pending` and Commit gated by pending-batch readiness rather than outstanding transaction acceptance. [Storage](../src/storage/import_store.cpp) also enforces `single_pending` per account and rejects another pending import. Removing the QML condition alone cannot satisfy the requirement.

The parsers already share internal transaction fields. However, `candidate_where` searches committed included rows in the selected account only. `populate` automatically excludes a byte-identical previously committed source, but otherwise marks candidates unresolved, including candidates with a matching bank transaction ID (`FITID`). [The engine matcher](../src/engine/import_review.cpp) compares currency, original amount, date, payee/reference and available IDs; it treats different nonempty IDs as different transactions and allows differing-date candidates when an OFX-family source is involved. This confirms an incomplete decision policy above the shared representation, not independent per-extension transaction stores. It does not establish which condition caused every warning in the owner's private exports.

### Accepted strict bank-ID matching — 2026-10-29

The owner approved strict overlap recognition and gave Go. Match within the resolved destination account using a unique bank transaction ID and agreeing original currency, exact amount, posted calendar date and transaction type. OFX, QFX, QBO and ASO labels, export timestamps, request identifiers, selected ranges and statement balances do not establish transaction identity. Payee/memo presentation (including character escaping) is not a condition of this ID-based rule. Match original source facts, preserving later ledger corrections and categorization. No private sample paths, contents or identifying details are retained here; regression examples must be independently synthetic.

The target behavior applies to accepted and uncommitted transactions. Associations from several exports to one transaction count as one target. Matching must be one-to-one within an import. Repeated incoming IDs, conflicting facts, several target identities, or removed targets remain reviewable. Importing a shorter range must not remove transactions outside that range.

Synthetic decision table (all amounts below are illustrative CAD values):

| Existing source facts | Incoming source facts | Decision |
|---|---|---|
| Account A, ID BANK-A, 2026-02-01, -1.00, DEBIT | Same facts, different export label/request and escaped payee text | Already present when the target is unique and active |
| Same source facts, ledger payee/date/amount subsequently corrected | Original bank facts unchanged | Already present; keep the correction |
| Account A, IDs BANK-A and BANK-B, both -1.00 on the same date | Same two IDs in reverse order | Two one-to-one matches; retain both payments |
| Account A, BANK-A | Same ID in resolved account B | Independent account; no cross-account match |
| BANK-A, -1.00 | BANK-A, -1.01, or changed date/type/currency | Review; no automatic exclusion |
| Two ledger transactions carrying BANK-A | One incoming BANK-A | Review ambiguous identity |
| One BANK-A | Incoming source repeats BANK-A | Review every repeated-ID occurrence |
| Removed BANK-A | Incoming BANK-A | Existing removed-record review policy |
| IDs absent, including ordinary QIF | Same date/amount/text | Existing candidate review; no new fuzzy/date-amount auto-match |
| Different nonempty IDs | Same date/amount | Existing distinct-ID policy; no automatic merge |

Missing IDs, export options that change IDs, uncertain date/text differences and indistinguishable repeated occurrences are not approved for new automatic matching. They do not block implementing the strict rule. A future proposal to loosen matching needs evidence and explicit agreement. Existing byte-identical, ID-less source matching remains a separate previously implemented path.

Implemented slice: automatic strict matches against committed ledger identities, using retained bank source associations and preserving existing review stamps/history. Every repeated incoming bank ID now requires review, including the first occurrence. The bank-ID path also checks byte-identical imports rather than letting file identity bypass conflicting-ID checks. This does **not** complete pending-target matching or the storage work below. One-pending-import restrictions, empty-batch Commit and legacy per-account publication remain until the transaction-state/source-membership refactor. Do not describe this slice as completing repeated imports or dataset-wide acceptance.

### Implementation order and acceptance evidence

1. Complete the transaction-state/source-membership storage design below, removing the one-pending-import assumption throughout queries, commands, UI selection and history verification. Keep failures independent of acceptance.
2. Specify and test the common matching decision table; wire automatic matches and genuine review concerns into that model. Match uncommitted as well as committed rows and preserve multiplicity.
3. Finish duplicate-only imports without an empty Commit obligation; expose useful outcome counts. Wire Import and dataset-wide Commit to the accepted model, not legacy batch state. Remove remaining actual/projected presentation and obsolete batch-specific acceptance wording as part of that wiring.
4. Verify: repeated identical ASO; equivalent ASO/OFX/QFX/QBO and QIF where identity fields support it; changed export text/IDs; partial overlaps; multiple imports before Commit; duplicate-only import while older new transactions remain; same-day/same-amount legitimate repeats; repeated IDs and contradictory fields; removed/corrected targets; generated transfer matches; exact balances and counts; reopen; account rollback with shared ownership; restored backups; and I/O failure, interruption and retry on both sides of each durable boundary. Never use a net-zero amount as proof that there is nothing to accept.

### Date-order control: completed slice, not next implementation

The owner separately gave Go to removing the ledger date-order selector. That slice is implemented and verified: QIF order is inferred across transaction sections; only ambiguous dates prompt, with a real example and both interpretations, no preselection, Continue and Cancel import. OFX-family imports never ask. Conflicting valid orders are refused explicitly. See [the current contract](../features/009-qif-profile.md#conditional-date-order-prompt--accepted-and-implemented-2026-10-29) and [verification](status.md#automatic-import-date-order--2026-10-29).

That change removed only the date-choice prerequisite and mistakenly retained the old pending-import gate as a “safeguard.” The owner explicitly rejected that gate. It is an implementation limitation to remove, not an accepted product rule. The date-order slice does not complete any of the matching/acceptance work above.

## Accepted transaction and commit model

- Imported transactions live in the normal ledger with a **new/uncommitted** status. They are saved as work proceeds and survive quitting/reopening. Uncommitted does not mean unsaved.
- Keep the word **Commit**. It means: “At this point in time, I am happy that the new transactions in these accounts reflect reality.”
- Commit accepts **all current uncommitted transactions across the dataset**, not one account at a time. Generated transfer counterparts are included. Change their status together atomically; do not require a separate copy of their financial contents merely to accept them.
- **Committed means accepted, not permanently uneditable.** Later corrections remain possible. Forgetting new transactions and correcting accepted transactions are distinct actions.
- No routine confirmation is needed for an explicit Commit with no issues. If there are review concerns, explain them, let the user navigate to affected transactions, and offer **Ignore and commit** for overridable concerns. Do not treat an uncertain application judgment as more authoritative than the user.
- On quit with new/uncommitted transactions, ask whether to commit them now. Yes commits before quitting; No quits with everything still saved and uncommitted. Reopening restores that status. A Cancel option returns to the application. A failed commit must not be presented as successful or silently lose pending work.

## Atomic acceptance replay — correction 2026-11-06

The whole-manual walkthrough exposed an audit error after merging into a later
unaccepted import and then accepting all transactions. An earlier batch can
retain another included transaction while its duplicate points to a survivor
published later within the same dataset acceptance. The same ordering can occur
between rows in one publication, or in both directions between two batches.

Under the owner’s follow-up Go, replay recognizes the existing atomic boundary:
a duplicate target may be an included transaction in the same publication or
in a publication with the same dataset-acceptance parent. It is not inserted
into the replayed heads early; its initial snapshot, financial reference and
balances still undergo normal replay and final checks. A separate future
acceptance does not qualify. This corrects verification of current-format data;
it does not change stored transactions, acceptance ordering, matching policy or
the schema. See [status](status.md) for regression and recovery results.

## Import errors and retained material

The owner rejects a required separate permanent “import evidence” archive. Loaded transactions, before and after acceptance, are the working records. Creation/matching ownership needed for correct rollback is not a requirement to preserve original files forever.

Detect import errors during import. Show successfully imported transactions as uncommitted and make failed records or incomplete import visible. Raw QIF for a failed record and manual repair are possible treatments; a clear “some data could not be imported” result is another. Accepted completion policy: successfully imported rows remain usable and can be committed while failed records remain visibly flagged until repaired or explicitly discarded. Commit never dismisses those failures. The exact error UI remains an implementation-design detail. An unreadable amount or failed write cannot be made valid by Ignore and commit. Do not invent values or silently drop failed records.

**Forget means remove the import's material, not retain a discarded-import archive.** For an entirely forgotten import, remove its exclusively owned transactions, raw records if retained, review state and matching associations. No requirement remains to retain evidence that this forgotten import occurred. This describes application data removal, not a promised forensic erasure of storage media or previously created backups. Do not add an audit tombstone solely to preserve a forgotten import. Treatment of shared material follows ownership below; account/category creation is deferred.

## Transfer creation, linking and forgetting

A transfer creates a counterpart in the other account if none exists, or links to an existing counterpart. Track that distinction so forgetting can remove newly created objects without deleting pre-existing transactions.

Accepted example:

1. Checking imports a payment to Visa and creates an uncommitted Visa counterpart.
2. Dataset-wide Commit accepts both transactions together.
3. A later Visa import contains the matching payment. Show a match without counting a second payment.
4. Forgetting that later Visa import removes its new material and matching association, leaving the Visa transaction originally generated from checking intact.

If an uncommitted origin is forgotten, its exclusively generated uncommitted counterpart goes too. If the import only linked to an existing transaction, remove that association, not the existing transaction. A committed transaction must not disappear as a side effect of forgetting a later import.

Before dataset-wide commit was selected, the owner preferred keeping an independently committed generated Visa transaction when its still-uncommitted checking origin is forgotten, ending the link instead. Dataset-wide commit eliminates that partial-acceptance case for new transactions: both sides are accepted together.

## Transaction decisions settled — 2026-10-29

- **Account-wide rollback:** the action previously called Forget rolls back all uncommitted work belonging to the selected account, including its effects in other accounts. It preserves committed transactions. Selected-row removal remains Delete; this is not a selected-import or selected-row command. “Discard uncommitted changes” is suggested wording, not a finalized UI label.
- **Created versus linked counterparts:** remove an exclusively generated uncommitted counterpart when its originating work is discarded. When the work only linked to an existing transaction, remove the association in both directions and preserve the existing transaction.
- **Shared ownership:** checking creates an uncommitted Visa entry (one source); Visa independently imports and matches it (two sources); checking rolls back (one source remains). Remove checking's contribution and transfer link, but retain Visa's transaction as uncommitted. Rollback removes only its own sources, never another retained source. The owner describes this as garbage collection/reference counting. The implementation tracks source identities and preserves facts needed by retained owners. Committed transactions remain protected. Matching must preserve legitimate repeated-payment multiplicity.
- **One balance:** include all active committed and uncommitted transactions. Remove the actual/projected distinction; Commit changes acceptance status, not the balance. Future reports use the same default inclusion policy, with report implementation still deferred.
- **Import errors:** successful rows remain usable and can be committed; failed records remain visibly flagged until repaired or explicitly discarded. Commit must never silently dismiss failures or invent missing values. Unrepresentable amounts and failed writes cannot be overridden into valid transactions.

## Implementation proposal and deferred scope

The four transaction policy questions are settled; do not reopen them as undecided on the next handoff.

Changing account/category creation, opening-balance handling and their cleanup is deferred. Preserve their existing behavior during the transaction-model change, as explicitly confirmed by the owner after Go. The current implementation checkpoint above records the adopted storage and retry representation. The subsequent owner decision below supersedes the earlier undecided validation-cache suggestion.

## Performance context

The owner reports that a fresh QIF import improved substantially after matching/alias changes, with six opening-balance confirmations remaining, but an account commit took approximately **18 seconds**. This is owner-reported end-to-end time, not a measured database-commit duration. Validation, writes, durable storage and UI refresh costs have not been separated for that workload.

The owner objects to repeating expensive readiness checks after readiness has already been established. Financial correctness is not a blanket justification for repeated scans or copying transactions. The simpler acceptance model is a product decision independent of any unproven performance claim. Measure relevant phases on synthetic workloads when performance implementation is authorized; private data requires separate authorization.

## Historical implementation — before this refactor


Read-only inspection found that `ImportStore::commit` checks group readiness and opening confirmation, loads rows to validate decisions/transfer resolution and retain source associations, updates transfer relationships, and then loads included rows again to write their initial committed revisions, splits and metadata. It also records history and freezes the opening. These changes share one atomic database transaction. The UI subsequently refreshes the ledger and balances. Drafts and immutable evidence are already persisted before commit; commit publishes a separate authoritative interpretation. This explains the operations, not why they should take 18 seconds.

Relevant implementation: [commit](../src/storage/import_store.cpp), [group readiness and opening publication](../src/storage/import_group_store.inc), [committed revisions](../src/storage/committed_store.inc), and [UI command/refresh](../src/ui/import_model.cpp).


Before this refactor, the application used per-account publication, copied committed revisions and protected drafts. Abort retained discarded material, and the UI had a redundant account-commit confirmation. Those were the gaps this implementation replaced.

## Next work and verification

Resume from this accepted model and the settled transaction decisions above; bring account/category creation back into scope when requested. Continue the authorized implementation using the storage proposal below; another Go is not required for this accepted scope. Do not substitute a cache-only optimization for the requested model change.

The original handoff was documentation-only. The current implementation slice and actual verification are recorded in [project status](status.md). No private data was accessed.

## Storage implementation proposal — not yet implemented

Historical proposal, now implemented by the checkpoint above. The original heading is retained for incoming links.

Inspection confirms that batch state currently controls transaction acceptance, duplicate lookup, transfer protection, editing, and history replay. `commit` copies drafts and allocations into initial committed revisions; blocked batches cannot publish any rows. `discard` and grouped Abort retain immutable source/history material. Wrapping those account commits in an outer transaction would not implement the accepted model.

The proposal required these changes to proceed together:

1. Give each ledger transaction independent acceptance state and a stable identity, separate from import/error completion. Commit records references to existing financial revisions, not copies of their amounts and splits. Corrections of accepted transactions retain persistent revisions and strict Undo dependencies.
2. Represent ownership with identifiable source memberships. A source contribution has an account owner; a transfer relationship is not itself another ownership root. Account rollback removes that account's uncommitted contributions, then removes only uncommitted transactions with no retained owner. This avoids reference cycles between reciprocal transfer links. Preserve independently supported and committed transactions, and remove both directions of a discarded link.
3. Store import failures separately from acceptance. Successful transactions can be accepted while failed records remain reviewable and repairable; explicit discard removes a failure. Duplicate review and transfer matching must use transaction state rather than assuming every row in a batch has the same state.
4. Implement one dataset acceptance command and one account rollback command, each with a single durable boundary and retry-safe outcome resolution. Acceptance includes generated counterparts. Keep currencies separate in receipts; never add currency totals together.
5. Rework history verification around those memberships and acceptance references before enabling physical cleanup. Delete exclusively owned raw records, review state and matching associations; keep a shared source only while retained work needs it. No forgotten-import audit tombstone. Preserve existing account/category creation and opening-review behavior.
6. Wire Commit without a routine confirmation, review/Ignore for actual overridable concerns, account rollback with a cross-account effect preview, and quit-time Yes/No/Cancel. No keeps uncommitted work saved. A failed or uncertain commit must not close the document as if it succeeded.

Use a new experimental schema and reject earlier formats unchanged; do not add migration work. Verify mixed valid/failed imports, repeated matching occurrences, shared-source ownership, committed corrections/Undo after acceptance, cross-account rollback, exact balance reconciliation, encrypted backup restoration, and failure/interruption/retry on both sides of each durable boundary. The existing per-account publication tests do not establish these new guarantees.


## Commit uses current review — accepted follow-up, 2026-10-29

The owner observed another approximately 13-second commit and gave Go for this rule: imports and edits validate their changed transactions and affected relationships when they happen; Commit uses the resulting warnings. Zero warnings should not trigger another financial/duplicate review pass. Existing overridable warnings need the accepted warning prompt, not another validation pass. Storage constraints, operation identity/revision checks, exact arithmetic and atomic durable writes still apply. Reopening verifies stored data; recovery from a failed operation may reconstruct session review state.

Implemented a connection-owned prepared row review, refreshed before mutation durability and reconstructed after verified opening. Row/version and duplicate-target/source-association stamps retain unaffected reviews; transfer warnings follow current account names and resolved allocation endpoints. All mutation families route through review refresh. Rollback drops speculative results, including source/transfer caches. Commit consumes prepared inclusion, duplicate and transfer-warning results and the reviewed drafts, replacing its two source-row parsing/validation loops. Group readiness consumers reuse the current review snapshot; immutable mapped-source parsing and unchanged transfer graphs are reused. The ordinary bounded validated-row cache now follows row revisions rather than discarding every row after every write; live duplicate/association/retention fields refresh separately.

At that earlier checkpoint this was an incremental change to publication, **not completion of dataset-wide acceptance**. It still writes initial committed entry revisions, allocations, associations, category publication, transfer transitions and history. Group aggregate review and dependency enumeration still scan affected/current collections; this is not a claim that every edit is constant-time. Full verification still runs at open and explicit verification. Existing blocking versus overridable-warning UI policy is unchanged. Those remaining acceptance, rollback, partial-error and quit workflows are implemented by the current checkpoint above. No private workload timing improvement is claimed.

Verification and remaining limits are recorded in [project status](status.md).
