# SpentIt contracts

Start with [contributor instructions](../AGENTS.md), [project status](status.md),
and the [source layout](project-layout.md). Read the contracts for the work at
hand, then check the implementation and tests.

| Area | Start here |
| --- | --- |
| Build and verification | [Application build and test commands](building.md), [Linux setup](linux-development-setup.md), [Windows setup](windows-development-setup.md), [fuzzing and recovery checks](hardening.md) |
| Documents and recovery | [Creation, protection, open, and close](documents.md), [release schema and upgrades](release-schema.md), [document locks](document-locking.md), [open and close checks](fast-startup.md) |
| Accounts and money | [Account commands and exact money](accounts.md), [split validation](split-editing.md) |
| Import and acceptance | [Import entry points and workflow](import-and-review.md), [QIF grammar](qif-profile.md), [OFX-family grammar](statement-profile.md), [account mapping](qif-account-imports.md), [acceptance and recognition](import-commit-discussion.md) |
| Review and transfers | [Review decisions](review-sheet.md), [transfer lifecycle](transfers.md), [QIF destinations](qif-transfers.md) |
| Ledger and editing | [Ledger and navigation](ledger.md), [search and selection](search-ledger.md), [editing and amounts](ledger-editing.md), [manual transactions](manual-transactions.md), [accepted transaction revisions](committed-changes.md) |
| Undo and history | [File-wide Undo and Redo](action-undo-design.md), [import Undo](import-undo.md), [session history](session-history-baseline.md), [Change history](action-undo-design.md#change-history) |
| Categories and rules | [Categories and pickers](categories.md), [rules workspace and application](rules.md), [rule evaluator](rules-engine.md), [rule storage](durable-rules.md) |
| Reporting and exchange | [Report totals and drill-down](reporting.md), [PDF export](report-pdf.md), [QIF export and SpentIt JSON exchange](file-exchange.md) |
| Help and UI | [Contextual Help](help.md), [user-facing language](user-facing-language.md), [operation progress](documents.md#operation-progress), [manual update backlog](manual-update-backlog.md), [status explanation source](status-message-help-draft.md) |
| Diagnostics | [Agent use and attachment](agent-diagnostics-guide.md), [diagnostics contract](application-diagnostics.md), [QIF diagnostic](qif-diagnostics.md) |
| Distribution | [Licensing](licensing.md), [platform packaging](../packaging/README.md) |
