# Help viewer prototype handoff

Last updated: 2026-11-05

The viewer prototype Go on 2026-11-05 does not cover application-wide context
links. A second Go the same day covered the introduction; a subsequent Go covers
chapter 2, its screenshots, and checking it in Help. The design
is [013 — Help system and user manual](../features/013-help-system.md). This
file is the progress record for the next session. Update it when a step changes.

## Current step

The Help window opens [Understand your spending](../manual/index.md), followed
by [Create or open your file](../manual/create-open-file.md). Both are shipped. Sample topics used to check the viewer are in
`tests/fixtures/help/` and are not shown in Help. Further chapters and context
links need their own Go. Leave `rulesLeaveDialog` at `Qt.ApplicationModal`.

The introduction follows a grocery purchase through Chequing and a credit card;
a Market purchase of 42.18 on 2026-01-15; a 6.00 refund on 2026-01-20; and a
36.18 credit-card payment from Chequing on 2026-01-28. Dates in the guide are
year-month-day. Amounts use a comma for thousands and a dot for cents. The
chapter says the reader's file follows the date and amount settings saved for
that file. It has no application screenshot. It names Import…, + New
transaction, and Accept transactions. It says transactions that are not yet
accepted stay in the file and count in balances and reports, and that a
recorded transfer is left out of report spending.

## Chapter 2 — 2026-11-05

The owner gave Go for [Create or open your file](../manual/create-open-file.md),
its screenshots, and verification in Help, with explicit direction to keep the
plain-English voice concise. The chapter covers creation choices, password and
overwrite consequences, initial orientation, opening/unlocking, common opening
problems, closing/switching, and saved versus unfinished work. It is the second
bundled topic; chapter 1 remains the opening topic.

Three screenshots show the real `ImportMain.qml` through the native Qt test
harness, using an isolated temporary Household file, Chequing, CAD, and light
appearance. No private data or user settings were used. Computer-use attachment
to the standalone executable was unavailable; this is a test-harness walkthrough,
not a person clicking through `./build/SpentIt` or the native file picker.

| Asset under `manual/images/` | Captured state | Capture basis |
| --- | --- | --- |
| `create-file-setup.png` | Setup scrolled to the creation controls; password fields empty | macOS, Qt 6.12.0 Basic, 1100 × 900 logical pixels, 2× capture |
| `create-file-workspace.png` | Newly created Chequing account and zero Opening Balance row | Same harness, 1440 × 900 logical pixels, 2× capture |
| `create-file-unlock.png` | Closed protected file waiting for its password | Same harness, 1100 × 900 logical pixels, 2× capture |

Reproduce captures in the rules-enabled Release tree with:

```sh
SPENTIT_GUIDE_CAPTURE="$PWD/manual/images" ./build/import-ui-rules-release/src/ui/spentit_help_ui_tests createOpenChapterWalkthrough
```

Then rebuild the application and Help tests to embed the refreshed assets. The
walkthrough creates and removes only its own temporary synthetic file. Capture
paths are not required to understand or use the manual.

Full-resolution screenshots revealed inline overflow (2880-pixel image in a
576-pixel article). The existing Qt Markdown renderer is retained; inline image
formats now fit the column with their aspect ratios preserved, recomputing on
width/text changes. Full-size sources remain available in the image viewer.
The selected Contents topic also now uses explicit readable selection colors.
CMake watches all manual Markdown/PNG inputs so adding or editing a chapter or
screenshot regenerates the bundle. Test-only sample images remain separate.

Verification on macOS, rules-enabled Release, Qt 6.12.0:

- Built `spentit_import_experiment`, `spentit_help_ui_tests`, and
  `spentit_help_tests`; `build/SpentIt` contains both chapters and all three images.
- The five-check CTest command in “Build and check” passed 5/5 in 27.90s.
- Native `spentit_help_ui_tests` passed 10/10 including init/cleanup in 24211ms.
- The new walkthrough exercised the visible creation controls, File → Close
  file, a rejected password, and successful reopening with the correct password.
