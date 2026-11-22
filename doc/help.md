# Help and the user manual

Help is an offline viewer for the bundled manual. Contextual Help opens
fixed `help:topic#section` targets from workspaces, dialogs, leave windows,
and error or status actions. Test topics are separate fixtures.

Do not edit `manual/` or its pictures unless the owner asks. Log the next
user-visible change in [manual update backlog](manual-update-backlog.md).

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
- Quitting closes Help, including an enlarged picture, before a quit question
  or the close itself. Help opened from that question stays until the next
  quit attempt. Cancel leaves Help closed. **SpentIt Help** still returns to
  the same place.
- Keyboard use includes Tab through contents and results, article links and
  pictures, and Close Help beside a decision dialog. The platform Close
  shortcut (**Command+W** on macOS) closes Help. In a narrow window it closes
  Help even when Contents is open. Escape still closes Contents first. On an
  enlarged picture, **Escape** or **Close** returns to the article.

Topic and section identifiers stay stable.

## Scope

A public documentation site, platform-specific manuals, printing or PDF export,
translations, interactive tours, executable examples, runtime AI answers, and
a separate help updater.
