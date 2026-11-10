# Unified Undo — operation adaptation inventory

Last updated: 2026-11-09

`ImportStore` and the UI use `ActionChain` with formats 70/69. Read the
[design](action-undo-design.md) first. Do not resume generic per-row capture.
The table is the command coverage map.

## Command coverage

Each financial write joins one typed root. Existing domain receipts/snapshots are
the restoration source; an action header references them. The table below identifies
the adapters; runtime evidence is recorded in the implementation report.

| Family and current entry points | Treatment and implementation locations |
| --- | --- |
| `create_manual`, `undo_manual` | Creation action; replace latest-generation-only eligibility with common head; remove only its created ownership scope on Undo. `src/storage/manual_store.inc`. Add durable retry protection after creation is undone/collected. |
| `save`, `decide`, `edit_transaction`, `edit_transactions` | User edit/decision action, or child when called by importer. Preserve decisions, split identities, originals and links; reuse Review snapshots. Set memo uses the existing `edit_transactions` / `ledger-edit-batch` adapter. `import_store.cpp`, `review_sheet_store.inc`, `ledger_selection_store.inc`. |
| `apply_committed_change`, `apply_committed_change_with_catalog`, `apply_bulk` | Typed transaction/group action; append restoration revisions. Replace standalone strict inverse selection with common-head authorization and verified restoration lineage. `committed_store.inc`, `bulk_store.inc`. |
| `answer_review`, `merge_review_batch`, `save_link_exclusion` | One decision/group action including source reassociation and counterparts; exclusions need their own small prior-state recipe. Reuse `review_members`, `review_sources`, `review_link_history`. `review_sheet_store.inc`. |
| `assign_categories`, `assign_transfer_batch`, `apply_transfer` | One transaction/relationship action or caller's child. Include all affected accounts and independent/manual counterparts. `category_store.inc`, `review_sheet_store.inc`, `transfer_store.inc`. |
| `apply_account_rules`, `apply_pending_rules`, `apply_rule_reapply` | User invocation is one action for changed entries; automatic import invocation is an import child with zero ordinary Undo members for new imported rows. `rule_account_store.inc`, `rule_store.inc`, `rule_reapply_store.inc`. |
| New composed Save and apply command | One atomic forward save/application. Reference transaction before-states only; Undo retains the saved definition. Replace the UI Save → later Apply mutation split for this command. |
| `correct_opening`, `reassign_currency` | Monetary account actions. Reuse exact prior amount/currency state; restore only owned monetary fields and related postings, preserving independent account metadata. `account_lifecycle.inc`, account money/history helpers. |
| `accept_all`, legacy `commit` | One acceptance action or child within it. Add typed pre-decision/lifecycle restoration and revoked-acceptance verification; reuse membership and snapshots. `dataset_acceptance.inc`, publication and committed verification paths. |
| `ingest`, `ingest_controlled`, `prepare_import_group`, `resume_import_group`, initial `resume`, `resolve_import_categories`, `resolve_import_transfers` | One preparing/published import root, explicit stage/parent/source ownership. Separate later user repairs/resolution into actions. `import_store.cpp`, `import_group_store.inc`, `import_category_decisions.inc`. |
| `cleanup_incomplete_imports`, `abort_import_group` | Cancel/clean only the recorded preparing import; materialized roots need whole-source restoration, not capture-only cleanup. Later abort/removal routes through the head or empty-chain policy. `incomplete_import_cleanup.inc`, `import_group_store.inc`. |
| `undo_import` | Current-session head: typed whole-import reversal and head pop. Older import: separate confirmed removal with empty-chain guard. Preserve source conflict/acceptance checks; replace stack clearing and one-slot retry behavior. `account_rollback.inc`. |
| `rollback_account`, `discard` | User destructive removal requires empty chain; internal manual/import reversal can call a scoped child under the coordinator. No public escape through legacy calls. `account_rollback.inc`, import/transfer discard helpers. |
| `save_rules`, `save_reports`, `set_amount_format` | Compatible definition/settings commands; no Undo step, advance epoch and preserve referenced historical definitions. `rule_store.inc`, report/preference storage in `import_store.cpp`. |
| `apply_category_change` | Definition-only: no Undo step, including Income fast path; dependency-aware deletion and historical archived assignments. `category_store.inc`. |
| `create_account`, `rename_account`, `archive_account`, `reactivate_account`, `delete_account`, alias updates | Definition-only, preserving account references needed by Undo. Guard archive/delete against active action dependencies. Initial document account is baseline; import setup belongs to import. `account_lifecycle.inc`, `account_aliases.inc`, `import_store.cpp`. |
| `opening`, `review_import_balance` | Initial opening is creation/setup, not a backdoor edit. Retired staged-balance API remains refusing; later monetary corrections use the proper action. |
| `undo_review`, `undo_transaction_edit`, `undo_account_rules`, `undo_account_operation`, other legacy inverse request variants | Delegate to common head or become internal restoration helpers; never independent eligibility/selection systems. |
| Open/close, backup/restore and pruning | Explicit lifecycle context. Include active typed payload references in reachability; preserve clean-marker/expiry policy. `document_gateway.cpp`, `session_prune.inc`, `session_snapshot.inc`, verification paths. |

