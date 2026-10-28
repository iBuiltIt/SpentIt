# Category catalog and assignments

Last updated: 2026-10-27

State: bounded experimental category catalog verified on available macOS configurations. Current rules-enabled documents use schema 20 and rules-off documents use schema 21. Schemas 12 and 13 describe earlier checkpoints. The owner accepted the [category lifecycle policy](../features/010-categories-and-selection.md). This implements a catalog, editor, allocation picker, and account transfer destinations within the durable import experiment. It does not implement category reports, automatic starter categories, or migrations. The rules page is specified in [durable rules](durable-rules.md). Account destinations use the [transfer commands](transfer-engine.md).

The 2026-10-26 follow-up adds committed bulk assignment to every selected allocation, with exact preview, one atomic operation, and group Undo. See the [search/ledger contract](search-ledger.md). Experimental-format migration and compatibility obligations start only at an owner-announced release.

## Identity, organization and history

Categories have stable identities. A category is permanently a non-assignable group or an assignable leaf; leaves may stand alone or belong to one group. Deeper nesting is rejected. Names are exact UTF-8 strings, bounded to 256 bytes, with no silent normalization or case folding. Siblings, including archived entries, have unique names. Top-level groups and leaves share one namespace. Empty names, surrounding Unicode whitespace, control characters and QIF structural characters `[]/|:` are rejected with an explanation. Colon separates rendered group/leaf paths and is not part of either segment's name.

Catalog changes append immutable definition history and durable operation receipts. Rename and move preserve identity. Current ledger display, combined search and picker paths resolve current definitions, while source records and earlier draft/committed snapshots retain their recorded labels. This implements the accepted current-organization direction without claiming reports exist.

Archive hides a leaf from new assignments. Archiving a group hides its descendants without changing their individual archive flags; unarchiving restores only individually active leaves. Existing assignments and history remain readable. Deletion is restricted to empty entries that have never been referenced by draft or committed history; deleted definitions retain a tombstone/history rather than disappearing from the audit record. There is no implicit merge or reclassification.

## Explicit assignment and source ownership

The picker distinguishes active catalog leaves, Uncategorized, and unmapped text. Imported labels remain visibly unmapped until the owner deliberately chooses a category; no name-based matching creates an identity automatically. Unmapped text remains editable through the ordinary saved-draft or committed-correction workflow. A group cannot receive an allocation.

An assignment records category identity, the catalog revision at assignment, and the label snapshot. New assignments check current catalog freshness and effective availability. Unchanged archived bindings survive unrelated edits and eligible Undo. Catalog renaming alone does not block financial Undo; committing a newly reviewed assignment still checks the current catalog revision.

Schema 5 also retains stable allocation identities, explicit original source-allocation associations, and field ownership. Parser-created allocations receive source associations; new manual allocations do not. Reordering allocations never infers a new association from the display index. Explicit manual edits, including blanks, stay manual; a caller cannot erase that protection merely by relabeling ownership. Older metadata-free callers are conservatively treated as manual rather than receiving guessed source associations. Schema 5 rejects rule ownership. Schema 6 stores a rule mark beside a rule-owned field and rejects the owner without the mark. See [durable rules](durable-rules.md).

## Atomicity and compatibility

Category preview is read-only. Apply writes its definition revision, head, catalog revision, operation receipt and shared history order atomically. A repeated operation identity with identical content returns its durable result; conflicting reuse fails. Failed or uncertain acknowledgements use the same durable outcome identity. The UI preserves failed form edits; an unreadable outcome requires explicit reopen rather than assuming success or rollback.

Current builds use the [account-scoped formats](accounts.md): schema 13 without rules or schema 12 with rules and bulk receipts. The category catalog remains document-wide. Earlier experimental layouts are rejected without migration. Authenticated version/shape mismatches are rejected. Backups retain all accounts and category/assignment history; original import evidence and financial receipts remain immutable.

## UI and verification boundary

The Categories page provides search, create group/leaf, rename/move, archive/unarchive, eligible deletion and definition history. Forms use Save / Discard / Stay navigation. The allocation picker searches parent and leaf paths and distinguishes archived existing assignments. Worker-local view decoration supplies current labels without rewriting storage snapshots or authoritative money.

Independent tests cover namespace and name validation, group archive inheritance, explicit assignments, stale previews, historical references/deletion, archived carry/Undo, source associations, manual blanks, shared operation identities, legacy schemas, backups, authenticated corruption, I/O faults and interrupted operations. Native and offscreen category UI checks passed 4/4 each, with manager/picker rendering inspected. Debug covered all 61 cases across separate invocations; Release passed all 64 combined cases. ASan/UBSan covered all 64 with passing results across its full run and two extended backup reruns. Final affected checks passed after rebuilding. Actual results and the recorded intermittent Qt interaction warning are in [project status](status.md); no other-platform, full accessibility or production claim follows.

Opening verifies catalog history and allocation metadata on the worker, with cooperative checkpoints while replaying operations/snapshots. Earlier 100,000/200,000-row durable-UI timings predate this format and do not establish schema-5 performance. Large-catalog/source-metadata replay responsiveness remains unmeasured.
