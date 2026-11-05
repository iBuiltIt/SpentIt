# 013 — Help system and user manual

Status: **active**  
Last updated: 2026-11-05  
Implementation: design only. No manual content, help viewer or help packaging has
been implemented by this work. Implementation awaits explicit owner Go.

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
manual and viewer. Engineering choices below are for contributors, not material
to explain to users. This request authorizes documenting the design, not building
the viewer or writing the complete manual.

## Proposed reading experience

Use one reusable, nonmodal **SpentIt Help** window. It can sit beside the main
window while the user follows instructions. Opening help must not navigate away
from an editor, save/discard an edit, change selection, or mutate a financial
file. Help remains available at the welcome and unlock screens without an open
file. Closing help leaves SpentIt where it was.

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

## Content progression

The following is a proposed editorial outline, not a claim that chapters exist.
Authors must check the current application before describing each workflow.

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
| 9 | Accepting transactions, now or later | Explain what the current Commit/accept action does, what leaving transactions new means, and how that differs from saving edits, discarding or undoing an import. |
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

Use visible application labels exactly. Where the language guide and a current
label differ, record that discrepancy for UI work rather than silently writing
instructions for a button that does not exist. Search keywords may include both
“Commit” and “accept transactions”. Do not turn historical feature contracts or
status entries directly into manual pages: they contain superseded behavior and
unimplemented proposals.

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
the opposite viewer theme. Captions should identify significant platform
differences; add alternate captures only when those differences affect the task.
Annotations must use legible labels also explained in the text.

## Search and context help

Use ordinary case-insensitive text search initially, with title and keyword
matches preferred to incidental body matches. Search all bundled topics, not
financial data. Show readable excerpts and highlight matches, with a distinct
current match for in-topic navigation. Regex search is deferred unless a concrete
user need warrants its extra controls and failure cases.

Assign stable topic and section identifiers independently of titles and file
paths. Help actions from a workspace, dialog or error resolve those identifiers.
An error-help link opens instructions; it must not repeat or perform the failed
operation. Pass a topic identifier, not account names, amounts, paths or error
payloads. Missing targets show a plain explanation with Home and Search available.
Validate all application-to-help targets at build/test time.

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
It does not require a separately installed executable. Keep help independent of
the document worker, financial engine and storage; reading help needs no SQL or
file unlock. Resource loading and search must not block normal application use.

First prove representative rendering, section navigation, highlighting, image
handling and accessibility. If Qt's renderer cannot meet these requirements,
evaluate generated HTML in an embedded viewer. That changes the renderer, not the
Markdown source or the user's experience of an integrated manual. Adding Qt Help,
WebEngine or a standalone executable requires an explicit design decision based
on demonstrated benefit and packaging cost. No browser launch is part of the
manual-reading flow.

All essential content, fonts and images must work offline. Internal links resolve
only to bundled help content; help links cannot execute financial commands or
load arbitrary local files. External reference links are unnecessary for the
initial manual; any later policy for opening them outside the viewer must be
explicit. No remote content, analytics or automatic online updates are proposed.

## Implementation and editorial sequence — proposed

1. Review this design and settle any material changes before implementation Go.
2. Build a representative content fixture and viewer prototype: prose, headings,
   lists, table, long text, cross-topic/section links, annotated screenshot and
   enlarged image. This is not the finished manual.
3. Verify rendering/accessibility and settle the renderer, metadata convention,
   resource packaging and search implementation from that evidence.
4. Implement the integrated viewer and context targets, with automated resource
   checks and interaction tests.
5. Author the manual in encounter order against the working application; capture
   screenshots only after the corresponding instructions and UI are checked.
6. Review the complete guide as both a first-time reader and someone finding a
   specific error or action. Ship only content matching the bundled version.

Screenshot naming and a contributor inventory should identify the topic, depicted
screen/state and capture basis, without exposing those maintenance details in
the manual. Changes to screens, labels, workflows and errors must include review
of affected topics, screenshots, search terms and context targets.

## Acceptance and verification obligations

These are future checks, not completed verification:

- Open help without a file and while an unsaved editor is active; no financial
  state or selection changes, and closing help restores normal interaction.
- With networking unavailable, navigate every topic, search, follow links and
  view every image from the installed bundle without a source checkout.
- Validate unique IDs, navigation order, Markdown syntax in the supported subset,
  links, anchors, image presence and alternative text, and all context targets.
- Check wide and narrow windows, increased text size, high-density displays,
  both themes and display changes; no clipped controls or page-wide overflow.
- Check keyboard-only use, focus restoration, screen-reader reading order and
  image alternatives on each supported platform before claiming accessibility.
- Verify search ordering, empty/no-match queries, excerpts, highlighting and
  navigation restoration with a representative full-manual-sized fixture.
- Verify missing topics/images fail visibly with usable navigation; bounded
  search and large-image handling keep the app responsive.
- Walk through the current app using synthetic data and the manual alone; check
  each described control, consequence, error and recovery instruction.
- Test installed macOS, Linux and Windows packages before claiming portability;
  record unavailable checks as unverified in project status.

## Remaining decisions and exclusions

Exact window defaults, responsive breakpoints, topic granularity and search
ranking are prototype choices. The renderer and metadata convention remain
proposed. The chapter sequence may change where application walkthroughs reveal
a clearer order, while retaining the accepted high-level introduction and
encounter-based progression.

Initial scope excludes a public documentation website, platform-specific native
manuals, printing/PDF export, translations, interactive tours, executable examples,
AI-generated answers at runtime and a separate help updater. None is needed to
deliver the embedded manual described here.