All source names above are relative to `src/storage/` unless stated otherwise.
The public contracts live in `src/app/include/spentit/import_repository.hpp` and
related command types; UI consumers are `src/ui/import_model.cpp` and
`src/ui/ImportMain.qml`. Financial logic stays outside QML.

## Specific traps to close

- `review_stack` currently owns mixed Review/edit/rule ordering; `sessionEdits` in
  the UI can forget an older reachable edit. Replace eligibility with one durable
  head. Reusing receipts does not mean retaining parallel authoritative stacks.
- Existing Undo often appends revisions, so a previous forward result's physical
  revision will no longer equal current. Reuse/extend typed restoration lineage;
  do not loosen legacy guards to value equality or rewind all schema histories.
- `account_rollback.inc` clears the Review stack and collects source/history rows.
  Retain active parent-action dependencies when using it as Undo Import.
- Category Income changes explicitly advance mutation generation and currently
  disable Review Undo in cached state. Under the replacement they invalidate
  previews but preserve the action head. Other compatible definitions need the
  same distinction.
- Acceptance changes warning decisions, counterpart protection, freezes and source/
  batch state. Current `commit` is not an inverse-capable implementation. Add a
  typed unaccept transition and verifier support together, including reacceptance
  without reusing retained overlay revisions.
- Undo opening/currency currently uses exact physical anchors and account images.
  Adapt only the financial fields and lineage; account rename is compatible and
  must not be overwritten by a later financial Undo.
- Current initial category resolution is a separate durable call after materializing
  the group. Persist the preparing root across that prompt; block unrelated saved
  commands and distinguish it from a completed import with ordinary Review issues.
- Source order currently uses `sources.rowid`; manual sequence uses MAX+1. Where
  removal could reuse an identity/order token, use a durable allocator. Do not
  retrofit unrelated dense catalog histories into backwards-head storage.
- Full verification/pruning must understand restored domain states and revoked
  acceptance. Forward handlers and inverse handlers sharing a test are insufficient:
  compare canonical affected state and exercise existing independent verification.
- An import's new rows must not acquire ordinary edit memberships through nested
  rule/category/transfer helper calls. Assert this structurally before size testing.

## Existing tests to extend, not claims of new passes

- `tests/storage/review_sheet_tests.cpp`: mixed decisions, edits, category/transfer
  assignment and grouped restoration.
- `tests/storage/statement_store_tests.cpp`: `import_undo` scenarios, shared sources,
  several accounts, preserved earlier edits, generated/accepted transfer restoration,
  backup/reopen, fault and process-exit sweeps.
- `tests/storage/manual_transaction_tests.cpp`: manual creation, independent manual
  counterparts and originals.
- `tests/storage/committed_tests.cpp`, `account_tests.cpp`, existing rule and session
  lifecycle suites: acceptance/edit histories, monetary account operations, retained
  dependencies and successful-close expiry.
- Existing UI/import/Reports tests: common Undo availability, native text Undo,
  unrelated drafts and editable drill-down. Tests of old per-family availability
  must be updated to assert the new head policy, not simply removed.
- `tests/storage/session_storage_measure.cpp` and existing page analysis: retain
  original comparator checkpoints; add realistic bulk edits and zero ordinary
  imported-row membership assertions. Rejected-prototype numbers are historical.

## Rejected approach

Generic per-row capture is not the implementation. Do not restore a probe that
journals an inverse for every imported row. The active chain is the typed
`ActionChain` described above.
