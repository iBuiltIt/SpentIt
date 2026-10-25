# Feature discovery

These documents describe SpentIt — Where did my money go? They capture the initial discussion as of 2026-10-25 without requiring access to private notes or chat history.

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
| [002 — Accounts, money, and splits](002-accounts-money-splits.md) | active | Currency setup, exact money, balances, transfers, allocation invariants |
| [003 — QIF import and review](003-qif-import-review.md) | active | Immutable source, duplicate recognition, durable drafts, atomic commit |
| [004 — Rules and explanations](004-rules-and-explanations.md) | active | Regex rules, split classification, ordering, conflict feedback |
| [005 — Transaction sheet and navigation](005-transaction-sheet-navigation.md) | active | Accepted main layout, inline editing, balance toggle, search direction, navigation |
| [006 — Reporting and drill-down](006-reporting-drill-down.md) | active | Period comparisons, category totals, reproducible filters |
| [007 — Licensing and distribution](007-licensing-and-distribution.md) | active | Free-access intent, resale restriction, unresolved project license |
| [008 — Transfer matching and lifecycle](008-transfer-matching-and-lifecycle.md) | active | Per-account commits, protected drafts, matching provenance, repair, removal, navigation |

## Initial design sequence (proposed)

1. Specify money, split, transfer, draft, and history invariants and synthetic examples.
2. Validate encrypted persistence, recovery, and backup/restore behavior in an isolated prototype.
3. Prototype the demanding transaction-sheet interactions before finalizing UI architecture.
4. Build a vertical slice: import evidence → duplicate review → rule preview → edit → atomic commit → reconciled report.

The owner subsequently authorized the engine/build and currency/split-editing milestones; its implemented subset is recorded in feature 002 and [project status](../doc/status.md). Remaining sequence items are proposals, not a delivery commitment.

The owner also authorized verified historical-currency definitions followed by the isolated SQLCipher prototype. Those bounded scopes are implemented and documented in features 002 and 001; the larger features remain active.

The owner approved documenting the main UI layout and follow-up decisions on 2026-10-25. Feature 005 records the accepted shell, sidebar, ledger, search direction, and edit-navigation behavior; feature 002 records opening balances, account archiving, and intentional currency reassignment. Detailed search grammar and editor contents remain open. This is documentation approval, not implementation or final adoption of Qt.

The owner also accepted the [transfer lifecycle](008-transfer-matching-and-lifecycle.md): commits are explicit per account; committing an origin protects its generated destination draft without committing it. Matching, repair, provenance-dependent removal, and counterpart navigation are documented there. Feature 005 also records selection/view restoration when search clears. These requirements remain unimplemented.

The owner subsequently authorized the [synthetic sheet prototype](../doc/sheet-prototype.md). Its bounded scope is implemented and verified on macOS, with 100,000/200,000-row workload measurements. Production UI behavior, persistence integration and final Qt adoption remain open; feature 005 stays active.
