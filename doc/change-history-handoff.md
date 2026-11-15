# Change history

Last updated: 2026-11-14

History is a viewer of the file-wide linear Undo/Redo stack. It lists applied
saved actions newest first across all accounts, with their existing transaction
and field-change details. The available Redo actions are listed separately,
next action first. There is no account filter or selective branch Undo.

Only the newest applied action offers Undo. Edit → Undo and Cmd+Z / Ctrl+Z
perform that same operation. Edit → Redo and the platform Redo shortcut replay
the next undone action. Native text editing keeps its own Undo/Redo. Ordinary
saved Undo/Redo runs directly; import Undo asks for confirmation and explains
that it clears Redo and cannot itself be redone.

An import is one action. Its automatic rules and mapping work belong to it.
A later explicit rule application, acceptance, merge, unmerge, bulk edit, or
transaction edit is its own action. Standalone definition and settings saves
remain outside Undo. Saved rules remain when their transaction application is
undone.

A new saved change clears Redo. Import Undo also clears it; the user can reimport
the original file. Successful close clears both stacks. Failed/cancelled close
preserves both. A crash preserves the existing durable Undo chain and saved
state, but loses in-memory Redo.

There is no new history table or schema change. See
[action Undo/Redo design](action-undo-design.md#version-1-linear-redo--2026-11-14).
The old connected-Undo storage helpers are retained for compatibility testing,
but are not exposed by the application.
