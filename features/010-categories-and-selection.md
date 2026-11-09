# 010 — Categories and selection

Current follow-up (owner Go, 2026-11-03): the
[transaction-origin contract](../doc/transaction-origin-handoff.md)
supersedes origin-specific category/transfer restrictions below for saved ledger
entries. Manual and imported postings share transfer assignment, explicit Link,
accepted editing/history and independent-counterpart preservation. Formats are
50 with rules / 49 without. Rule matching policy is unchanged. See
[verification](../doc/status.md).

The [opening/ledger cleanup and interrupted-import rollback](../doc/ui-import-cleanup.md)
is implemented under the owner's 2026-10-31 Go on experimental formats **40/39**.
It includes import routing, Review repair/Commit, account-context Discard and
atomic cleanup of incomplete preparation. Completed uncommitted imports remain
durable. See [current verification and limits](../doc/status.md); earlier Resume,
source-retention and toolbar descriptions below are historical.

Status: **active**  
Last updated: 2026-11-09  
Implementation: independent assignable categories, one optional colon, case-insensitive identity matching, and the creation-time starter catalog are implemented on experimental schemas 30/29. This supersedes the earlier explicit group/leaf model below. See [current category behavior](../doc/categories.md) and [verification](../doc/status.md). Reporting stays deferred.

The 2026-10-26 follow-up adds committed bulk assignment to every selected allocation, with exact preview, one atomic operation, and group Undo. See the [search/ledger contract](../doc/search-ledger.md). Experimental-format migration and compatibility obligations start only at an owner-announced release.

## Income grouping — owner Go 2026-11-06

Accepted and implemented: Categories has an **Income** checkbox column before
Name. New categories (including starter/import-created categories) default off;
only an explicit checkbox edit marks income. The flag is saved with category
history, independent of names, transaction signs and category assignments.
Renaming and archiving preserve it; unchecking reverses the classification.

Responsiveness correction (2026-11-06): the Income checkbox uses one immediate
validated save, with a partial indicator and **Saving…** while pending. Its saved
check and sorted position arrive together. Income-only changes reuse unchanged
ledger and Review facts; mutation revisions and Undo barriers still advance.
Import completion, balances, transaction contents and report results are unchanged.
Other category edits retain their existing full refresh behavior.

Default workspace and all category pickers use Expenses A–Z, Income A–Z, then
Accounts A–Z. Uncategorized/Choose a category remains first where offered.
Workspace Z–A and Order added apply within each ordinary group; accounts stay
A–Z. Visit additions still start at the bottom, but changing their Income flag
removes that exception immediately. Selection follows the moved category.

Reports keeps its separate account checklist. Its category checklist starts with
Uncategorized, then Expenses and Income, each with a tri-state group checkbox.
A checked group clears all its categories; unchecked or partial selects all.
Group actions include categories hidden by search, including archived report
choices, and do not affect Uncategorized or the other group. Empty groups have
no heading. Group actions use each category's own flag, without colon expansion;
individual category selection retains the existing colon-prefix behavior.

Saved reports retain explicit category IDs: changing a flag or adding a category
does not change their selection. Amounts, net-spending signs, report result order,
refund treatment and transfer exclusion are unchanged. The flag does not classify
individual transactions by sign. This supersedes earlier neutral-only category
presentation and alphabetic-only selector wording below.

Storage formats are **58 with rules / 57 without**. Earlier experimental formats
are rejected unchanged without migration. See [status](../doc/status.md) for
verification and remaining limits.

## Category selector widths and visit sorting — owner Go 2026-11-02

Category selectors measure current category/account labels with the UI font and
allow padding, indicators and scrollbars. Fields fit available layout space;
popups may be wider than their field and are bounded to the window. This includes
the transaction inline field/completion, ledger category menu and rule selector.
The existing large category-picker dialog retains its bounded layout.