- Reading checks cover Next/Previous topic, the opening-section link, all three
  image sources, Actual size and Close/scroll return, and no horizontal overflow
  at normal width and at 500 pixels with 150% text. Existing modal-dialog Help
  checks continue to pass.
- Source captures and rendered normal/narrow chapter captures were visually
  inspected. The selected Contents title is legible after the color correction.
- Initial checks caught a test-resource alias error and the image overflow;
  both were corrected before these final runs.

Native Browse dialogs, complete keyboard-only traversal, screen readers, dark
appearance for the new chapter, Linux, and Windows remain unverified. Some setup
labels still say “dataset” (group heading, file-picker titles, password
placeholder, overwrite title); the chapter uses actual action labels and the
screenshots preserve those existing labels. This chapter did not change setup UI
copy. Later chapters and application-wide context links need their own Go.

## Picture-click correction — 2026-11-05

The owner reported that selecting a chapter screenshot did not enlarge it.
The previous image checks invoked `followLink` directly, so they verified the
viewer but not the clickable image. A native mouse-click regression reproduced
the failure: the rendered image fragment had `isAnchor == false` and an empty
href, and clicking it left the image viewer closed.

Qt Markdown drops the anchor around the linked image. The existing document
formatting pass now restores the corresponding `help-image:` anchor only for
images in the loaded catalog's bundled-image allowlist. Normal runtime link
validation still applies. No renderer or dependency change was needed.

The chapter-reading test now locates and scrolls to each of the three rendered
images and sends a mouse click to its visible area. It requires the viewer to
open and load the image, then checks Actual size and return to the same scroll
position. These clicks replace the direct `followLink` calls for this test.
The first native click reproduction failed; the same test passed after the fix.

Rebuilt `spentit_import_experiment`, `spentit_help_ui_tests`, and
`spentit_help_tests` in rules-enabled Release, updating `build/SpentIt`.
The five-check Help regression suite passed 5/5 in 27.77s; native Help UI tests
passed 10/10 including init/cleanup in 24346ms. `git diff --check` passed.
Native testing used macOS Qt 6.12.0; Linux/Windows and a manual application
walkthrough remain unverified.

## Approved wording follow-up — 2026-11-05

The owner approved a new chapter draft and explicitly requested writing it to
`manual/index.md` and rebuilding the application. That exact draft is now bundled
in `./build/SpentIt`. It opens with “You remember the groceries” and uses separate
purchase, refund, and card-payment sections, followed by getting started,
categories/reports, and acceptance at the reader's pace.

The existing introduction test checks the new opening question with whitespace
normalized, spacing before “Start with your own accounts”, and at least one list.
The paragraph-gap and readable-width checks remain. The earlier text and two-list
observations below describe the preceding draft.

The three build targets in “Build and check” succeeded. The same five-check
CTest command passed 5/5 in 26.45s; native Help UI tests passed 8/8 including
init/cleanup in 2764ms. Sandbox attempts lacked display/application-data access;
rerunning with access passed. No manual visual walkthrough or Linux/Windows
verification was performed. Restart the application to see the updated Help.

## Scope of the chapter Go

In:

- `manual/index.md` bundled into `./build/SpentIt` as the Help introduction.
- The sample banner hidden, because this topic is not sample content.
- The household example named above, checked against the current application
  before the sentences about accepting, balances, reports, and transfers were
  written.
- Viewer checks still load the sample fixture, including Help beside the Rules
  and Reports leave dialogs.

Out:

- Any chapter after the introduction.
- Screenshots of the application.
- Help actions on workspaces, dialogs, and errors.
- A new renderer.

## Scope of the prototype Go

