# Review: session-close storage growth

Status: investigation authorized and measured on 2026-11-08; see [results and qualified recommendations](03-retention-investigation-results.md). Implementation remains incomplete and requires a separate owner Go. The original proposed work below is review context, not an accepted six-part implementation plan.

Date: 2026-11-07.

## Question

After a successful close, which of the histories below are still required, and which are ordinary edits that Check file still replays?

The published comparison in `doc/session-storage-comparison.md` holds for import-only, repeated payee-edit, and cross-session import-Undo workloads against `cf8f1a5`. Those workloads do not save rules, rename categories, toggle link exclusions, correct openings, change currency, undo Review links, or change a reported-balance choice. Growth on the workloads below is a separate question. It is judged against the retention boundary in `doc/session-history-baseline.md`: current state, imported originals, recognition, necessary relationships, and eligible import Undo survive; ordinary intermediate edits expire once nothing retained refers to them.

## What to preserve

- Current rule heads and any definition version still cited by a retained transaction’s rule mark.
- Current category heads and any catalog revision still selected by a retained allocation.
- The latest link exclusion per identity, including a row whose latest state is removed. Active exclusions stay capped at 128.
- The current account row, `account_creations`, and a retirement row. Kind 3 retirement is permanent.
- The current opening and the latest currency command, including the kind-5 ledger anchor that command still needs.
- The current `import_group_accounts` opening columns, and one balance event when import Undo or Check file still reads it.
- The current saved Review decision, and the one prior transfer payload import Undo still restores.
- Compact `command_completions`. Those are allowed receipts, not the problem.

Check file must pass after any collapse. Today these verifiers walk the full chain and treat a gap as corruption:

- `ImportStore::verify_rules` (`src/storage/rule_store.inc`, around line 418) requires `ruleset_before` / `ruleset_after` to be contiguous from 0.
- `ImportStore::verify_categories` (`src/storage/category_store.inc`, around line 214) requires `count(category_versions) = catalog.revision` and a gap-free catalog chain.
- `ImportStore::verify_link_exclusions` (`src/storage/review_sheet_store.inc`, around line 111) requires exclusion sequences `1..N`.
- `ImportStore::verify_account_commands` (`src/storage/account_lifecycle.inc`, around line 312) replays every `account_operations` row from creation.
- `ImportStore::verify_import_groups` (`src/storage/import_group_store.inc`, around line 455) replays every `import_balance_events` row. The last row must match `import_group_accounts`. A creation-time choice (`balance_choice >= 5`) must be the first event.

`prune_session` (`src/storage/session_prune.inc`) does not delete these chains. Several keep-triggers forbid the delete even during cleanup. A purge that leaves the verifiers unchanged will fail Check file.

## Work items

Ranked by payload size and by how often a normal session repeats them.

### 1. Rule saves keep every ruleset body

`ImportStore::save_rules` (`src/storage/rule_store.inc`, around lines 218–232) inserts, on every changed save:

- one `rule_operations` row
- one `rule_operation_requests` row per rule, including the expression and templates
- one `rule_snapshots` row per rule
- a new `rule_definitions` version when the text changes
- `history_order` and `operation_registry` rows

An unchanged save returns before those inserts. Preview does not write them. `save_rules` does not write `command_completions`. `prune_session` deletes `rule_applications` only (around line 91). The keep-triggers `rule_operations_keep`, `rule_operation_requests_keep`, `rule_definitions_keep`, and `rule_snapshots_keep` in `src/storage/import_store.cpp` abort a delete.

Prove: after N changed saves of one ruleset, each followed by a successful close, request and snapshot rows stay at one set per live rule, plus definition versions still cited by retained marks. `verify_rules` accepts that file. A repeated operation id still returns already-completed and does not run again.

### 2. Category edits never expire

`write_category_change` (`src/storage/category_store.inc`, around lines 157–163) appends one `category_versions` row and one `category_operations` row per create, rename, income change, archive, or erase. `category_history` hides rows that are no longer in `session_history`, and close deletes `session_history`, so the user-facing list is already gone. The rows remain, and `category_versions_keep` / `category_operations_keep` have no cleanup exception. `verify_categories` requires one version row per catalog revision.

Prove: N income toggles or renames of one category, each followed by a successful close, leave the head version plus only catalog revisions still referenced by retained allocations. Check file passes. A category that was created and then erased leaves no replay of the intermediate names unless a retained payload still names one.

### 3. Undone Review links stay stored

