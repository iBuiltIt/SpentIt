# Help viewer prototype handoff

Last updated: 2026-11-05

Owner Go on 2026-11-05 covers the viewer prototype only. It does not cover real
chapters or application-wide context links. The design is
[013 — Help system and user manual](../features/013-help-system.md). This file
is the progress record for the next session. Update it when a step changes.

## Current step

The viewer prototype is implemented and the macOS checks below passed. Stop
here. Real chapters and context links need their own Go. Leave
`rulesLeaveDialog` at `Qt.ApplicationModal`.

## Scope of this Go

In:

- Sample topics bundled into `./build/SpentIt`, loaded from the build tree.
- The Help window identifies the text as sample content.
- Contents, previous/next, Back/Forward, Home, find, and search.
- Highlighted matches that stay selectable, with sample links still working.
- Captions, section navigation, a wide table, and an image that opens Fit,
  actual size, zoom, and pan.
- Keyboard use of those controls, with a visible focus mark.
- Help beside `rulesLeaveDialog` (“Save changes before leaving Rules?”).
  The dialog stays application-modal. Closing Help returns focus to it.
  Escape in Help does not choose Stay.
- Build-time and runtime rejection of external URLs, arbitrary file paths, and
  paths that leave the bundle.

Out:

- Real manual chapters and screenshots of the application.
- Help actions on workspaces, dialogs, and errors.
- A new renderer dependency (Qt Help, WebEngine, or a second process).
- Search ranking, the 720-pixel contents collapse, and the sample topic split
  are prototype choices recorded below. They are not a permanent manual design.

## Status

| Step | State | Where to look |
| --- | --- | --- |
| 1. Sample bundle and link checker | macOS check passed | `manual/`, `tools/prepare_help.py`, `tests/ui/help_prepare_tests.py` |
| 2. Viewer, search, highlight, image | macOS check passed | `src/ui/help/` |
| 3. Keyboard focus | macOS check passed, with the key-window limit below | `src/ui/help/HelpWindow.qml` |
| 4. Rules Save / Discard / Stay | macOS check passed | `src/ui/ImportMain.qml`, `src/ui/ReportsWorkspace.qml` |
| 5. Rejected links, build and runtime | macOS check passed | `tools/prepare_help.py`, `src/ui/help/help_catalog.cpp` |
| 6. Renderer record | recorded; anchors and table scroll are limits | the notes below |
| 7. `./build/SpentIt` and tests | macOS commands below passed | this section |

## What the checks showed

Help is one nonmodal window, created when the user opens it. It does not use
the document worker, the engine, or storage. The Help menu item is “SpentIt
Help”. The same opener is bound to the Help key on the main window and on the
Rules and Reports leave windows.

While a leave dialog is open, Help’s transient parent is that dialog. The
parent is set back to the main window before the dialog hides, so hiding the
dialog leaves Help open. `dismiss()` clears the transient parent and then
hides the window. `close()` on a transient child of the dialog was not a
reliable hide by itself.

Escape on `rulesLeaveDialog` calls `stayInRules()`. Escape on the Reports
leave window calls `stay()`. An experiment that made those keys close Help
first was removed. It answered Stay’s key as Close Help, which the design
forbids.

The picture view and the Help page handle Escape on the focused item, in
addition to the window shortcut. A window shortcut does not run unless that
window is the key window. On an earlier Cocoa run of this test process,
`QGuiApplication::focusWindow()` stayed empty, including before any dialog,
while a key delivered to the focused rule field still changed its text. The
final run did not log `focusWindow()`. It did show the item behavior below.

The passing checks do not send Escape to Help while a leave dialog is open.
On the offscreen plugin, that key was handled by the dialog as Stay. The Cocoa
check closed Help with `close()` instead, which is the window close path. A
person whose key window is still the dialog may still choose Stay with Escape.
Use the Close button or the window close control for that case. Do not route
the dialog’s Escape to Help.

Offscreen `import.help_ui` prints `This plugin does not support raise()`.
That warning is from the offscreen plugin. The checks still passed.

## Renderer notes from Qt 6.12

A small offscreen probe against the installed Qt 6.12 Quick `TextEdit`
Markdown renderer showed:

- A GitHub-style table becomes a real table (3 columns in the probe). At a
  narrow text width the table wraps inside the article instead of growing a
  separate horizontal scroller.
- An image loaded from a `file:` URL is present in the document. The bundled
  sample PNG also loads from `qrc:/spentit/help/images/sample-mark.png` in the
  Help window (`Image.Ready`).
- Link targets are kept, including a `help:` target.
- A background highlight on a found range leaves the surrounding links in
  place, and the highlighted word stays selectable.
- Heading anchors are not created. `{#overview}` is shown as ordinary heading
  text. A raw HTML `<a id>` is removed. `anchorNames` was 0.

The prototype therefore keeps Qt Markdown. The preparer strips a `{#id}`
marker, stores the id, and rewrites a section link to `help:topic#section`.
The viewer scrolls to the heading text. That is navigation around the missing
anchor, not a second renderer.

