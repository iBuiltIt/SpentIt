# Category catalog and assignments

The [Income grouping contract](../features/010-categories-and-selection.md#income-grouping--owner-go-2026-11-06) now adds a saved Income checkbox and grouped sorting, plus Reports group selection. It supersedes the alphabetical-only ordering below.

The [2026-11-02 selector and sorting contract](../features/010-categories-and-selection.md#category-selector-widths-and-visit-sorting--owner-go-2026-11-02)
supersedes persistent sorting: Categories starts A–Z on every visit, with additions
at the bottom until departure or an explicit sort. Transaction/rule selectors
always sort alphabetically and size from current labels within available space.
Accounts follow ordinary categories as a separate section, always A–Z even when
the workspace uses Z–A or Order added. Visit-only new categories stay below them.


Last updated: 2026-11-01

State: independent assignable categories are implemented in experimental schema 40 with rules and 39 without. Earlier experimental schemas, including 24/23 and 22/21, are rejected without migration. The owner-approved [category model and exact starter list](../features/010-categories-and-selection.md#independent-categories--accepted-2026-10-28) supersede the earlier group/leaf hierarchy. Verification is recorded in [project status](status.md).

## Identity, organization and history

Each category has a stable identity and a complete name. `Auto` and `Auto: Gas` are separately assignable; neither creates or owns the other. Names permit one colon separator and display it as `: `, preserving capitalization. A second colon, empty segment, controls, surrounding whitespace, invalid UTF-8, or QIF structural characters `[]|` are rejected. The total name is bounded to 256 UTF-8 bytes. Ampersands and literal slashes are ordinary editable-name characters; QIF slash interpretation is described in the [catalog profile](../features/009-qif-profile.md#category-catalog-and-literal-slash-names--accepted-2026-10-28). QIF accepts them literally; OFX XML text uses `&amp;`, decoded by the statement parser. No exporter is implemented by this change.

Uniqueness and exact picker lookup use the same bounded PCRE2 Unicode caseless literal comparison as the application matching helper, without locale-dependent collation or Unicode normalization. Archived names reserve their spelling case-insensitively. Deleted names can be reused with a new identity. Typing an existing full name in any capitalization and pressing Enter in the picker selects its saved spelling.

Creation and rename normalize only separator spacing. Rename preserves identity and changes only that category. Moving to a different stem is a rename of the complete name. Archive, unarchive, and eligible deletion act only on the selected category. Delete requires no retained references; otherwise archive. No parent object, inherited archive state, or group action is involved. Definition history and durable operation receipts remain atomic and retained.

New Dataset offers **Use default categories**, checked by default. The default categories, initial account, and immutable dataset preferences are written together in one transaction, each category with its own creation history. Unchecked creates an empty catalog. Missing imported names follow the [import-wide category decision](qif-account-imports.md#import-category-decisions--owner-go-2026-11-02); seed names do not create categorization rules. Income names are ordinary neutral categories, not reporting policies.

`category:Auto` in ledger search includes the exact category and names with the `Auto:` stem boundary; `category-exact:Auto` includes only the direct category. Names with spaces can be quoted, for example `category:"auto: gas"`. These filters apply to bound categories and combine with existing text/date filters. Ordinary free-text search remains substring search. They select whole ledger rows, retaining the existing whole-transaction withdrawal/deposit totals; they are not allocation-total reports. Report selection and presentation remain deferred.

## Explicit assignment and source ownership

The picker distinguishes active catalog leaves, Uncategorized, and unmapped text. Without a source category catalog, imported labels remain visibly unmapped until the owner deliberately chooses a category. A complete QIF catalog stages its names separately and binds source-owned allocations at account commit using the first group publication’s immutable catalog revision. Categories used by other accounts’ retained source drafts cannot be deleted after that publication; truly unused imported definitions can be deleted. Unmapped text remains editable through the ordinary saved-draft or committed-correction workflow. All catalog entries are assignable when active.

An assignment records category identity, the catalog revision at assignment, and the label snapshot. New assignments check current catalog freshness and effective availability. Unchanged archived bindings survive unrelated edits and eligible Undo. Catalog renaming alone does not block financial Undo; committing a newly reviewed assignment still checks the current catalog revision.

Schema 5 also retains stable allocation identities, explicit original source-allocation associations, and field ownership. Parser-created allocations receive source associations; new manual allocations do not. Reordering allocations never infers a new association from the display index. Explicit manual edits, including blanks, stay manual; a caller cannot erase that protection merely by relabeling ownership. Older metadata-free callers are conservatively treated as manual rather than receiving guessed source associations. Schema 5 rejects rule ownership. Schema 6 stores a rule mark beside a rule-owned field and rejects the owner without the mark. See [durable rules](durable-rules.md).

## Atomicity and compatibility

Category preview is read-only. Apply writes its definition revision, head, catalog revision, operation receipt and shared history order atomically. A repeated operation identity with identical content returns its durable result; conflicting reuse fails. Failed or uncertain acknowledgements use the same durable outcome identity. The UI preserves failed form edits; an unreadable outcome requires explicit reopen rather than assuming success or rollback.

Current builds use schema 39 without rules and schema 40 with rules and bulk receipts. The category catalog remains document-wide. Earlier experimental layouts are rejected without migration. Authenticated version/shape mismatches are rejected. Backups retain all accounts and category/assignment history; original import evidence and financial receipts remain immutable.

## UI and verification boundary

All search fields use a shared inline × clear control, including the ledger, category manager/picker, currency picker and synthetic sheet. Clear returns focus to the field; the ledger retains its guarded query and view-restoration behavior. Separate search-clear buttons are removed.

The Categories page provides search with an inline clear button and a compact, left-aligned table filling the remaining height, with a directly draggable scrollbar. New category sits before the sort controls, separated by a small gap; a thin divider separates that toolbar from the list. Show archived includes archived categories only; deleted categories never appear, while their internal definition history and receipts remain retained. New category adds and focuses a temporary bottom row, including under an active search filter. Creation and rename save on Enter or focus loss; Escape cancels, and an empty new row is discarded. Validation failures preserve the typed name and display an error. Navigation waits for a successful save. Rename, archive/unarchive and eligible deletion are right-click row actions. There is no separate save/review dialog or visible definition-history section; the existing validated atomic commands and stored history remain. The allocation picker searches parent and leaf paths and distinguishes archived existing assignments. Worker-local view decoration supplies current labels without rewriting storage snapshots or authoritative money.

The editor preserves its existing order until the user presses **A–Z ↑**, **Z–A ↓**, or **Order added**. The chosen sort is highlighted and remembered in application settings, including across restarts. A–Z/Z–A compare complete names without case distinctions using locale comparison; Order added puts oldest creations first and new saves at the bottom. Renames retain original creation order. Blank editing rows always start at the bottom; successful saves and sorting keep the category selected and visible. Saved names that do not match an active search remain temporarily visible until that search is edited. Sort controls never change category names, identities or assignments.

Creation order is exposed through the application catalog as the initial category version's catalog revision. The storage query keeps the previous ID ordering and reads this extra metadata from existing immutable history. No schema change or migration is involved.

Independent tests cover namespace and name validation, group archive inheritance, explicit assignments, stale previews, historical references/deletion, archived carry/Undo, source associations, manual blanks, shared operation identities, legacy schemas, backups, authenticated corruption, I/O faults and interrupted operations. Native and offscreen category UI checks passed 4/4 each, with manager/picker rendering inspected. Debug covered all 61 cases across separate invocations; Release passed all 64 combined cases. ASan/UBSan covered all 64 with passing results across its full run and two extended backup reruns. Final affected checks passed after rebuilding. Actual results and the recorded intermittent Qt interaction warning are in [project status](status.md); no other-platform, full accessibility or production claim follows.

Opening verifies catalog history and allocation metadata on the worker, with cooperative checkpoints while replaying operations/snapshots. Earlier 100,000/200,000-row durable-UI timings predate this format and do not establish schema-5 performance. Large-catalog/source-metadata replay responsiveness remains unmeasured.