`prune_session` collapses `review_link_history` and drops answers only when the question is `ledger-edit`, `ledger-edit-batch`, `rules-account`, `ledger-delete`, `transfer-assignment`, or `ledger-delete-%` (around lines 16–19 and 107–109). Review questions are named `link:` and `same:` in `src/storage/review_sheet_store.inc` (around lines 248 and 352). Those answers, their `review_choices`, both transfer blobs in `review_link_history`, and the `transfer_events` rows those blobs pin stay after Undo and after close.

A saved, still-effective link is retained. An answer that already has a `review_undos` row, or an older payload superseded by a newer live link, is an expired edit. `session_linked_pruning` covers the ledger-edit questions and does not count `link:` / `same:` after Undo.

Prove: N link-then-undo cycles, then a successful close, leave no `review_link_history` for answers in `review_undos`. The surviving prior payload is only the import-undo input still referenced by the current transfer head. A later import Undo still restores that prior relationship and still refuses a conflict with newer work.

### 4. Exclusion edits are append-only

`save_link_exclusion` inserts one `link_exclusion_events` row per save (`review_sheet_store.inc`, around lines 146–147), including a repeat edit or a removal of the same identity. The live set is the latest row per identity (`link_exclusions`, around lines 105–108). `link_exclusion_keep` (`src/storage/review_sheet_schema.inc`, lines 5–6) aborts every delete, with no `spentit_cleanup()` exception. Each insert also registers `operation_registry` domain 6. `verify_categories` requires that registry count to match the exclusion rows, so a collapsed log has to shrink both.

Prove: toggling one exclusion N times and closing leaves one row for that identity. `verify_link_exclusions` accepts the collapsed sequences. The current exclusion still suppresses or restores the same Review comparisons.

### 5. Opening and currency history never expires

`correct_opening` inserts account-operation kind 4 (`src/storage/account_lifecycle.inc`, around lines 105–123). `reassign_currency` inserts kind 5 and a ledger row cast to kind 5 (around lines 181 and 307), plus one kind-4 amount row per transfer endpoint. `account_operations_keep` aborts every delete (`src/storage/import_store.cpp`, around line 633). `prune_session` always retains ledger kinds 0 and 5 (around line 83) and deletes `account_amount_revisions` only for kinds 0–3 (around line 75), so kind-4 conversion rows stay.

Opening corrections and renames do not change transactions. The current name, opening, and currency already live on `account`. Kind 6 is the in-session undo of kind 4 or 5. After close, the user-facing undo list is gone with `session_history`. Decide explicitly whether the chain is still a product record. The session-history contract treats ordinary edits as expired and treats the current opening as retained state. Retirement and creation stay either way.

Prove, if the chain is expired: N opening corrections and closes leave the live account row and no superseded kind-4 operations. A second currency change does not keep the previous kind-5 ledger row or kind-4 amount rows. `verify_account_commands` accepts the file, and the displayed opening and currency match the last command.

### 6. Balance-choice toggles are kept

`balance_history` (`src/storage/import_store.cpp`, around line 390) inserts an `import_balance_events` row on every update of the balance columns. `review_import_balance` updates those columns once per confirmed choice. The current values already live on `import_group_accounts`. Deletes of these events happen for incomplete capture and whole-import rollback. Close does not collapse them. `balance_events_keep` allows a delete only while cleanup is enabled.

`verify_import_groups` also reconstructs each historical choice from `versions` at that event’s `balance_revision` (around line 468). A later edit close may already have pruned intermediate draft versions. An intermediate event that points at a pruned revision can fail Check file, which is both a storage and a verification defect.

Prove: changing the reported-balance choice N times, accepting, and closing leaves one event per source account, and that event matches `import_group_accounts`. Check file passes after a later draft edit has pruned intermediate `versions`. Import Undo still restores the applied opening.

## How to measure

Use the synthetic driver pattern in `tests/storage/session_storage_measure.cpp` and the row-count checks in `tests/storage/committed_tests.cpp` (`session_pruning`, `session_linked_pruning`). For each work item, record physical file bytes, freelist pages, and row counts of the named tables after 1, 8, and 32 repetitions, each repetition ending in a successful close. Compare a second close of the unchanged file so the early return in `prune_session` (no pending draft, entry, or ledger change) is visible.

Do not use a private statement. Do not treat a smaller file produced by vacuum or padding as meeting the criterion.

## Out of scope

- Reintroducing permanent per-revision transaction facts or historical monetary replay.
- Changing the compact `command_completions` receipt.
- Routine full-file rewrite or vacuum at quit.
- Help, reports presentation, and parser behavior.
