# File menu exchange

Last updated: 2026-11-10. Owner accepted the design and authorized implementation.
Implementation/verification status is recorded in [status](status.md).

## Accepted behavior

File offers QIF Export…, SpentIt Export… and SpentIt Import… while a file is
open. Existing unsaved-edit decisions apply before these commands. There is
no Backup command. A copy of the file is made after a successful close, while
SpentIt is not running.

QIF Export lists live and archived accounts with checkboxes and independent inclusive date ranges,
initially their oldest/newest active ledger dates. All accounts initially checked. It exports the current unfiltered
ledger, including new and accepted transactions and current edits, excluding removed
or duplicate-suppressed postings. One destination file contains all selected accounts.
Include categories controls ordinary category information. Transfer account references
remain regardless. Include opening balance defaults on: full-range exports use the
stored opening; partial ranges add all current transactions before the start date.
Unchecked exports have no opening. Export requires at least one selected account and
ranges are validated before writing; Cancel remains available. Empty accounts use their opening date.

SpentIt Export independently selects Rules, Reports and Categories, all initially checked. Export requires
at least one group. A versioned UTF-8 JSON exchange format is independent of the
experimental database schema. It contains saved definitions, not transactions,
credentials, history, deleted definitions or unsaved editor drafts. Categories retain
full names and Income flags. Rules retain ordering, enabled state and complete actions;
reports retain account/category selections, chart choice and moving/fixed periods.

SpentIt Import displays vertically stacked Rules, Reports and Categories sections,
omitting empty sections. Items initially selected; each section has All on/off.
Selected rules and reports receive fresh identities, even with matching names; rules
append in source order without applying them. Categories merge by full name using the
existing caseless category matching policy. Income conflicts require a choice between
keeping the existing flag and using the imported flag.

References use portable category names and account names/currency definitions.
Resolve existing unique matches automatically. Missing categories must be selected
from the file for creation or explicitly mapped to existing assignable categories;
missing/ambiguous accounts require explicit mapping. Never silently drop a report
filter, broaden a selection or import a rule with a missing category. Import selected
definitions together atomically. Definition imports preserve transaction Undo and
invalidate stale previews; they are not financial Undo actions.

## Implementation choices

Exports are intentionally plaintext, stated in the export dialogs. They are a
reconstruction path, not a copy of the SpentIt file or a new database migration guarantee.
QIF uses named Bank account sections and exact ungrouped decimal amounts. Its account
B field carries the optional opening, which SpentIt's existing import mapping can
use literally. Dates use unambiguous four-digit years. Currency remains per-account;
QIF import requires the existing explicit currency selection (no conversion).
Owner follow-up: normalize text only in the QIF output. Replace each consecutive
run of C0/C1 controls (including tabs, CR/LF and DEL) or Unicode line/paragraph
separators with one space; preserve other Unicode and all stored/imported values.
An ampersand is written as `&`.
Invalid UTF-8 still refuses. Account-name normalization must not make distinct
accounts match, because QIF transfer references use names. The importer preserves
permitted tabs and embedded CR in source text and strips CRLF record endings;
export must accept that imported text.

Export dialogs show progress and errors inside the modal; Export is disabled while
busy. The destination picker uses Save.

JSON version 1 identifies `SpentIt` and uses distinct definitions and reference lists.
Readers reject unsupported versions, malformed fields and bounded-size violations.
The file picker is followed by atomic export publication; never overwrite the active
dataset, its sidecars, symlinks or a database file. Cancel/failure must not truncate
an existing destination. Import mapping is a preview, revalidated before one durable
save. Full import requests are not retained as permanent historical archives.

## Verification requirements

Synthetic round trips cover current values, splits, transfers, signed exact money,
period boundaries/openings, optional categories, all definition kinds, duplicate
names, missing dependencies, Income conflicts, invalid JSON/version, stale preview,
atomic failure/cancellation and retry. Verify menu/dialog guards and selection states.
Record actual platforms and limitations. Manual changes are deferred in
[the backlog](manual-update-backlog.md).

## JSON version 1 wire format

File suffix: `.spentit.json`. All arrays below are present, including empty arrays.
Unknown object fields, duplicate JSON keys (including escaped aliases), invalid
UTF-8/unpaired Unicode surrogates, unsupported versions and nesting beyond 32 levels
are rejected. Maximum file size is 16 MiB; at most 4,096 rules/categories/category
references, 1,000 reports, 10,000 account references and 32 periods per report.
Existing domain limits also apply to the resulting combined catalogs.

```json
{
  "format": "SpentIt",
  "version": 1,
  "rules": [{
    "name": "Groceries", "enabled": true,
    "scope": "transaction", "input": "payee", "expression": "Market",
    "payee": null, "memo": null, "category": "food"
  }],
  "reports": [{
    "name": "Spending", "graph": "bar",
    "accounts": ["checking"], "categories": ["food", ""],
    "periods": [{"preset": "this_year", "start": "", "end": ""}]
  }],
  "categories": [{"key": "food", "name": "Food", "income": false}],
  "categoryReferences": [{"key": "food", "name": "Food"}],
  "accountReferences": [{
    "key": "checking", "name": "Checking", "currency": "six-2026-09-17:CAD"
  }]
}
```

The currency string is the engine's retained currency-definition ID (see
`amount_currency_definitions`); the example uses the current CAD definition.
Keys are local to the exchange file. They are never reused as destination identities.
The empty report category string means Uncategorized. Reference lists describe
identities needed by exported definitions even when Categories is unchecked; they
do not themselves authorize category creation. Category entries describe saved,
nondeleted definitions; imported new categories are active. An existing archived
category must be unarchived or left unselected and references mapped elsewhere.

Rule scopes are `transaction`/`split`; inputs `payee`/`memo`/`category`. Payee and memo
are null for absent actions, or exact template strings. Category is mandatory.
Rule matching/template semantics remain those of the current engine; a future
incompatible interpretation requires an exchange-version decision, not silent reuse.
Report graph and period strings use the saved report contract (`none`, `bar`, `pie`,
`grouped_bar`; Custom dates are ISO). Moving presets remain moving.

The importer validates the complete file structure before showing choices and the
selected definitions before saving. Only dependencies
of selected definitions must resolve. Account mappings require the same exact currency
definition; no implicit conversion or account creation occurs. Many-to-one mappings
that would produce duplicate filters refuse instead of silently rewriting filters.
New categories and explicit Income updates, appended rules and appended reports save
in one database transaction. Exact retries use a compact completion digest; failures
before commit restore all catalogs, and uncertain acknowledgements are checked before
success. No database schema bump is needed. The release rules database remains version 1.

QIF is limited to 64 MiB per export; select smaller periods when necessary. The export
uses `MM/DD/YYYY`, independent of dataset display settings, and includes category
catalog entries only for categories used in the selected transactions. Split amounts
and memos remain even without categories. Transfer fields use `[Account name]`.
The account header's optional `B` amount is not an extra transaction: importing it
uses the existing explicit zero/reported-opening decision. QIF does not carry a
portable currency definition; choose the appropriate currency during its import.
