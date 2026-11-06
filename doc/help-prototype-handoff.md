# Help viewer prototype handoff

Last updated: 2026-11-05

The viewer prototype Go on 2026-11-05 does not cover application-wide context
links. A second Go the same day covered the introduction; a subsequent Go covers
chapter 2, its screenshots, and checking it in Help. The next Go covers chapter 3
using the documented verification/writing/review workflow. The owner then
requested its commit and gave Go for chapter 4. The design
is [013 — Help system and user manual](../features/013-help-system.md). This
file is the progress record for the next session. Update it when a step changes.

## Current step

The Help window opens [Understand your spending](../manual/index.md), followed
by [Create or open your file](../manual/create-open-file.md) and
[Set up your accounts](../manual/accounts.md), and
[Bring in transactions](../manual/import-transactions.md). Chapter 3 was accepted
and committed as `467a20e`; the owner also accepted chapter 4 and requested
its commit (`ec23d60`). Chapter 5,
[Find your way around transactions](../manual/find-transactions.md), is now bundled
accepted and committed as `bbd3a8b`. Chapter 6,
[Review duplicates and link transfers](../manual/review-duplicates-transfers.md),
is accepted and committed as `a8a2739`. Chapter 7,
[Add and correct transactions](../manual/add-correct-transactions.md), is accepted
and committed as `2b9c41e`. Chapter 8,
[Organize spending with categories](../manual/categories.md), is bundled under
the owner’s next Go and is owner-accepted for commit. Sample topics used to check the viewer are in
`tests/fixtures/help/` and are not shown in Help. Chapter 9, [Use rules for repeated categorization](../manual/rules.md), is now
bundled under the owner’s Go and owner-accepted for commit. Chapter 10,
[Accept transactions — now or later](../manual/accept-transactions.md), is bundled
accepted and committed as `b1c55fc`. Chapter 11,
[Answer questions with reports](../manual/reports.md), is bundled under the next
Go and owner-accepted for commit. Chapter 12 and context links need their own Go. Leave `rulesLeaveDialog` at `Qt.ApplicationModal`.

The introduction follows a grocery purchase through Chequing and a credit card;
a Market purchase of 42.18 on 2026-01-15; a 6.00 refund on 2026-01-20; and a
36.18 credit-card payment from Chequing on 2026-01-28. Dates in the guide are
year-month-day. Amounts use a comma for thousands and a dot for cents. The
chapter says the reader's file follows the date and amount settings saved for
that file. It has no application screenshot. It names Import…, + New
transaction, and Accept transactions. It says transactions that are not yet
accepted stay in the file and count in balances and reports, and that a
recorded transfer is left out of report spending.

## Chapter 11 — 2026-11-05

The owner accepted chapter 10 and requested its commit (`b1c55fc`), then gave Go
for [Answer questions with reports](../manual/reports.md). Chapter 11 is bundled
with four screenshots and owner-accepted for commit. Supporting verification checked
current engine/storage/UI behavior; the primary writer owned prose and captures;
an independent reviewer checked accuracy, scope, brevity, continuity, images and
test inventories. Review clarified descending category order and capture basis.
No financial or Help viewer behavior changed.

The chapter covers report creation and selection, moving presets/fixed dates,
calendar interaction, ordered comparisons, signed net spending, direct category
rows, separate currencies, all four graph choices, partial-split contributions,
editing/Undo/refresh, saved report maintenance and remaining problems. It names
the distinct selected Save and all-draft leave Save behaviors, including saving
a deletion with Save all changes. Printing/export remain unavailable.

The temporary Household file uses Credit card opening zero and Chequing
1,000.00 on January 1. January card purchases are Market -42.18, refund +6.00
and Corner shop -20.00 split Groceries -12.00 / Household -8.00. February adds
Groceries -55.00 and Household -15.00. Chequing has Income +2,000.00 and the
January 28 card payment -36.18 with its generated counterpart. Selecting
Groceries/Household across both accounts yields January 56.18 and February
70.00, excluding income, opening balances and transfer principal. The card
purchases are accepted; income/transfer postings remain new.

Double-clicking January Groceries opens three transactions totalling 48.18.
Corner shop contributes only 12.00 despite its 20.00 full withdrawal. Saving its
date in February retains the opened row as no longer included and updates the
period totals to 36.18 / 90.00; Undo restores 56.18 / 70.00. The walkthrough
also changes grouped bars to Pie and exercises unsaved-report Stay/Discard.

| Asset under `manual/images/` | Captured state |
| --- | --- |
| `reports-editor.png` | Two account/category selections and January/February custom dates |
| `reports-comparison.png` | Grouped bars and exact 56.18 / 70.00 table |
| `reports-pie.png` | Separate equal-sized pies with period totals |
| `reports-transactions.png` | Groceries detail with Corner shop's included split marked |

Capture basis: actual ImportMain.qml in a native macOS Qt 6.12.0 Basic light
harness, 1440 × 900 logical pixels at 2×, temporary synthetic file/settings.
Setup, categories, openings, imports, transfer creation, accepted-state setup,
transaction inspection and Undo use model commands or existing helpers. Real
mouse clicks exercise Reports/New report, category checkboxes, graph selection,
Add comparison, Save and show, amount double-click and Back buttons. Text fields
use Qt properties/textEdited signals. Calendar and graph selection use real key
events; transaction Enter saves through the editor. Report leave choices emit
the controls' clicked signals. Scrolling brings controls into view through Qt
properties. This is not a manual standalone-app walkthrough. All four source
images were visually inspected and independently reviewed.

The Graph combo currently clips the long choice text, and the editor's bottom
Save controls require scrolling at this size. These existing UI limitations
are captured honestly; no screenshot retouching or UI change was made.

```sh
QT_QPA_PLATFORM=offscreen ./build/import-ui-rules-release/src/ui/spentit_help_ui_tests reportsChapterWalkthrough
SPENTIT_GUIDE_CAPTURE="$PWD/manual/images" ./build/import-ui-rules-release/src/ui/spentit_help_ui_tests reportsChapterWalkthrough
ctest --test-dir build/import-ui-rules-release -R '^(engine\.reports|import\.reports|import\.reports_ui)$' --output-on-failure
```

The focused offscreen walkthrough passed 3/3 including init/cleanup in 4549ms;
native capture passed 3/3 in 6244ms. An initial fixture omitted the transfer
operation identifier and sequence and failed; supplying the existing command's
required arguments corrected it. An earlier test invocation ran before linking
finished and reported an unknown test function; it was rerun after the build.
No private data or application behavior was changed for these harness corrections.

