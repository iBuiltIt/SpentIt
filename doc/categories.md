# Category catalog and assignments

Last updated: 2026-10-25

State: bounded experimental schema-5 implementation verified on available macOS configurations. The owner accepted the [category lifecycle policy](../features/010-categories-and-selection.md). This implements a catalog, editor and allocation picker within the existing one-account QIF experiment. It does not implement category reports, account transfer destinations, automatic starter categories, a rules editor or migrations.

## Identity, organization and history

Categories have stable identities. A category is permanently a non-assignable group or an assignable leaf; leaves may stand alone or belong to one group. Deeper nesting is rejected. Names are exact UTF-8 strings, bounded to 256 bytes, with no silent normalization or case folding. Siblings, including archived entries, have unique names. Top-level groups and leaves share one namespace. Empty names, surrounding Unicode whitespace, control characters and QIF structural characters `[]/|:` are rejected with an explanation. Colon separates rendered group/leaf paths and is not part of either segment's name.

Catalog changes append immutable definition history and durable operation receipts. Rename and move preserve identity. Current ledger display, literal filtering and picker paths resolve current definitions, while source records and earlier draft/committed snapshots retain their recorded labels. This implements the accepted current-organization direction without claiming reports exist.

Archive hides a leaf from new assignments. Archiving a group hides its descendants without changing their individual archive flags; unarchiving restores only individually active leaves. Existing assignments and history remain readable. Deletion is restricted to empty entries that have never been referenced by draft or committed history; deleted definitions retain a tombstone/history rather than disappearing from the audit record. There is no implicit merge or reclassification.

## Explicit assignment and source ownership

The picker distinguishes active catalog leaves, Uncategorized, and unmapped text. Imported labels remain visibly unmapped until the owner deliberately chooses a category; no name-based matching creates an identity automatically. Unmapped text remains editable through the ordinary saved-draft or committed-correction workflow. A group cannot receive an allocation.

An assignment records category identity, the catalog revision at assignment, and the label snapshot. New assignments check current catalog freshness and effective availability. Unchanged archived bindings survive unrelated edits and eligible Undo. Catalog renaming alone does not block financial Undo; committing a newly reviewed assignment still checks the current catalog revision.

Schema 5 also retains stable allocation identities, explicit original source-allocation associations, and field ownership. Parser-created allocations receive source associations; new manual allocations do not. Reordering allocations never infers a new association from the display index. Explicit manual edits, including blanks, stay manual; a caller cannot erase that protection merely by relabeling ownership. Older metadata-free callers are conservatively treated as manual rather than receiving guessed source associations. Rule ownership remains rejected by this format until durable rule definitions/provenance are implemented.

## Atomicity and compatibility

Category preview is read-only. Apply writes its definition revision, head, catalog revision, operation receipt and shared history order atomically. A repeated operation identity with identical content returns its durable result; conflicting reuse fails. Failed or uncertain acknowledgements use the same durable outcome identity. The UI preserves failed form edits; an unreadable outcome requires explicit reopen rather than assuming success or rollback.

New documents use schema 5. Schemas 1–4 remain readable with their earlier capabilities and do not expose catalog commands; there is no automatic migration. Authenticated version/shape mismatches are rejected. Backups retain the actual format and category/assignment history. Original import evidence and financial receipts remain immutable.

## UI and verification boundary

The Categories page provides search, create group/leaf, rename/move, archive/unarchive, eligible deletion and definition history. Forms use Save / Discard / Stay navigation. The allocation picker searches parent and leaf paths and distinguishes archived existing assignments. Worker-local view decoration supplies current labels without rewriting storage snapshots or authoritative money.

Independent tests cover namespace and name validation, group archive inheritance, explicit assignments, stale previews, historical references/deletion, archived carry/Undo, source associations, manual blanks, shared operation identities, legacy schemas, backups, authenticated corruption, I/O faults and interrupted operations. Native and offscreen category UI checks passed 4/4 each, with manager/picker rendering inspected. Debug covered all 61 cases across separate invocations; Release passed all 64 combined cases. ASan/UBSan covered all 64 with passing results across its full run and two extended backup reruns. Final affected checks passed after rebuilding. Actual results and the recorded intermittent Qt interaction warning are in [project status](status.md); no other-platform, full accessibility or production claim follows.

Opening verifies catalog history and allocation metadata on the worker, with cooperative checkpoints while replaying operations/snapshots. Earlier 100,000/200,000-row durable-UI timings predate this format and do not establish schema-5 performance. Large-catalog/source-metadata replay responsiveness remains unmeasured.
