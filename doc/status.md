# Project status

Last updated: 2026-11-09

SpentIt is the local rules-enabled application. Working experimental formats
are **70 with rules / 69 without**. Older experimental files are rejected
unchanged, with no migration. The SpentIt exchange file is JSON version 1.

Build and launch are in the [README](../README.md) and [building](building.md).
macOS arm64 with Qt 6.12.0 is the host used for product work. Linux and Windows
have development notes and are not release-ready. Do not describe a check as
passed unless you ran it.

## What the application does

- Encrypted local files, optional explicit no-password mode, backup and restore,
  and private document locks. Normal open and close use an encrypted clean
  marker. A crash or unclean file gets a full check. Check file does not repair
  data. There is no forgotten-password recovery and no password change.
- QIF bank/cash/credit-card files under the [bank-v6 profile](../features/009-qif-profile.md),
  and OFX, QFX, QBO, and ASO bank and credit-card statements under the
  [statement profile](../features/003-ofx-profile.md). New-account mappings must
  choose zero or the reported opening literally.
- One balance that includes new and accepted transactions. Accept applies to
  new transactions across the file. Undo last import rolls back one source and
  leaves accepted transactions in place.
- Review for duplicates, transfers, and import problems, plus ledger duplicate
  resolution. Same-calendar-date duplicate suggestions. Strict bank IDs when
  both sides have them.
- Manual transactions, ledger editing, splits, amount expressions, amount
  formatting, Set memo, Rename payee, and Restore imported values.
- Categories with an Income flag, rules that assign an explicit category to
  Uncategorized items only, and reports with exact allocation totals and
  drill-down.
- File menu QIF export, SpentIt export/import of rules, reports, and categories,
  and Backup.
- One file-wide Undo chain and a Change history page. A successful close
  expires ordinary Undo.
- Bundled Help for the current manual, with contextual links. The manual pass
  through MAN-020 is done.
- The owner's usage-driven UI pass is complete. Those changes were made
  piecemeal and are in the application.

The feature documents and the contracts they name are the behavior. This page
does not repeat them.

## Open work

These items are not done. They need their own discussion and Go before
implementation, except where a document already says the owner deferred them.

- Current-value rule matching is accepted for the first release and is not
  implemented. Rules still match immutable imported fields.
- The broader busy/readiness audit is not done. The rotating indicator, import
  and shutdown progress, and the shared calendar are in place. Feedback should
  eventually last until the resulting screen is usable and conflicting actions
  are unavailable.
- Moving a committed transaction to another account, and rewriting
  source-occurrence links, are not done.
- Cross-currency matching, fee matching, and broader fuzzy automatic matching
  are deferred. Reports do not convert currencies.
- Multiline notes and saved layout customization are deferred.
- Document lifecycle and recovery is not complete. Password changes are
  deferred. Explicit schema migrations wait until the first release is
  announced; until then older experimental files stay rejected. Backup and
  restore exist. A further polished recovery workflow is still open. There is
  no forgotten-password recovery.
- Distribution is not complete. SpentIt is under the Unlicense in
  [LICENSE](../LICENSE). Linux and Windows are development builds and are not
  release validation. Packaging for publication and signing are open. The
  application UI is Qt. See the [release plan](first-release-plan.md).
- Category history, undone Review-link history, link-exclusion history, and
  account, opening, and currency command history still survive successful close.
  The owner deferred that retention cleanup. It is not the next task. Revisit
  it only after a separate discussion.

## Limits that still bind a claim

- The owner-reported Rules crash is a native accessibility notification during
  a model update (`EXC_GUARD` / `GUARD_TYPE_MACH_PORT` through
  `QQuickAccessibleAttached::setName`). A bounded accessible-name mitigation is
  in place. The crash was not reproduced, and it is not proven fixed.
- Spoken screen-reader output, Linux, Windows, and a person clicking through
  Help were not established as release verification.
- Interactive native file-picker overwrite for export was not established.
  Export uses QSaveFile and the Qt file dialog.
- Do not describe the Rules accessibility crash, another platform, or a full
  repository run as passed without a new run.

## Manual

MAN-001 through MAN-020 are in the bundled manual. The backlog is empty. A
later user-visible change is logged in
[manual-update-backlog.md](manual-update-backlog.md) and waits for an
owner-authorized manual pass. Do not edit `manual/` as part of feature work.