Supporting checks passed 3/3 in 16.62s (engine 0.01s, storage 11.61s, offscreen
UI 4.99s), with authorized application-data lock access. These include exact
cell reconciliation, signs/currencies, boundaries, transfer fees, new/accepted
and edited entries, definition retries, write/sync failure positions, pre/post
commit interruption and restored backup. Supporting UI checks cover scoped
editing, refresh/Undo, calendar behavior and negative-pie data.

Final rules-enabled Release verification on macOS Qt 6.12.0:

- Rebuilt `spentit_import_experiment`, `spentit_help_ui_tests` and
  `spentit_help_tests`; `build/SpentIt` bundles eleven chapters and 36 images.
- The documented five-check CTest selection passed preparation, catalog, QML
  and Rules UI (4/5). Full offscreen Help reached its known 120-second timeout
  during Rules reading; total selection time 144.78s. This is not a full
  offscreen pass, and that timing limit remains unresolved.
- Full native `spentit_help_ui_tests`, with `SPENTIT_GUIDE_READING_CAPTURE` set
  to a temporary path, passed 30/30 including init/cleanup in 271744ms.
  Chapter 11 checks previous/next and section navigation, actual clicks on all
  four images, loading/Actual size/close/scroll return, and 500-pixel reading at
  150% text. Normal, inline-image and narrow/larger-text views were visually
  inspected. All existing native walkthrough, scrolling and modal Help checks
  passed as part of that run.
- Catalog search for “grouped periods” finds chapter 11. Preparation validates
  bundled links/images; changed-document relative links and `git diff --check`
  passed.

Dark appearance, complete keyboard-only/screen-reader use, Linux and Windows
remain unverified. The owner accepted chapter 11 and requested its commit.
Chapter 12 and application-wide context links require their own Go.

## Chapter 10 — 2026-11-05

The owner gave Go for [Accept transactions — now or later](../manual/accept-transactions.md)
using the documented workflow. The chapter is bundled with three screenshots
and owner-accepted for commit. Supporting verification checked implementation and
focused tests; the primary writer owned prose and captures; an independent
reviewer checked accuracy, continuity, brevity, images and tests. Review clarified
the payday example and the bottom-of-Review action location and corrected a
search-count expectation and a walkthrough comment. No financial or Help viewer
behavior changed. Chapter 11 and context links need their own Go.

The chapter covers all-account acceptance, unchanged inclusive balances,
remaining warnings versus blockers and failed imported records, quit choices,
accepted corrections, import Undo eligibility and the difference between saving,
accepting and removal commands. It uses the current action labels rather than
historical Yes/No wording. Existing UI discrepancies remain: the quit prompt
elides “Quit without accepting” at its fixed width, and Review still says
“What’s blocking commit?”. The manual uses the full quit label and does not
retouch the captured application.

The temporary Household file has Credit card opening zero and Chequing
1,000.00 on January 1. The card’s Market -42.18, refund +6.00 and Corner shop
-20.00 split total -56.18; Chequing has a 2,000.00 payday. One click accepts all
four entries without changing either balance. A later export has Coffee -5.00
and an unreadable amount; Accept valid transactions accepts Coffee and leaves
one failed record. Bus fare -3.00 is left new through quitting and reopening,
with the card balance -64.18 and the failed record still present.

| Asset under `manual/images/` | Captured state |
| --- | --- |
| `accept-ready.png` | Review with no questions, before acceptance across both accounts |
| `accept-review.png` | Unreadable amount remaining and enabled Accept valid transactions |
| `accept-quit.png` | Quit choice with a saved new Bus fare entry |

Capture basis: actual ImportMain.qml in a native macOS Qt 6.12.0 Basic light
harness, 1440 × 900 logical pixels at 2×, temporary synthetic data/settings.
Setup, categories, openings and imports use model commands or existing test
helpers. Real mouse clicks exercise Review, both acceptance actions, quit Cancel
and Quit without accepting; the window close request opens the quit prompt.
A fresh model reopens the file to verify persistence. This is not a manual
standalone-app walkthrough. All three source images were visually inspected
and independently reviewed.

```sh
QT_QPA_PLATFORM=offscreen ./build/import-ui-rules-release/src/ui/spentit_help_ui_tests acceptanceChapterWalkthrough
SPENTIT_GUIDE_CAPTURE="$PWD/manual/images" ./build/import-ui-rules-release/src/ui/spentit_help_ui_tests acceptanceChapterWalkthrough
```

The final offscreen walkthrough passed 3/3 including init/cleanup in 3054ms;
native capture passed 3/3 in 4070ms. An earlier harness incorrectly reused a
window after completing its quit lifecycle, so a second quit prompt did not
open. The corrected test reopens through a fresh model. Native Accept and quit
is not claimed by this walkthrough; the focused existing UI test covers that
choice and its failure path.

Supporting checks:

```sh
QT_QPA_PLATFORM=offscreen ./build/import-ui-rules-release/src/ui/spentit_import_ui_tests datasetAcceptanceAndRollback warningAndQuitAcceptance commitOutcomeRecovery
ctest --test-dir build/import-ui-rules-release -R '^(statement\.(overlap|rollback|import_undo)|import\.manual_transactions)$' --output-on-failure
```

Results: 5/5 including init/cleanup in 3816ms; storage selection 4/4 in 50.55s.
These cover valid acceptance with failures retained, warnings, both linked
postings, accepted corrections, quit/reopen, failed or uncertain outcomes,
rollback and import Undo. Initial restricted supporting UI execution could not
create application-data locks and crashed after the failed fixture setup;
the authorized rerun passed. No private data was used.

Final rules-enabled Release verification on macOS Qt 6.12.0:

- Rebuilt `spentit_import_experiment`, `spentit_help_ui_tests` and
  `spentit_help_tests`; `build/SpentIt` bundles ten chapters and 32 images.
- The documented five-check CTest selection passed rules UI, catalog and QML;
  Help again reached its 120-second offscreen limit (during Rules reading).
  Preparation initially failed its old nine-chapter/29-image inventory, which
  was updated. Final preparation/catalog rerun passed 2/2 in 0.37s.
  The full offscreen suite remains unverified beyond its recorded timeout.
- Final full native Help, with `SPENTIT_GUIDE_READING_CAPTURE` set to a temporary
  path, passed 28/28 including init/cleanup in 242679ms. Chapter 10 checks
  previous/next and section navigation, actual clicks on all three images,
  image loading/Actual size/close/scroll return, the comparison-table section,
  and 500-pixel reading at 150% text. Normal, inline-image, table and narrow
  rendered views were visually inspected; the table wraps within the article.