Category selectors always sort complete names A–Z using case-insensitive locale
comparison and identity as a tie-break, with Uncategorized/Choose a category first.
Owner follow-up: accounts form a separate trailing section in every category
list and selector, always A–Z among themselves, regardless of the ordinary
category sort. Newly created categories in the Categories workspace remain below
even that account section during the current visit.

The Categories workspace opens A–Z on each visit, including Back/Forward. Newly
created rows remain at the bottom for that visit. Explicit A–Z, Z–A or Order added
re-sorts immediately; these choices last only for the current visit. Selection,
search, inline creation and validation guards remain. This supersedes persistent
sort preferences and immediate insertion after save in the 2026-10-28 section.
See [status](../doc/status.md) for verification.

## Category view controls — owner Go 2026-11-01

Accepted: show an inline clear button when category search contains text; clearing
restores the full filtered list and focuses search. Show archived includes only
archived entries, with deleted entries always hidden. Retain internal deleted
definition history and receipts; physical history removal is outside this change.
Place New category before the three sort controls with a small gap. A thin
horizontal divider separates the toolbar from the first category row.
Follow-up Go extends the same inline clear control to every search field, removing
separate Clear buttons while preserving existing search application and navigation
guards. The ledger also shows × when an applied filter remains despite empty input.
See [status](../doc/status.md) for verification.

## Selected payee rename and restoration — owner Go 2026-11-02

