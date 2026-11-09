# Rejected unified action Undo — historical conversion inventory

**Owner stop, 2026-11-08:** the generic row-capture Undo architecture was rejected.
See the [rethink handoff](action-undo-rethink-handoff.md). This material is historical investigation
evidence, not authority to resume implementation, review or budget approval.

Date: 2026-11-08. Source baseline: `b4b65df`, formats 68/67.
State: source-inspected conversion inventory; **not a claim of converted commands,
capture enforcement or production readiness**. See the [then-approved, now rejected design](action-undo-rejected-design.md).
The [replacement inventory](action-undo-inventory.md) defines the current proposal.
The prototype and runtime schema coverage checks must validate this inventory before rollout.

## Table policies

The list below includes every distinct persistent `CREATE TABLE` in the current
schema constructor and its transfer/Review schema includes. Both rules branches
are included. SQLite-owned `sqlite_sequence` is a monotonic allocator, never an
inverse target. Temporary cleanup working sets are derived, not semantic roots.

- **R**: reversible semantic rows, including legacy history needed to interpret
  current state. Domain operation rows are not disposable retry receipts.
- **P/R**: split immutable bodies from reversible active memberships/references;
  pin existing bodies, never copy them into inverse blobs. Precise codecs follow
  the prototype. Not every column in these tables is an immutable body.
- **M**: monotonic receipt/identity/session state; split any semantic fields from
  the monotonic representation rather than reversing the whole mixed row.
- **Replace**: old Undo bookkeeping superseded by the common action chain; no
  independent legacy Undo path may remain in the converted format.