- Catalog search for “acceptance” finds chapter 10. Preparation checks all
  bundled links/images. Relative file links and `git diff --check` passed.

An initial bundle build rejected a multiline caption; it was shortened to the
supported one-line italic caption before successful preparation. Dark appearance,
complete keyboard-only/screen-reader use, Linux and Windows remain unverified.
The owner accepted chapter 10 and requested its commit.

## Regex wording clarification — 2026-11-05

After accepting chapter 9 and its rendering fix (commit `5ca3e3d`), the owner
gave Go for three table clarifications: “0 through 9”, a concrete Market 1234
example that names the matched trailing space, and `CARD[0-9]{4}$` without an
unexplained whitespace requirement. Only those three manual rows changed.
Rebuilt the application and both Help test targets; `build/SpentIt` includes
the corrected wording. Help preparation/catalog checks passed 2/2 in 0.23s;
preparation validates bundled links/images. `git diff --check` passed. Native
interaction checks were not repeated for this wording-only update. The owner accepted these clarifications and requested their commit.

## Long Help chapter rendering correction — 2026-11-05

The owner reported perceptible chapter opening, missing prose when scrolling
backwards past the regex table, and a blank next chapter. The earlier image/link
checks did not compare rendered pixels after reverse scrolling. A native
`longChapterScrollRendering` reproduction confirmed the missing heading and
paragraph above the table, with 2.28% of viewport pixels changing after the
round trip. The before/after images were visually inspected. An initial exact
pixel comparison also caught insignificant raster differences; the regression
now tolerates at most 0.5% of pixels changing by more than 24 channel levels.

