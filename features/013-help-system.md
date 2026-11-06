# 013 — Help system and user manual

Status: **active**  
Last updated: 2026-11-06  
Implementation: the viewer prototype from the 2026-11-05 Go is in the tree.
The same day's chapter Go added [Understand your spending](../manual/index.md)
as the opening topic. The subsequent chapter-2 Go adds Create or open your file
with three screenshots and responsive inline image sizing. Chapter 3, Set up your
accounts, was accepted and committed. Chapter 4, Bring in transactions, is now
bundled with five screenshots under the subsequent Go and accepted chapter
workflow, accepted by the owner for commit. Sample topics remain a test
fixture. Chapter 5, Find your way around transactions, is bundled under its
subsequent Go with three screenshots, accepted and committed. Chapter 6, Review duplicates and link transfers, is
bundled with four screenshots and committed after owner acceptance. Chapter 7,
Add and correct transactions, is bundled with four screenshots under its Go and
is accepted and committed. Chapter 8, Organize spending with categories, is bundled
with four screenshots under the next Go and is owner-accepted for commit. Chapter 9, Use rules for repeated categorization, is bundled with three screenshots
under its Go and owner-accepted for commit. Chapter 10, Accept transactions — now or later,
is accepted and committed as `b1c55fc`. Chapter 11, Answer questions with reports,
is bundled with four screenshots under its Go and owner-accepted for commit.
Chapter 12, [Look after your work and resolve remaining problems](../manual/backup-recovery.md),
is bundled with two screenshots under the next Go and is owner-accepted for commit.
The first quick reference, [Where do I find…?](../manual/where-to-find.md), is
bundled under the subsequent Go and is owner-accepted for commit. Remaining quick
references and context links need their own Go. [Keyboard and mouse](../manual/keyboard-mouse.md)
is committed as `8c3c830`. [Search examples](../manual/search-examples.md) is
committed as `6b1333c`, with the shared pattern table moved from Rules under
the owner’s follow-up permission. [Words used in SpentIt](../manual/words.md)
is bundled under the next Go and is owner-accepted for commit. macOS checks and limits
are in [the handoff](../doc/help-prototype-handoff.md).
[Messages and problems](../manual/messages-problems.md) is bundled under the
next Go and was accepted for commit on 2026-11-06. It links verified message excerpts to supported
next steps; application-wide context links remain separate work.
The 2026-11-06 whole-manual Go adds a continuous synthetic file walkthrough
under both guide and alternate date/amount settings, problem-driven lookups,
and a complete prose/screenshot audit. A discovered acceptance replay defect
received a separate implementation Go; see [status](../doc/status.md) for results.

