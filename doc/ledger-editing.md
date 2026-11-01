# Ledger editing and session Undo

Status: implemented under owner Go, 2026-11-01; verification is recorded in
[status](status.md). This supersedes the older inline Save draft / Cancel edit
and explicit committed-edit opt-in presentation.

## Accepted interaction

- Double-click opens a transaction; double-clicking that same transaction closes
  it. Double-clicking another switches the one active editor.
- Single-click selection remains independent. The open editor stays attached to
  its transaction, with no empty reserved space or move to the bottom panel.
  Filtered-out and otherwise absent records retain the existing fallback inspector.
  Follow-up Go (2026-11-01): ordinary single-click selection does not enter the
  global busy state or flash the editor/sidebar disabled styles. An unchanged
  selection is a no-op. Plain selection uses the published ledger and cached
  transfer flags; range/toggle selection retains the existing bounded worker path.
  Actual operations continue to disable controls while running.
- Enter accepts the complete valid edit durably and retains its viewport and field
  focus. Ordinary category-picker choices also accept the edit. There are no
  Save draft / Cancel edit buttons or separate committed-correction Apply step.
- Escape abandons the current, unaccepted buffer. Invalid input remains visible;
  it cannot replace saved values. Leaving an unaccepted buffer still offers
  Save / Discard / Stay. These choices concern that buffer, not previous saves.
- A split edit is accepted as a complete transaction. Intermediate split amounts
  may be inconsistent while typing, but acceptance requires exact reconciliation.
- Cmd+Z / Ctrl+Z in a clean editor or the Undo edit button reverses accepted
  edits. While typing unaccepted text, the text field retains native text Undo.
  Enter does not advance to another transaction.

## Undo scope and persistence

Accepted edits survive closing and reopening. The editor Undo capability is
session-only, for a consecutive sequence of accepted edits; other dataset
mutations end that sequence. Selection, inspection and filtering do not end it.
Undo restores values, split identities, ownership/provenance, and any transfer
relationships changed by the edit together. No partial restoration is permitted.
There is no new Redo command.

The worker retains only operation identities and transaction/account references
for this capability. Exact snapshots use the existing encrypted durable journal,
its mutation-generation gate and stacked reversal logic. Accept and Undo are
complete atomic repository operations with durable outcome lookup. Failed
validation preserves the buffer; an unknown outcome or failed post-save refresh
requires reopening, with an accurate explanation. Committed edits remain in
persistent history, whose existing strict individual Undo is still available
when eligible after a restart. Session Undo does not erase history.

The ledger command requires an existing valid transaction. First repair of a
malformed import still uses the existing repair path in Review; restoring an
absent pre-repair transaction is outside this ledger-edit scope. Existing generated
counterpart and protected-transfer edit restrictions remain in force.

No schema change: formats remain 40 with rules / 39 without. The full-audit
verifier identity changes with the implementation. No migrations or private
financial fixtures are introduced.
