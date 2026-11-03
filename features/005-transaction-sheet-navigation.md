# 005 — Transaction sheet and navigation

## Sidebar account drag ordering — owner Go 2026-11-03

Accepted and implemented: drag an active account's name/balance row to reorder
active accounts. Movement past the platform drag threshold starts a translucent
rendered copy of the row; its original dims. A blue insertion line identifies
the destination. Release saves the order. Escape, losing window activation or
releasing outside the active account list cancels. Near-edge dragging scrolls a
long list. Ordinary clicks still open accounts. No new controls, handles,
Settings dialog or context-menu actions are added. Archived accounts and fixed
navigation items remain separate and are not drag targets.

The order is a local display preference keyed to the canonical document path,
using the existing Settings preference group (hashed path and account IDs only,
no account names or balances). Reopening that path restores the order on this
installation; copying/moving the file or restoring it elsewhere does not carry
this preference. Explicit file replacement clears it with the other display
preferences. Unknown IDs are omitted; newly created accounts append. Archived
IDs can retain their previous position until the active list is reordered again;
otherwise reactivated accounts append. This does not change home currency,
account selection, transaction order, financial history or session Undo.

The model validates an exact permutation of current active account IDs and
synchronizes Settings before publishing the reordered list. Failed persistence
shows an error and retains the displayed order. Busy operations prevent dragging.
Verification and platform limits are in [status](../doc/status.md#sidebar-account-drag-ordering--2026-11-03).

## Ledger transfer arrows — owner Go 2026-11-02

Accepted: a small arrow button precedes the category on a linked single-split
transaction. Incoming money shows →, outgoing money shows ←, relative to the
current account. A zero-value linked transfer uses ↔. The tooltip and accessible
name say “Transfer from/to [account] — show matching transaction” (with “with”
for zero). The category text remains beside the button.

Clicking follows the saved transfer endpoint, opens the other ledger, reveals and
selects that transaction even if its prior search hid it. Existing navigation
and edit guards apply; Back restores the previous account position. Split rows
show an independent arrow beside each linked saved split, using that split's
signed amount. Ordinary, unresolved, released and removed links offer no arrow.
This adds navigation only; it does not infer matches or change financial data.
See [verification](../doc/status.md).

## Remember account sorting — owner Go 2026-11-01

Accepted and implemented: remember the ledger sort column and direction for each
account, writing local application settings immediately after a successful sort
change. No separate Settings control. Accounts without a valid saved choice use
date ascending. Account navigation, reopening and a new application instance
restore the preference; renaming an account does not change it.

Preferences use the canonical document location (hashed in the settings key) and
stable account identity, keeping different files separate without storing account
names or transaction data. Moving/copying a file to another location starts with
fresh preferences; replacing a file through Create clears its prior preferences.
Search, selection and scroll positions remain session-local. Failed queries do
not save a new preference; unchanged sorting does not rewrite settings. A settings
write error is surfaced. No document-format or financial-storage change.
See [verification](../doc/status.md).

## Ledger return position — owner Go 2026-11-01

Accepted and implemented; see [verification](../doc/status.md). Leaving the ledger
for Categories, Rules or another workspace records its position separately for
each account. Back/Forward and account buttons restore that position after the
ledger and open inline editor have laid out. Hidden model refreshes must not
overwrite the saved position. The anchor is a transaction identity plus its
screen offset; the saved row index supplies a fallback if that identity no longer
appears. A shorter result list clamps to its valid end. Horizontal scroll is also
retained where possible. Existing account selection, editor, search, sort and
navigation guards remain in charge of their own state; returning does not issue
a new search or select another transaction merely to restore the viewport.

Positions are session-local and clear on closing/switching documents. Explicit
counterpart navigation continues to reveal its requested transaction rather than
restoring a previous position. This does not introduce persistent layout settings
or change transaction/category storage.


Current ledger-editing follow-up (owner Go, 2026-11-01): double-click toggles the
editor; ordinary selection leaves it attached; Enter accepts edits directly;
Escape discards unaccepted text; session Undo restores accepted edits. See the
[contract](../doc/ledger-editing.md) and [verification](../doc/status.md). This
supersedes the older Save draft / Cancel edit presentation below.

Settings is available only in the lower-left sidebar; the duplicate upper-right
button was removed at the owner's request on 2026-10-31. Backup/restore remains in
Settings. Show the sidebar first if it is hidden.

The [opening/ledger cleanup and interrupted-import rollback](../doc/ui-import-cleanup.md)
is implemented under the owner's 2026-10-31 Go on experimental formats **40/39**.
It adds import routing, Review repair/Commit, account-context Discard and automatic
cleanup of incomplete preparation. Completed uncommitted imports remain durable.
See [current verification and limits](../doc/status.md); older Resume and toolbar
descriptions below are historical.

Status: **active**  
Last updated: 2026-11-03  
Implementation: partial. The durable QIF UI now has combined text/date/source/regex search, identity-based multiselection, and atomic committed bulk category/removal/group Undo; see [contract and verification](../doc/search-ledger.md). Point 8 adds the [session sidebar balances, column ratios, narrow scroll, and viewport retention](#point-8-sheet-slice--2026-10-27). The synthetic sheet and broader account/navigation design remain separate bounded scopes. The optional Qt Quick sheet implements the shell, sorting/literal filtering, stable selection, inline engine-backed editing, and Save / Discard / Stay for session navigation/closing. The [durable UI](../doc/import-ui.md) adds setup/review/edit/commit with persistence; it does not implement this full sheet design. See [scope, actual verification and measurements](../doc/sheet-prototype.md). Full acceptance criteria below remain open; this feature is **active**.

## Next tasks: Review responsiveness and batch decisions — 2026-10-31

State: **owner Go, 2026-10-31; implementation and verification recorded in [status](../doc/status.md)**. The owner
requested this next-session record after reporting that the common same-day
matching correction is much better. Preserve that working-tree correction and
its verification; do not restore the withdrawn adjacent-day experiment. See
[current status](../doc/status.md#common-duplicate-matching-policy--2026-10-31).

### Reported problems

- Each Review **Yes** takes roughly 3–5 seconds. With 34 match groups, repeated
  waits add substantial overhead to the time spent inspecting transactions.
  These are owner observations, not instrumented timings.
- A transient status line below **Hide sidebar** pushes the entire view down.
  Its removal and the disappearing resolved group then produce another jump.
- The owner proposed marking reviewed matches and applying them together rather
  than waiting after every decision. A single batch taking 3–5 seconds is a hope,
  not an established performance result.

### Proposed order of work

1. **Stabilize the Review layout.** Put status/progress in reserved space, such as
   a fixed Review toolbar. Showing, changing or clearing status must not shift
   the ledger. Preserve a meaningful viewport anchor when resolved groups leave
   the list, so the next group stays in a predictable position.
2. **Measure one Yes operation before choosing an optimization.** Follow the
   [diagnostics guide](../doc/agent-diagnostics-guide.md). Correlate the owner's
   click with worker execution, validation, durable save, snapshot refresh,
   model update and presentation. Determine which costs are per decision,
   per transaction or dataset-wide; do not assume the durable commit is the
   source of the whole delay. Use synthetic fixtures for repeatable mutations
   and owner-driven actions for a representative native observation.
3. **Design and then implement batch application of reviewed match groups.**
   Keep this useful workflow even if individual decisions become faster, but
   let profiling determine which repeated work the batch can avoid.

### Proposed batch interaction

- Add selection at the **match-group level**, visually distinct from the existing
  transaction checkboxes. Existing row selection means which transactions within
  one ambiguous group to merge; it must not acquire a second meaning.
- Checking a group records pending UI intent only. It does not merge, accept,
  discard or otherwise change financial records.
- A fixed action such as **Merge 12 selected matches** applies only groups the
  user inspected and selected. Do not interpret this as auto-selecting all
  suggestions or broadening matching eligibility.
- Any required survivor choice must be settled before applying that group.
  Preserve the sole-committed-survivor rule, effective-total checks, relationship
  protections, retained evidence and history.
- Proposed storage direction: save the selected decisions as one atomic operation
  and refresh once, avoiding repeated work where the measurements support it.
  Do not promise constant batch time; necessary per-merge work still scales.

### Accepted decisions — owner Go, 2026-10-31

- A permanent status line occupies the bottom of the right pane in every workspace,
  including accounts, Review, Categories and Rules. Progress, feedback and errors
  use its reserved space. Long errors remain readable through Details; errors
  requiring a decision retain explicit controls.
- Match-group selection is separate from transaction-row selection. Ambiguous
  groups need at least two explicitly chosen rows and any required survivor choice.
  The sole committed selected row remains the automatic survivor.
- Selections are UI intent only. They remain while navigating within Review and
  inspecting without mutation, but clear on leaving Review or dataset mutation.
  They are not saved across restarts.
- Revalidate every selected group and apply all or none. No silent skipping. A
  stale/invalid group identifies the affected selection and leaves all financial
  records unchanged. A failed unchanged attempt may retain UI selections so the
  numbered selection can be inspected; a changed generation clears them.
- One Undo reverses the whole batch, retaining existing later-mutation barriers.
- Cancellation before the final durable commit gate rolls back the whole batch.
  Once the commit gate is sealed, the operation completes and Undo is available.
- Scope is duplicate Merge only; Link remains an individual decision.

### Verification expectations

Use synthetic groups containing accepted and uncommitted rows, repeated amounts,
ambiguous candidates, mixed survivor choices, retained source identities and
linked-row restrictions. Verify exact balances, no unintended merges, stale-group
handling, durable outcome/retry, chosen Undo behavior and failure rollback. Check
that progress and group removal do not move unrelated content unexpectedly, and
that work stays off the UI thread. Compare one-by-one and batch timings using
consistent workloads; record actual results and unavailable native/platform checks.

## Next-session handoff: Review workspace and link learning — 2026-10-30

State: **implemented in the working tree following owner Go, 2026-10-30**. The owner requested this
handoff after trying the initial implementation and subsequently settled the
matching policy below and authorized the complete revision. This section supersedes
the earlier modal/card presentation and permissive link-suggestion direction.

### Problem and evidence

The owner reported thousands of outstanding questions after a single-file import,
with the visible link suggestions overwhelmingly false. Supplied screenshots show
same-date suggestions between wildly different amounts and an equal-amount
suggestion years apart. Do not copy private payees, account identifiers, source
records, screenshots or attachment paths into fixtures or repository documents.

Baseline code inspection found that `ImportStore::review_sheet` admitted
opposite-sign, same-currency candidates on **equal magnitude OR same date**. There
is no date-distance bound on equal amounts. This explains the classes of weak
suggestions; it does not establish the full private-workload false-positive rate.
The owner also reports Review appearing automatically after import. Trace that
entry path during implementation; the approved behavior is an available button,
not an automatic popup. Prior functional/recovery tests establish mechanics, not
useful matching quality or acceptance of the current presentation.

### Accepted requirements

- **Review earns the user's attention.** Same date alone must not qualify a link.
  Equal opposite amounts also need reasonably close posting dates. Unequal amounts
  require stronger transfer evidence; support for fee splitting is not evidence
  that arbitrary amounts belong together. Explain why each suggestion appears.
- Replace the modal Review with a compact **account-level workspace** in the
  sidebar, with its outstanding count and an ordinary Review entry button. It is
  a virtual view, not a financial account or a second set of postings. Users can
  enter and leave freely. Commit routes to the same workspace when review is
  needed; importing does not automatically open it.
- Use normal ledger capabilities, not a summary of selected fields: inspect and
  edit actual transactions, expand splits, and inspect original import evidence,
  including QIF source data. Preserve account grouping and question grouping.
  Question lines contain inline Yes/No/Selected yes or link controls; transaction
  lines retain ordinary ledger interactions. Preserve the accepted survivor,
  partial-group, saved-negative-decision, difference-category and Undo behavior.
- Add **Don't suggest links…** on each transaction within a link question. The
  editor defaults to Payee; support Payee or Memo, exact text or regex, and either
  account-specific or dataset-wide scope. Editing this text edits the matching
  condition, not the transaction itself. Preview the affected current suggestions
  before saving. The accepted exact matching semantics are specified below.
- Manage these explicit exclusions through the existing **Rules** button, in a
  clearly separate **Link suggestions** section from categorisation rules.
  Rules can be inspected, edited and removed. Apply them to either endpoint of
  current and future link suggestions. Removing/changing a rule re-evaluates the
  affected candidates; separately saved No decisions still retain their meaning.
- Exclusions only suppress link suggestions. They do not delete transactions,
  prevent manual linking, hide possible duplicates or alter automatic re-import
  recognition. There are **no learned duplicate-exclusion rules** in this scope.
  Ordinary No rejects the displayed comparison without silently creating a rule.
- Learning is explicit and understandable: each saved lesson improves future
  reviews. Do not hard-code recurring transactions or mobile deposits as universally
  ineligible; either can be a transfer. A usable initial matcher is still required.
- Review references the actual transactions. Changes in an account or in Review
  update affected comparisons and the shared count, including newly eligible
  candidates. Rule changes likewise update affected suggestions. Inspection and
  navigation alone do not resolve work.

### Proposed implementation direction and remaining decisions

Prefer mutation-maintained candidate state or targeted invalidation over rebuilding
all matches whenever Review is opened. Recheck both old and new candidate neighborhoods
when an amount, account, date, payee, memo or relationship changes. Opening Review
should display current state cheaply. The owner prefers this incremental direction;
its storage/index design is not yet adopted. Persistent IDs, atomic updates, restart
reconstruction and safe Undo must stay consistent with the financial model.

### Accepted matching policy and implementation Go — 2026-10-30

- Suggestions require different accounts, the same currency, opposite nonzero
  amounts and posting dates at most seven calendar days apart (inclusive, either
  direction). Compare eligible allocations for split transactions, not unrelated
  whole totals. Keep all eligible alternatives; proximity never selects a winner.
- Owner follow-up Go: an ordinary category on either allocation disqualifies
  that allocation from link suggestions, including imported text still shown as
  unmapped. Empty, Uncategorized/Uncategorised, or a resolved account destination
  are eligible; an account destination must identify the other endpoint's account.
  Evaluate splits separately and recheck the chosen allocations when accepting a
  suggestion. Category edits refresh eligibility. Manual Link remains available.
  Imported rows whose complete payee is Opening Balance or Starting Balance
  (case-insensitive) do not receive link suggestions; no substring guessing.
- Review is the first button in the sidebar's action group, above Categories.
- Equal magnitudes need no descriptive-text agreement. Unequal magnitudes require
  an outgoing amount larger than the incoming amount, with the difference at most
  4% of the outgoing allocation, and an explicit resolved transfer destination to
  the other account. Use exact integer arithmetic. Generic payee/memo words are
  not destination evidence. Confirming a difference requires explicit remainder
  categorisation. These are suggestion rules, not new automatic-link permission.
- Link exclusions default to dataset-wide; “Only in this account” restricts the
  endpoint tested, not the counterpart. Match current Payee or transaction/split
  Memo, separately. Exact means whole-field Unicode caseless matching without
  whitespace/punctuation normalization. Regex uses the bounded existing adapter.
  Any matching endpoint suppresses a suggestion; exclusions have no ordering.
  Saved pair-specific No decisions survive edits and rule removal. Link exclusions
  are available independently of categorisation-rules support. Preview before save.
- A valid explicit QIF destination such as `L[Visa]` (or split `S[Visa]`), resolved
  to an account, records a transfer even when no independent counterpart is found:
  create its generated counterpart atomically and keep one persistent Review task
  awaiting bank evidence. Unknown accounts, invalid amounts and currency mismatches
  remain correction tasks. No duplicate generated posting may be created on retry.
- Owner clarification: generate a counterpart for **every unresolved explicit
  transfer**, even when plausible independently imported postings already exist.
  Keep those real postings and the generated counterparts visible until linked;
  their temporary balance effects are intentional. Never choose an uncertain
  pairing merely to avoid the generated posting. Review Link retires generated
  duplicates atomically; Undo restores the pre-link state.
- Generated-counterpart attention survives restart and is distinct from commitment.
  Existing approved automatic matching may resolve it; otherwise show candidates
  or allow manual Link. “Accept without bank match” dismisses that attention without
  deleting money or preventing later matching. Rejecting a candidate does not dismiss
  the waiting task. Suggestion exclusions do not hide this known transfer task.
- Transfer attention/resolution belongs in Review, replacing the separate Transfers
  entry workflow. Reuse existing financial commands and safe generated replacement.

### Suggested work sequence and verification

First inspect current candidate construction, model refresh, navigation, Rules and
source-inspection paths. Present concrete choices for the open matching policy.
After Go, implement conservative candidate eligibility and explicit exclusion
persistence, then the ledger workspace and incremental updates, integrating and
verifying the entire feature before calling it complete. This is a proposed work
sequence, not authorization to start or to create new chats/subagents.

Use independently synthetic fixtures for same-date/wildly-different amounts,
equal amounts years apart, plausible nearby postings, fees with/without positive
evidence, repeated legitimate transfers, ambiguous candidates, currency boundaries
and both endpoints of an exclusion. Verify rule preview/save/edit/remove/reopen,
manual linking despite exclusion, unchanged duplicate detection, bounded malformed
regex handling, and updates after edits in either workspace. Include a many-row
workload to measure candidate quality and update cost; counts alone are insufficient.
Verify no automatic post-import popup, sidebar navigation, compact inline controls,
full transaction/split/source inspection, shared Commit routing/counts and Undo.
Exercise relevant atomic failure/retry/recovery paths for persisted rule/candidate
changes. Keep private data out of automated tests.

The starting baseline was the uncommitted working-tree implementation documented in
[Review implementation and verification](../doc/review-sheet.md), schemas 34/33.
Preserve that work. Main entry points are `src/storage/review_sheet_store.inc`,
`src/storage/review_sheet_schema.inc`, `src/app/include/spentit/review_commands.hpp`,
`src/ui/import_model.cpp`, and `src/ui/ImportMain.qml`; regression entry points are
`tests/storage/review_sheet_tests.cpp` and `tests/ui/import_model_tests.cpp`.
The owner subsequently gave Go. The revision is implemented and verified;
see [current status](../doc/status.md#next-review-workspace-and-link-learning--2026-10-30),
including verification of the accepted generated-counterpart policy for ambiguous imports.

## Unified Review sheet — next implementation item, 2026-10-29

Priority update (2026-10-30): the [duplicate-matching correction](../doc/import-commit-discussion.md#first-priority-correct-matching-in-the-common-internal-representation--2026-10-30) comes first. This sheet follows it and must use the corrected match/review states.

State: **implemented in the working tree following owner Go, 2026-10-30**. See the [implementation contract and verification](../doc/review-sheet.md). The broader transaction-sheet feature remains active.

### Implementation Go and interaction decisions — 2026-10-30

The owner gave Go for the complete feature, including the underlying Merge, Link,
retained-source recognition and safe Undo work. The following settles the earlier
layout questions; implementation and verification must be recorded separately.

- Account separators contain questions, not a dense transaction editor. Same-account
  groups ask **Are these the same?** with Yes / No, plus **Selected yes** for groups
  larger than two (enabled with at least two selected). Resolve only the selected
  subset; two or more remaining rows stay for another round.
- Exactly one committed selected row survives automatically. Otherwise ask which
  selected row to keep. Preserve its current fields and splits. Conflicting totals
  require editing first; never silently blend values or relationships.
- Link questions show one source and candidate counterparts with account names.
  Select one then Yes; No needs no selection and rejects the displayed candidates.
  Save both positive and negative decisions. Inspection never resolves work.
- Unequal same-currency, opposite-sign allocations show a popup: **Link 10.00 and
  categorise 0.50 as [Uncategorized]**, with an ordinary-category selector and Yes /
  No. No only dismisses the popup. Yes atomically splits the larger absolute amount,
  preserves both account totals and other splits, and links the common amount.
  Ambiguous allocation endpoints require selection. Never infer currency conversion.
- Actions save immediately, without routine confirmation. Cmd+Z / Ctrl+Z and one
  shared Undo button reverse decisions as a stack. Text editing keeps its own Undo.
  Undo restores questions and financial state together and refuses unsafe reversal
  after later changes. Saved history remains available after reopening.
- Invalid records appear under **Needs correction** in the same sheet. Failures
  without an assigned account use an Import problems section. Review and Commit
  share the sheet and the same dataset-wide outstanding-question count.
- Candidate suggestions remain distinct from automatic recognition. No fuzzy
  automatic exclusion or memo fallback is authorized.

### Accepted direction

- After import, whenever any review issue exists, expose a clearly visible **Review** button on the normal account screen. Users choose when to review; attempting Commit must not be the discovery mechanism.
- Review opens one sheet containing all transactions needing attention across the dataset. Group items beneath clearly named account separators, for example Checking followed by Visa. Do not restrict the list to the currently selected account or import.
- Let users work through the items systematically in that sheet. Commit must also offer a route to this same review screen, rather than a separate competing list of problems.
- Explain each issue and expose the relevant resolution. Possible duplicates, failed/invalid import records and unresolved transfers belong in this unified workflow. Preserve a distinction between overridable review concerns and invalid data requiring repair; merely acknowledging an issue cannot make invalid financial data valid.
- Use consistent, explicitly scoped counts across entry points. Opening a record, selecting another import/account, opening or closing the sheet, and attempting Commit must not appear to resolve an issue. Only a saved resolution or an actual relevant state change changes the outstanding work. Unresolved work remains available after reopening the dataset.

### Problem evidence and investigation

The owner reported an unresolved count changing from one to zero after Review Record, then returning to one on Commit. This exact sequence has not been reproduced. Code inspection found a selected-import outcome count beside dataset-wide acceptance warnings, so scope changes are a concrete suspected contributor; do not present that as a verified complete diagnosis. Reproduce and fix the inconsistency as part of this item. No private transaction details or attachment paths are needed in fixtures or documentation.

The current duplicate editor mixes split editing, rule creation, internal identifiers and candidate selection. The owner found it too dense to understand the decision. The existing fallback candidate rule can suggest equal amounts with different dates/payees when an OFX-family source is involved. That is separate from strict automatic bank-ID matching. This Review sheet decision does **not** authorize changing matching criteria or silently excluding transactions.

### Layout and count decisions

The Go decisions above settle the question-card layout. Counts mean outstanding questions across the dataset, including correction tasks; they do not count candidate rows. A record can participate in more than one question. Account changes and inspection do not write decisions. Invalid records remain correction tasks even when valid transactions are committed. Expanded editing uses the existing record editor.

### Required verification

Use synthetic fixtures with at least 51 outstanding review items across multiple accounts/imports, including multiple issues on a transaction and a failure without a usable row. Verify account grouping, reachability from both Review and Commit, systematic progression, saved resolutions and reopen, and consistent counts during inspection and navigation. Reproduce the reported count flip before claiming it fixed. Verify that non-overridable errors cannot be accepted through the override action. Record actual results and limits in [status](../doc/status.md); actual results are recorded in the [Review implementation record](../doc/review-sheet.md).

## Accepted Merge and Link interaction — 2026-10-30

State: **implemented following the subsequent Review Go, 2026-10-30**. Automatic matching remains bounded to the accepted same-account rules.

### Separate intentions and commands

| Command | User intention | Financial result | Preferred shortcut |
|---|---|---|---|
| **Merge** | These same-account rows represent the same transaction | One active ledger posting, a survivor determined by the committed-count rule, retained source evidence and merge history | **Alt+M** |
| **Link** | These are the two account postings of a transfer | Both independent postings remain, connected as a transfer; each keeps its posting date | **Alt+L** |

Use these names in the ledger context menu and relevant Review actions. Alt+M and Alt+L are implemented. Native keyboard-layout and operating-system shortcut conflicts remain unverified; the offscreen Qt tests exercise Link and the standard Undo key sequence. This replaces the older transfer Merge/Ctrl+M terminology while preserving transfer safety rules.

The owner considered one context-sensitive Merge command, then selected separate **Merge** and **Link** because reducing duplicate postings differs from connecting two retained postings. The earlier Join/Alt+J idea and automatic Merge-to-Link dispatch are not the chosen direction. Context should guide suggestions and command availability, not silently change the operation. Selecting another account while using Merge should offer Link explicitly instead of performing it automatically. Account location alone is not enough to classify every case: generated transfer placeholders have special lifecycle rules.

### Merge selection and survivor

1. Select two or more same-account transactions. Support groups larger than two, such as three imports of the same activity; do not assume all selected rows come from one import.
2. Invoke **Merge selected** through the context menu or shortcut.
3. Count the committed transactions in the selected group. **If exactly one is committed, keep it automatically and skip the survivor-choice step.** The user's earlier commitment already determines the survivor; do not ask them to confirm that choice again. Show which row is kept and which postings are consolidated as part of the operation's normal presentation.
4. **If none or more than one is committed**, keep the group highlighted and prompt **“Choose the transaction to keep.”** Click a transaction within that group to choose the survivor, clearly marked **“Keep this transaction.”** Selection order does not choose the survivor. Escape cancels the selection workflow without a financial write.

| Committed transactions in the selected group | Survivor behavior |
|---|---|
| Exactly one | Keep that committed transaction automatically; no survivor prompt/click |
| None | Ask the user to choose the survivor |
| More than one | Ask the user to choose the survivor |

This rule applies to groups of two and larger groups. Commitment is determined per transaction, not from the source import's completion state. The owner wants minimal obstacles: use an established decision when it determines the outcome, and ask only where a real choice remains. The stated aspiration to do the right or acceptable thing in nearly every case is product intent, not a measured accuracy claim or permission to silently resolve financial conflicts.

The survivor preserves its identity, current fields, categories, splits and relationships by default. No silent field blending, amount change, transfer breakage or loss of later corrections. The automatic survivor rule settles who is kept, not whether a conflicting merge is financially valid. Conflicting amounts or relationships still require explicit resolution; the implementation requires equal effective totals for Merge and refuses a losing row with an existing link. Do not add a routine survivor confirmation to the exactly-one-committed case. This supersedes the earlier suggestion that an accepted existing row would merely be suggested and always require a survivor click.

The Review sheet should present unresolved candidate rows together, show why they may match, and allow the user to confirm they are the same through this Merge workflow. A missed automatic match does not itself require looser automatic criteria. In particular, a proposed exact-memo fallback for differing payees has **not** been approved for implementation; manual review remains appropriate.

Start with selection-based Merge. Invoking Merge with no selection may guide the user to select the first and subsequent rows, but group completion for two or more rows and the detailed empty-selection flow remain to be specified. Ordinary clicks must not accidentally complete a merge outside the explicitly entered mode.

### Link selection and transfer repair

Select the first transaction, invoke **Link**, navigate to the other account and select the counterpart. Neither independent posting is a master or survivor. Preserve each account's amount and posting date. If more than one split allocation could be an endpoint, require explicit allocation selection. Existing currency, sign, identity, relationship and validation constraints continue to apply; selection is not proof of a valid transfer.

Support a guided first/second selection flow when Link is invoked with nothing selected. Preserve the chosen first endpoint across account navigation, and honor Save / Discard / Stay and cancellation. Detailed handling of preselected pairs, busy operations and unsupported selections remains design work.

Link/repair can retire a purely generated placeholder when attaching the real imported counterpart under the existing [transfer lifecycle](008-transfer-matching-and-lifecycle.md). Its preview must explicitly identify that removal and any balance effect. Never silently retire an independently supported posting. This exception remains a transfer Link/repair operation, not a reason to decide between Merge and Link solely from the selected accounts.

### Retained representations and repeat imports

The non-surviving rows cease contributing monetary postings; their original source facts remain attached to the survivor. Record the explicit decision and preserve the history needed to inspect and safely reverse it. Merge is not destruction of import evidence. Subsequent imports should recognize **any retained original representation**, including a non-survivor's different payee, and resolve it to the survivor without asking the same duplicate question again when identity remains sufficiently certain.

This is a requirement to extend matching, not a claim that it already works for every format. Current source associations retain duplicate evidence. Identical-file matching can follow an earlier duplicate decision, and strict bank-ID matching examines attached occurrences. The ID-less exact-key path now searches attached original representations and follows their survivor associations. See [the import handoff](../doc/import-commit-discussion.md#merge-evidence-and-repeat-import-follow-up--2026-10-30).

Deduplicate candidate identities after following associations: several representations of one survivor are one target. Preserve legitimate repeated occurrences, account/currency boundaries, conflicting IDs, removed-target policy and per-import occurrence consumption. “An attached source matches” is not permission to collapse arbitrarily many incoming identical purchases. Retention must follow the accepted ownership/Forget model; this does not reinstate a compulsory permanent archive of every original file.

### Remaining design and verification obligations

- Define eligible combinations of accepted/uncommitted transactions, amount/split differences, linked transfers and groups larger than two. Define how survivor selection interacts with conflicts and the final preview/apply boundary; do not invent financial resolutions during implementation.
- Specify atomic source reassociation, retirement, saved acceptance state and merge-history receipts. A merge must not implicitly accept unrelated pending work. Define safe Undo after later edits, links, source attachments or subsequent merges; it must not overwrite newer work or resurrect postings silently.
- Specify occurrence consumption when an import contains multiple retained representations or repeated identical purchases, and behavior when a retained source is explicitly forgotten.
- Verify context-menu/keyboard parity, shortcut conflicts, automatic survivor selection with no prompt for exactly one committed row, explicit choice for zero or multiple committed rows independent of selection order, Escape/cancel, guided Link across accounts, split selection, generated-placeholder previews and explicit suggestions rather than silent command switching.
- Synthetic financial regressions must cover two and three-plus inputs with zero, exactly one and multiple committed rows, exact before/after ledger and sidebar balances, preserved survivor splits/edits/transfer relationships, both original representations reimported in changed files, ambiguity/repeated occurrences, conflicting IDs, removed targets, reopen, restored backups, ownership cleanup, atomic failure/retry and interruptions on both sides of durable commit. Verify persistent history and safe Undo, and that only a saved resolution changes Review counts.

The [implementation record](../doc/review-sheet.md) records the implemented boundaries and actual verification. The obligations above remain the regression checklist; they are not a claim that every platform or interaction has been verified.

## One inclusive balance — 2026-10-29

The owner accepted one balance including saved uncommitted transactions and gave Go. The working tree now shows one balance in the account sidebar and ledger header; the committed/projected selector is removed. The normal ledger continues to show valid saved rows when another record needs repair or review. Uncommitted rows are labeled. Commit readiness remains separate from this read path. This does not complete dataset-wide acceptance or account rollback; see the [handoff](../doc/import-commit-discussion.md) and [verification/status](../doc/status.md#single-balance-and-visible-uncommitted-ledger--2026-10-29). Earlier descriptions of toggling the two balances below are superseded.

## Performance-first application pass — 2026-10-29

The owner gave Go for performance before repeated-transfer matching. The bounded implementation reduces grouped-import preparation and account-switch latency through scoped SQL-plan reuse, committed-batch-first candidate queries and invalidated group snapshots; it also reduces repeated verification work and UI progress notifications. [Current workload results and limitations](../doc/import-ui-performance.md#application-performance-work--2026-10-29) distinguish offscreen timings from native rendering. Full opening remains slower than the few-seconds target. Matching, exact money, evidence, per-account commit and durability policies are unchanged. This is not completion of the broader sheet feature. The subsequent diagnostics-guided follow-up removes duplicate startup verification and adds bounded, write-invalidated validated-row reuse for read-only switching; see [scope and measured limits](../doc/import-ui-performance.md#diagnostics-guided-startup-and-switching-follow-up--2026-10-29). Startup remains slower than the owner’s near-instant target, but the owner subsequently accepted the roughly seven-second startup and considers the application usable. The [startup/loading-screen redesign](001-documents-storage-security.md#startup-presentation-follow-up--deferred-2026-10-29) is deferred.

## Application identity — accepted 2026-10-28

The durable UI is SpentIt, the actual application being improved toward release. Remove prototype-only banners, synthetic setup defaults, obsolete schema-specific guidance and development-plan placeholders from its user-facing screens. Explain real limitations where they affect decisions: forgotten-password recovery is unavailable, and unsupported document formats must still be refused without modifying the file. Keep the separate synthetic development harnesses and historical verification records accurately labeled. This cleanup does not authorize reports, format migrations or distribution work.

## Welcome layout and scrollbars — accepted 2026-10-28

Use the minimal welcome states and creation defaults in [document setup](001-documents-storage-security.md#welcome-and-creation--accepted-2026-10-28). Center a comfortably spaced column with room above the title for future icon artwork. Keep setup scrollable on smaller windows. Currency menus, other selection menus, transaction lists and scrollable panes use draggable 16 px scrollbars with a visible track/thumb whenever content overflows; they do not fade away while idle. Ledger columns reserve space for the vertical bar.

## Accepted requirements

Category hierarchy, naming, neutral income/expense classification, and account destinations in the selector are specified in [feature 010 — Categories and selection](010-categories-and-selection.md). The bounded catalog and account transfer destinations are implemented. Category reports remain unimplemented.

- Portable, responsive desktop UI with a consistent appearance on macOS, Linux, and Windows.
- Sidebar exposes accounts and setup areas such as accounts and rules.
- Sheet columns: Date, Payee, Category, Withdrawal, Deposit, Balance. Committed and draft views must be distinguishable.
- Click selects; multiple selection is supported. Context actions include Delete and Change category.
- Double-click expands a transaction inline for editing rather than opening a separate popup dialog. Only one transaction editor is active; other split transactions can be expanded for viewing.
- Editor exposes date, payee, category, memo, expense/income, note, split controls, rule controls, QIF/source toggle, OK, and Cancel.
- Split allocations have their own category, memo, and amount. Display split structure without implying that every allocation has the parent category.
- Source view is read-only. Double-clicking a source field copies it into its corresponding editable field, subject to validation. Source absence for manual records is explicit.
- Matched rule opens for editing; unmatched transaction can create a rule. If multiple rules contributed, the design must expose that fact.
- After categorizing an uncategorized transaction, offer to apply that category to other matching uncategorized transactions. When accepted, the intended memo/note can also be copied.
- Back/forward navigation traverses visited screens, including accounts and rules, within the session.

## Accepted main layout and behavior — 2026-10-25

This is the owner's current intended layout, expected to evolve through use. It establishes the application shell now; it does not finalize Settings, the report generator, category editor, or rules editor. Those designs will follow separately. Qt Quick/QML remains a technology proposal.

### Application shell

- Two main panes: a narrow left navigation pane and a wide right content pane for the selected account ledger, report, Settings, categories, or rules.
- A toolbar spans the window above both panes and stays visible when navigation is hidden. It contains a sidebar hide/show control, Back and Forward controls for the session's visited right-pane screens, a title identifying the current view, and a search field with a magnifying-glass control at the right.
- The sidebar can be resized horizontally or completely hidden/restored with its toolbar control or a keyboard shortcut. Its maximum width is less than the window width, leaving room for content. Exact limits and shortcut are unresolved.
- The sidebar contains Accounts above Reports, separated by a draggable horizontal divider that controls their heights. Each list scrolls independently when its contents exceed its allocated height.
- First-run setup and the password-entry/dataset-browser screen follow the [document setup and recovery requirements](001-documents-storage-security.md#accepted-setup-unlock-and-recovery-behavior--2026-10-25), including optional password protection and a browsable suggested Documents/SpentIt location.
- Settings includes the path specifying which dataset to load. Each running application has one active dataset; users can close it and open another. See [document lifecycle](001-documents-storage-security.md). This does not establish an application-wide instance limit.
- Fixed controls at the bottom open Settings, the category editor, and the rules editor in the right pane. A gear is intended for Settings; the other icons remain undecided.
- Accounts and Reports each have a `+` control. Clicking an account or report selects its right-pane display. Right-click offers account-specific actions through guarded selection (see [implemented account menu](../doc/accounts.md#ui-behavior-and-limits)). Nonempty accounts are archived, not deleted; at least one account must remain (see [account semantics](002-accounts-money-splits.md)).
- Adding an account appends an inline name editor at the bottom of the account list and scrolls it into view. Enter creates the named account using the document's base/home currency, with an editable special Opening Balance entry dated today and initially zero. Currency correction is a separate deliberate modal action described in feature 002.
- Adding a report opens a modal dialog. **Ask the owner for its contents when implementing report creation; do not invent the report-generator design.** Saved report names appear in the Reports list. The sketch's YTD, Last Month, and This Month illustrate entries, not a settled built-in report set.

### Sidebar balances and sizing

Account names truncate with an ellipsis when needed. Aim to show the largest displayed balance in full, plus a space and at least a few account-name characters. The original rough 99-million sizing estimate is not a financial limit. Allow negative signs, supported currency scales, large values, and unambiguous currency identity; a bare dollar sign cannot distinguish CAD from USD. Behavior when the available window width cannot accommodate these requirements remains to be designed.

Normally the sidebar shows committed account balances, independent of ledger search or sorting. Once an import supplies a draft projection, default to showing the projected balance. Clicking the balance toggles between committed and projected states, allowing reconciliation with the bank without scrolling the ledger to its beginning. Distinguish states with an explicit label/indicator as well as colour. Draft edits and exclusions update the projection; committing makes the published result the committed balance. Multiple pending batches and state persistence still require a policy.

### Ledger, resizing, and edit navigation

- Columns are Date, Payee, Category, Withdrawal, Deposit, and Balance. Every column supports ascending and descending sorting, with an indicator showing the current sort column/direction.
- Running balances retain their chronological account meaning. Searching hides rows without recalculating their balances; sorting reorders rows with their existing chronological balances. Same-date ordering still needs a deterministic definition.
- A bottom totals row displays Withdrawal and Deposit totals for all rows matching the current ledger filter, including rows outside the viewport. Label these as filtered totals, distinct from account balances. Totals reconcile exactly to the displayed ledger amounts; matching a split field does not silently turn these into category-allocation totals.
- Window resizing adjusts the layout, column widths, and pane heights. Exact proportional/minimum sizing rules remain open. The initial direction is a compact date column, money columns narrower than Payee/Category, and Category generally narrower than Payee, informed by category-name widths. User column-resizing behavior and persistence remain unspecified.
- Leaving a changed, unsaved inline edit through navigation, Back/Forward, document switching, or closing offers Save / Discard / Stay. Save validates and completes the applicable operation before leaving; failed validation keeps the editor open. Discard abandons that edit; Stay leaves it active. Enter or OK is the explicit save path; an unchanged editor needs no unsaved-change prompt. Multiline-note Enter behavior remains a proposed newline exception to define with keyboard interactions.

### Transfer interaction

Accounts appear as distinguishable transfer destinations in the category selector. Selecting one initiates counterpart matching/linking or generates a destination draft. A match icon on either linked transaction opens the other account ledger, selects the counterpart, and brings it on-screen. Its match icon returns to the original; toolbar Back also returns. Both automatic and manual matches support this navigation, subject to Save / Discard / Stay for an active edit.

Offer Fix match from either side for missed or incorrect matches, and highlight generated counterparts still needing attention after import. Show protected drafts distinctly: they remain uncommitted and contribute only to the projected balance even after their origin commits. Clearing their destination workspace does not remove them. See [transfer lifecycle](008-transfer-matching-and-lifecycle.md) for provenance, repair, and removal rules.

### Search selection and return behavior

Remember the selected transaction and ledger view before a search begins. If the user clears/cancels search without selecting a result, restore the pre-search selection and view, even if that transaction was excluded by the search. If the user selects a search result, clearing/cancelling search instead keeps that transaction selected and brings it on-screen in the unfiltered ledger. Do not silently select a different result and treat that as an explicit user choice. Retain transaction identity rather than a volatile row index. Exact keyboard/multiselection and deleted-row fallback behavior remains to be designed.

### Search direction; grammar remains unresolved

Search filters the selected account's ledger across the whole editable/display record, including payee, categories, notes, and split memos. A `QIF:` mode searches original QIF evidence instead of the display interpretation. Source evidence remains read-only. Define behavior for manual records with no QIF source and the exact scope of the prefix before implementation.

The owner wants a query language combining text with period filters. Illustrative intent: `period:may-june starbucks` finds Starbucks transactions during May and June of the current year. Desired date forms include `20261001`, `2026-10-01`, and `2026/10/01`. Settings will provide a date-format preference used by period parsing. These examples express requirements, not a finalized grammar: range delimiters, boundaries, quoting, precedence, and ambiguity handling must be designed together with the owner. Ambiguous dates must not be silently guessed.

Regex capability is desired, with case-insensitive matching by default where appropriate. Whether ordinary text is literal or regex, how regex is requested, flag overrides, and interaction with query operators remain open. Invalid expressions and resource-limit failures must produce explicit errors rather than appear to be successful searches with no matches. Bound search work and keep it off the UI thread as appropriate.

## Accepted search and selection policy — 2026-10-26

The owner accepted the following policy for the next search/ledger package. These decisions supersede the corresponding open grammar and selection questions above; they are requirements, not implementation evidence.

- Ordinary terms are case-insensitive literals joined with AND. Double quotes delimit a phrase. Regex is explicit: `regex:"pattern"`.
- A query-wide `QIF:` prefix searches original record evidence instead of the editable interpretation. Records without QIF evidence do not match that mode. For example: `QIF: period:2026-05-01..2026-06-30 "coffee shop"`.
- Date ranges use `..` and include both endpoints. Accepted full dates are `YYYYMMDD`, `YYYY-MM-DD`, and `YYYY/MM/DD`. `period:may-june` means May through June of the current year. Ambiguous numeric dates are rejected until a date-format preference has been explicitly chosen.
- Shift selects a range; Cmd/Ctrl toggles individual rows. Selection follows stable identities. Clearing search restores the pre-search selection if no result was explicitly chosen; otherwise it retains only explicitly selected results. If all selected records were removed, selection is empty.
- Bulk Delete and Change category require an exact preview and one atomic operation. A loop of separately committed single-record commands does not satisfy this requirement.
- The first bulk implementation operates on committed rows only and excludes Opening Balance. Mixed draft/committed selections are rejected explicitly. Change category updates every allocation of each selected transaction, preserving amounts and memos; the preview lists every affected allocation. The owner accepted this bounded scope separately.
- Schema 7 is approved for durable bulk receipts. The owner subsequently clarified that all formats are pre-release and disposable: migration, upgrades, and backward compatibility are unnecessary until the owner announces a release. Existing experimental-format support may remain, but it is not a completion obligation for this package.

Invalid syntax, invalid patterns, cancellation, and exhausted resources must remain explicit outcomes. Chronological balances and exact whole-transaction filtered totals retain the accepted behavior above. Detailed engineering limits and escape syntax must be specified without silently changing financial interpretations.

## Proposed technology and behavior

Use C++ for domain/storage/import/rule/report logic and Qt Quick/QML for presentation. C remains an option for a justified isolated library, not a requirement for the entire UI. Keep authoritative money operations out of QML numeric arithmetic.

Use a shared application style and platform-appropriate input conventions. Consistent appearance does not promise identical OS window chrome or font rasterization. Virtualize sheet rows/cells and keep editor state outside recycled visual delegates. Background work must not access UI state unsafely.

Before finalizing Qt Quick, prototype a realistically large sheet with selection, keyboard navigation, scrolling, editable row expansion, and simultaneous read-only split expansion. Establish a representative dataset and measurable responsiveness targets.

Use an explicit split indicator/summary rather than showing an ambiguous single category. Offer a preview/count for bulk categorization. A matching payee alone is not a license to replace unrelated memos or committed data silently.

## Acceptance criteria

- Keyboard and pointer interactions work on all three target platforms, including multiselection and context actions.
- Editing one row prevents a second editor without preventing read-only inspection.
- OK validates and persists the appropriate draft or committed operation; Cancel leaves that operation unchanged.
- Source-copy actions preserve the original source and obey money/split invariants.
- Bulk categorization clearly identifies affected rows and copied fields and is recoverable.
- Sorting, filtering, scrolling, and recycled delegates cannot move edits to the wrong transaction.
- Navigation reproduces the visited screen sequence and handles an active edit explicitly.
- Background import/rule/report work leaves interaction responsive under the agreed workload.

- Sidebar resizing/hiding preserves access to the toolbar; Accounts and Reports scroll independently.
- Committed/projected balance toggling is visibly identified without relying solely on colour and is unaffected by ledger filtering.
- Filtered Withdrawal/Deposit totals include every matching ledger row and reconcile exactly; chronological balances do not change with search or sort.
- Search distinguishes editable fields from original QIF evidence and surfaces invalid/ambiguous input and execution failures.
- Clearing search without selecting a result restores the prior view/selection, including an excluded original row. Selecting a result makes that row the visible selection when search clears.
- Match navigation selects and reveals the counterpart and supports return by its match icon or toolbar Back; protected drafts remain clearly uncommitted.
- Leaving a changed edit honors Save / Discard / Stay without silently losing or publishing work.

## Authorized prototype and verification — 2026-10-25

The owner authorized the bounded sheet prototype and specified 100,000 transactions/account as representative, 200,000 as stress, based on fifty-year personal-account use. These are workloads, not application limits. Qt Quick is selected for this experiment only. Same-date fixture-ID ordering, projected ledger balances including drafts, ASCII literal filtering and session-only saves are prototype conventions, not adopted production policies.

All 13 engine/session/model/QML/interaction CTest entries passed in Debug, Release and ASan/UBSan on macOS arm64. Tests cover exact totals and draft exclusion, unchanged chronological balances under sorting/filtering, invalid edits, combined split/total edits, explicit zero and mixed-sign allocations, original totals, stable editor identity through recycling, pointer/keyboard selection, search return, guarded account navigation and window closing. Native scrolling and CPU/memory measurements are recorded in the [prototype report](../doc/sheet-prototype.md). No Windows/Linux, accessibility, production durability, full query-language or complete feature acceptance is claimed.

## Open questions

- Bulk-match definition: raw or normalized payee, account boundaries, draft-only default, split handling, and memo/note overwrite rules. Draft-only with preview is proposed.
- Persistence of unfinished keystrokes versus accepted edits; detailed keyboard behavior, including multiline notes. Save / Discard / Stay on leaving a changed edit is accepted above.
- Same-date chronological ordering; multiple pending-batch projections and persistence of the sidebar balance toggle. Sorting/filtering and the two balance states are settled above.
- Accessibility, keyboard shortcuts, exact column/pane sizing and persistence, narrow-window fallback, large-data performance targets, and supported OS versions.
- Search grammar, regex activation/flags, date ambiguity and range boundaries, QIF-prefix scope, records without source evidence, keyboard/multiselection during search, and restoration if the selected transaction no longer exists.
- Settings contents (beyond the date-format preference and dataset-load path), category/rules editor contents, and report-generator contents. Ask the owner when each is designed.

## References

- [Qt supported platforms](https://doc.qt.io/qt-6/supported-platforms.html)
- [Qt Quick TableView and delegate reuse](https://doc.qt.io/qt-6/qml-qtquick-tableview.html)

## Accepted durable-import experiment — 2026-10-25

The owner accepted the bounded policies in [feature 009](009-durable-import-milestone.md), including its explicit deferrals. Those decisions govern that experiment only; broader open questions in this feature remain unless explicitly settled there. Headless import commands and ledger queries now have a bounded writable UI; one selected record now expands inline with parsed source-field copying. Simultaneous read-only split views are now implemented in the durable UI. Full navigation and broader selection behavior remain incomplete; see the [implementation and verification](../doc/import-ui.md#inline-editing-and-source-copying).

## Durable UI performance follow-up — 2026-10-25

The [bounded native workload report](../doc/import-ui-performance.md) records 100,000/200,000-row measurements through the real durable model and QML view. Read-only queries reuse verified ledger data, presentation maps are created on demand, and large retired views are released on the worker. Atomic saves, full document verification and financial semantics are unchanged. This does not complete the broader acceptance criteria or establish responsiveness on other platforms.


## Durable filter return follow-up — 2026-10-25

The durable UI now implements the accepted search-return behavior for its existing literal filter: retain the pre-filter occurrence/view unless a result is explicitly inspected, then retain and reveal that result when clearing. Clear and Escape honor Save / Discard / Stay. See [scope and verification](../doc/import-ui.md#filter-return-navigation--2026-10-25). This does not implement the proposed query language, multiselection or full navigation shell.

## Account navigation follow-up — 2026-10-26

The durable UI now includes a scrolling account list, inline name entry using the setup currency, independent durable account workspaces and guarded switching/Back/Forward. The [account contract](../doc/accounts.md) records verification and limits. At this 2026-10-26 checkpoint, account switches reset selection, search, and viewport, and counterpart navigation, account lifecycle actions, report panes, and the full sidebar balance layout were unfinished. Counterpart navigation and the point-4 account commands later landed. Report panes remain unfinished. Point 8 later added a session sidebar balance toggle. Persistence, a reports list, and the rest of the layout remain unfinished.


## Durable ledger hardening — 2026-10-26

The current durable ledger now exposes named list/row accessibility semantics with selection, currency and transaction status, and announces operation/error text. Keyboard/focus regressions cover navigation, editor traversal, modal cancellation, saving and search return. Closed guard controls are marked invisible through Qt accessibility; the final focused Cocoa test passed. A native accessibility-tree inspection observed the named rows and fields, but actual spoken VoiceOver output remains unverified. See [hardening evidence and limitations](../doc/hardening.md).

Disabling asynchronous offscreen delegate caching addresses the reproduced Qt reset warning while retaining visible virtualization, reuse and existing viewport/unsaved-edit behavior. Warning-fatal repeated reset regressions and large-workload scrolling measurements are recorded with their exact scope. The point 8 slice below covers a bounded part of the layout. Multiline notes, the exact shortcut, persisted columns, and the rest of this feature stay open.

## Point 8 sheet slice — 2026-10-27

The owner said Go for point 8. The durable UI now does this much of the accepted layout, and no more.

The sidebar lists each account with a separate balance control. The text is Committed or Projected, the currency code, and the amount. The amount is the account's existing committed or projected total. The choice starts on projected when a projection exists, and on committed when it does not. A click switches that account only. It does not change ledger search, sort, or the Projected ledger checkbox. The choice lasts for the session and is cleared when the document is created, opened, unlocked, or left. It is not stored. A restart was not run as its own case. The value is a window property, not a saved setting.

Ledger columns are Date, Payee, Category, Withdrawal, Deposit, and Balance, in the ratios 0.14, 0.24, 0.17, 0.15, 0.14, and 0.16. Money columns are right-aligned. The user cannot drag a column. Widths are not saved. The window minimum is 720 by 560. The default size stays 1440 by 850. The ledger content width is at least 1080, so a narrower window scrolls sideways. The top toolbar keeps the sidebar toggle, Back, and Forward. The sidebar shortcut stays Ctrl+Shift+B. The accepted layout still leaves the exact shortcut unresolved.

Bulk removal, bulk category, rule reapply, and history undo go through Save / Discard / Stay. The visible row is captured only after the user leaves the edit. Bulk and reapply put that row back after preview and Apply. History undo puts it back on Apply. Stay does not arm a restore. A missing row opens at the top. An ordinary one-row save does not capture. The new test clicks bulk category. It does not click history undo or reapply.

Reports are a note: "Reports stay outside this work. Nothing is created here." There is no add button and no report dialog. On 2026-10-28 the owner said the eight-point sequence was the work before reports. Building a report waits for an explicit Go and the open questions in [feature 006](006-reporting-drill-down.md).

This slice does not add a separate note field, a multiline Enter exception, a date-format preference, same-date ordering, persisted column or sidebar sizes, or a new money formula. Feature 005 stays active. Verification is in [status](../doc/status.md#point-8-sheet-and-sidebar-interactions--2026-10-27).

## Verified fast startup — owner Go 2026-10-31

Normal startup reuses a prior full audit only for an identical full-file SHA-256
and verifier version, with authentication, locking, format and journal gates
retained. Graceful quit hides the window while the worker audits and atomically
publishes owner-private approval in the platform cache. Missing/stale approval
falls back to verification; failures never trigger automatic repair. Initial
Review suggestions load on demand, including before Commit. See
[the implementation contract](../doc/fast-startup.md) and [status](../doc/status.md) for verification and limits.

## Remember window position — implemented 2026-10-31

Historical position-only slice, superseded by the whole-window correction below.

Owner Go implemented: Settings includes “Remember window position”, off by default.
When enabled, the normal window's frame position is restored before showing the
window. At graceful quit, coordinates are saved before the audit-time hide, only
if they differ from the previous saved position (or none has been saved yet).
Disabling the option retains the old coordinates but stops both restoration and
position writes. The flag itself persists immediately when changed.

This is an application preference, independent of the selected dataset. Existing
`QSettings` stores the flag at `Window/rememberPosition` and a point at
`Window/position`, using the existing organization `SpentIt` and application
`Durable import experiment` identity. On macOS this resolves to
`~/Library/Preferences/com.spentit.Durable import experiment.plist`. Category sort
and dataset-path preferences retain their existing keys. Disposable verification
receipts remain separate under the application cache directory.

Restoration chooses the available display with the largest overlap, or the primary
display when none overlaps, and clamps the position into its available area.
Oversized windows are anchored at the available area's origin without changing
size. Minimized, maximized and full-screen coordinates are not saved as normal
window positions. Window size/state and other layout persistence are outside this
change. Forced termination cannot save a new position.

Preference-write failures are surfaced. If the quit-time position write fails,
the window remains visible; the user can disable the option and quit without
saving position. Both ordinary quit and quitting after an audit failure capture
position before hiding. Closing just the dataset does not save window placement.

Focused offscreen checks passed 11 Qt cases including the existing startup and
close-audit cases and init/cleanup. Native Cocoa placement and Settings/quit checks
passed 4 cases including init/cleanup. The full Release UI executable passed all
62 cases. Tests cover default-off behavior, saved
coordinates, unchanged-position no-op, reopening/restoration, disabling/retaining
coordinates, off-screen fallback, and saving before hide. See [status](../doc/status.md)
for broader verification; Linux/Windows and actual monitor unplugging remain unverified.

## Remember window geometry — owner correction 2026-10-31

The owner clarified that the entire window should return as left, including size.
The setting is now labelled “Remember window geometry” and retains the existing
opt-in flag so an enabled preference stays enabled. Position, client size and
normal/maximized/full-screen state are saved in native preferences using
`Window/position`, `Window/size` and `Window/state`. Only changed values are written.
A previous position-only record remains usable; size is learned on the next quit.

Normal geometry is tracked in memory through moves/resizes. Maximized and
full-screen windows retain that geometry for restoring down. Sampling is deferred
until the window-state transition settles, avoiding overwriting normal dimensions
with maximized/full-screen dimensions. Minimized windows reopen visibly in their
previous non-minimized state. Disabling the preference leaves all saved geometry
untouched and ignores it on opening.

Restoration occurs before showing, chooses the display with greatest overlap (or
primary as fallback), and fits size/position into its available area subject to
application minimum dimensions. The existing quit-time save-before-hide behavior
and error handling remain. The closing-audit handoff is unchanged.

Focused Release offscreen geometry/startup checks passed 10 Qt cases including
init/cleanup. Native Cocoa tests passed 5 cases, including a taller resized window,
maximized reopening, restoring normal size, disabling persistence, off-screen
position recovery and quit capture. Native full-screen Spaces transitions, actual
monitor removal and other OSes remain unverified. See [status](../doc/status.md)
for full regression (63 cases passed) and additional offscreen full-screen-state
verification (3 cases passed including init/cleanup).
