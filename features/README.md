# Feature discovery

**Next-session handoff (2026-11-04):** [Reports and reusable calendar](../doc/reports-calendar-handoff.md).
The report workspace and calendar interaction are settled; the compact-results
follow-up adds grouped-period bars and editable ledger drill-down.
The owner gave implementation Go. The initial scope is implemented; see
[verification and limits](../doc/status.md#reports-and-shared-calendar--2026-11-04).
This supersedes the earlier reporting creation-dialog/sidebar proposal.


**Next-session handoff (2026-11-03):** [transaction origin and ordinary operations](../doc/transaction-origin-handoff.md).
Includes the fixed manual category menu, remaining transfer gaps, proposed cleanup
and the separately undecided rule-matching policy.


[Manual transactions](../doc/manual-transactions.md) add ledger context-menu and
search-bar entry points, an unsaved editor, and explicit matching to later imports.
Current experimental formats are **48/47**; see [status](../doc/status.md).

The [completed-import Undo contract](../doc/import-undo.md) adds account-menu
**Undo last import…** and Review **Undo import…**, including reversal of a later
match into committed transfer work. Formats stay 46/45 with descriptive source
labels. See [status](../doc/status.md) for verification and limits.


**Next work (2026-11-02):** [category-gated rules and imported categories](004-rules-and-explanations.md#next-implementation-category-gated-rules-and-imported-categories--2026-11-02).
Accepted direction, not implemented; the handoff distinguishes remaining split
and import-lifecycle decisions from the settled category gate.

The [single rules workspace](004-rules-and-explanations.md#single-rules-workspace-and-independent-edits--owner-go-2026-11-01)
replaces the focused/all-rules presentation with independent selected-rule saves,
optional previews and a nonmodal leave-screen decision for all unsaved edits.
See [status](../doc/status.md) for verification.

The [account rule scope and Apply follow-up](004-rules-and-explanations.md#rule-scope-and-account-apply--owner-go-2026-11-01)
adds All transactions / New transactions scope, single-rule Apply, Save and apply,
and one Undo for mixed new/committed changes. See [status](../doc/status.md) for
verification.

The [ledger editing and session Undo follow-up](../doc/ledger-editing.md) is
implemented under the 2026-11-01 Go. Double-click toggles, selection keeps the
editor attached, Enter accepts a valid edit and session Undo reverses it. See
[status](../doc/status.md) for verification and limits.


The [opening/ledger cleanup and interrupted-import rollback](../doc/ui-import-cleanup.md)
is implemented under the owner's 2026-10-31 Go. It includes import routing,
Review repair/Commit, account-context Discard and automatic incomplete-preparation
cleanup. Completed uncommitted imports remain durable. Current formats are
**40/39**; see [status](../doc/status.md) for verification and platform limits.

These documents describe SpentIt — Where did my money go? They capture the initial discussion and accepted follow-ups through 2026-10-31 without requiring access to private notes or chat history.

## Status legend

| Status | Meaning |
|--------|---------|
| **active** | Current design or implementation work; not a claim that the feature exists. |
| **landed** | The stated scope is implemented and verified; retained for history and follow-ons. |
| **abandoned** | Not pursuing; retained only when useful as negative knowledge, with a reason. |

Every feature document carries a status, last-updated date, and implementation state. Separate **accepted requirements**, **proposed design**, and **open questions**. A document can contain accepted requirements while its implementation proposal remains unsettled.

Acceptance criteria are verification obligations, not by themselves reports of passing tests. Feature 002 records the implemented engine subset; feature 001 records the optional storage prototype; feature 005 records the optional synthetic sheet experiment. Both link actual verification. When implementation starts, add evidence of actual checks and identify unverified criteria. Keep this index and `doc/status.md` aligned with lifecycle changes.

Current correction (owner Go, 2026-10-30): [same-account duplicate matching in the common internal representation](../doc/import-commit-discussion.md#first-priority-correct-matching-in-the-common-internal-representation--2026-10-30), now implemented for unique exact original-field matches with ambiguity and bank-ID guards; see status for verification. Following item: the [unified Review sheet](005-transaction-sheet-navigation.md#unified-review-sheet--next-implementation-item-2026-10-29), covering all accounts with account separators, a normal-screen Review entry point, a shared route from Commit, and consistent outstanding-issue counts. The owner subsequently gave Go; the sheet, saved decisions, Merge/Link, amount-difference categorisation and stacked Undo are implemented. See the [implementation and verification](../doc/review-sheet.md).

The [2026-10-30 Merge/Link decision](005-transaction-sheet-navigation.md#accepted-merge-and-link-interaction--2026-10-30) specifies separate same-account Merge (preferred Alt+M), automatically keeping the sole committed transaction or asking for a survivor when zero or multiple selected transactions are committed, and transfer Link retaining both independent postings (preferred Alt+L). Merge retains source representations/history and must support future recognition through either original representation. The bounded implementation and safe Undo are recorded in the Review baseline; native platform shortcut behavior remains unverified.

The [dataset-wide commit/new-transaction model](../doc/import-commit-discussion.md) is implemented. Multiple imports accumulate saved transactions; strict bank-ID overlap recognizes accepted and uncommitted targets; duplicate-only sources complete without accepting older work. Commit accepts new transactions across accounts while import failures remain visible. Account-wide rollback removes exclusively owned uncommitted material and preserves accepted transactions and other retained sources. Existing creation/opening-review behavior remains. See [current status and verification](../doc/status.md#dataset-acceptance-storage-refactor--2026-10-29). This supersedes older per-account commit and compulsory permanent-evidence requirements, not the deferred scope of the broader features below.

The [Review workspace and link learning](005-transaction-sheet-navigation.md#next-session-handoff-review-workspace-and-link-learning--2026-10-30) revision is implemented under its subsequent Go and settled matching policy. The owner also authorized [Review responsiveness and batch decisions](005-transaction-sheet-navigation.md#next-tasks-review-responsiveness-and-batch-decisions--2026-10-31): permanent status in every right pane, stable viewport, profiled candidate rebuilding and atomic batch duplicate Merge with group selection and one Undo. See [the contract](../doc/review-sheet.md) and [status](../doc/status.md) for implementation, measurements and verification limits.

The [verified fast-startup contract](../doc/fast-startup.md) implements the owner's
2026-10-31 Go: full-file audit approval in the platform cache, graceful close-time
auditing, and deferred Review suggestions. See status for timings and recovery
verification; the subsequent closing-optimization Go adds safe opening-audit reuse
and bounded reconstruction caches within full audits. This does not change
financial acceptance or authorize automatic repair.

## Initial features

| Document | Status | Scope |
|----------|--------|-------|
| [001 — Documents, storage, and security](001-documents-storage-security.md) | active | Local documents, encryption, persistence, backup, recovery, history |
| [002 — Accounts, money, and splits](002-accounts-money-splits.md) | active | Account-scoped workspaces, rename/archive/delete, opening correction, currency reassignment, exact money and allocation invariants |
| [003 — QIF import and review](003-qif-import-review.md) | active | Immutable source, duplicate recognition, durable drafts, atomic commit; OFX/QFX/QBO/ASO bank and credit-card statements in schema 40/39; remaining QIF product scope stays open |
| [004 — Rules and explanations](004-rules-and-explanations.md) | active | Accepted rule policy, evaluator, preview adapter, Save/Apply, focused transaction Create/Edit, compact match-only previews and preservation of unmatched fields during Apply, and selected-account committed reapplication; baseline verified in Release and ASan/UBSan; starter catalog and independent colon-category model accepted on 2026-10-28; account aliases protected during rule evaluation |
| [005 — Transaction sheet and navigation](005-transaction-sheet-navigation.md) | active | Bounded combined search, multiselection, atomic committed bulk changes, and the point 8 session sidebar balances, column ratios, narrow scroll, and viewport retention; multiline notes and the rest of the layout stay open |
| [006 — Reporting and drill-down](006-reporting-drill-down.md) | active | Initial Rules-style workspace, encrypted definitions, exact allocation totals and editable ledger drill-down, compact striped results, separate/grouped-period Bar charts, Pie charts and reusable calendar implemented under Go; native/platform verification limits remain |
| [007 — Licensing and distribution](007-licensing-and-distribution.md) | active | Free-access intent, resale restriction, unresolved project license; theme-aware icons and local packaging with recorded platform limits |
| [008 — Transfer matching and lifecycle](008-transfer-matching-and-lifecycle.md) | active | Bounded same-currency durable creation, per-account commit/protection, provisional bank matching, counterpart navigation, allocation-level Merge, unlink, replacement, and a read-only journal implemented; QIF transfer syntax with explicit resolution and confirmed-mapping one-to-one repeated-group matching implemented; committed link edits for the locked 2026-10-27 cases implemented; account moves and source-link rewrite remain open |
| [009 — First durable import workflow](009-durable-import-milestone.md) | active | Original one-account scope, subsequently extended to independent account workspaces; headless parser/storage/reconciliation, bounded setup/review UI, inline editing/source copying, split previews, filter-return navigation, v3 ordinary profile and nullable-original repairs, controlled backup cancellation and measured durable-UI workloads; whole-source repair progress/Abort, explicit multi-account QIF mapping and staged reported-balance decisions and complete category-catalog publication implemented; selected current-format verification recorded |
| [010 — Categories and selection](010-categories-and-selection.md) | active | Accepted current taxonomy/lifecycle; independent colon categories, optional starter catalog, editor, allocation picker, and account transfer destinations/confirmed aliases on schemas 40/39; account rows visible read-only in the editor; ordinary-category bulk assignment and atomic account-destination assignment with group Undo; initial category reports implemented in feature 006 |
| [011 — Committed editing, removal, and undo](011-committed-editing-and-reversal.md) | active | Accepted ordinary correction/removal/strict-undo policy and explicit removed-record reimport review; ordinary implementation and schema-7 atomic bulk/group Undo verified on macOS within the recorded test scope; locked committed transfer edits implemented; account moves and source-link rewrite remain open |
| [012 — Application diagnostics](012-application-diagnostics.md) | active | Explicitly enabled local timing/state diagnostics, bounded collector, semantic UI and read-only IPC/CLI; seven-stage implementation has recorded coverage limits; screenshots and MCP excluded |
| [013 — Help system and user manual](013-help-system.md) | active | Viewer and chapters 1–7, through Add and correct transactions, with screenshots under the 2026-11-05 Gos. Chapter 6 is committed; chapter 7 is owner-accepted for commit; macOS checks and limits are recorded. Later chapters and context links still need their own Go. See [the handoff](../doc/help-prototype-handoff.md) |
| [014 — Busy feedback and operation progress](014-busy-feedback-and-progress.md) | active | Separate documented work item: reliable global activity feedback, account-switch readiness, import mapping/category progress transitions and required verification. Implementation awaits Go; independent of Help. |

The [coordinated roadmap](../doc/roadmap.md) records historical delegated implementation and the current per-change Go requirement. Diagnostics implementation is authorized; its runtime contract and verification record precede any further performance optimization or repeated-transfer matching. Material financial/security decisions remain explicit gates; feature 011 records the accepted ordinary committed-change policy and its bounded implementation on available macOS configurations.

Category work is ahead-of-roadmap discussion, recorded in [010 — Categories and selection](010-categories-and-selection.md). It preserves the owner's proposed list and rationale, accepted shallow hierarchy and neutral categories, QIF naming constraints, and unresolved catalog details. It does not expand the current import milestone.

## Initial design sequence (proposed)

1. Specify money, split, transfer, draft, and history invariants and synthetic examples.
2. Validate encrypted persistence, recovery, and backup/restore behavior in an isolated prototype.
3. Prototype the demanding transaction-sheet interactions before finalizing UI architecture.
4. Build a vertical slice: import evidence → duplicate review → rule preview → edit → atomic commit → reconciled report.

The owner subsequently authorized the engine/build and currency/split-editing milestones; its implemented subset is recorded in feature 002 and [project status](../doc/status.md). Remaining sequence items are proposals, not a delivery commitment.

The owner also authorized verified historical-currency definitions followed by the isolated SQLCipher prototype. Those bounded scopes are implemented and documented in features 002 and 001; the larger features remain active.

The owner approved documenting the main UI layout and follow-up decisions on 2026-10-25. Feature 005 records the accepted shell, sidebar, ledger, search direction, and edit-navigation behavior; feature 002 records opening balances, account archiving, and intentional currency reassignment. Search grammar and bounded bulk behavior were subsequently accepted and implemented on 2026-10-26; broader editor/layout contents remain open. The original approval concerned documentation; subsequent bounded implementations do not establish final adoption of Qt.

Historically, the owner accepted the [transfer lifecycle](008-transfer-matching-and-lifecycle.md): commits are explicit per account; committing an origin protects its generated destination draft without committing it. Matching, Merge, provenance-dependent removal, and counterpart navigation are documented there. Feature 005 also records selection/view restoration when search clears. The bounded transfer workflow, allocation-level Merge and unlink, and search selection/view restoration are implemented. Committed transfer reversal remains outstanding.

The owner subsequently authorized the [synthetic sheet prototype](../doc/sheet-prototype.md). Its bounded scope is implemented and verified on macOS, with 100,000/200,000-row workload measurements. Production UI behavior, persistence integration and final Qt adoption remain open; feature 005 stays active.

The owner authorized preparing the [first durable import milestone design](009-durable-import-milestone.md). The owner subsequently accepted its five policy groups, including a smaller first vertical slice with rules, transfers and category reports deferred. QIF will be completed first. Future QFX, ASO, QBO and OFX support is an accepted requirement, with detailed dialects and implementation deferred. The owner has matching private exports of the same real-world dataset in those four formats for possible future comparison. The owner clarified that each running application has one active dataset, selected by a Settings path, and can close it and open another. No application-wide instance limit is required; exclusive document locking remains in place. The owner subsequently gave Go; [headless implementation](../doc/durable-import.md) is in progress and a bounded [setup/review UI](../doc/import-ui.md) now connects it; broader UI and milestone work remain incomplete. Point 3 on 2026-10-27 implements OFX, QFX, QBO, and ASO statement imports in the working tree. The inspected ASO export is OFX 1.0.2 SGML credit-card statement syntax; no contents or paths were recorded. Feature 003 stays active. See the [statement profile](003-ofx-profile.md).

The owner also accepted [optional-password setup and recovery](001-documents-storage-security.md#accepted-setup-unlock-and-recovery-behavior--2026-10-25): protection defaults on, no-password mode deliberately waives confidentiality using a documented public format key, and setup suggests a browsable Documents/SpentIt location. The bounded [UI gateway](../doc/import-ui.md) now implements these requirements, with installation-independent recovery tests.