This records the prototype Go. The chapter Go replaced the sample topics in
`./build/SpentIt` with the introduction. The checks below still use the sample
fixture.

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
- Qt Markdown leaves block margins at zero, so paragraphs and headings sit
  together. After the text loads, the viewer sets paragraph, list, and heading
  spacing, and a slightly opener line height. The article width stops growing
  at 36 times the text size, so a wide window keeps a readable line.
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
`cmake --build build/import-ui-rules-release --target spentit_import_experiment spentit_help_ui_tests spentit_help_tests -j 2`.

Commands that passed on this Mac, 2026-11-05, after the introduction was
bundled:

```
ctest --test-dir build/import-ui-rules-release -R '^(help\.prepare|help\.catalog|import\.help_ui|import\.qml|import\.rules_ui)$' --output-on-failure
```

5 tests passed, 26.01s, after the reading-spacing change. `import.help_ui`
runs with `QT_QPA_PLATFORM=offscreen` and passed in 1.98s. `import.rules_ui`
passed in 23.12s. `import.qml` passed in 0.76s. `help.prepare` passed in
0.11s. `help.catalog` passed in 0.04s. The same command passed in 26.24s
before that spacing change.

```
./build/import-ui-rules-release/src/ui/spentit_help_ui_tests
```

No `QT_QPA_PLATFORM` was set, so this used Cocoa. QtTest reported 8 passed,
0 failed, 2878ms. That count includes init and cleanup. The six Help checks
passed, including `introductionBeforeAFileIsOpen`. The same binary passed in
2615ms before the spacing change.

The introduction check opens Help with no file, shows no sample banner, and
shows `understand-spending` with “Where did the money go?”, `42.18`,
`Accept transactions`, and `1,234.56`. The 42.18 line has space under it. The
heading “Two ways to begin” has a larger gap above it. The page has two lists.
At a window width of 1400, the article line is shorter than the page. The file
stays closed.

The earlier same-day prototype run passed the same two commands in 26.31s and
2.26s, before this chapter replaced the shipped sample topics. The sample
checks below still describe that viewer behavior:

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
path that leaves the prepared directory, and an unknown section. It also
checks that the shipped bundle is only `understand-spending`, with no sample
banner. The running viewer’s `https://example.com` check shows “That link is
not part of Help.” A missing sample image in the window was not given its own
check.

Keyboard checks use `forceActiveFocus` and `QTest::keyClick`. They do not walk
every control with Tab only.

## Files

- `manual/index.md` — Understand your spending, the topic Help opens.
- `manual/outline.md` — the remaining chapter plan and the shared example.
- `tests/fixtures/help/` — sample topics and the sample image, for checks only.
- `tools/prepare_help.py` — builds the help bundle and rejects unsafe links.
- `src/ui/help/help_catalog.*` — manifest, search, allowlist, highlight.
- `src/ui/help/HelpWindow.qml` — the Help window and image view.
- `src/ui/ImportMain.qml` — Help menu, Help key, and the Rules dialog parent.
- `src/ui/ReportsWorkspace.qml` — the same parent hook for the Reports leave window.
- `tests/ui/help_prepare_tests.py`, `help_catalog_tests.cpp`, `help_ui_tests.cpp`.

## Next agent

1. Read this file and the design section “Implementation sequence”.
2. The next chapter is “Set up your accounts” in `manual/outline.md`.
   Do not write it, and do not add context links, unless the owner gives that Go.
3. Do not change `rulesLeaveDialog` away from `Qt.ApplicationModal`, and do
   not add a second process, to make Help easier to reach.
4. If you change Help, rerun the two commands in “Build and check” and write
   the new result here. The shipped bundle is `manual/`. The sample checks
   read `tests/fixtures/help/`.

## Tradeoff

None. Help and the Rules dialog stay in one process. The dialog stays
application-modal, and the Cocoa check showed that a key which changes the
rule expression before the dialog does not change it while the dialog is open.
Help still took a click, closed without hiding the dialog, and returned Qt
focus to Stay.
