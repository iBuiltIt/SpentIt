# Search, selection, and bulk ledger changes

Last updated: 2026-10-26

This is the bounded durable QIF experiment; search and bulk actions operate within the selected [account workspace](accounts.md). Requirements and owner decisions are in [feature 005](../features/005-transaction-sheet-navigation.md). The owner confirmed that experimental formats are disposable until an explicitly announced release; migration and backward-compatibility work are not required during this phase. This work introduces no migration.

## Search contract

- Space-separated terms are case-insensitive Unicode literals combined with AND. A term can match a different field from another term. Double quotes form a phrase within one field. Quoting an entire operator-looking term makes it literal, for example `"regex:word"`.
- `regex:"pattern"` explicitly uses the existing bounded PCRE2 adapter. Inline pattern flags can override case matching. In quoted text, `\"` and `\\` escape a quote and a backslash; other backslashes are preserved for regex syntax. There is no OR/NOT grammar or implicit regex interpretation.
- `period:2026-05-01..2026-06-30` includes both endpoint dates. Full dates accept `YYYYMMDD`, `YYYY-MM-DD`, and `YYYY/MM/DD`; a four-digit year means that whole year. English full month names and three-letter abbreviations use the local current year. `period:may-june` means May 1 through June 30 of that year. Reversed/cross-year month ranges, impossible dates, ambiguous numeric dates, and multiple period clauses are rejected. An ambiguous-date preference has not been added.
- Ordinary matching includes the effective date, payee, transaction memo, allocation category display names and memos, exact formatted total/allocation amounts, chronological balance, and currency code. The bounded record model does not yet have a separate note field.
- A leading, separate `QIF:` token switches text matching to the complete original record slice, including field codes and line breaks. The period filter still selects the effective ledger date. Opening Balance has no QIF evidence and is excluded. Source files are read on the worker and parsed once per batch per query; stored bytes remain unchanged.
- A split-field match includes the complete transaction in filtered totals. Searching never recalculates chronological running balances; sorting reorders their display without changing their meaning.
- Invalid syntax, regex execution limits, and cancellation are errors, not successful empty results. An unsuccessful query retains the previously successful filter, rows, totals, and selection and keeps the document open. Query publication happens after successful evaluation.

Engineering rejection boundaries: 4,096 query bytes, 16 text terms, the existing per-pattern PCRE2 limits (including a 64 KiB subject), 256 MiB of subject bytes visited, four million field comparisons, twenty million cancellation/callout checks, and 256 MiB of retained QIF bytes per source query. An oversized QIF record subject is rejected, never truncated. Empty queries use the ordinary ledger path. Work runs on the existing serial worker; these limits do not establish a general responsiveness guarantee.

The reusable PCRE2 adapter now has its own `spentit_regex` target. Import-workflow builds use it for search even when rules persistence is disabled. `SpentIt::engine` remains independent of PCRE2, Qt, and concrete storage. The ledger application target has no SQL or Qt dependency.

## Selection and interactions

Click selects a row; Shift selects a displayed range; Cmd/Ctrl toggles individual rows. Arrow/Home/End keys move selection, and Enter opens the focused record. Double-click still opens the inline editor. Identity-based selection survives sorting and delegate recycling. Searches remember pre-search multiselection; clearing without an explicit result choice restores it, while choosing results retains those choices. Removed identities drop from selection. Dataset changes clear it. Selection is bounded at 4,096 rows with an explicit error, not silent truncation.

Delete and Change category are available from ledger controls and the context menu; Delete also has a keyboard action. Opening Balance and draft/mixed selections are refused by the repository, regardless of UI selection. Dirty-editor navigation retains Save / Discard / Stay guards.

## Atomic bulk operations

Schema 7 introduced immutable `bulk_operations` and `bulk_members`; current schema 10 retains them with account-scoped behavior. The group receipt references the existing financial operation history; member revisions, financial history, dependency stamps, and group membership are written inside one SQLCipher transaction. A group is never implemented by invoking separately committing single-row commands.

A category command explicitly assigns the chosen category, including Uncategorized, to every allocation in every selected committed row. Amounts, split structure, memos, dates, payees, source associations, and original evidence remain unchanged. Category ownership becomes manual and its previous rule mark is cleared. Other field marks remain intact. Removal changes active balance effects while preserving evidence and history. The preview lists every affected allocation and exact before/change/after account totals. Previews are bounded at 4,096 rows, 16,384 allocations, and 16 MiB of text.

Apply recomputes the canonical proposal and checks member revision/dependency tokens, the financial sequence, and catalog revision before writing, then rechecks under the transaction. Stable operation identities and complete group receipts distinguish retries from conflicting reuse. Lost acknowledgements are resolved by durable outcome lookup; unknown outcomes close the session for explicit reopen. A known saved operation whose refresh fails is reported as durably saved, with instructions to inspect history rather than reapply.

Group Undo is an atomic operation over all original members. Any later member change or recorded dependency change blocks the complete group reversal. Restored rule provenance names its retained ruleset snapshot. Group membership is exposed from committed history after reopening. Individual committed-history semantics remain available; the group does not erase per-record history.

Existing experimental-format support remains where already implemented. It is not a compatibility commitment, and no upgrade or migration infrastructure was added.

## Verification

Debug, Release and ASan/UBSan covered the bounded search/bulk paths, including complete bulk fault sweeps. The initial sanitizer run found a test-only temporary-lifetime bug; the corrected case passed. The known Qt delegate-reset warning remains despite passing behavioral assertions. Full results, selected-suite scope, workload measurements and limitations are recorded in [project status](status.md). All fixtures are synthetic. Linux, Windows, physical power loss, and instrumentation of the installed PCRE2 library remain outside the verified scope.