The chapter-9 table follow-up reproduced missing text on reverse scrolling in
Qt 6.12. Help now keeps the complete scene graph for table articles and batches
paragraph formatting; native pixel/scroll/navigation checks and limits are
recorded in the [rendering correction](../doc/help-prototype-handoff.md#long-help-chapter-rendering-correction--2026-11-05).

## Accepted requirements — owner direction 2026-11-05

SpentIt needs an embedded, offline user manual presented in its own application
interface. Users must not need to open a browser or navigate a hierarchy of files.
Markdown is the authoring format; rendered HTML is acceptable if it proves a
better fit. The reader must accommodate different window sizes and display
screenshots appropriately.

The manual begins with a short, entirely nontechnical explanation of what
SpentIt helps the user achieve and its approach to understanding spending. It
then explains the application in the order a user is likely to encounter it:
setup and accounts, imports, transaction review/linking, categories, rules,
accepting transactions or leaving them new, and eventually reports. Screenshots
and explanations cover screens, menus, buttons and errors. Remaining recovery
and error topics follow this progression. This is a readable guide that also
supports looking up a particular task, rather than a compulsory guided tour.

The owner proposed repository directories `manual/` for Markdown and
`manual/images/` for images. This design adopts those locations. Application code
belongs under `src/`, following the existing dependency boundaries.

The [user-facing language guide](../doc/user-facing-language.md) applies to the
manual and viewer. Help is also written in short pieces a reader can take one
at a time. The voice is plain and human: it can start from why someone wants
to know where the money went. It stays accurate. Engineering choices below are
for contributors, not material to explain to users. This request authorizes
documenting the design, not building the viewer or writing the complete manual.

## Accepted chapter workflow — 2026-11-05

Develop chapters sequentially, using one primary writer for continuity and
sub-agents for verification and independent review. The owner asked to record
this approach after reviewing chapters 1 and 2. This authorizes the supporting
agent roles within an approved chapter; it does not authorize writing all
remaining chapters at once. Retain the per-chapter Go and owner review.

For each approved chapter:

1. **Verify the task.** A supporting agent checks the current implementation
   and relevant tests for labels, prerequisites, choices, results, restrictions,
   and common errors. Distinguish observed behavior from code-only findings and
   unresolved questions. Historical requirements are not proof of current UI.
2. **Write and illustrate.** The primary agent owns the chapter and edits it
   into the established voice. Use chapters 1 and 2 and the language guide as
   references: warm, plain English, short pieces, no unnecessary words. Continue
   the shared synthetic household example. Capture only useful real screens,
   with captions and alternative text; keep instructions usable without images.
3. **Review independently.** A separate supporting agent reviews accuracy,
   omissions, repetition, verbosity, and continuity with earlier chapters.
   Return concrete findings to the primary writer, who resolves them before
   delivery. Keep one agent responsible for UI interaction and screenshot
   capture; reviewers use code and captured results without competing for the UI.
4. **Verify the reader's experience.** Build the bundled manual, check links,
   search, navigation, image sizing, narrow windows and larger text, and exercise
   the chapter's workflow. Test actual user interactions where promised: clicking
   an image must open it; calling the viewer function directly is insufficient.
   Record the commands, results, capture basis, and unavailable checks honestly.
5. **Review with the owner.** Rebuild `build/SpentIt` for reading in Help,
   address feedback, and commit when requested. Update the handoff after each
   chapter with what shipped, verification, limitations, unresolved questions,
   and the next chapter. Do not proceed to the next chapter without its Go.

Sub-agents should receive bounded verification/review tasks, not separate
assignments to draft the rest of the manual in parallel. The primary writer
remains responsible for the final prose and for reconciling findings.

A fresh session is useful at a chapter boundary but is not required for every
chapter. For the present handoff, start chapter 3 in a new session. Read the
handoff, this workflow, the outline, the language guide, and the two shipped
chapters before proceeding; do not rely on this chat to recover the voice or
accepted decisions. The owner's next-session Go starts chapter 3 implementation.

After the last chapter, follow the complete synthetic example from an empty
file through reports. Separately test lookup use through Contents and Search,
including entry from a particular problem. Check continuity, terminology,
cross-references, screenshots, and missing decisions across the whole manual.

## Chapter 2 Go and writing direction — 2026-11-05

The owner authorized Create or open your file, its screenshots, and verification
in Help. Keep the chapter-1 plain-English voice while using no more words than
needed: users need concise, accessible pieces, not a long compulsory read.
The chapter and its verification are recorded in [the handoff](../doc/help-prototype-handoff.md#chapter-2--2026-11-05).
Later chapters and application-wide context links remain separate work.

## Accepted refinements — owner direction 2026-11-05

These corrections are part of the design. The first Go covers the viewer
prototype only. That Go was given on 2026-11-05.

- Help remains usable beside a decision dialog. The dialog’s existing protection
  against changes elsewhere in SpentIt stays as it is. Modality is not weakened
  to make Help reachable. The prototype proves this with the Rules
  Save / Discard / Stay window, including focus returning to that dialog when
  Help closes. If this cannot work cleanly in one process, report the
  architectural tradeoff and stop.
- The prototype uses representative sample content and covers navigation,
  search with highlighting, image enlargement, keyboard interaction, the
  dialog case and bundled-link checks. It must run through the normal
  `./build/SpentIt` launch. Real chapters and application-wide context links
  are later work.
- The prototype tests the renderer before any real chapter is written. If Qt
  Markdown falls short, report the specific failures and a proposed embedded
  HTML approach before adding a rendering dependency.
- Chapters are written and reviewed incrementally against the current
  application, using one stated synthetic example. A final end-to-end review
  still checks continuity and omissions.
- Valid targets are bundled topic and section links and bundled images. The
  build and the running viewer both reject external URLs, arbitrary file paths
  and paths that escape the bundle.

Search ranking, layout breakpoints and precise chapter divisions remain
prototype decisions.

## Proposed reading experience

Use one reusable **SpentIt Help** window. Help itself does not block the rest of
SpentIt. It can sit beside the main window while the user follows instructions.
Opening Help must not navigate away from an editor, save or discard an edit,
change selection, or mutate a financial file. Help remains available at the
welcome and unlock screens without an open file. Closing Help leaves SpentIt
where it was, except when a decision dialog is waiting, as specified below.

The window offers:

- Contents with readable chapter and topic names, showing the current topic.
- Search across the manual, with meaningful result titles and short excerpts.
- A reading area with selectable text, links, screenshots and captions.
- Back, Forward, Home, and previous/next topic actions in reading order.
- Find within the current topic and adjustable text size with a reset action.

Contents describe the user's tasks; they do not expose filenames or folders.
Previous/next follow the authored sequence. Back/Forward follow actual visits,
including search and context-help navigation, and restore the reading position.
Selecting a search result opens its relevant passage; returning to results
retains the query and position. A search with no matches says so and lets the
user revise the query or browse Contents.

Reuse an already-open help window. A general Help action restores its current
topic; an explicit context-help request opens the requested topic. First use
opens the introduction. Retain topic and reading position within the application
session; persistent window size and text-size preferences can use application
settings, separate from financial files. Clamp restored geometry to available
screens. Persisting searches or reading history across launches is not proposed.

## Help beside a decision dialog

Opening or using Help must not answer a decision dialog, dismiss it, or let the
user change the file, selection or workspace that the dialog is protecting.
Closing Help returns keyboard focus to that dialog. If a control inside the
dialog had focus, focus returns to that control. The dialog keeps waiting for
its own answer.

The required prototype case is the Rules leave window, `rulesLeaveDialog`,
titled “Save changes before leaving Rules?”. It is a separate application-modal
window with Save, Discard and Stay. Escape and its close control mean Stay
while that prompt is active, and the Rules workspace underneath stays undimmed
and unchanged. The Reports leave window is the same kind of window. The
mechanism has to apply to that class; the prototype demonstration is the Rules
window.

Both entry orders are required: open Help while the dialog is already open, and
open the dialog while Help is already open. In either case the user can read,
search, follow sample links and enlarge an image while Save, Discard and Stay
remain unanswered. The main window still does not accept input. Help does not
change the Rules edits, selection, search or scroll position held behind the
dialog.

While Help or its enlarged-image view is the active window, Escape and the
window close control affect only that Help surface. They must not run the
dialog’s Stay action or activate the main window. Help controls must not invoke
main-window or dialog actions. Closing the image view returns to the same Help
position; closing Help then returns focus to the dialog.

Do not change this dialog from application-modal to window-modal or nonmodal,
and do not remove modality to make Help easier to open. Whether Help can sit in
the dialog’s existing modal group, while every other SpentIt window stays
blocked, is what the prototype has to find out. If Help cannot receive input
without weakening that protection, or the result is unstable — a dismissed
dialog, a lost reading position, focus left in the main window, or a
platform-specific hole in the block — stop and report the tradeoff before
proceeding. A second process is not part of the prototype.

Most other decision prompts, including the ledger “Unsaved changes” dialog, are
modal popups inside the main window rather than separate application-modal
windows. The prototype does not have to exercise every popup. Help must not
dismiss those popups or perform the action they protect.

## Content progression

The following is a proposed editorial outline, not a claim that chapters exist.
Precise chapter divisions remain a prototype decision. Authors must check the
current application before describing each workflow. Real chapters are later
work, written and reviewed one at a time as described in the implementation
sequence.

| Order | Chapter | What it helps the user understand or do |
| --- | --- | --- |
| 1 | Understanding your spending | What SpentIt is for; bring transactions together, review them, organize spending and explore reports. Explain the approach in ordinary language. |
| 2 | Getting started | Create or open a file, choose protection and preferences, understand the main window and where to go next. |
| 3 | Your accounts | Create accounts, choose currency and opening balance, find an account and manage it. Explain that an import may also create accounts. |
| 4 | Bringing in transactions | Choose a supported bank export, map accounts, decide on missing categories, understand completion and what to review next. |
| 5 | Reviewing transactions and transfers | Recognize repeated imports and possible duplicates; distinguish Merge from Link; understand both sides of a transfer and unresolved choices. |
| 6 | Working in the ledger | Read columns, search and select, add a transaction, edit dates/text/amounts, use splits and understand available Undo actions. |
| 7 | Organizing categories | Create and choose categories, change selected transactions and distinguish an ordinary category from an account destination. |
| 8 | Using rules | Create a category rule, choose scope, preview matches, save and apply; explain why a transaction may not change. |
| 9 | Accepting transactions, now or later | Explain what Accept transactions does, what leaving transactions new means, and how that differs from saving edits, discarding or undoing an import. |
| 10 | Understanding reports | Choose accounts, categories and periods; compare results, interpret signs/currencies/charts and inspect or edit contributing transactions. |
| 11 | Looking after your work and resolving problems | Backup/restore and supported recovery actions; remaining opening, importing, editing and reporting problems; how to proceed when an operation's outcome is uncertain. |

Introduce concepts briefly at their first encounter and link to the full topic.
For example, import completion needs an immediate explanation that leaving
transactions new does not mean losing them on exit; readers should not have to
reach chapter 9 to learn this. Explain common errors alongside the task that
causes them. The final problem-solving chapter provides a searchable entry point
and links back, rather than repeating conflicting instructions.

Each task topic should state the goal, show where to start, describe the relevant
controls and choices, and explain how to recognize the result. Cover empty,
selected, editing and busy states when they affect the user's next action.
Describe disabled controls when understanding their prerequisites is useful.
Explain consequences before destructive choices. Avoid an exhaustive catalogue
of obvious buttons with no task context.

Use visible application labels exactly. The current acceptance action is labeled
“Accept transactions”. Where the language guide and a current label differ,
record that discrepancy for UI work rather than silently writing instructions
for a button that does not exist. Search keywords may include both “Commit” and
“accept transactions”. Do not turn historical feature contracts or status
entries directly into manual pages: they contain superseded behavior and
unimplemented proposals.

Use one consistent synthetic example for the whole manual. State the date order
and amount grouping used in that example, and explain that the reader’s own
file follows the date and amount settings saved for that file. Check a chapter
against the current application when the chapter is added, and check it again
when a later change affects its screens, labels, workflows or errors. A final
read of the finished guide still looks for breaks in continuity and for topics
the individual chapters never covered.

## Window layout and accessibility

At a comfortable desktop width, show a contents/search pane beside the article.
Constrain article line length for comfortable reading instead of stretching text
across a large monitor. At narrow widths, collapse navigation behind a clearly
labelled Contents/Search control and give the article the available width.
Opening and dismissing navigation retains the reading position and returns focus
appropriately. Exact breakpoints should follow testing with real content.

Body text wraps without horizontal scrolling. Long links and examples must not
force the whole window wider. Prefer short lists to wide tables; an unavoidable
wide table may have its own labelled horizontal scrolling region. The article
has one main vertical scroll. Controls stay reachable at increased text sizes.

All navigation, search, find and image controls must work by keyboard, with
visible focus, accessible names and logical focus order. Provide meaningful
heading structure, image alternatives, sufficient contrast and light/dark
styling consistent with SpentIt. Do not communicate an instruction solely through
color, hover or a screenshot. Verify assistive-technology behavior; rendering
Markdown alone does not establish accessibility. Display platform-appropriate
shortcut notation that matches actual application bindings.

## Screenshots and images

Use screenshots of the actual application with a consistent synthetic example
file. Never use private accounts, imports, paths or financial records. Introduce
a screen with an overview, then use focused crops for small controls and details.
Every screenshot needs a purpose, a useful caption and alternative text; its
surrounding prose must remain sufficient to perform the task.

Inline images preserve their aspect ratio and fit within the reading column.
Do not stretch a small image merely to fill the available width. A screenshot
shrunk to fit can be too small to read, so selecting it opens an in-application
image view with Fit, actual-size, zoom and pan controls. Escape/Close returns to
the same article position and focused image. This is separate from text-size
adjustment. Large source images must not make navigation stall or consume
unbounded memory; define and check asset dimensions/size budgets during the
implementation prototype.

Use sufficient source resolution for high-density displays, with explicit
display sizing where necessary. Keep screenshots in their captured theme rather
than inverting their colors. Put a neutral frame around them where needed in
the opposite viewer theme. One platform and one theme are sufficient where the
task is unchanged. Add another capture only when the platform or theme
difference changes the task, and say so in the caption. Annotations must use
legible labels also explained in the text.

## Search and context help

Use ordinary case-insensitive text search initially, with title and keyword
matches preferred to incidental body matches. Search all bundled topics, not
financial data. Show readable excerpts and highlight matches, with a distinct
current match for in-topic navigation. Regex search is deferred unless a concrete
user need warrants its extra controls and failure cases.

Assign stable topic and section identifiers independently of titles and file
paths. Application-wide context links are later work and are added only with
the chapter they open. The prototype may use sample identifiers to prove
navigation; it does not add Help actions across workspaces, dialogs or errors.

When those links are added, a Help action from a workspace, dialog or error
resolves a topic identifier. An error-help link opens instructions and must not
repeat or perform the failed operation. Pass a topic identifier, not account
names, amounts, paths or error payloads. Missing targets show a plain
explanation with Home and Search available. Validate every
application-to-help target at build/test time.

Searchable problem topics include the relevant visible message, what it means
for the current task, and supported next steps. Distinguish an operation that
failed from one whose saved outcome is unknown. Do not advise repeating an
operation that may have succeeded, invent repair actions, or promise forgotten
password recovery. Detailed engineering diagnostics stay outside the manual.

## Source layout and bundled delivery

Proposed repository organization:

```text
manual/
  index.md                 # introduction and start of the reading sequence
  accounts.md              # illustrative topic filename, not an existing page
  ...                      # task-oriented Markdown topics
  images/                  # reviewed synthetic screenshots and illustrations
src/ui/help/               # viewer QML and C++ presentation/search adapters
tools/                    # help preparation/checking scripts, when implemented
tests/ui/                 # help interaction and rendering checks
```

Keep titles, stable IDs, reading order and optional search keywords in a small
documented metadata convention. Choose frontmatter or a single manifest during
the prototype; do not maintain competing navigation lists. Use relative image
and topic links in authored Markdown. Prepare resources and the search index at
build time, and bundle them with the application version they describe. Generated
output belongs in the build tree. Installed users see the help window, not this
source organization, and need no Markdown tools or local web server.

The preferred implementation is a separate QML window within the existing
SpentIt process, using Qt's Markdown/rich-text rendering and local resources.
It does not require a separately installed executable. The normal
`./build/SpentIt` launch loads those resources from the build tree; an install
step is not required to read Help. Keep help independent of the document worker,
financial engine and storage; reading help needs no SQL or file unlock.
Resource loading and search must not block normal application use.

The prototype decides the renderer. It must show captions, section anchors, a
wide table, selectable highlighted text and working links. If Qt Markdown
cannot do those, report each specific failure and a proposed embedded-HTML
approach, then stop. Do not add Qt Help, WebEngine or another rendering
dependency in that prototype. Generated HTML, if later accepted, changes the
renderer only. The authored source stays Markdown, and reading stays inside
SpentIt. No browser launch is part of the manual-reading flow.

All essential content, fonts and images must work offline. No remote content,
analytics or automatic online updates are proposed.

Authored links may name only a bundled topic, a section in a bundled topic, or
a bundled image. Preparation resolves relative links and fails the build when a
link is an external URL, uses a network, file or script scheme, is an absolute
or arbitrary local path, escapes the help resources through `..` or another
link that leaves the bundle, or does not match a known topic, section or image.

The running viewer enforces the same allowlist when a link or image is
activated, including links that were rendered from Markdown and any later
generated HTML. A rejected or missing target shows a plain explanation with
Home and Search available. It does not open a browser, another program, a file
outside the bundle, or a financial command. The enlarged-image view loads only
an allowlisted bundled image. A link that passed the build is still checked at
activation. No later kind of external link is included unless a separate design
says how it opens and how the allowlist treats it.

## Implementation sequence

The owner gave implementation Go for the viewer prototype on 2026-11-05, and
Go the same day for the first real chapter. Further chapters and
application-wide context links still need their own Go. Progress, commands,
and any modality tradeoff are recorded in
[the prototype handoff](../doc/help-prototype-handoff.md).

1. Bundle a small sample fixture through `./build/SpentIt`, loaded from the
   build tree. The Help window identifies it as sample content. It is not a
   manual chapter and it is not the finished guide.
2. Show contents, previous/next, Back/Forward, Home, find within the topic,
   and search. Highlight matches, keep that text selectable, and keep sample
   links working. Include captions, section anchors, a wide table with its own
   scrolling region, and an annotated image that opens Fit, actual-size, zoom
   and pan.
3. Operate those controls from the keyboard, with visible focus.
4. Prove the Rules Save / Discard / Stay case in [Help beside a decision dialog](#help-beside-a-decision-dialog).
5. Fail the build and the running activation path on the disallowed links in
   the bundled-link rules above. Confirm a permitted topic, section and image
   still open.
6. Record the renderer result. On shortfalls, report the failures and the
   proposed embedded-HTML approach, and stop before adding a dependency.
   Record the metadata convention, packaging, search ranking and breakpoints
   the prototype actually used.

The first real chapter, Understand your spending, is bundled from
`manual/index.md`. It uses the household example and has no application
screenshot. Sample topics are prepared from `tests/fixtures/help/` for the
viewer checks and are not part of the Help window. Chapter 2 is now also bundled, with setup, unlock, and workspace screenshots.
Chapter 3 is also bundled with account setup and management screenshots.
Chapter 4 adds import, mapping, category-choice and Undo instructions/screenshots.
Chapters after chapter 4 and application-wide context links need their own Go. Write each chapter against
the current application, capture its screenshots only after the instructions
and the screen agree, and review that chapter before the next one.
Use the synthetic example and the date, amount, platform and theme rules above.
After the last chapter, read the guide once as a first-time reader and once
while looking up a specific action and a specific error. Bundle only content
that matches that application version.

Screenshot naming and a contributor inventory should identify the topic,
depicted screen or state, and capture basis, without showing those maintenance
details in the manual. A change to a screen, label, workflow or error includes
review of the affected topics, screenshots, search terms and context targets.

## Acceptance and verification obligations

The prototype obligations below were exercised on macOS on 2026-11-05, with the
limits recorded in [the handoff](../doc/help-prototype-handoff.md). The same
day's introduction check opened Understand your spending with no file and with
no sample banner. Linux, Windows, and a person clicking through
`./build/SpentIt` were not part of that run. Further chapters and context-link
checks still wait for their own Go.

Prototype, when its Go is given:

- Launch Help from `./build/SpentIt` with no separate install. Open it with no
  file. Separately, open it while the Rules leave dialog is holding unsaved
  rule edits. Sample reading, search, links and image enlargement do not answer
  the dialog or change those edits, the selection or the file.
- Closing the image view returns to the same Help position. Closing Help
  returns keyboard focus to the Rules dialog, and to the control that had focus
  when one did. Escape and close in Help do not choose Stay.
- With networking unavailable, navigate the sample topics, search, follow
  permitted links and open the sample image from the build-tree bundle.
- The fixture covers captions, section anchors, a wide table, selectable
  highlighted text and working links. Record any Qt Markdown failure instead of
  silently switching renderers.
- Keyboard-only use reaches navigation, search, find, text size and the image
  controls, with focus restored as above.
- The build rejects an external URL, an absolute or arbitrary file path, and a
  path that escapes the bundle. Runtime activation of those same links shows
  the missing-target explanation and does not open anything outside Help.
- A missing sample image fails visibly and leaves Contents, Home and Search
  usable. Search and the large sample image do not block the Rules dialog.

Later manual and context-link work:

- Open Help without a file and while an unsaved editor is active; no financial
  state or selection changes, and closing Help restores the prior interaction.
- With networking unavailable, navigate every shipped topic, search, follow
  links and view every image from the bundle that `./build/SpentIt` loads,
  without a source checkout.
- Validate unique IDs, navigation order, the supported Markdown subset, links,
  anchors, image presence and alternative text, and every context target.
- Check wide and narrow windows, increased text size, high-density displays,
  both themes and display changes; no clipped controls or page-wide overflow.
- Check keyboard-only use, focus restoration, screen-reader reading order and
  image alternatives on each supported platform before claiming accessibility.
- Verify search ordering, empty and no-match queries, excerpts, highlighting
  and navigation restoration with a full-manual-sized fixture.
- Walk through the current application with synthetic data and the manual
  alone. Check each described control, consequence, error and recovery
  instruction, including the stated date and amount example against a file
  whose saved settings differ.
- Test installed macOS, Linux and Windows packages before claiming portability.
  Record unavailable checks as unverified in project status.

## Remaining decisions and exclusions

Exact window defaults, responsive breakpoints, topic granularity and search
ranking are prototype choices. The metadata convention remains a prototype
choice. The renderer stays Qt Markdown unless the prototype reports specific
shortfalls and a later decision accepts embedded HTML. How Help receives input
beside an application-modal dialog is likewise settled by that prototype
evidence, or by the tradeoff report if it does not work cleanly. The chapter
sequence may change where application walkthroughs reveal a clearer order,
while retaining the accepted high-level introduction and encounter-based
progression.

Initial scope excludes a public documentation website, platform-specific native
manuals, printing/PDF export, translations, interactive tours, executable examples,
AI-generated answers at runtime and a separate help updater. None is needed to
deliver the embedded manual described here.
