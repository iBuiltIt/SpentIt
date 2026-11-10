# 015 — File exchange and backup

Status: **active**
Last updated: 2026-11-09

File offers QIF Export, SpentIt Export, SpentIt Import, and Backup while a file
is open. Unsaved-edit decisions run first. Backup is the Settings backup flow.
The format and failure rules are in [doc/file-exchange.md](../doc/file-exchange.md).

QIF exports the current ledger for the selected accounts and inclusive dates,
including new and accepted rows. Categories are optional. Transfer references
remain. An opening balance is included by default. Text controls in the QIF
output are normalized to spaces. Stored text, amounts, and Undo are unchanged.

SpentIt Export selects Rules, Reports, and Categories independently. Import
shows the nonempty sections, appends rules and reports with new ids, and merges
categories by full name. Income conflicts require a choice. Missing
dependencies must be mapped or created from the imported categories. The save
is atomic, does not apply rules, and does not clear transaction Undo.

Exports are unencrypted text. They are a reconstruction path, not a backup.
They omit originals, recognition, history, and document settings.

## Not done

Report-results export, a database migration, and native file-picker overwrite
as a verified interaction. Database formats stay 70/69. This feature does not
change them.