| Table | Policy | Current declaration |
| --- | --- | --- |
| `acceptance_members` | R | [import_store.cpp:562](../src/storage/import_store.cpp) |
| `acceptance_operations` | R | [import_store.cpp:564](../src/storage/import_store.cpp) |
| `accepted_transactions` | R | [import_store.cpp:561](../src/storage/import_store.cpp) |
| `account` | R | [import_store.cpp:230](../src/storage/import_store.cpp) |
| `account_amount_revisions` | R | [import_store.cpp:623](../src/storage/import_store.cpp) |
| `account_creations` | M | [import_store.cpp:233](../src/storage/import_store.cpp) |
| `account_operations` | R | [import_store.cpp:622](../src/storage/import_store.cpp) |
| `account_retirements` | R | [import_store.cpp:621](../src/storage/import_store.cpp) |
| `account_status` | R | [import_store.cpp:620](../src/storage/import_store.cpp) |
| `allocations` | P/R | [import_store.cpp:274](../src/storage/import_store.cpp) |
| `amount_preferences` | R | [import_store.cpp:1203](../src/storage/import_store.cpp) |
| `batch_source_currency` | R | [import_store.cpp:624](../src/storage/import_store.cpp) |
| `batches` | R | [import_store.cpp:234](../src/storage/import_store.cpp) |
| `bulk_members` | R | [import_store.cpp:545](../src/storage/import_store.cpp) |
| `bulk_operations` | R | [import_store.cpp:544](../src/storage/import_store.cpp) |
| `category_heads` | R | [import_store.cpp:338](../src/storage/import_store.cpp) |
| `category_operations` | R | [import_store.cpp:339](../src/storage/import_store.cpp) |
| `category_state` | R | [import_store.cpp:335](../src/storage/import_store.cpp) |
| `category_versions` | R | [import_store.cpp:337](../src/storage/import_store.cpp) |
| `change_dependencies` | R | [import_store.cpp:312](../src/storage/import_store.cpp) |
| `command_completions` | M | [import_store.cpp:1171](../src/storage/import_store.cpp) |
| `dataset_acceptances` | R | [import_store.cpp:563](../src/storage/import_store.cpp) |
| `dataset_preferences` | R | [import_store.cpp:1204](../src/storage/import_store.cpp) |
| `document_session` | M | [import_store.cpp:1169](../src/storage/import_store.cpp) |
| `draft_allocation_meta` | P/R | [import_store.cpp:343](../src/storage/import_store.cpp) |
| `draft_fields` | P/R | [import_store.cpp:341](../src/storage/import_store.cpp) |
| `draft_rule_allocations` | R | [import_store.cpp:531](../src/storage/import_store.cpp) |
| `draft_rule_fields` | R | [import_store.cpp:528](../src/storage/import_store.cpp) |
| `entry_allocation_meta` | P/R | [import_store.cpp:344](../src/storage/import_store.cpp) |
| `entry_allocations` | P/R | [import_store.cpp:309](../src/storage/import_store.cpp) |
| `entry_fields` | P/R | [import_store.cpp:342](../src/storage/import_store.cpp) |
| `entry_heads` | R | [import_store.cpp:310](../src/storage/import_store.cpp) |
| `entry_revisions` | P/R | [import_store.cpp:308](../src/storage/import_store.cpp) |
| `entry_rule_allocations` | R | [import_store.cpp:537](../src/storage/import_store.cpp) |
| `entry_rule_fields` | R | [import_store.cpp:534](../src/storage/import_store.cpp) |
| `events` | R | [import_store.cpp:279](../src/storage/import_store.cpp) |
| `generated_source_matches` | R | [import_store.cpp:584](../src/storage/import_store.cpp) |
| `history_order` | R | [import_store.cpp:313](../src/storage/import_store.cpp) |
| `import_balance_events` | R | [import_store.cpp:247](../src/storage/import_store.cpp) |
| `import_category_choices` | R | [import_store.cpp:243](../src/storage/import_store.cpp) |
| `import_category_decisions` | R | [import_store.cpp:242](../src/storage/import_store.cpp) |
| `import_completions` | R | [import_store.cpp:594](../src/storage/import_store.cpp) |
| `import_file_errors` | R | [import_store.cpp:593](../src/storage/import_store.cpp) |
| `import_group_accounts` | R | [import_store.cpp:240](../src/storage/import_store.cpp) |
| `import_group_events` | R | [import_store.cpp:246](../src/storage/import_store.cpp) |
| `import_groups` | R | [import_store.cpp:239](../src/storage/import_store.cpp) |
| `ledger_operations` | R | [import_store.cpp:311](../src/storage/import_store.cpp) |
| `link_exclusion_events` | R | [review_sheet_schema.inc:2](../src/storage/review_sheet_schema.inc) |
| `manual_creations` | R | [import_store.cpp:574](../src/storage/import_store.cpp) |
| `mutation_generation` | M | [import_store.cpp:577](../src/storage/import_store.cpp) |
| `operation_registry` | M | [import_store.cpp:340](../src/storage/import_store.cpp) |
| `operations` | R | [import_store.cpp:277](../src/storage/import_store.cpp) |
| `records` | P/R | [import_store.cpp:265](../src/storage/import_store.cpp) |
| `report_accounts` | R | [import_store.cpp:1199](../src/storage/import_store.cpp) |
| `report_categories` | R | [import_store.cpp:1200](../src/storage/import_store.cpp) |
| `report_periods` | R | [import_store.cpp:1201](../src/storage/import_store.cpp) |
| `report_state` | R | [import_store.cpp:1196](../src/storage/import_store.cpp) |
| `reports` | R | [import_store.cpp:1198](../src/storage/import_store.cpp) |
| `retained_account_aliases` | R | [import_store.cpp:583](../src/storage/import_store.cpp) |
| `retained_category_names` | R | [import_store.cpp:582](../src/storage/import_store.cpp) |
| `retained_group_counts` | R | [import_store.cpp:587](../src/storage/import_store.cpp) |
| `retained_opening_reviews` | R | [import_store.cpp:581](../src/storage/import_store.cpp) |
| `retained_source_counts` | R | [import_store.cpp:590](../src/storage/import_store.cpp) |
| `retained_transfer_rows` | R | [transfer_schema.inc:2](../src/storage/transfer_schema.inc) |
| `retired_import_accounts` | R | [import_store.cpp:241](../src/storage/import_store.cpp) |
| `review_answers` | R | [review_sheet_schema.inc:7](../src/storage/review_sheet_schema.inc) |
| `review_choices` | R | [review_sheet_schema.inc:12](../src/storage/review_sheet_schema.inc) |
| `review_link_history` | P/R | [review_sheet_schema.inc:16](../src/storage/review_sheet_schema.inc) |
| `review_members` | R | [review_sheet_schema.inc:11](../src/storage/review_sheet_schema.inc) |
| `review_merges` | R | [review_sheet_schema.inc:15](../src/storage/review_sheet_schema.inc) |
| `review_sources` | R | [review_sheet_schema.inc:14](../src/storage/review_sheet_schema.inc) |
| `review_stack` | Replace | [review_sheet_schema.inc:10](../src/storage/review_sheet_schema.inc) |
| `review_stamps` | R | [import_store.cpp:315](../src/storage/import_store.cpp) |
| `review_undo_members` | Replace | [review_sheet_schema.inc:9](../src/storage/review_sheet_schema.inc) |
| `review_undos` | Replace | [review_sheet_schema.inc:8](../src/storage/review_sheet_schema.inc) |
| `rollback_receipt` | R | [import_store.cpp:579](../src/storage/import_store.cpp) |
| `rule_applications` | R | [import_store.cpp:460](../src/storage/import_store.cpp) |
| `rule_definitions` | P/R | [import_store.cpp:514](../src/storage/import_store.cpp) |
| `rule_heads` | R | [import_store.cpp:517](../src/storage/import_store.cpp) |
| `rule_operation_requests` | R | [import_store.cpp:521](../src/storage/import_store.cpp) |
| `rule_operations` | R | [import_store.cpp:456](../src/storage/import_store.cpp) |
| `rule_reapplication_members` | R | [import_store.cpp:553](../src/storage/import_store.cpp) |
| `rule_reapplications` | R | [import_store.cpp:550](../src/storage/import_store.cpp) |
| `rule_snapshots` | P/R | [import_store.cpp:525](../src/storage/import_store.cpp) |
| `rule_state` | R | [import_store.cpp:452](../src/storage/import_store.cpp) |
| `session_history` | R | [import_store.cpp:1184](../src/storage/import_store.cpp) |
| `source_associations` | R | [import_store.cpp:606](../src/storage/import_store.cpp) |
| `source_headers` | P/R | [import_store.cpp:604](../src/storage/import_store.cpp) |
| `source_occurrences` | P/R | [import_store.cpp:605](../src/storage/import_store.cpp) |
| `sources` | P/R | [import_store.cpp:232](../src/storage/import_store.cpp) |
| `transfer_events` | R | [transfer_schema.inc:5](../src/storage/transfer_schema.inc) |
| `transfer_heads` | R | [transfer_schema.inc:7](../src/storage/transfer_schema.inc) |
| `transfer_versions` | P/R | [transfer_schema.inc:6](../src/storage/transfer_schema.inc) |
| `versions` | P/R | [import_store.cpp:269](../src/storage/import_store.cpp) |