The ledger context menu adds Rename payee and confirmed Restore imported values
for single/multiple selections. Each applies atomically across new and committed
rows, with one group Undo and retained selection/viewport. See the
[accepted contract](../doc/ledger-editing.md#selected-rename-payee-and-restore--owner-go-2026-11-02)
for original-value validation, transfer restrictions and the session Undo scope,
and [status](../doc/status.md) for verification. This does not add a rule override
mode; rules still match imported text and operate only on Uncategorized items.

## Selected Set memo — owner Go 2026-11-09

The ledger context menu adds **Set memo…** beside Rename payee. One dialog sets
the transaction memo for a single or multiple selection. Blank memos are filled
unless **Overwrite N existing memos** is checked. A blank field with that box
checked clears existing memos. Split memos stay unchanged, including the memo
on a transaction’s only split. The command is one
atomic group Undo and uses the Rename payee selection guards. A memo it writes
is manual, so a later rule leaves it alone. See the
[contract](../doc/ledger-editing.md#selected-set-memo--owner-go-2026-11-09)
and [status](../doc/status.md). Manual text is MAN-019.

## Direct ledger category menu — owner Go 2026-11-01

Accepted and implemented: right-click **Change category ▸** opens a bounded,
scrollable submenu. Choosing an ordinary category, Uncategorized or an account
applies immediately to the selected transactions, without category selection and
confirmation dialogs. New and committed rows use the existing atomic assignment
command together; this replaces the committed-only bulk preview route for this
UI. Existing validation, transfer behavior and durable group Undo remain.

A split transaction asks which split to change; multiple selection containing
split transactions requires selecting those transactions individually. Current
account destinations are disabled. The ledger retains its selected identities,
vertical anchor/offset and horizontal position through success or failure, with a
clamped row fallback if filtering removes the anchor. Undo category change uses
the same viewport restoration. The inspected editor reloads after assignment.
Ordinary category edits also accept new rows awaiting duplicate review, retaining
their unresolved decision instead of silently including them. Transfers keep their
existing eligibility checks. No storage format change. See [status](../doc/status.md)
for actual verification and limits. Earlier dialog/preview descriptions below
are superseded for the ledger category menu.

## Right-click transfer assignment — owner Go 2026-10-31

The ledger's right-click **Change category** dialog accepts an account destination
for all selected transactions. Each selected allocation creates or retargets its
own transfer through the existing engine command, preserving its amount and date.
The whole assignment saves atomically with one durable Undo. The owner confirmed
that after Review finds no counterpart, assignment should create the destination
entries directly; this command does not perform a new candidate search.

The command uses ledger selection independently of the inspected record. A single
split transaction requires an explicit allocation choice; ambiguous split rows in
a multiple selection must be assigned individually. The current account is
refused. Existing transfer eligibility, currency and committed-row restrictions
remain authoritative: an invalid member rejects the whole batch. Ordinary
categories retain bulk preview/apply. Cancel writes nothing; cancellation before
the final durable gate rolls back the entire assignment.

**Undo transfer assignment** is available in the ledger and through the existing
Undo shortcut. The group and its link snapshots use the durable Review history;
Undo survives reopen and backup restoration. A later dataset mutation blocks Undo
rather than overwriting newer work. Undo removes newly generated drafts and
restores prior links without changing the source postings. Ledger category labels
and literal filtering derive live account destinations from the links, reverting
to ordinary classifications when undone. See [verification](../doc/status.md).

## Changing a transfer back to a category — owner Go 2026-10-31

Choosing Uncategorized or an ordinary category in the right-click dialog now
ends selected transfer links and assigns the chosen classification atomically.
Purely generated uncommitted counterparts are removed. Independently imported
or committed counterparts remain, with the link ended. Origin amounts/dates stay.
The same operation supports one or multiple selected transactions, including
mixed selections of linked and ordinary rows. A split allocation must still be
chosen explicitly. Existing generated-side and relationship restrictions remain;
an invalid member rejects the whole operation with an error.

One Undo restores the links, removed generated drafts, original categories,
field ownership and rule provenance. The operation and its Undo must survive a
full audit, restart and backup restoration. The earlier ordinary bulk preview
remains for selections without transfers. See [verification](../doc/status.md).

## Account names and aliases — accepted 2026-10-29

Accounts participate in every category-selection list as typed transfer destinations. They also appear in the category editor, labelled Account, with aliases visible/searchable; their category-editor rename/archive/delete controls are disabled. Account management owns those operations. Ordinary categories, account display names and confirmed aliases share a case-insensitive namespace. New conflicting names are refused explicitly. A source catalog definition naming an existing account is retained as evidence but creates no ordinary category; the import catalog review displays an informational warning.

An account retains its creation name and every explicit rename as aliases. A named QIF source account explicitly mapped to that identity also supplies an alias. Display always uses the creation name or latest explicit rename; imports never override it. Names are derived from retained creation records, rename history and confirmed source mappings, including mappings to surviving accounts after Abort. Retired accounts supply no active aliases. No filesystem-local alias state or new schema is required.

A bracketed or unbracketed allocation naming an account/alias is transfer intent. Confirmed grouped imports resolve reciprocal eligible allocations using the same exact-money/date/currency and one-to-one count rules. The transaction's total may include ordinary fee splits; matching operates on the transfer allocation. Linking preserves source bytes and every occurrence, and each account still commits explicitly. Existing drafts can use Resolve mapped transfers. Ordinary category assignment cannot bypass transfer commands by writing an account name as text. Account references established in transfer history use stable identities and survive renames.

Existing category/account conflicts remain inspectable and require renaming the ordinary category before automatic resolution. Ambiguous account aliases are reported as blockers, never chosen arbitrarily; new mappings that introduce such ambiguity are refused. Historical committed ordinary classifications are not retroactively changed solely by adding this policy. Imported catalog duplicates are not errors when they identify the same account.

Rules preserve recognized account allocations even with Replace manual fields. Rules cannot create account transfers. The right-click category dialog supports atomic transfer assignment for the selected transactions as described above. This is an explicit capability boundary, not storage of account names as ordinary categories. Single-allocation account selection continues to invoke the existing atomic transfer command.

This supersedes earlier statements below that an ordinary category may share an account name or that unbracketed account names always remain ordinary assignments. Exact verification is recorded in [status](../doc/status.md#account-aliases-and-transfer-categories--2026-10-29).

## Compact category editor — accepted and implemented 2026-10-28

The owner approved a compact, left-aligned table filling the available page height, with a draggable scrollbar and the guidance “Use a colon for a subcategory, for example Auto: Gas makes Gas a sub-category of Auto”. This describes the naming convention; it does not restore parent objects or dependent lifecycle behavior.

New category appends a temporary blank row at the bottom, brings it into view even under a search filter, and focuses the name. Enter or focus loss saves; Escape cancels. An untouched empty creation row is discarded. Rename is inline with the same keyboard/focus behavior; Escape retains the saved name. Invalid or duplicate names retain the edit and show an error only in the permanent bottom status line, with Details available. Category operation failures use that same line; the category table has no separate error area. Navigation waits for a successful save; invalid edits remain on the page.

Rename, Archive/Unarchive, and Delete unused category are right-click row actions. The separate form, Review category save / Cancel edit buttons, entire review dialog, and visible definition-history section are removed. Menu actions save directly through the existing validated atomic commands. Retained history, reference-based deletion restrictions, and durable outcome handling remain in storage. See [verification](../doc/status.md).

## User-controlled category sorting — accepted and implemented 2026-10-28

The owner explicitly rejected automatically choosing Order added as a new default. Preserve the existing catalog order until the user chooses a sort. The editor offers **A–Z ↑**, **Z–A ↓**, and **Order added** (oldest first). The explicit choice is retained in application settings across navigation, dataset switches and restarts; creation and rename do not change it.

Alphabetical order uses case-insensitive locale comparison of the complete category name, with identity as a deterministic tie-break. Order added uses the immutable creation catalog revision, not the latest edit revision or an invented timestamp. This is read from existing history without changing the dataset format. New editing rows still start at the bottom. On successful save, the saved category is selected and brought into view in the chosen order. Changing sort retains selection and brings it into view. A saved category excluded by the current search is temporarily revealed until the user edits the search; archive/deleted visibility remains controlled separately.

## Exact starter categories — accepted 2026-10-28

**Approved by the owner, with the three Income entries added below.** This translates the original supplied list below into the newly agreed independent-category model. Each line is one selectable, assignable category. No parent-only categories are created. The creation checkbox defaults to checked; unchecked means an empty catalog. Names remain editable after creation.

```text
Housing: Rent & Mortgage
Housing: Home Maintenance
Housing: Home Improvement
Housing: Furniture & Appliances
Utilities: Electricity, Gas & Heating
Utilities: Water, Sewer & Garbage
Utilities: Internet
Utilities: Phones
Utilities: TV & Streaming
Auto: Gas & Fuel
Auto: Maintenance & Repair
Auto: Insurance
Auto: Parking & Tolls
Auto: Payment & Other
Transportation: Public Transit
Transportation: Rideshare & Taxi
Food: Groceries
Food: Restaurants & Dining
Food: Coffee & Fast Food
Amazon
Shopping: Clothing
Shopping: Electronics & General Shopping
Medical: Doctor & Hospital
Medical: Pharmacy
Health & Wellness: Gym & Fitness
Health & Wellness: Personal Care
Entertainment & Hobbies: Entertainment
Entertainment & Hobbies: Hobbies
Travel: Flights
Travel: Hotels & Lodging
Travel: Other
Family & Education: Child Expenses & Childcare
Family & Education: Education
Other & Miscellaneous: Gifts & Donations
Other & Miscellaneous: Pets
Other & Miscellaneous: Insurance
Other & Miscellaneous: Fees, Interest & Bank Charges
Other & Miscellaneous: Other
Income: Salaries & Wages
Income: Pension
Income: Investment Income
```

## Independent categories — accepted 2026-10-28

The owner superseded the explicit parent/group model below and authorized implementation. Every category is independently assignable. A name may have one colon separator, with display spacing `Stem: Name`; a second colon is rejected. Creating `Auto: Gas` creates only that category. Creating `Auto` separately makes it independently assignable. Renaming, archiving, or deleting one does not change others sharing its stem. Stable identities and retained assignment history remain required.

Identity lookup and uniqueness are case-insensitive; preserve the created capitalization for display (until an explicit rename). Typing `auto: gas` selects an existing `Auto: Gas`. Stem matching includes the exact category plus names beginning with that stem and a colon boundary; `Auto` includes `Auto: Gas`, but excludes `Automobile Club`.

For future reports, selecting `Auto` can select all matching categories, after which the user may deselect individual choices. Direct-category values and rollups must be distinguishable and never double-count allocations. Exact report presentation is deferred. The earlier non-assignable parent requirements, group lifecycle, and hierarchy questions below are historical and superseded by this decision.

## Purpose and roadmap position

The owner discussed categories ahead of their implementation milestone. Preserve these decisions for category editing, selection, rules, and reporting work; implementation is now authorized under the coordinated roadmap, subject to the accepted lifecycle policy below. Read alongside [accounts and splits](002-accounts-money-splits.md), [rules](004-rules-and-explanations.md), [transaction navigation](005-transaction-sheet-navigation.md), [reporting](006-reporting-drill-down.md), and [transfers](008-transfer-matching-and-lifecycle.md).

The organizing principle is useful distinctions that the owner can reliably make from statements at a sustainable review effort. Review may happen only every three months. Do not require receipt reconstruction or fine-grained purchase classification merely to satisfy a theoretically uniform taxonomy.

## Earlier requirements and decisions — hierarchy superseded 2026-10-28

### Shallow hierarchy and familiar names

- One grouping level with assignable leaves: parent → leaf, with no deeper nesting. Parents cannot receive transaction allocations.
- The hierarchy has reporting meaning: a parent groups its descendant allocations for filtering, totals, and drill-down. A parent subtotal and its children represent the same allocations and must not be added together again.
- Category names are user classifications, not triggers for financial behavior. Renaming Travel to Holidays must not change financial treatment. An ordinary category named Transfers does not establish an account relationship.
- Preserve the owner's Auto terminology. Typing Auto must reveal its choices, including Parking, without requiring the owner to remember or type Transportation first.
- Utilities is the intended home for phones, TV, streaming, cable, electricity, and similar services. Do not relocate TV/Streaming to Entertainment merely for taxonomic consistency.
- Uncategorized is special: absence of classification, distinct from a deliberate Other/Miscellaneous assignment. It must remain visible in reporting; exact storage and UI representation remain to be designed.

### Amazon is intentional

Keep Amazon as a category in the owner's proposed set. Amazon statement entries do not reliably reveal the purchased goods. The owner makes frequent purchases and does not intend to reconstruct roughly 90 purchases during quarterly review to distinguish washing powder from darts. Amazon represents useful miscellaneous household/general spending, and its total is meaningful on its own.

Do not replace it with a merchant-only filter or require item-level splits. Category and payee may intentionally overlap. Automatic assignment by a rule is a useful proposed convenience, subject to the existing rule design; this discussion does not implement or settle rule precedence.

### Income and expense

Categories do not require an expense/income designation. A category can receive allocations of either sign: an Amazon purchase and its refund can share Amazon. Salary can be a category without another mandatory type setting.

Direction alone does not identify income or spending. Transfers, opening balances, and refunds need explicit reporting treatment. Keep transfer/opening-balance identity separate from category names. Refund/net-spending and income-report policy remain open in feature 006; neutral categories do not settle those policies. Optional reporting groups were discussed as a possible later convenience, not an accepted requirement to implement now.

### Accounts in the category selector

Accounts appear as distinguishable transfer destinations in the same selector. From the user's perspective they can be selected where a category is selected; internally selecting an account establishes a typed account reference, not an ordinary category-name match.

For example, choosing the actual CIBC: Visa account on a Savings payment initiates a transfer. Its account identity survives renaming; a same-named ordinary category does not trigger a transfer. The colon in the account's name does not by itself establish a category hierarchy.

The durable selector lists each other account with its name, currency, and identity. Choosing it on an unlinked allocation creates a generated destination draft. Choosing it on an existing transfer origin replaces the destination under the [2026-10-27 replacement policy](008-transfer-matching-and-lifecycle.md#accepted-transfer-management-decisions--2026-10-27). It does not auto-link an existing independent row. Candidates remain an explicit confirmation step. A category whose name matches an account stays an ordinary category assignment.

Use the accepted [transfer lifecycle](008-transfer-matching-and-lifecycle.md): preserve evidence, avoid counting payment principal as spending again, and require explicit commit per account. Committing Savings protects a generated Visa draft but does not commit Visa. Automatic matching confidence and cross-currency links remain deferred there. Allocation-level links, including a fee split beside a transfer allocation, are accepted.

### QIF-compatible category names

Category names must respect the supported QIF format. The original proposal prohibited a literal slash because it has structural meaning in QIF category/class notation. That restriction is superseded by the accepted complete-catalog behavior below. Validate at category creation/rename with an explanation; do not defer errors until export or silently strip/replace characters. Preserve immutable imported source evidence.

The original restricted parser rejected category text containing `[]/|`; the current parser accepts catalog-declared literal slashes and keeps conventional class notation outside that context. Before implementation, verify the full naming contract against the supported QIF profile, including reserved account-transfer notation, hierarchy delimiters, control characters, and import/export behavior. Do not interpret the slash-separated labels in the discussion list below as approved literal names.

## Owner's proposed category set — retained for discussion

This preserves the supplied grouping and labels, including alternatives separated by slashes. It is a discussion inventory, not a finalized seed catalog or directly importable QIF. The owner rejected suggestions to remove Amazon, deepen Auto under Transportation, relocate streaming, distribute general insurance across purposes, separate Gifts/Donations or Fees/Interest, or rename Coffee/Fast Food. Do not treat those suggestions as accepted changes.

| Discussion heading | Supplied categories |
|---|---|
| Housing | Rent / Mortgage; Home Maintenance; Home Improvement; Furniture & Appliances |
| Utilities | Electricity / Gas / Heating; Water / Sewer / Garbage; Internet; Phone / Mobile; TV / Streaming |
| Transportation | Auto: Gas / Fuel; Auto: Maintenance / Repair; Auto: Insurance; Auto: Parking & Tolls; Auto: Payment / Other; Public Transit; Rideshare / Taxi |
| Food | Groceries; Restaurants / Dining; Coffee / Fast Food |
| Shopping | Amazon; Clothing; Electronics / General Shopping |
| Health & Wellness | Medical: Doctor / Hospital; Medical: Pharmacy; Gym / Fitness; Personal Care (hair, beauty, toiletries, etc.) |
| Entertainment & Hobbies | Entertainment (movies, events, streaming beyond the TV category if wanted, games, etc.); Hobbies |
| Travel | Travel: Flights; Travel: Hotels / Lodging; Travel: Other (car rental, activities, etc.) |
| Family & Education | Child Expenses / Childcare; Education |
| Other / Miscellaneous | Gifts & Donations; Pets; Insurance (general — health, life, home, etc.; or split further if desired); Fees / Interest / Bank Charges; Miscellaneous / Other |

The owner's later clarification prefers names such as Utilities: Phones and Utilities: TV & Streaming. Travel is an explicit grouping parent with no direct allocations. Auto must be discoverable directly. The original heading-plus-prefix arrangement is not permission to introduce three levels.

## Earlier implementation direction and deferred options

- Store stable category identities and explicit parent relationships; render a path such as Travel: Flights without making mutable text the identity.
- Match picker searches against both parent and leaf names: Auto reveals its leaves; Parking finds Auto: Parking & Tolls. Show enough context to distinguish identically named leaves, and make parents non-assignable.
- Starter categories remain optional and editable; this bounded implementation starts with an empty catalog so unresolved starter names do not become financial assumptions.
- Optional trip/project tags could collect meals, taxis, shopping, and flights for one holiday across categories. This was discussed but not adopted; do not add tags or require them as part of category implementation.

## Earlier open questions — hierarchy and starter names resolved above

- Reconcile the original headings with the accepted depth limit. Is Auto a top-level parent alongside Transportation? Similarly, how should Medical relate to Health & Wellness? Do not silently add an extra hierarchy level or a separate presentation-section system. The suggestion to make Auto top-level was not explicitly resolved.
- Finalize QIF-safe leaf spellings without changing their intended meaning. Standalone assignable categories are accepted; their presentation beside groups is an implementation choice.
- Implement the accepted lifecycle policy below with stable identities, retained evidence/history, and explicit mapping of existing free text. No automatic document migration is authorized.
- Define parent expansion/filter behavior and Uncategorized representation, while preserving exact allocation-level reconciliation and currency separation.
- Settle refund/net-spending and income reporting policy separately; do not infer it from a name or amount sign alone.
- Mortgage and Auto Payment labels do not settle loan-principal, interest, or cash-flow reporting policy.

## Acceptance criteria — future verification obligations

- Parent nodes cannot receive allocations; the supported category model rejects deeper nesting.
- Parent totals reconcile exactly to their included leaf allocations and drill-down without double counting or combining currencies.
- Typing Auto finds its available categories, including Parking; actual picker details remain to be designed.
- Amazon remains an ordinary usable category without mandatory item reconstruction.
- Both signs are accepted without mandatory income/expense typing; Uncategorized remains distinct from Other.
- Account selections use account identity and initiate the specified transfer lifecycle; category names alone never do so.
- Invalid QIF category names are explained and rejected without silent modification or loss of imported evidence.

## Verification and implementation limits

The original discussion was documentation-only. Configuration-specific verification is recorded in [project status](../doc/status.md). The durable selector's account entries are part of the point-5 transfer commands. Category-report reconciliation remains a future acceptance obligation.

Documentation validation: all 93 relative link file targets across the eight changed documents resolved, and `git diff --check` passed. Runtime tests were not run because this change only records requirements and discussion.

## Accepted lifecycle policy for coordinated implementation — 2026-10-25

Use stable category identities with optional standalone assignable leaves and one non-assignable parent level. Siblings have unique names; top-level leaves and groups share a namespace, while different parents may have equally named leaves. Renaming or moving a category preserves its identity and allocation assignments. Current ledger/report views for any financial period use the current names and hierarchy, while original import evidence and recorded change history retain their earlier labels/definitions.

For example, moving Parking & Tolls from Auto to Travel would move last year's parking allocations into Travel in a newly generated current report. The owner accepted this current-taxonomy behavior. Refund/income and historical-as-of reporting remain separate policies.

Archive removes a category from new assignment choices but keeps existing allocations, balances, history and report visibility. Delete only never-referenced empty entries, retaining definition audit; otherwise use archive. Do not merge or reclassify allocations implicitly. Existing free-text classifications remain visibly unmapped until deliberate mapping; do not silently convert them to Uncategorized. A committed allocation reassignment uses the accepted committed-correction history rather than catalog renaming.

The starter catalog remains optional/proposed so its unresolved headings and final spellings do not prevent the model and editor from being implemented. This accepted lifecycle policy does not settle transfer behavior or reporting sign/refund policy.

## Complete imported catalogs — accepted 2026-10-28

The owner approved importing the full source category catalog when porting an application's data, normally into a dataset created without starter categories. Similar names must not be merged. Existing case-insensitive category identity matching remains; all unused source definitions are included. Imported categories remain draft state until the first account explicitly commits; that commit atomically publishes the complete catalog. Whole-import Abort rolls back every staged definition, including definitions used by draft transactions. Preserved encrypted evidence is not an active category.

This supersedes the earlier blanket slash prohibition: literal `/` is allowed in category names. QIF catalog context disambiguates literal names from class suffixes where possible; ambiguous references are blocked explicitly. Income/expense markers, descriptions, budgets and tax metadata remain source evidence, not new category types or report policy. See [QIF profile](009-qif-profile.md#category-catalog-and-literal-slash-names--accepted-2026-10-28) and [staging/publication contract](../doc/qif-account-imports.md#staged-category-catalog--2026-10-28). Category reports remain unimplemented.
