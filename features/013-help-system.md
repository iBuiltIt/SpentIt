# 013 — Help and the user manual

Status: **active**
Last updated: 2026-11-09

Help is an offline viewer of the bundled manual: twelve chapters, five quick
references, and 39 pictures. Sample topics in tests are fixtures, not the
manual. Contextual Help opens fixed `help:topic#section` targets from
workspaces, dialogs, leave windows, and error or status actions.

Do not edit `manual/` or its pictures unless the owner asks. Log the next
user-visible change in [doc/manual-update-backlog.md](../doc/manual-update-backlog.md).

## Behavior

- **SpentIt Help** keeps the reading position. **Help for this screen**, the
  workspace Help button, and the platform Help shortcut open the current task.
  A visible dialog takes priority.
- Dialog Help explains the decision and does not accept, reject, or run the
  command. Focus moving into Help pauses amount-expression normalization,
  unfinished category completion, and category draft autosave. Explicit saves
  still work after return.
- Rules and Reports Help stays available from their leave dialog, including
  while work is busy.
- Error and status Help opens Messages and problems, or the report problem
  section. Those actions do not retry the operation or pass its payload into
  Help.
- Targets are literal identifiers in the application QML. `help.prepare` checks
  them against the bundled topics. An unknown target leaves Home, Contents, and
  Search available and explains the miss.
- Search excerpts are readable text without Markdown markers, and they keep
  whole words at both ends. Long table chapters keep the full scene graph so
  reverse scrolling does not drop text.
- Help does not change financial state, selection, or unsaved edits. Closing
  it restores the previous interaction.
- Keyboard use includes Tab through contents and results, article links and
  pictures, and Close Help beside a decision dialog.

Topic and section identifiers stay stable. Picture capture for a future
authorized pass is in the manual backlog.

## Not in scope

A public documentation site, platform-specific manuals, printing or PDF export,
translations, interactive tours, executable examples, runtime AI answers, and
a separate help updater.

## Not verified

Linux, Windows, and a person clicking through Help. The owner closed standalone
screen-reader verification as skipped, not passed. Spoken accessibility is not
a release claim.