Distinct persistent tables: **94**. Schema trigger side effects and
foreign-key cascades participate in the same capture boundary. This list is not
a substitute for runtime `sqlite_schema` comparison in both configurations.

## Root command inventory

All names below are current repository entry points, not new APIs. A standalone
legacy call must create the same root; calls inside another command join its root.
The final API adds operation identity and `ActionToken` to prepared writes where
currently absent. Reads and session account selection do not create roots.

| Family | Mutating entry points | Conversion boundary |
| --- | --- | --- |
| Accounts | `create_account`, `opening`, `rename_account`, `archive_account`, `reactivate_account`, `delete_account`, `correct_opening`, `reassign_currency` | Account, status, aliases, definitions, opening and all rewritten amount/transfer state. Creation during import is its child. Document creation is baseline. |
| Imports | `ingest`, `ingest_controlled`, `prepare_import_group`, `resume_import_group`, `resume`, `resolve_import_categories`, `resolve_import_transfers`, `review_import_balance` | Preparing root pins parent before setup/capture; publication advances head once. Later repair/setup is a new root. Retained terminal failures count. |
| Import cleanup | `cleanup_incomplete_imports`, `abort_import_group` | Preparing-root cancellation only; restore setup and source ownership. Failed cleanup preserves preparing state and blocks writes. |
| New transactions | `create_manual`, `save`, `decide`, `edit_transaction`, `edit_transactions` | Include original/reference, split, decision, metadata and automatic transfer effects. |
| Accepted transactions | `apply_committed_change`, `apply_committed_change_with_catalog`, `apply_bulk` | Include all linked rows and relationships; old `ChangeKind::undo` must not bypass head. |
| Review | `answer_review`, `merge_review_batch`, `save_link_exclusion` | Decisions, source retargeting, exclusions and counterpart retirement together. |
| Category/transfer assignment | `assign_categories`, `assign_transfer_batch`, `apply_transfer` | All selected rows, allocations, generated counterparts, transfer versions and heads. |
| Category definitions | `apply_category_change` | Includes Income-only fast path, lifecycle, catalog and derived name/alias consequences. Publication inside Import/Accept is a child. |
| Rules | `save_rules`, `apply_account_rules`, `apply_pending_rules`, `apply_rule_reapply` | Save all is one exact saved set. New atomic composed Save and apply must own both halves and validate one token. |
| Reports | `save_reports` | One saved catalog change, including selected-save/delete/save-all scope; report calculation afterward is a read. |
| File settings | `set_amount_format` | Add operation identity/token. Date format currently has a getter and creation-time setup, no independent save API. |
| Acceptance | `accept_all`, `commit` | Newly accepted membership plus decisions, category publication, account freezes, local transfer acceptance/protection and batch/source state. |
| Removal | `undo_import`, `rollback_account`, `discard` | Prior-session import removal/account Discard are forward undoable roots; current-session source removal must use strict ordinary Undo. Retain removed bodies via pins before collectors run. |
| Legacy Undo | `undo_account_operation`, `undo_manual`, `undo_transaction_edit`, `undo_review`, `undo_account_rules`, committed/bulk/rule inverse request variants | Route to the single head-checked API; no selective inverse escape. |

