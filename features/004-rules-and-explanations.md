# 004 — Rules and explanations

Status: **active**
Last updated: 2026-11-11

Rules assign an explicit ordinary category. They do not create transfers,
change amounts, dates, or split structure, or force a category that is already
set. Only Uncategorized allocations are eligible. There is no parent-category
inheritance.

The evaluator is in [doc/rules-engine.md](../doc/rules-engine.md). A rules-off
file has no rule commands.

## Matching and application

- Source-split rules run first, then transaction rules, in list order inside
  each phase. The first categorizing match wins. Transaction rules fill only
  the remaining Uncategorized allocations.
- Matching searches immutable imported fields. Explicit anchors select a whole
  field. Case-insensitive UTF-8 is the default. Invalid patterns and resource
  failures are visible errors.
- A manual field, including a blank, stays until the user replaces it.
  Restore imported values returns eligible text to source ownership when it
  matches the original. A memo written by Set memo is manual.
- Disabled or deleted rules do not reverse earlier assignments.
- Scope is **All transactions in this account** (default) or **New transactions
  only**. Preview is optional and shows one row per eligible match. Apply and
  Save and apply use that scope. Save and apply saves the selected definition
  and applies it in one transaction. Undo restores the transactions and keeps
  the saved rule.
- One Apply is one Undo action, including mixed new and accepted rows.
  Unchanged rows add no Undo members. Rules that run during an import stay
  inside that Import action.
- Accepted-account reapply of a saved rule still has its own preview limit.
  Account and pending previews do not use the old 4,096-row ceiling.
- A live transfer category is left unchanged and named as a skip. A payee
  change on that row can still be written.

## Missing imported categories

Before automatic rules or acceptance, a missing imported category needs an
import-wide choice: create it or use Uncategorized. The choice is durable for
that import.

## Workspace

One searchable list and one expanded editor. One new draft at a time. Save rule
and Discard changes affect the selected definition. Leaving with outstanding
edits offers Save all changes, Discard all changes, or Cancel. Save validates
every outstanding edit and does not apply transactions.

Delete rule… is a named confirmation. It removes that definition and does not
reverse transactions. Afterward, select the existing new draft first.

The Apply section has its own account picker and the scope control. Switching
accounts stays in Rules, keeps drafts, and clears the preview.

The ledger **Create rule…** command needs exactly one eligible selection. It
does not open the transaction editor. Several targets use the existing chooser.

## Retention

Successful close drops full rule-save requests and superseded ruleset
snapshots. It keeps one current baseline, including deleted identities, and any
older definition still referenced by a retained draft, entry, or allocation.
Compact request fingerprints still reject an exact retry and a conflicting
operation id. A failed close keeps the session. Verification checks the
baseline and later saves. It does not replay expired requests.

The owner-reported Rules accessibility crash is not proven fixed. See
[status](../doc/status.md).
