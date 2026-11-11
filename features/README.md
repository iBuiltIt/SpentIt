# Feature discovery

These documents are the current requirements. A feature stays **active** while
open work or unverified release scope remains. **Landed** means the stated
scope is implemented and the required verification is recorded. **Abandoned**
means it is not being pursued.

Accepted requirements, proposed design, and open questions stay distinct.
Acceptance criteria are obligations, not reports that tests passed.

| Document | Status | Current scope |
| --- | --- | --- |
| [001 — Documents, storage, and security](001-documents-storage-security.md) | active | Encrypted local files, locks, clean-marker open/close. A file copy after a successful close is the backup. Password change is deferred. |
| [002 — Accounts, money, and splits](002-accounts-money-splits.md) | active | Exact money, account lifecycle, openings, currency reassignment without a rate. |
| [003 — QIF import and review](003-qif-import-review.md) | active | Bank-v6 QIF and OFX-family statements, dataset-wide acceptance, source-owned import Undo. |
| [004 — Rules and explanations](004-rules-and-explanations.md) | active | Category-gated rules on imported fields. Current-value matching is accepted and not implemented. |
| [005 — Transaction sheet and navigation](005-transaction-sheet-navigation.md) | active | Ledger, search, selection, return position. Multiline notes and saved layout are deferred. |
| [006 — Reporting and drill-down](006-reporting-drill-down.md) | active | Saved reports, exact allocation totals, charts, calendar, editable drill-down. |
| [007 — Licensing and distribution](007-licensing-and-distribution.md) | active | SpentIt is under the Unlicense. Third-party components keep their own licenses. Publication is not done. |
| [008 — Transfer matching and lifecycle](008-transfer-matching-and-lifecycle.md) | active | Explicit same-currency links. Account moves and source-link rewrite are not done. |
| [009 — Durable import](009-durable-import-milestone.md) | active | The import workflow in the application. Further dialects are not authorized. |
| [010 — Categories and selection](010-categories-and-selection.md) | active | Independent categories, Income flag, starter list, ledger category menu. |
| [011 — Committed editing and reversal](011-committed-editing-and-reversal.md) | active | Corrections, removal, and strict Undo. Account moves stay open. |
| [012 — Application diagnostics](012-application-diagnostics.md) | active | Opt-in local read-only diagnostics. No MCP and no screenshot feature. |
| [013 — Help](013-help-system.md) | active | Bundled manual and contextual Help. Do not edit `manual/` unless the owner asks. |
| [014 — Busy feedback](014-busy-feedback-and-progress.md) | active | Indicator and progress dialogs are in place. The readiness audit is not done. |
| [015 — File exchange](015-file-exchange.md) | active | QIF export and SpentIt export/import. |

Open release work is in [doc/first-release-plan.md](../doc/first-release-plan.md)
and [doc/status.md](../doc/status.md).
