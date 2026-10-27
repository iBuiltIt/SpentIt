# Feature discovery

These documents describe SpentIt — Where did my money go? They capture the initial discussion and accepted follow-ups through 2026-10-26 without requiring access to private notes or chat history.

## Status legend

| Status | Meaning |
|--------|---------|
| **active** | Current design or implementation work; not a claim that the feature exists. |
| **landed** | The stated scope is implemented and verified; retained for history and follow-ons. |
| **abandoned** | Not pursuing; retained only when useful as negative knowledge, with a reason. |

Every feature document carries a status, last-updated date, and implementation state. Separate **accepted requirements**, **proposed design**, and **open questions**. A document can contain accepted requirements while its implementation proposal remains unsettled.

Acceptance criteria are verification obligations, not by themselves reports of passing tests. Feature 002 records the implemented engine subset; feature 001 records the optional storage prototype; feature 005 records the optional synthetic sheet experiment. Both link actual verification. When implementation starts, add evidence of actual checks and identify unverified criteria. Keep this index and `doc/status.md` aligned with lifecycle changes.

## Initial features

| Document | Status | Scope |
|----------|--------|-------|
| [001 — Documents, storage, and security](001-documents-storage-security.md) | active | Local documents, encryption, persistence, backup, recovery, history |
| [002 — Accounts, money, and splits](002-accounts-money-splits.md) | active | Account-scoped durable workspaces, currency setup, exact money, balances, transfers, allocation invariants |
| [003 — QIF import and review](003-qif-import-review.md) | active | Immutable source, duplicate recognition, durable drafts, atomic commit |
| [004 — Rules and explanations](004-rules-and-explanations.md) | active | Accepted rule policy, evaluator, preview adapter, Save/Apply and rules sheet; baseline verified in Release and ASan/UBSan; committed rule reapplication remains deferred |
| [005 — Transaction sheet and navigation](005-transaction-sheet-navigation.md) | active | Bounded combined search, multiselection and atomic committed bulk changes; broader layout/navigation requirements remain active |
| [006 — Reporting and drill-down](006-reporting-drill-down.md) | active | Period comparisons, category totals, reproducible filters |
| [007 — Licensing and distribution](007-licensing-and-distribution.md) | active | Free-access intent, resale restriction, unresolved project license |
| [008 — Transfer matching and lifecycle](008-transfer-matching-and-lifecycle.md) | active | Bounded same-currency durable creation, per-account commit/protection, provisional bank matching and counterpart navigation implemented; QIF transfer syntax with explicit resolution implemented; repair and reversal remain unfinished |
| [009 — First durable import workflow](009-durable-import-milestone.md) | active | Original one-account scope, subsequently extended to independent account workspaces; headless parser/storage/reconciliation, bounded setup/review UI, inline editing/source copying, split previews, filter-return navigation, v3 ordinary profile and nullable-original repairs, controlled backup cancellation and measured durable-UI workloads; whole-source repair progress/Abort, explicit multi-account QIF mapping and staged reported-balance decisions implemented; selected current-format verification recorded |
| [010 — Categories and selection](010-categories-and-selection.md) | active | Accepted current taxonomy/lifecycle; bounded catalog, editor and allocation picker verified on macOS for schema 5 and retained on current schemas 14/15, including bulk assignment; reports and transfer destinations remain deferred |
| [011 — Committed editing, removal, and undo](011-committed-editing-and-reversal.md) | active | Accepted ordinary correction/removal/strict-undo policy and explicit removed-record reimport review; ordinary implementation and schema-7 atomic bulk/group Undo verified on macOS within the recorded test scope; transfer changes remain deferred |

The [coordinated roadmap](../doc/roadmap.md) records the owner's authorization to delegate and continue routine implementation without per-step approval. Material financial/security decisions remain explicit gates; feature 011 records the subsequently accepted ordinary committed-change policy; its bounded implementation is verified on available macOS configurations.

Category work is ahead-of-roadmap discussion, recorded in [010 — Categories and selection](010-categories-and-selection.md). It preserves the owner's proposed list and rationale, accepted shallow hierarchy and neutral categories, QIF naming constraints, and unresolved catalog details. It does not expand the current import milestone.

## Initial design sequence (proposed)

1. Specify money, split, transfer, draft, and history invariants and synthetic examples.
2. Validate encrypted persistence, recovery, and backup/restore behavior in an isolated prototype.
3. Prototype the demanding transaction-sheet interactions before finalizing UI architecture.
4. Build a vertical slice: import evidence → duplicate review → rule preview → edit → atomic commit → reconciled report.

The owner subsequently authorized the engine/build and currency/split-editing milestones; its implemented subset is recorded in feature 002 and [project status](../doc/status.md). Remaining sequence items are proposals, not a delivery commitment.

The owner also authorized verified historical-currency definitions followed by the isolated SQLCipher prototype. Those bounded scopes are implemented and documented in features 002 and 001; the larger features remain active.

The owner approved documenting the main UI layout and follow-up decisions on 2026-10-25. Feature 005 records the accepted shell, sidebar, ledger, search direction, and edit-navigation behavior; feature 002 records opening balances, account archiving, and intentional currency reassignment. Search grammar and bounded bulk behavior were subsequently accepted and implemented on 2026-10-26; broader editor/layout contents remain open. The original approval concerned documentation; subsequent bounded implementations do not establish final adoption of Qt.

The owner also accepted the [transfer lifecycle](008-transfer-matching-and-lifecycle.md): commits are explicit per account; committing an origin protects its generated destination draft without committing it. Matching, repair, provenance-dependent removal, and counterpart navigation are documented there. Feature 005 also records selection/view restoration when search clears. The bounded transfer workflow and search selection/view restoration are implemented; full transfer repair/reversal remains outstanding.

The owner subsequently authorized the [synthetic sheet prototype](../doc/sheet-prototype.md). Its bounded scope is implemented and verified on macOS, with 100,000/200,000-row workload measurements. Production UI behavior, persistence integration and final Qt adoption remain open; feature 005 stays active.

The owner authorized preparing the [first durable import milestone design](009-durable-import-milestone.md). The owner subsequently accepted its five policy groups, including a smaller first vertical slice with rules, transfers and category reports deferred. QIF will be completed first. Future QFX, ASO, QBO and OFX support is an accepted requirement, with detailed dialects and implementation deferred. The owner has matching private exports of the same real-world dataset in those four formats for possible future comparison. The owner clarified that each running application has one active dataset, selected by a Settings path, and can close it and open another. No application-wide instance limit is required; exclusive document locking remains in place. The owner subsequently gave Go; [headless implementation](../doc/durable-import.md) is in progress and a bounded [setup/review UI](../doc/import-ui.md) now connects it; broader UI and milestone work remain incomplete.

The owner also accepted [optional-password setup and recovery](001-documents-storage-security.md#accepted-setup-unlock-and-recovery-behavior--2026-10-25): protection defaults on, no-password mode deliberately waives confidentiality using a documented public format key, and setup suggests a browsable Documents/SpentIt location. The bounded [UI gateway](../doc/import-ui.md) now implements these requirements, with installation-independent recovery tests.