Qt 6.12 enables viewport-limited text rendering above 10,000 source characters;
the expanded Rules chapter crosses that threshold. Its table-frame traversal
loses surrounding text on reverse scrolling. See the upstream
[TextEdit implementation](https://github.com/qt/qtdeclarative/blob/v6.12.0/src/quick/items/qquicktextedit.cpp)
for `setText`, `transformChanged` and `updatePaintNode`. Help now clears the
public `ItemObservesViewport` flag for table-containing articles, after each
text replacement. The Flickable still clips the article; short/plain articles
retain Qt's normal behavior. This keeps the whole table article's scene graph
in memory, a deliberate cost for this finite bundled manual. No private Qt APIs,
renderer replacement, second process or modal-dialog change was introduced.

Paragraph formatting now skips identical formats and batches changes with
layout disabled, restoring its prior state. Deferred layout is completed on
the GUI thread before rendering. The first batching attempt emitted Qt timer
thread warnings; completing document layout before returning removed those
warnings in the subsequent native reproduction.

The same native harness reported Rules selection at 283ms before and 153–161ms
after, including an intentional 100ms settling wait. Thus the measured work
fell from about 183ms to 53–61ms in these local runs; this is not a broad
performance benchmark. The expanded regression checks visible initial text,
full downward/upward pixel sweeps, real wheel events, actual Contents clicks,
category chapter rendering and restored nonzero Rules scroll positions at both
880-pixel/100% and 500-pixel/150% sizes. It uses no financial file. Independent
review found no blocking issue and prompted the initial-visible-text assertion.

Verification: rebuilt `spentit_import_experiment`, `spentit_help_ui_tests`
and `spentit_help_tests`, including `build/SpentIt`. The expanded native scrolling
case passed 3/3 including init/cleanup in 21370ms before the final visible-baseline
assertion; the final focused offscreen case passed 3/3 in 23312ms. The documented
five-check CTest selection passed rules UI, preparation, catalog and QML (4/5),
but full offscreen Help hit the existing 120-second limit after chapter reading
passed (145.69s total). The isolated new scrolling case passed, so that timeout
is not evidence of a failing pixel assertion. The final full native suite passed 26/26 including init/cleanup in 253250ms,
including the visible-baseline assertion, repeated scroll/chapter restoration,
all chapter images, search, text resizing and modal Help checks. No timer-thread
warning appeared. In that longer suite the Rules selection measurements were
197–220ms including the same 100ms settling wait, showing the local timing
variation. Full offscreen timing remains an unresolved limit, rather than a
claimed full pass. Linux/Windows and screen-reader
checks remain unavailable.

## Chapter 9 regex cheat-sheet follow-up — 2026-11-05

The owner requested and gave Go for a fuller optional regex reference at the
bottom of chapter 9. It now includes a common-pattern lookup table, repetition
and bracket explanations, a worked PAYPAL *EBAY Jewellery capture/template,
a bank-reference example and a template lookup table. The eBay example produces
Jewellery from eBay using `${1} from eBay`; it explains specific-before-broad
rule ordering and the need for a suitable category. No application behavior or
source screenshots changed.

Supporting verification compiled temporary probes against the actual built
SpentIt regex wrapper and PCRE2 10.49. Both capture/template examples and the
listed syntax passed, including literal punctuation, whitespace/digits,
quantifiers, ranges/negation and quoted text. The probe also confirmed Unicode
digits, whitespace including newlines, dot excluding newline by default and
`$` permitting a final newline. The manual uses conventional “end of field”
wording for `$`. Independent review corrected “word” to “text” for an unanchored
Market match, which can also occur within a longer word.

Rebuilt all three application/Help targets. Native Help passed 25/25 including
init/cleanup in 66837ms, with `SPENTIT_GUIDE_READING_CAPTURE` set to a temporary
path. Chapter 9 additionally checks navigation to Common pattern pieces and the
capture-template text; its normal table and 500-pixel/150% text views were captured
and visually inspected. Tables wrap within the reading width; narrow rows become
taller rather than extending past the article. The combined CTest run passed regex, rules UI, preparation, catalog and QML
checks (5/6), but offscreen Help reached its 120-second timeout after the chapter
reading checks passed (144.85s total). It ran alongside native Help; this does
not establish that concurrency caused the timeout. An isolated rerun also timed
out at 120 seconds, after all chapter-reading cases passed and before the next
walkthrough completed. The offscreen full-suite timing issue remains unresolved;
the full native suite passed. Relative file links and
`git diff --check` passed. The owner accepted the chapter, cheat-sheet and rendering correction and
requested their commit. Existing platform/accessibility limits still apply.

## Chapter 9 — 2026-11-05

The owner gave Go for Use rules for repeated categorization under the documented
workflow. [The chapter](../manual/rules.md) is bundled with three screenshots and
owner-accepted for commit. A supporting agent verified code and focused tests; the
primary writer owned prose and captures; an independent reviewer checked both.
Review clarified scenario continuity, eligible preview rows, automatic import
application and selecting a rule before saving its changes. No financial or Help
viewer behavior changed; `rulesLeaveDialog` remains application-modal.

The chapter covers creation, matching fields/patterns, explicit category and
Uncategorized gate, original imported versus current manual text, split-first
ordering, selected-rule Save/Apply, account/new-only scope, Undo, maintenance and
leave choices. The current UI has no all-enabled Apply action. Optional pattern
and template examples stay at the end. Later chapters and context links remain
separate work.

The temporary Household example uses Credit card opening zero and Chequing
1,000.00 on January 1. Market -42.18 starts Uncategorized; the +6.00 refund has
Groceries and Corner shop -20.00 has -12.00 Groceries/-8.00 Household. The card
balance stays -56.18. Market groceries matches the whole original payee Market.
Preview lists one eligible entry; Save and apply categorizes it; Undo restores
eligibility; applying again leaves zero eligible matches. An unfinished name
change exercises Stay and Discard on leaving.

| Asset under `manual/images/` | Captured state |
| --- | --- |
| `rules-editor.png` | New Market groceries rule, whole-field pattern and category |
| `rules-preview.png` | Eligible Market row, date/amount and preview summary |
| `rules-leave.png` | Unsaved-rule Save / Discard / Stay window content |

Capture basis: actual ImportMain.qml in a native macOS Qt 6.12.0 Basic light
harness, 1440 × 900 logical pixels at 2× (dialog captured separately), temporary
synthetic file and isolated settings. Setup/openings/import use model commands;
field text uses Qt properties/signals. Mouse clicks exercise Rules/New rule,
Whole field, preview, both Apply labels, Undo, navigation and Stay/Discard.
Category selection uses Enter; preview scroll uses its Qt property. This is not
a manual standalone-app walkthrough. No private data is used.

```sh
QT_QPA_PLATFORM=offscreen ./build/import-ui-rules-release/src/ui/spentit_help_ui_tests rulesChapterWalkthrough
SPENTIT_GUIDE_CAPTURE="$PWD/manual/images" ./build/import-ui-rules-release/src/ui/spentit_help_ui_tests rulesChapterWalkthrough
```

Offscreen passed 3/3 including init/cleanup in 3246ms before native capture.
Final native capture passed 3/3 in 4842ms. The first capture passed but clipped
the preview row; visual review caught it. The final harness waits for layout and
asserts the match row is on screen before capture. All three final images were
visually inspected and independently reviewed; no crashes occurred.

Supporting checks:

```sh
QT_QPA_PLATFORM=offscreen ./build/import-ui-rules-release/src/ui/spentit_rule_ui_tests accountScopeSaveAndApplyUndo focusedCreationPreviewAndAttribution independentDraftAndSelectedSaveApply simplePatternsAndWholeFieldKey appliedSplitAndTransactionTargets removedRuleKeepsEarlierAssignments
ctest --test-dir build/import-ui-rules-release -R '^(rules\.(regex|evaluation)|rulestore\.(category_gate|category_multi|account_apply|validation))$' --output-on-failure
```

Results: 8/8 including init/cleanup in 6330ms and 6/6 in 5.05s. These cover
matching, category gates, split-first import, durable missing-category choices,
multi-account resolution, selected Save/Apply and Undo. Initial supporting test
sandbox access to application-data locks failed; the authorized rerun passed.

The three application/Help targets rebuilt to `build/SpentIt`, bundling nine
chapters and 29 images. The documented five-check Help regression passed 5/5 in
60.71s. Native Help passed 25/25 including init/cleanup in 194615ms.
Chapter 9 checks previous/next and section navigation, all three actual image
clicks/loading/Actual size/close/scroll return, and 500-pixel reading at 150% text.
A separate native `createOpenChapterReading:rules` capture run passed 3/3 in
21650ms; normal, inline-image and narrow/larger-text rendered views were visually
inspected. Catalog search for “regular expression” finds chapter 9; preparation checks
all bundled links/images. Relative file links and `git diff --check` passed.

Dark appearance, complete keyboard-only/screen-reader use, Linux and Windows
remain unverified. Chapter 10, Accept transactions — now or later, needs its own Go.

## Chapter 8 — 2026-11-05

The owner accepted chapter 7, committed as `2b9c41e`, and gave Go for the next
chapter under the documented workflow. Organize spending with categories is
bundled with four screenshots and is owner-accepted for commit. Supporting verification
checked current code/tests; one primary writer owned prose and captures; an
independent reviewer checked accuracy, continuity and images. Review removed an
incorrect report-selection claim and corrected Chequing's opening in the fixture.
No financial or Help viewer behavior changed.

The chapter covers independent colon names, creation/rename/archive/unarchive,
unused deletion, inline completion, selected-row assignment, split choice,
Uncategorized, account destinations, transfer consequences and eligible Undo.
The existing workspace says “subcategory”; the chapter clarifies independent
names. The Choose split screenshot retains the existing source-0/source-1 labels.
Those copy issues are recorded, not changed in this Help scope.

The synthetic Household file has Credit card opened at zero and Chequing at
1,000.00 on January 1. The card uses Market -42.18, its Groceries refund +6.00,
and Corner shop -20.00 split -12.00 Groceries/-8.00 Household, ending -56.18.
Food: Meals out is created, archived/unarchived and renamed Food: Eating out.
Market starts Uncategorized, is assigned Groceries through completion, then
cleared through the category menu and restored with Undo. The Household split
is likewise cleared and undone; the account total remains -56.18.

| Asset under `manual/images/` | Captured state |
| --- | --- |
| `categories-workspace.png` | Three categories and the two read-only account rows |
| `categories-completion.png` | “gro” matches Groceries in the Market editor |
| `categories-menu.png` | Ordinary categories and account destinations |
| `categories-split.png` | Choose split for Corner shop |

Capture basis: actual ImportMain.qml, macOS Qt 6.12.0 Basic light, 1440 × 900
logical pixels at 2×, temporary synthetic file/settings. Setup/openings/imports
use model commands; text uses Qt properties/signals. Real mouse clicks exercise
workspace navigation, sorting, row context menus, archive/unarchive/rename,
ledger double-click, category choice, split choice and Undo. Category creation
and completion save through Enter. The submenu opens through its Qt method after
a real row right-click. This is not a manual standalone-app walkthrough.

```sh
SPENTIT_GUIDE_CAPTURE="$PWD/manual/images" ./build/import-ui-rules-release/src/ui/spentit_help_ui_tests categoriesChapterWalkthrough
```

The corrected native capture run passed 3/3 including init/cleanup in 6578ms;
the preceding offscreen walkthrough passed 3/3 in 5194ms. Initial harness runs
used an invalid account-selection key, dereferenced a dropdown outside its
owning item, and read a cleared selection after a menu save. These caused test
process crashes and macOS crash dialogs noticed by the owner. The harness now
uses the correct account key, resolves the popup from its field, checks split
list presence and verifies the available Undo after menu assignment. It was
checked offscreen before resuming native captures. These failures used only
temporary synthetic files, not the owner's data. An intermediate build correctly
rejected the draft's not-yet-captured images; it was bundled after captures existed.

Supporting checks passed on macOS Qt 6.12.0:

```sh
QT_QPA_PLATFORM=offscreen ./build/import-ui-rules-release/src/ui/spentit_category_ui_tests inlineCategoryCompletion catalogPickerCurrentNamesAndArchive explicitSortingAndRetainedSelection inlineEditingAndScroll
QT_QPA_PLATFORM=offscreen ./build/import-ui-rules-release/src/ui/spentit_import_navigation_tests categorySubmenuScrolls acceptedManualTransferAssignment categoryMenuSavesMixedRowsAndReopens
ctest --test-dir build/import-ui-rules-release -R '^(category\.(names|assignments|history|validation)|import\.transfer_assignment)$' --output-on-failure
```

Results: 6/6 in 13951ms and 5/5 in 2623ms including init/cleanup; storage selection
5/5 in 10.40s. The scrolling test emitted two existing mouse-outside-window warnings
but passed. The first sandbox attempt lacked application-data lock access; the
authorized rerun passed. Bulk, transfer and deletion constraints have focused
existing checks rather than new native chapter screenshots.

Final rules-enabled Release checks on macOS Qt 6.12.0:

- Rebuilt `spentit_import_experiment`, `spentit_help_ui_tests` and
  `spentit_help_tests`; `build/SpentIt` bundles eight chapters and 26 images.
- The documented five-check Help regression passed 5/5 in 55.79s.
- Native `spentit_help_ui_tests` passed 23/23 including init/cleanup in 184933ms,
  without further crashes. Chapter 8 checks previous/next and section navigation,
  all four actual image clicks/loading/Actual size/close/scroll return, and
  500-pixel reading width at 150% text.
- A separate native `createOpenChapterReading:categories` capture run passed
  3/3 in 23703ms. All four source images and normal, inline-image and narrow
  larger-text rendered views were visually inspected.
- Search for “unarchive” finds chapter 8; preparation checks every bundled
  link/image. Relative file links and `git diff --check` passed.

Dark appearance, full keyboard-only traversal, screen readers, Linux and Windows
remain unverified. Chapter 9 and context links require their own Go.

## Chapter 7 — 2026-11-05

The owner gave Go for Add and correct transactions using the documented workflow.
A supporting agent checked implementation and focused tests; the primary writer
owned prose and native captures; an independent reviewer checked code, continuity,
brevity and all four images. Review corrected the new-entry category-save exception
and the refund category in the capture fixture. No financial behavior or Help
viewer implementation changed. The owner accepted the chapter and requested its commit, then gave Go for
chapter 8, Organize spending with categories. Context links remain separate.

The chapter covers manual creation, inline correction, calendar, amount expressions,
splits/remainder, mixed signs, original values, selection rename/restore/deletion,
transfer restrictions and Undo. The synthetic Credit card opens at zero on
2026-01-01, with the original Market -42.18 and Groceries refund +6.00. Adding
Corner shop -20.00 on January 22 gives -56.18; Groceries -12.00 and Household
-8.00 split that purchase without changing its total. The chapter explicitly
says not to add another copy if following chapter 5's example.

| Asset under `manual/images/` | Captured state |
| --- | --- |
| `editing-new.png` | Unsaved Corner shop date/payee/amount/memo |
| `editing-calendar.png` | January 2026 picker beside the date |
| `editing-splits.png` | Saved -12.00/-8.00 split, automatic remainder |
| `editing-mismatch.png` | Unsaved -10.00/-8.00 versus -20.00 total; choices |

Capture basis: actual ImportMain.qml, macOS Qt 6.12.0 Basic light appearance,
1440 × 900 logical pixels at 2×. Temporary synthetic Household file and isolated
settings; no private data. Setup, opening and imports use model commands; field
text uses Qt properties/signals. Real mouse clicks open creation/calendar, add
splits, choose Keep editing and Stay; real keys select a calendar day, save fields
and discard an unfinished memo. Category choices use text/signals and Enter.
Undo is invoked through its model command. This is a native Qt harness walkthrough,
not a manual standalone-app session.

```sh
SPENTIT_GUIDE_CAPTURE="$PWD/manual/images" ./build/import-ui-rules-release/src/ui/spentit_help_ui_tests editingChapterWalkthrough
```

The corrected source capture run passed 3/3 including init/cleanup in 4627ms.
An initial sandbox run had no display; the authorized native run passed. Rebuild
all three application/Help targets after capture changes.

Supporting offscreen verification passed on macOS Qt 6.12.0:

```sh
QT_QPA_PLATFORM=offscreen ./build/import-ui-rules-release/src/ui/spentit_import_ui_tests inlineSourceRestore qmlEditorNavigation existingSplitRemainder removedInitialRemainder amountExpressions remainderSelection splitAmountEditing immediateEditor
QT_QPA_PLATFORM=offscreen ./build/import-ui-rules-release/src/ui/spentit_import_navigation_tests manualTransactionEntry
QT_QPA_PLATFORM=offscreen ./build/import-ui-rules-release/src/ui/spentit_committed_ui_tests selectedRenameRestoreAndUndo addThenRemoveEmptySplit correctionGuardsRemovalAndUndo
```

Results, including init/cleanup: 10/10 in 7004ms, 3/3 in 1771ms and 5/5 in 3953ms.
These cover Restore/selection Undo, persisted remainder choices, exact arithmetic,
creation and leave choices, reopening, corrections/removal and blocked history
reversal. The first sandbox attempt lacked application-data lock access; rerunning
with authorized access passed.

Final rules-enabled Release verification on macOS Qt 6.12.0:

- Rebuilt `spentit_import_experiment`, `spentit_help_ui_tests` and
  `spentit_help_tests`; `build/SpentIt` bundles seven chapters and 22 images.
- The documented five-check Help regression passed 5/5 in 127.29s.
- Native `spentit_help_ui_tests` passed 21/21 including init/cleanup in 30784ms.
  Chapter 7 checks next/previous, section navigation, all four actual image
  clicks, loading, Actual size, closing/scroll return, and normal/narrow width
  at 500 pixels with 150% text.
- Catalog search for “arithmetic” finds the new chapter. Preparation validates
  every bundled link and image. Relative file links and `git diff --check` passed.
- A separate native `createOpenChapterReading:editing` capture run passed 3/3
  in 22192ms. All four refreshed source images and normal, inline-image and
  narrow/larger-text rendered views were visually inspected.

Dark chapter appearance, full keyboard-only traversal, screen readers, Linux and
Windows remain unverified. Restore, deletion and transfer restrictions have code
and existing offscreen checks rather than new native chapter captures.

## Chapter 6 — 2026-11-05

The owner accepted chapter 5, committed as `bbd3a8b`, and gave Go for the next
chapter using the documented workflow. Chapter 6, Review duplicates and link
transfers, is bundled with four screenshots and is owner-accepted for commit. Supporting
verification checked current code and existing tests; one primary writer owned
prose/captures; an independent reviewer inspected accuracy and all four images.
Corrections distinguish imported transaction totals from account balances when a
generated counterpart is removed, specify whole-field exclusion matching, and
limit the short ledger Link route to single-split entries. No dedicated Unlink
control is exposed, so the chapter covers eligible Review Undo without inventing
an Unlink workflow. No financial behavior or Help viewer implementation changed.

The chapter covers same-account Merge, survivor choices/manual entries, No and
Selected yes, batch group versus row selection, cross-account Link and split
selection, generated counterparts, amount differences, explicit ledger routes,
exclusions and Undo restrictions. Later editing topics remain chapter 7.

The examples explicitly return to the original purchase/refund, without chapter
5's extra Corner shop purchase. Credit card opens at zero on 2026-01-01. Two
Market purchases of -42.18 and a 6.00 refund total -78.36; keeping one purchase
restores -36.18. The native walkthrough merges, undoes, answers No (keeping both),
undoes again and merges. Chequing opens at 1,000.00; its January 28 payment of
-36.18 is linked to the card's January 29 +36.18. Balances remain 963.82 and zero.
A real click on the card's transfer arrow visits Chequing.

A separate amount-difference scenario imports an explicit -37.18 Chequing
transfer and +36.18 card posting. The generated counterpart makes the pre-link
card balance 73.36. Link replaces it, preserves the two imported totals, and
leaves the card at 36.18 with a 1.00 Uncategorized remainder on the outgoing
transaction. Cancel keeps the question; Link resolves it; Undo restores it.
The chapter says to choose a suitable category only after confirming the fee.

| Asset under `manual/images/` | Captured state |
| --- | --- |
| `review-keep.png` | Both purchase copies new; choose the transaction to keep |
| `review-link.png` | Opposite payment postings one day apart, candidate selected |
| `review-linked.png` | Card payment linked to Chequing, arrow visible, zero balance |
| `review-difference.png` | Link 36.18 and categorize 1.00; Link and Cancel |

Capture basis: actual ImportMain.qml, macOS Qt 6.12.0 Basic, light appearance,
1440 × 900 logical pixels at 2×, isolated temporary Household files. Setup,
opening/account creation and imports use model commands. Real mouse events
exercise Review Yes/No, survivor choice, Undo, account navigation, transfer arrow,
and difference Link/Cancel. Captures are not a manual standalone-app walkthrough;
no private data or user settings are used.

```sh
SPENTIT_GUIDE_CAPTURE="$PWD/manual/images" ./build/import-ui-rules-release/src/ui/spentit_help_ui_tests reviewChapterWalkthrough
```

The initial native capture walkthrough passed 4/4 including init/cleanup in
7326ms. Rebuild the three application/Help targets after refreshing images.
Supporting verification passed:

```sh
QT_QPA_PLATFORM=offscreen ./build/import-ui-rules-release/src/ui/spentit_import_ui_tests batchReviewSurvivorSelection batchReviewWorkspace unifiedReviewSheet linkExclusions reviewMultipleLinkCandidates unifiedReviewLinkDifference transferManagementUi
ctest --test-dir build/import-ui-rules-release -R '^(import\.review_policy|import\.review_state|import\.manual_transactions)$' --output-on-failure
```

The offscreen selection passed 12/12 including init/cleanup in 9929ms. The three
storage/policy selections passed 3/3 in 42.62s. These cover multiple candidates,
survivor/batch choices, rejection persistence, exclusions, amount-difference
Cancel/Link/Undo, explicit ledger linking and manual/import matches.

Final rules-enabled Release verification on macOS Qt 6.12.0:

- All three application/Help targets rebuilt; `build/SpentIt` contains six
  chapters and 18 images.
- The documented five-check Help regression passed 5/5 in 103.21s.
- Native `spentit_help_ui_tests` passed 19/19 including init/cleanup in 45829ms.
  Chapter 6 checks next/previous and section navigation, all four actual image
  clicks, image loading, Actual size, close/scroll return, normal width and
  500-pixel width at 150% text.
- A separate native `createOpenChapterReading:review` capture run passed 3/3 in
  2542ms. All four source images and rendered normal, inline-image and narrow
  larger-text views were visually inspected.
- Search for “unlink” finds the new chapter. The preparer validates every
  bundled link/image. Relative file links and `git diff --check` passed.

Dark chapter appearance, full keyboard-only traversal, screen readers, Linux and
Windows remain unverified. Batch selection, exclusions and manual/import matching
have existing offscreen coverage rather than new native chapter captures.
The owner accepted chapter 6 and requested its commit; chapter 7 and context links need Go.

## Chapter 5 — 2026-11-05

The owner gave Go for Find your way around transactions using the documented
chapter workflow. A verification agent checked implementation and focused
existing tests; the primary writer owned prose, native interaction and captures;
a separate reviewer checked accuracy, continuity and all three source images.
Findings corrected the missing-entry advice, the visible × clear control,
opening chronology, and the possible-duplicate balance explanation.

The chapter covers ledger columns/signs/balances, memo inspection, accepted
transaction History, selection versus editing, split previews, transfer arrows,
search/clear/errors, sorting, scrolling, Back/Forward and Review orientation.
There is no ordinary visible acceptance badge in the current ledger; the chapter
uses the existing History control. No financial or viewer behavior changed.
Chapter 6 decisions and application-wide context links remain separate work.

The synthetic Credit card opens on 2026-01-01 at zero. Market -42.18 and refund
6.00 are followed by Corner shop -20.00, split into Groceries -12.00 and Household
-8.00. These entries are accepted for the History check. Searching Market shows
two rows while the account balance remains -56.18. A later differently named
Market groceries -42.18 import produces a possible-duplicate question; both rows
remain, so the balance is -98.36. No question is answered in this walkthrough.

| Asset under `manual/images/` | Captured state |
| --- | --- |
| `transactions-ledger.png` | Market selected, Corner shop saved splits expanded |
| `transactions-search.png` | Market results, unchanged sidebar balance |
| `transactions-review.png` | One Credit card duplicate question, both entries retained |

Capture basis: actual ImportMain.qml, macOS Qt 6.12.0 Basic, light appearance,
1440 × 900 logical pixels at 2×, temporary synthetic Household file. Setup,
imports, opening correction and acceptance use model commands; search text uses
Qt properties/signals. Real mouse events exercise row selection, Shift/Ctrl
selection, double-click editor, saved splits, Apply, sort, clear, Review and
Back/Forward. The test checks memo/History and that selection leaves the editor
closed. It bypasses the native file picker and is not a standalone-app manual
walkthrough. An initial harness run lacked an explicit import destination; a
subsequent run tried acceptance before the category dialog was ready and failed,
then crashed during continued test execution. The final harness supplies the
account, waits for the category decision, and sets the January opening explicitly.

Reproduce source captures, then rebuild the three Help/application targets:

```sh
SPENTIT_GUIDE_CAPTURE="$PWD/manual/images" ./build/import-ui-rules-release/src/ui/spentit_help_ui_tests transactionsChapterWalkthrough
```

The standalone native capture walkthrough passed 3/3 including init/cleanup,
5115ms. Supporting offscreen checks passed:

```sh
QT_QPA_PLATFORM=offscreen ./build/import-ui-rules-release/src/ui/spentit_import_ui_tests combinedSearchAndSelection searchReturnNavigation readOnlySplitViews selectionDoesNotFlash unifiedReviewSheet
QT_QPA_PLATFORM=offscreen ./build/import-ui-rules-release/src/ui/spentit_import_navigation_tests firstSplitTransferNavigation accountSortSurvivesRestart ledgerViewportSurvivesCategoryNavigation guardedNavigationAndIsolation
```

Results: 7/7 in 6539ms and 6/6 in 5994ms, including init/cleanup. These checks
include search-result selection restoration, failed queries, account return
positions and per-account sort persistence. An initial sandbox attempt lacked
application-data lock access; the authorized rerun passed.

Final rules-enabled Release verification on macOS Qt 6.12.0:

- The three targets in “Build and check” rebuilt; `build/SpentIt` contains five
  chapters and 14 images.
- The documented five-check Help regression passed 5/5 in 38.95s.
- Native `spentit_help_ui_tests` passed 16/16 including init/cleanup in 73435ms.
  The new reading row checks next/previous navigation, the Find a purchase
  section, all three actual image clicks, image loading, Actual size, close and
  scroll return, and width at normal size and 500 pixels with 150% text.
- A separate native `createOpenChapterReading:transactions` capture run passed
  3/3 in 21484ms. All three source screenshots and the rendered normal,
  inline-image and narrow/larger-text chapter views were visually inspected.
- Catalog lookup for “ledger” selects chapter 5. The preparer validates all
  bundled links, sections and images. Relative file-link checks and
  `git diff --check` passed.

Dark chapter appearance, complete keyboard traversal, screen readers, Linux and
Windows remain unverified. Native transfer-arrow traversal is not part of this
new walkthrough; related existing offscreen tests and code were reviewed.
The owner accepted chapter 5 and requested its commit, then gave Go for chapter 6.

## Chapter 4 — 2026-11-05

The owner's “Commit and Go” accepted chapter 3 and authorized Bring in
transactions using the same workflow. A supporting agent verified current import
code/tests, the primary writer owned prose and captures, and an independent
reviewer checked accuracy, continuity and screenshots. Review clarified the
Use Uncategorized consequence and removed unhelpful refusal wording.

The chapter covers supported exports, account routing/mapping, date-order choices,
new-account openings, missing categories, transfer destinations, result counts,
repairs/cancellation, repeated exports, and whole-import Undo versus account-wide
Discard. It links the prior chapter's opening-balance section. No financial or
viewer implementation changes or application-wide context links were added.

The synthetic sequence starts Chequing at 1,000.00, imports Salary 1,500.00,
then maps a second QIF containing Credit card's Market purchase/refund and a
5.00 Cash bakery purchase. Credit card opens at zero; Cash explicitly uses a
known reported opening of 50.00. Credit card ends at -36.18 and Cash at 45.00.
Groceries is created through Apply Selected. The same multi-account import is
repeated without changing the card balance; Undo first removes that duplicate-only
import, then the original multi-account import's three new transactions. Earlier
Salary and account openings remain. No private records or user settings are used.

| Asset under `manual/images/` | Captured state |
| --- | --- |
| `import-destination.png` | Unnamed export asks for Chequing as destination |
| `import-mapping.png` | Create Credit card and Cash, with explicit Cash 50.00 opening |
| `import-categories.png` | Groceries checked in missing-category decision |
| `import-results.png` | Saved Credit card purchase/refund, editor closed |
| `import-undo.png` | Three transactions to remove, Credit card and Cash affected |

Capture basis: actual ImportMain.qml, macOS Qt 6.12.0 Basic, light appearance,
1440 × 900 logical pixels, 2× source captures, isolated temporary Household file.
The harness supplies file-picker results through the import command, fills fields
and combo choices through Qt properties/signals, and accepts the destination
dialog through its accept method. It uses actual mouse clicks for Import,
Apply Selected, account context-menu Undo, confirmation, and ledger double-clicks.
This does not verify native file-picker interaction or an independent person
following the chapter in the standalone application.

```sh
SPENTIT_GUIDE_CAPTURE="$PWD/manual/images" ./build/import-ui-rules-release/src/ui/spentit_help_ui_tests importChapterWalkthrough
```

Rebuild the three targets in “Build and check” after refreshing the captures.
The refreshed native walkthrough passed 3/3 including init/cleanup in 5171ms.
Final verification on macOS, rules-enabled Release, Qt 6.12.0:

- Built `spentit_import_experiment`, `spentit_help_ui_tests`, and
  `spentit_help_tests`; `build/SpentIt` contains all four chapters and 11 images.
- The five-check CTest command in “Build and check” passed 5/5 in 35.27s.
- Native `spentit_help_ui_tests` passed 14/14 including init/cleanup in 72681ms.
  Chapter 4 reading checks click all five rendered images, verify loading,
  Actual size and scroll return, navigate previous/next and the Undo section,
  and check width at normal size and at 500 pixels with 150% text. Search finds
  the new topic with QFX; build preparation checks every bundled link/image.
- Focused import tests passed 13/13 including init/cleanup in 11735ms:

  ```sh
  QT_QPA_PLATFORM=offscreen ./build/import-ui-rules-release/src/ui/spentit_import_ui_tests importRoutingAndLedgerCleanup automaticImportDates mappingOpeningChoices mappingOpeningInvalidReport multiAccountImport mappedTransferImport unknownAmountRepair recordRepair groupedCancellationAndMappingPersistence undoSelectedImport importUndoOutcomes
  ```

- The five-check suite includes the existing missing-category restart test in
  `import.rules_ui`. Focused import tests cover repair, currency/mapping choices,
  ambiguous dates, cancellation, Undo selection and unknown/saved outcomes.
- All five source images and normal/image/narrow rendered chapter captures were
  visually inspected. Relative file-link checks and `git diff --check` passed.

All five source screenshots were inspected. The existing menu/dialog labels and
account identifiers are preserved as rendered. Dark appearance, complete keyboard
traversal, screen readers, Linux and Windows remain unverified. The owner accepted chapter 4 and requested its commit. Do not start chapter 5
without its Go.

## Chapter 3 — 2026-11-05

The owner gave Go for Set up your accounts using the accepted chapter workflow.
A supporting agent checked current UI/storage code and existing tests; the primary
writer authored the chapter and owned interaction/captures; a separate agent
reviewed accuracy, brevity, continuity, and screenshots. Review corrections clarify
that manual transactions also prevent deletion and that reviewing pending imports
alone does not permit archiving; acceptance applies across accounts.

The chapter covers inline account creation, home-currency defaults, opening
balances (including credit-card debt), switching and reading balances, reordering,
retained names, rename/archive/reactivate/delete, currency reassignment and
conditional Undo. Later management is a separate section in the same topic.
No financial behavior, viewer implementation, or context links changed.

Three screenshots use the real ImportMain.qml in the native Qt harness, with
an isolated temporary Household file, Chequing and Credit card, CAD, light
appearance, Qt 6.12.0 Basic on macOS, 1440 × 900 logical pixels and 2× capture.
No private financial data or user settings were used.

| Asset under `manual/images/` | Captured state |
| --- | --- |
| `accounts-new.png` | Credit card typed in the inline account-name field, before Enter |
| `accounts-opening.png` | Chequing opening editor with 2026-01-01 and 1,000.00, before saving |
| `accounts-menu.png` | Chequing right-click menu after saving that opening |

Reproduce the source captures with:

```sh
SPENTIT_GUIDE_CAPTURE="$PWD/manual/images" ./build/import-ui-rules-release/src/ui/spentit_help_ui_tests accountsChapterWalkthrough
```

Rebuild the three targets in “Build and check” afterward to embed refreshed
images. The walkthrough creates/removes only its temporary synthetic file.
It uses real mouse clicks for creation entry, switching, context menus, currency
reassignment/Undo and archive/reactivation, a double-click for the opening row,
and Enter for creation/opening saving. Fields are populated through Qt properties.
An initial native run clicked before a menu transition finished; the harness now
waits for popup transitions before mouse hit testing, and the walkthrough passes.
The first sandboxed attempt had no display; native runs used display access.

Verification on macOS, rules-enabled Release, Qt 6.12.0:

- Built `spentit_import_experiment`, `spentit_help_ui_tests`, and
  `spentit_help_tests`; the application linked to `build/SpentIt`.
- The five checks in “Build and check”, plus `import.ui`, `import.navigation`,
  and `import.category_ui`, passed 8/8 in 162.87s:

  ```sh
  ctest --test-dir build/import-ui-rules-release -R '^(help\.prepare|help\.catalog|import\.help_ui|import\.qml|import\.rules_ui|import\.ui|import\.navigation|import\.category_ui)$' --output-on-failure
  ```

- Native `./build/import-ui-rules-release/src/ui/spentit_help_ui_tests` passed
  12/12 including init/cleanup in 49542ms after the final prose corrections.
- Chapter reading checks cover previous/next, the currency section link, all
  three real image clicks/loading/Actual size/scroll return, and no horizontal
  overflow at normal width and 500 pixels with 150% text. Catalog search finds
  the chapter by “reassign”; preparation validates its metadata, links and images.
- Existing account/navigation checks also passed, including rename/delete menu
  behavior, keyboard dialogs, opening correction and account drag ordering.
- Relative file-link checks and `git diff --check` passed.

The source images and normal/narrow rendered chapter captures were visually
inspected. Long labels in the existing account menu remain truncated; the prose
supplies their full names. Native testing is a Qt harness walkthrough, not a
person following the manual in the standalone application. Dark appearance,
full keyboard-only traversal, screen readers, Linux and Windows remain unverified.
Chapter 3 was subsequently accepted and committed as `467a20e`; chapter 4
received its Go in the same instruction.

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

## Continuing the manual — 2026-11-05

Chapters 1 and 2 are committed; the latest implementation commit is `34e2662`
(chapter 2 and the clickable-picture fix). The owner requested documentation of
[the accepted chapter workflow](../features/013-help-system.md#accepted-chapter-workflow--2026-11-05):
sequential chapters, one primary writer, supporting verification and independent
review agents, actual interaction checks, then owner review and a requested commit.
The primary writer owns UI interaction/captures and integrates all findings.

The original continuation handoff nominated **chapter 3, Set up your accounts**
for a fresh session. Its subsequent Go and results are recorded above. Before
continuing with another approved chapter, read:

- `AGENTS.md` and [status](status.md).
- This handoff and the accepted workflow linked above.
- [The chapter-3 outline](../manual/outline.md#3-set-up-your-accounts).
- [The language guide](user-facing-language.md), including the brevity direction.
- [Chapter 1](../manual/index.md) and [chapter 2](../manual/create-open-file.md)
  for voice, terminology, and what the reader already knows.

Chapter 3's planned scope is accounts, currencies, opening dates/balances,
switching accounts, and later account management. Verify actual controls and
consequences before writing; do not repeat chapter 2's file-creation walkthrough.
Use the existing synthetic household example. Later chapters and application-wide
context links remain separate work requiring their own Go.

## Next agent

1. Read the “Continuing the manual” section above and its linked workflow and sources.
2. Chapter 11, “Answer questions with reports”, is owner-accepted for commit. Commit when requested. The next chapter is chapter 12, “Look after your work and resolve remaining problems” in `manual/outline.md`, after its Go.
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
