# 005 — Transaction sheet and navigation

Status: **active**  
Last updated: 2026-10-28  
Implementation: partial. The durable QIF UI now has combined text/date/source/regex search, identity-based multiselection, and atomic committed bulk category/removal/group Undo; see [contract and verification](../doc/search-ledger.md). Point 8 adds the [session sidebar balances, column ratios, narrow scroll, and viewport retention](#point-8-sheet-slice--2026-10-27). The synthetic sheet and broader account/navigation design remain separate bounded scopes. The optional Qt Quick sheet implements the shell, sorting/literal filtering, stable selection, inline engine-backed editing, and Save / Discard / Stay for session navigation/closing. The [durable UI](../doc/import-ui.md) adds setup/review/edit/commit with persistence; it does not implement this full sheet design. See [scope, actual verification and measurements](../doc/sheet-prototype.md). Full acceptance criteria below remain open; this feature is **active**.

## Application identity — accepted 2026-10-28

The durable UI is SpentIt, the actual application being improved toward release. Remove prototype-only banners, synthetic setup defaults, obsolete schema-specific guidance and development-plan placeholders from its user-facing screens. Explain real limitations where they affect decisions: forgotten-password recovery is unavailable, and unsupported document formats must still be refused without modifying the file. Keep the separate synthetic development harnesses and historical verification records accurately labeled. This cleanup does not authorize reports, format migrations or distribution work.

## Welcome layout and scrollbars — accepted 2026-10-28

Use the minimal welcome states and creation defaults in [document setup](001-documents-storage-security.md#welcome-and-creation--accepted-2026-10-28). Center a comfortably spaced column with room above the title for future icon artwork. Keep setup scrollable on smaller windows. Currency menus, other selection menus, transaction lists and scrollable panes use draggable 16 px scrollbars with a visible track/thumb whenever content overflows; they do not fade away while idle. Ledger columns reserve space for the vertical bar.

## Accepted requirements

Category hierarchy, naming, neutral income/expense classification, and account destinations in the selector are specified in [feature 010 — Categories and selection](010-categories-and-selection.md). The bounded catalog and account transfer destinations are implemented. Category reports remain unimplemented.

- Portable, responsive desktop UI with a consistent appearance on macOS, Linux, and Windows.
- Sidebar exposes accounts and setup areas such as accounts and rules.
- Sheet columns: Date, Payee, Category, Withdrawal, Deposit, Balance. Committed and draft views must be distinguishable.
- Click selects; multiple selection is supported. Context actions include Delete and Change category.
- Double-click expands a transaction inline for editing rather than opening a separate popup dialog. Only one transaction editor is active; other split transactions can be expanded for viewing.
- Editor exposes date, payee, category, memo, expense/income, note, split controls, rule controls, QIF/source toggle, OK, and Cancel.
- Split allocations have their own category, memo, and amount. Display split structure without implying that every allocation has the parent category.
- Source view is read-only. Double-clicking a source field copies it into its corresponding editable field, subject to validation. Source absence for manual records is explicit.
- Matched rule opens for editing; unmatched transaction can create a rule. If multiple rules contributed, the design must expose that fact.
- After categorizing an uncategorized transaction, offer to apply that category to other matching uncategorized transactions. When accepted, the intended memo/note can also be copied.
- Back/forward navigation traverses visited screens, including accounts and rules, within the session.

## Accepted main layout and behavior — 2026-10-25

This is the owner's current intended layout, expected to evolve through use. It establishes the application shell now; it does not finalize Settings, the report generator, category editor, or rules editor. Those designs will follow separately. Qt Quick/QML remains a technology proposal.

### Application shell

- Two main panes: a narrow left navigation pane and a wide right content pane for the selected account ledger, report, Settings, categories, or rules.
- A toolbar spans the window above both panes and stays visible when navigation is hidden. It contains a sidebar hide/show control, Back and Forward controls for the session's visited right-pane screens, a title identifying the current view, and a search field with a magnifying-glass control at the right.
- The sidebar can be resized horizontally or completely hidden/restored with its toolbar control or a keyboard shortcut. Its maximum width is less than the window width, leaving room for content. Exact limits and shortcut are unresolved.
- The sidebar contains Accounts above Reports, separated by a draggable horizontal divider that controls their heights. Each list scrolls independently when its contents exceed its allocated height.
- First-run setup and the password-entry/dataset-browser screen follow the [document setup and recovery requirements](001-documents-storage-security.md#accepted-setup-unlock-and-recovery-behavior--2026-10-25), including optional password protection and a browsable suggested Documents/SpentIt location.
- Settings includes the path specifying which dataset to load. Each running application has one active dataset; users can close it and open another. See [document lifecycle](001-documents-storage-security.md). This does not establish an application-wide instance limit.
- Fixed controls at the bottom open Settings, the category editor, and the rules editor in the right pane. A gear is intended for Settings; the other icons remain undecided.
- Accounts and Reports each have a `+` control. Clicking an account or report selects its right-pane display. Right-click offers rename and the appropriate removal action; nonempty accounts are archived, not deleted (see [account semantics](002-accounts-money-splits.md)).
- Adding an account appends an inline name editor at the bottom of the account list and scrolls it into view. Enter creates the named account using the document's base/home currency, with an editable special Opening Balance entry dated today and initially zero. Currency correction is a separate deliberate modal action described in feature 002.
- Adding a report opens a modal dialog. **Ask the owner for its contents when implementing report creation; do not invent the report-generator design.** Saved report names appear in the Reports list. The sketch's YTD, Last Month, and This Month illustrate entries, not a settled built-in report set.

### Sidebar balances and sizing

Account names truncate with an ellipsis when needed. Aim to show the largest displayed balance in full, plus a space and at least a few account-name characters. The original rough 99-million sizing estimate is not a financial limit. Allow negative signs, supported currency scales, large values, and unambiguous currency identity; a bare dollar sign cannot distinguish CAD from USD. Behavior when the available window width cannot accommodate these requirements remains to be designed.

Normally the sidebar shows committed account balances, independent of ledger search or sorting. Once an import supplies a draft projection, default to showing the projected balance. Clicking the balance toggles between committed and projected states, allowing reconciliation with the bank without scrolling the ledger to its beginning. Distinguish states with an explicit label/indicator as well as colour. Draft edits and exclusions update the projection; committing makes the published result the committed balance. Multiple pending batches and state persistence still require a policy.

### Ledger, resizing, and edit navigation

- Columns are Date, Payee, Category, Withdrawal, Deposit, and Balance. Every column supports ascending and descending sorting, with an indicator showing the current sort column/direction.
- Running balances retain their chronological account meaning. Searching hides rows without recalculating their balances; sorting reorders rows with their existing chronological balances. Same-date ordering still needs a deterministic definition.
- A bottom totals row displays Withdrawal and Deposit totals for all rows matching the current ledger filter, including rows outside the viewport. Label these as filtered totals, distinct from account balances. Totals reconcile exactly to the displayed ledger amounts; matching a split field does not silently turn these into category-allocation totals.
- Window resizing adjusts the layout, column widths, and pane heights. Exact proportional/minimum sizing rules remain open. The initial direction is a compact date column, money columns narrower than Payee/Category, and Category generally narrower than Payee, informed by category-name widths. User column-resizing behavior and persistence remain unspecified.
- Leaving a changed, unsaved inline edit through navigation, Back/Forward, document switching, or closing offers Save / Discard / Stay. Save validates and completes the applicable operation before leaving; failed validation keeps the editor open. Discard abandons that edit; Stay leaves it active. Enter or OK is the explicit save path; an unchanged editor needs no unsaved-change prompt. Multiline-note Enter behavior remains a proposed newline exception to define with keyboard interactions.

### Transfer interaction

Accounts appear as distinguishable transfer destinations in the category selector. Selecting one initiates counterpart matching/linking or generates a destination draft. A match icon on either linked transaction opens the other account ledger, selects the counterpart, and brings it on-screen. Its match icon returns to the original; toolbar Back also returns. Both automatic and manual matches support this navigation, subject to Save / Discard / Stay for an active edit.

Offer Fix match from either side for missed or incorrect matches, and highlight generated counterparts still needing attention after import. Show protected drafts distinctly: they remain uncommitted and contribute only to the projected balance even after their origin commits. Clearing their destination workspace does not remove them. See [transfer lifecycle](008-transfer-matching-and-lifecycle.md) for provenance, repair, and removal rules.

### Search selection and return behavior

Remember the selected transaction and ledger view before a search begins. If the user clears/cancels search without selecting a result, restore the pre-search selection and view, even if that transaction was excluded by the search. If the user selects a search result, clearing/cancelling search instead keeps that transaction selected and brings it on-screen in the unfiltered ledger. Do not silently select a different result and treat that as an explicit user choice. Retain transaction identity rather than a volatile row index. Exact keyboard/multiselection and deleted-row fallback behavior remains to be designed.

### Search direction; grammar remains unresolved

Search filters the selected account's ledger across the whole editable/display record, including payee, categories, notes, and split memos. A `QIF:` mode searches original QIF evidence instead of the display interpretation. Source evidence remains read-only. Define behavior for manual records with no QIF source and the exact scope of the prefix before implementation.

The owner wants a query language combining text with period filters. Illustrative intent: `period:may-june starbucks` finds Starbucks transactions during May and June of the current year. Desired date forms include `20261001`, `2026-10-01`, and `2026/10/01`. Settings will provide a date-format preference used by period parsing. These examples express requirements, not a finalized grammar: range delimiters, boundaries, quoting, precedence, and ambiguity handling must be designed together with the owner. Ambiguous dates must not be silently guessed.

Regex capability is desired, with case-insensitive matching by default where appropriate. Whether ordinary text is literal or regex, how regex is requested, flag overrides, and interaction with query operators remain open. Invalid expressions and resource-limit failures must produce explicit errors rather than appear to be successful searches with no matches. Bound search work and keep it off the UI thread as appropriate.

## Accepted search and selection policy — 2026-10-26

The owner accepted the following policy for the next search/ledger package. These decisions supersede the corresponding open grammar and selection questions above; they are requirements, not implementation evidence.

- Ordinary terms are case-insensitive literals joined with AND. Double quotes delimit a phrase. Regex is explicit: `regex:"pattern"`.
- A query-wide `QIF:` prefix searches original record evidence instead of the editable interpretation. Records without QIF evidence do not match that mode. For example: `QIF: period:2026-05-01..2026-06-30 "coffee shop"`.
- Date ranges use `..` and include both endpoints. Accepted full dates are `YYYYMMDD`, `YYYY-MM-DD`, and `YYYY/MM/DD`. `period:may-june` means May through June of the current year. Ambiguous numeric dates are rejected until a date-format preference has been explicitly chosen.
- Shift selects a range; Cmd/Ctrl toggles individual rows. Selection follows stable identities. Clearing search restores the pre-search selection if no result was explicitly chosen; otherwise it retains only explicitly selected results. If all selected records were removed, selection is empty.
- Bulk Delete and Change category require an exact preview and one atomic operation. A loop of separately committed single-record commands does not satisfy this requirement.
- The first bulk implementation operates on committed rows only and excludes Opening Balance. Mixed draft/committed selections are rejected explicitly. Change category updates every allocation of each selected transaction, preserving amounts and memos; the preview lists every affected allocation. The owner accepted this bounded scope separately.
- Schema 7 is approved for durable bulk receipts. The owner subsequently clarified that all formats are pre-release and disposable: migration, upgrades, and backward compatibility are unnecessary until the owner announces a release. Existing experimental-format support may remain, but it is not a completion obligation for this package.

Invalid syntax, invalid patterns, cancellation, and exhausted resources must remain explicit outcomes. Chronological balances and exact whole-transaction filtered totals retain the accepted behavior above. Detailed engineering limits and escape syntax must be specified without silently changing financial interpretations.

## Proposed technology and behavior

Use C++ for domain/storage/import/rule/report logic and Qt Quick/QML for presentation. C remains an option for a justified isolated library, not a requirement for the entire UI. Keep authoritative money operations out of QML numeric arithmetic.

Use a shared application style and platform-appropriate input conventions. Consistent appearance does not promise identical OS window chrome or font rasterization. Virtualize sheet rows/cells and keep editor state outside recycled visual delegates. Background work must not access UI state unsafely.

Before finalizing Qt Quick, prototype a realistically large sheet with selection, keyboard navigation, scrolling, editable row expansion, and simultaneous read-only split expansion. Establish a representative dataset and measurable responsiveness targets.

Use an explicit split indicator/summary rather than showing an ambiguous single category. Offer a preview/count for bulk categorization. A matching payee alone is not a license to replace unrelated memos or committed data silently.

## Acceptance criteria

- Keyboard and pointer interactions work on all three target platforms, including multiselection and context actions.
- Editing one row prevents a second editor without preventing read-only inspection.
- OK validates and persists the appropriate draft or committed operation; Cancel leaves that operation unchanged.
- Source-copy actions preserve the original source and obey money/split invariants.
- Bulk categorization clearly identifies affected rows and copied fields and is recoverable.
- Sorting, filtering, scrolling, and recycled delegates cannot move edits to the wrong transaction.
- Navigation reproduces the visited screen sequence and handles an active edit explicitly.
- Background import/rule/report work leaves interaction responsive under the agreed workload.

- Sidebar resizing/hiding preserves access to the toolbar; Accounts and Reports scroll independently.
- Committed/projected balance toggling is visibly identified without relying solely on colour and is unaffected by ledger filtering.
- Filtered Withdrawal/Deposit totals include every matching ledger row and reconcile exactly; chronological balances do not change with search or sort.
- Search distinguishes editable fields from original QIF evidence and surfaces invalid/ambiguous input and execution failures.
- Clearing search without selecting a result restores the prior view/selection, including an excluded original row. Selecting a result makes that row the visible selection when search clears.
- Match navigation selects and reveals the counterpart and supports return by its match icon or toolbar Back; protected drafts remain clearly uncommitted.
- Leaving a changed edit honors Save / Discard / Stay without silently losing or publishing work.

## Authorized prototype and verification — 2026-10-25

The owner authorized the bounded sheet prototype and specified 100,000 transactions/account as representative, 200,000 as stress, based on fifty-year personal-account use. These are workloads, not application limits. Qt Quick is selected for this experiment only. Same-date fixture-ID ordering, projected ledger balances including drafts, ASCII literal filtering and session-only saves are prototype conventions, not adopted production policies.

All 13 engine/session/model/QML/interaction CTest entries passed in Debug, Release and ASan/UBSan on macOS arm64. Tests cover exact totals and draft exclusion, unchanged chronological balances under sorting/filtering, invalid edits, combined split/total edits, explicit zero and mixed-sign allocations, original totals, stable editor identity through recycling, pointer/keyboard selection, search return, guarded account navigation and window closing. Native scrolling and CPU/memory measurements are recorded in the [prototype report](../doc/sheet-prototype.md). No Windows/Linux, accessibility, production durability, full query-language or complete feature acceptance is claimed.

## Open questions

- Bulk-match definition: raw or normalized payee, account boundaries, draft-only default, split handling, and memo/note overwrite rules. Draft-only with preview is proposed.
- Persistence of unfinished keystrokes versus accepted edits; detailed keyboard behavior, including multiline notes. Save / Discard / Stay on leaving a changed edit is accepted above.
- Same-date chronological ordering; multiple pending-batch projections and persistence of the sidebar balance toggle. Sorting/filtering and the two balance states are settled above.
- Accessibility, keyboard shortcuts, exact column/pane sizing and persistence, narrow-window fallback, large-data performance targets, and supported OS versions.
- Search grammar, regex activation/flags, date ambiguity and range boundaries, QIF-prefix scope, records without source evidence, keyboard/multiselection during search, and restoration if the selected transaction no longer exists.
- Settings contents (beyond the date-format preference and dataset-load path), category/rules editor contents, and report-generator contents. Ask the owner when each is designed.

## References

- [Qt supported platforms](https://doc.qt.io/qt-6/supported-platforms.html)
- [Qt Quick TableView and delegate reuse](https://doc.qt.io/qt-6/qml-qtquick-tableview.html)

## Accepted durable-import experiment — 2026-10-25

The owner accepted the bounded policies in [feature 009](009-durable-import-milestone.md), including its explicit deferrals. Those decisions govern that experiment only; broader open questions in this feature remain unless explicitly settled there. Headless import commands and ledger queries now have a bounded writable UI; one selected record now expands inline with parsed source-field copying. Simultaneous read-only split views are now implemented in the durable UI. Full navigation and broader selection behavior remain incomplete; see the [implementation and verification](../doc/import-ui.md#inline-editing-and-source-copying).

## Durable UI performance follow-up — 2026-10-25

The [bounded native workload report](../doc/import-ui-performance.md) records 100,000/200,000-row measurements through the real durable model and QML view. Read-only queries reuse verified ledger data, presentation maps are created on demand, and large retired views are released on the worker. Atomic saves, full document verification and financial semantics are unchanged. This does not complete the broader acceptance criteria or establish responsiveness on other platforms.


## Durable filter return follow-up — 2026-10-25

The durable UI now implements the accepted search-return behavior for its existing literal filter: retain the pre-filter occurrence/view unless a result is explicitly inspected, then retain and reveal that result when clearing. Clear and Escape honor Save / Discard / Stay. See [scope and verification](../doc/import-ui.md#filter-return-navigation--2026-10-25). This does not implement the proposed query language, multiselection or full navigation shell.

## Account navigation follow-up — 2026-10-26

The durable UI now includes a scrolling account list, inline name entry using the setup currency, independent durable account workspaces and guarded switching/Back/Forward. The [account contract](../doc/accounts.md) records verification and limits. At this 2026-10-26 checkpoint, account switches reset selection, search, and viewport, and counterpart navigation, account lifecycle actions, report panes, and the full sidebar balance layout were unfinished. Counterpart navigation and the point-4 account commands later landed. Report panes remain unfinished. Point 8 later added a session sidebar balance toggle. Persistence, a reports list, and the rest of the layout remain unfinished.


## Durable ledger hardening — 2026-10-26

The current durable ledger now exposes named list/row accessibility semantics with selection, currency and transaction status, and announces operation/error text. Keyboard/focus regressions cover navigation, editor traversal, modal cancellation, saving and search return. Closed guard controls are marked invisible through Qt accessibility; the final focused Cocoa test passed. A native accessibility-tree inspection observed the named rows and fields, but actual spoken VoiceOver output remains unverified. See [hardening evidence and limitations](../doc/hardening.md).

Disabling asynchronous offscreen delegate caching addresses the reproduced Qt reset warning while retaining visible virtualization, reuse and existing viewport/unsaved-edit behavior. Warning-fatal repeated reset regressions and large-workload scrolling measurements are recorded with their exact scope. The point 8 slice below covers a bounded part of the layout. Multiline notes, the exact shortcut, persisted columns, and the rest of this feature stay open.

## Point 8 sheet slice — 2026-10-27

The owner said Go for point 8. The durable UI now does this much of the accepted layout, and no more.

The sidebar lists each account with a separate balance control. The text is Committed or Projected, the currency code, and the amount. The amount is the account's existing committed or projected total. The choice starts on projected when a projection exists, and on committed when it does not. A click switches that account only. It does not change ledger search, sort, or the Projected ledger checkbox. The choice lasts for the session and is cleared when the document is created, opened, unlocked, or left. It is not stored. A restart was not run as its own case. The value is a window property, not a saved setting.

Ledger columns are Date, Payee, Category, Withdrawal, Deposit, and Balance, in the ratios 0.14, 0.24, 0.17, 0.15, 0.14, and 0.16. Money columns are right-aligned. The user cannot drag a column. Widths are not saved. The window minimum is 720 by 560. The default size stays 1440 by 850. The ledger content width is at least 1080, so a narrower window scrolls sideways. The top toolbar keeps the sidebar toggle, Back, and Forward. The sidebar shortcut stays Ctrl+Shift+B. The accepted layout still leaves the exact shortcut unresolved.

Bulk removal, bulk category, rule reapply, and history undo go through Save / Discard / Stay. The visible row is captured only after the user leaves the edit. Bulk and reapply put that row back after preview and Apply. History undo puts it back on Apply. Stay does not arm a restore. A missing row opens at the top. An ordinary one-row save does not capture. The new test clicks bulk category. It does not click history undo or reapply.

Reports are a note: "Reports stay outside this work. Nothing is created here." There is no add button and no report dialog. On 2026-10-28 the owner said the eight-point sequence was the work before reports. Building a report waits for an explicit Go and the open questions in [feature 006](006-reporting-drill-down.md).

This slice does not add a separate note field, a multiline Enter exception, a date-format preference, same-date ordering, persisted column or sidebar sizes, or a new money formula. Feature 005 stays active. Verification is in [status](../doc/status.md#point-8-sheet-and-sidebar-interactions--2026-10-27).