If heading scroll or wrapped tables are not good enough for real chapters, the
proposed next step is generated HTML in the existing `TextEdit` rich-text
path. That is already linked. Do not add Qt Help or WebEngine unless a later
decision says the rich-text path failed too.

## Prototype choices

- Metadata is frontmatter at the top of each topic: `id`, `title`, `order`,
  and optional `keywords`. One manifest is generated. There is no second
  navigation list.
- Image ids are file names. Names must be unique. Every image needs alt text
  and an italic caption line immediately after it.
- Search requires every word to match. Title matches rank above keyword
  matches, which rank above body matches. Ties follow topic order.
- Below 720 pixels, Contents collapses behind a Contents button. The article
  keeps its scroll position.
- Help geometry and text scale are stored in application `QSettings` under
  `help/`. The open topic and search are kept only for the session.
- The sample banner stays while every topic id starts with `sample-`. The
  banner says “This is sample content for checking Help. It is not the SpentIt
  manual.”
- Sample dates are year-month-day and sample amounts use comma thousands and
  dot cents (`2026-01-15`, `1,234.56`). The topic says the reader’s file
  follows its saved settings.

## Build and check

The rules-enabled Release tree is `build/import-ui-rules-release`. The
executable copied for normal use is `./build/SpentIt`. Qt is 6.12.0. The last
app link in this session was
`cmake --build build/import-ui-rules-release --target spentit_import_experiment -j 2`.

Commands that passed on this Mac, 2026-11-05:

```
ctest --test-dir build/import-ui-rules-release -R '^(help\.prepare|help\.catalog|import\.help_ui|import\.qml|import\.rules_ui)$' --output-on-failure
```

5 tests passed, 26.31s. `import.help_ui` runs with `QT_QPA_PLATFORM=offscreen`
and passed in 1.75s. `import.rules_ui` passed in 23.68s. `import.qml` passed
in 0.76s. `help.prepare` and `help.catalog` passed.

```
./build/import-ui-rules-release/src/ui/spentit_help_ui_tests
```

No `QT_QPA_PLATFORM` was set, so this used Cocoa. 7 checks passed, 2.26s:

- Help opens with no file, shows the sample banner and `1,234.56`, loads the
  sample image, follows the sample section link, selects the highlighted word
  “lavender”, searches “second row”, rejects `https://example.com`, and closes
  with Escape.
- With the Rules dialog open, modality stays `Qt.ApplicationModal`, Help’s
  transient parent is the dialog, and the expression stays `^Sample help$`.
  Before the dialog, Z appended `z` to that expression. During the dialog, Z
  left it unchanged. A click on Larger text increased the Help text scale.
  `close()` hid Help and left the dialog visible. Stay then had Qt focus.
  Reopening Help attached it to the dialog again. Stay hid the dialog, left
  Help visible, and set Help’s transient parent back to the main window. The
  expression was still `^Sample help$` and the rules page was still dirty.
- Help opened before the Rules dialog stayed open across Stay, and its
  transient parent returned to the main window.
- The same parent and Stay behavior held for the Reports leave dialog.

Linux and Windows were not run. Nobody clicked through a normal
`./build/SpentIt` session in this pass; the Cocoa test binary loaded the same
Help window and the same Rules dialog. The test calls `openHelp()`. It does
not click the Help menu item. The menu item is wired to that function.

`help.prepare` rejects an external URL, a `file:` URL, an absolute path, a
path that leaves `manual/`, and an unknown section. The running viewer’s
`https://example.com` check shows “That link is not part of Help.” A missing
sample image in the window was not given its own check.

Keyboard checks use `forceActiveFocus` and `QTest::keyClick`. They do not walk
every control with Tab only.

## Files

- `manual/sample/*.md`, `manual/images/sample-mark.png` — sample only.
- `tools/prepare_help.py` — builds the help bundle and rejects unsafe links.
- `src/ui/help/help_catalog.*` — manifest, search, allowlist, highlight.
- `src/ui/help/HelpWindow.qml` — the Help window and image view.
- `src/ui/ImportMain.qml` — Help menu, Help key, and the Rules dialog parent.
- `src/ui/ReportsWorkspace.qml` — the same parent hook for the Reports leave window.
- `tests/ui/help_prepare_tests.py`, `help_catalog_tests.cpp`, `help_ui_tests.cpp`.

## Next agent

1. Read this file and the design section “Implementation sequence”.
2. Do not author real chapters or context links unless the owner gives that Go.
3. Do not change `rulesLeaveDialog` away from `Qt.ApplicationModal`, and do
   not add a second process, to make Help easier to reach.
4. If you change Help, rerun the two commands in “Build and check” and write
   the new result here before calling the prototype done.

## Tradeoff

None. Help and the Rules dialog stay in one process. The dialog stays
application-modal, and the Cocoa check showed that a key which changes the
rule expression before the dialog does not change it while the dialog is open.
Help still took a click, closed without hiding the dialog, and returned Qt
focus to Stay.
