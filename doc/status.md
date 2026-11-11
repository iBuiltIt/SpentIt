# Project status

Last updated: 2026-11-11

SpentIt is the local rules-enabled application. Working experimental formats
are **70 with rules / 69 without**. Older experimental files are rejected
unchanged, with no migration. The SpentIt exchange file is JSON version 1.

Build and launch are in the [README](../README.md) and [building](building.md).
macOS arm64 with Qt 6.12.0 is the host used for product work. Linux and Windows
have development notes and are not release-ready. Do not describe a check as
passed unless you ran it.

## What the application does

- Encrypted local files, optional explicit no-password mode, and private
  document locks. A backup is a copy of the file made after a successful close,
  while SpentIt is not running. Normal open and close use an encrypted clean
  marker. A crash or unclean file gets a full check, which does not repair
  data. There is no forgotten-password recovery. Settings can change, add, or
  remove a password. A copy keeps the password it was made with.
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
- File menu QIF export and SpentIt export/import of rules, reports, and
  categories.
- One file-wide Undo chain and a Change history page. A successful close
  expires ordinary Undo.
- Bundled Help for the current manual, with contextual links.

The feature documents and the contracts they name are the behavior. This page
does not repeat them. The [release plan](first-release-plan.md) says what
release 1 does not add.

## Still to do

- Linux `python3 tools/package.py` produced
  `dist/SpentIt-0.1.0-linux-x64-setup.run` (60566028 bytes) on 2026-11-11 in WSL Ubuntu 24.04.5
  x86_64, with the Qt 6.8.3 `gcc_64` prefix. The staged app and the installed
  copy each created a 942080-byte file with development libraries hidden. The
  installer was then uninstalled from an empty home and left a saved file in
  place. It was not launched from an application menu, and aarch64 was not
  packaged. See [packaging](../packaging/README.md).
- Windows `python tools/package.py` produced
  `dist/SpentIt-0.1.0-windows-x64-setup.exe` (44881059 bytes) on 2026-11-11
  with MSVC 19.44.35229.0. The staged app created a 942080-byte file with
  development tools hidden from `PATH`. The installer was not run, so the
  Start menu entry, desktop icon, and `.spentit` association were not
  exercised. `--sign` was not used. `launchDocumentArgument` and
  `launchDocument` passed: 6 tests, 0 failed, offscreen Qt 6.8.3. A
  password-protected launch stayed on the welcome screen and asked for the
  password. The remembered file was left unchanged. SQLCipher logged an HMAC
  failure while that open was refused.
- macOS uses the same command. `--sign` with a notary keychain profile
  produces a Developer ID DMG aimed at macOS 14.4. A Mac running 14.4 was not
  used. Publication is not approved. `./build/SpentIt` stays the development
  build.
- When the owner says to make the release schema, follow
  [release schema](release-schema.md). That stamps the rules database as
  version 1, with no migrator, and sets the application version. Do not do
  either earlier.

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

MAN-001 through MAN-029 are in the bundled manual. The backlog is empty. A
later user-visible change is logged in
[manual-update-backlog.md](manual-update-backlog.md) and waits for an
owner-authorized manual pass. Do not edit `manual/` as part of feature work.