Implementation files include `import_store.cpp`, `account_lifecycle.inc`,
`account_aliases.inc`, `manual_store.inc`, `ledger_selection_store.inc`,
`committed_store.inc`, `bulk_store.inc`, `category_store.inc`, `rule_store.inc`,
`rule_account_store.inc`, `rule_reapply_store.inc`, `review_sheet_store.inc`,
`transfer_store.inc`, `dataset_acceptance.inc`, `import_group_store.inc`,
`import_category_decisions.inc`, `account_rollback.inc`,
`incomplete_import_cleanup.inc` and `session_prune.inc`.

Gateway creation/open/recovery/certification/backup/restore are explicit lifecycle
contexts. Certification expires ordinary inverses atomically, only after resolving
preparing roots. No normal-open scan/hash is added. `prepare_review` and other
recomputed in-memory caches are not action payload roots. Verify all purported
reads for hidden SQL writes during conversion, not merely `const` signatures.

## Identified revision and allocator dependencies

These are concrete conversion obligations found in source, not merely replacing
one counter. References name functions/SQL so they remain useful after line shifts.

| Location | Existing assumption | Required change |
| --- | --- | --- |
| `import_repository.hpp::next_account_identity` | Maximum surviving account + 1 | Durable high-water allocator; Undo cannot reuse removed account ID. |
| `import_store.cpp` batch/head/category transition triggers | `NEW.revision = OLD.revision + 1` | Explicit recorded predecessor plus allocated high-water revision; narrowly authorized exact restoration. |
| `import_store.cpp::verify` | Event count equals current revision minus baseline revision; max event is current | Validate explicit active ancestry, not dense arithmetic. |
| `committed_store.inc::verify_committed` | Prior event uses `event_revision-1`; after entry equals head + 1; acceptance event adjacency | Record/validate predecessor and root/child relation explicitly. |
| `category_store.inc::retained_ruleset` and category provenance verification | Prior accepted entry is `revision-1` | Resolve recorded predecessor rather than integer subtraction. |
| `rule_store.inc::verify_rules` | Ruleset after = before + 1; definition version = previous + 1 | Separate monotonic allocation from active predecessor. Ordered positions remain dense within one snapshot. |
| `account_lifecycle.inc` | Account revision and rewritten draft/entry revisions advance +1 | Allocate above high water; retain explicit prior references for amount/currency verification. |
| `review_sheet_store.inc`, `ledger_selection_store.inc`, `rule_account_store.inc` | Review/exclusion sequence from current maximum + 1 | Monotonic sequence allocation independent of restored rows. |
| `manual_store.inc` | Record sequence from max surviving records + 1 | Durable record-order high water. |
| `transfer_store.inc` | `entry_after.revision-1`, expected transfer sequence + 1, max prior transfer version | Explicit financial/revision predecessor, preserving referenced immutable versions. |
| `session_prune.inc`, `account_rollback.inc` | Maximum surviving revision/sequence identifies current/prior | Follow active heads and dependency edges; pinned abandoned versions must not become current. |
| `dataset_acceptance.inc`, `import_group_store.inc`, `import_category_decisions.inc` | Child draft revision = current batch revision + 1 | Root-scoped monotonic allocation with explicit child ancestry. |
| `save_reports`, `set_amount_format` | Missing caller command identity; report revision increment | Add retry-safe identity/token and report high water before broad conversion. |

`operation_registry` uniqueness and domain identity survive Undo. Existing
`register_*` triggers must distinguish forward registration from an exact
journal-authorized restoration; restoration must validate original registry
domain and owning action. Never disable all triggers or foreign-key checks.

## Verification completion criteria

Before declaring the inventory frozen, compare actual schema tables/triggers in
both configurations with a checked manifest, bind versioned typed keys/codecs,
exercise cascades, and make an unclassified semantic write fail. Every row in the
root-command table requires canonical before/forward/Undo assertions, mixed-chain
and branch tests, and appropriate failure/recovery tests. The source inventory
alone does not establish these properties. Prototype measurements and the full
storage gate are recorded separately; no UI conversion is authorized by a passing
inventory text check.

### Prototype discovery: source order

`undoable_imports()` reads `sources.rowid` as source order. Restoring only a
source's text ID can allocate a different rowid and change which import is newest.
The isolated payload prototype therefore carries explicit source order in its
reversible membership, allocated from a monotonic high-water value. Verification
must remove/restore the earlier of two sources; one-source fixtures hide this
defect. The source read view also requires an explicit column list for existing
source INSERTs once it exposes this order. See the [prototype investigation](../review/action-undo-prototype/README.md).
