# Session history

Last updated: 2026-11-09

Ordinary Change History and Undo belong to the current session. A successful
close makes the saved state the next baseline. Ordinary earlier edits are no
longer listed or undoable. Closing does not accept transactions, discard them,
or change balances.

Do not keep a permanent record, fingerprint, or monetary summary of every
expired ordinary revision. Compact command receipts may reject a duplicate
execution and a conflicting operation id. They must not archive transaction
history. After a successful close, an old retry may report already completed
and require a refresh. Never run a completed operation again, and never present
current data as that operation's old result.

## What survives a successful close

- Current transactions, amounts, currencies, splits, dates, text, acceptance,
  accounts, openings, categories, rules, preferences, and reports.
- Original imported facts and the source associations needed to show or restore
  them and to recognize duplicates. Do not invent an original for a manual
  entry. Do not keep a new archive of forgotten source files.
- Current transfer relationships, generated-counterpart ownership, saved Review
  decisions, exclusions, and their conflict tokens.
- Enough removed-transaction identity to keep a reimport unresolved.
- Source-scoped import Undo: prior transfer arrangements, ownership, and the
  dependency information [import Undo](import-undo.md) needs, including a match
  made in an earlier session. An import that contains accepted transactions is
  not eligible for whole-import Undo.
- Rule definitions still referenced by a retained draft, entry, or allocation,
  plus one current ruleset baseline including deleted identities. Full save
  requests and superseded snapshots expire. See
  [feature 004](../features/004-rules-and-explanations.md).

Expire ordinary edit snapshots once nothing retained refers to them. Records
kept only for import Undo must not appear as ordinary Change History rows.

## What is deferred

Category history, undone Review-link history, link-exclusion history, and
account, opening, and currency command history still survive close. That
cleanup is not authorized. See [status](status.md).

## Close and recovery

Publish the baseline, prune eligible session history, and mark the file clean
in one encrypted transaction. Resolve unsaved edits and any acceptance decision
first. Hold the document lock through completion. Do not copy the whole file
on every close. Deleting history may not shrink the file. Do not VACUUM as
part of ordinary close.

A failed or cancelled close, or a crash, keeps the session journal. The next
open recovers and runs a full check before writable use. A later successful
close expires that journal.

Do not reset conflict generations when compacting. Do not synthesize a
balancing transaction or change an opening to make history shorter.
