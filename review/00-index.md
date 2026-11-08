# RC1 review index — 2026-11-07

Read-only pass over the current tree. No application code, tests, or product documents were changed. Current formats remain **64 with rules / 63 without**.

Four bounded reads covered durability, storage growth, money and relationships, and import fidelity. The coordinator then read the cited functions before promoting a finding to a brief. A later session should start here and open only the briefs below. The owner subsequently authorized the OFX fixes, now implemented; see [status](../doc/status.md#ofx-statement-validation-and-incomplete-storage-follow-up--2026-11-07). Storage retention remains an incomplete step deferred for a separate owner discussion. The observations below describe the original read-only review.

## Investigation follow-up — 2026-11-08

[Measured results](03-retention-investigation-results.md): a reproducible legacy balance-history verification defect, ruleset-size-dependent storage waste, and smaller retention gaps. Current desktop fixed-opening mapping passed. Investigation is complete for the documented workloads; storage remediation is still unimplemented and awaits an owner decision.

## Briefs

1. [Session-close storage growth](01-session-close-storage-growth.md). Six append-only histories survive successful close. Rule text and category edits are the largest. Check file currently replays the chains, so a delete by itself will fail verification.
2. [OFX statement header fields](02-ofx-statement-header-fields.md). An unparseable ledger balance is presented as “Not supplied”, and a repeated account id, status, or balance silently replaces the earlier value.

## Closed in this pass

Durable close, backup, and incomplete-import cleanup match the session-history contract on the paths that were read.

- `ImportStore::certify` publishes prune, session-history removal, and `clean=1` in one commit. A throw before commit leaves the previous session. `Atomic::commit` clears its rollback flag after `COMMIT` returns.
- `ImportStore::connect` runs full verification when the marker is unclean or a journal, WAL, or shared-memory file is present. A clean marker with no journals skips that audit. That reduced offline-change detection is the accepted policy.
- `cleanup_incomplete_imports` deletes sources that still have a captured (`state=3`) batch, and it refuses a captured batch that already has materialized rows. Prepared and completed imports stay.
- `backup_to` verifies the copy, links it into place, syncs the directory, then removes the partial name. The existing fault tests describe crashes on both sides of that link. The Windows substitutes are `publish_link` (`CreateHardLinkW`), `truncate_owned_regular`, and `sync_parent` in `src/storage/windows_files.hpp`. Those functions were not executed here. Running the existing backup fault tests on Windows is the remaining check; it is already recorded as unverified in `doc/status.md`.

Money and transfer relationships match the invariants that were traced through save.

- `Transaction::Transaction` rejects a split sum other than the effective total. `SplitEditor::recalculate` fills the designated remainder with `exact_sum`. Overflow and a dropped fraction throw.
- `ImportModel::chooseSplitRemainder` (`src/ui/import_model.cpp`, around line 2646) skips transfer splits, prefers the total’s sign, and keeps the largest magnitude. `canonical_draft` keeps an existing remainder identity and the manual-amount lock.
- Reports keep one result per currency. Transfer rows are omitted from both the total and the drill-down. `tests/engine/report_tests.cpp` compares each cell with the negation of its entries.
- `plan_deletion` (`src/storage/ledger_selection_store.inc`) removes a real counterpart only when the request sets `delete_counterparts`. Otherwise it clears that allocation’s category. A generated local draft on the other side is the disposable counterpart.
- Ordinary duplicate suggestions require the same known calendar date. A repeated incoming bank id stays unresolved. Both sides having ids do not fall through to the payee key.

Reimport of a removed transaction stays unresolved.

- `review_targets` (`src/storage/import_store.cpp`, around line 595) exposes `entry_revisions.active` for a committed row. Committed removal sets that flag false.
- `automatic_target` and `exact_target` return no match when `review_target_active` is false.
- `duplicate_candidates` still sees the removed row: `candidate_where` keeps decision `include`, and `excluded_by_clear` is about cleared import rows and pending ledger deletion, so a committed removal is still a candidate. `populate` then leaves the new row unresolved instead of including or auto-matching it.

These measured or structurally capped tables are inside the retention boundary. The comparison workloads in `doc/session-storage-comparison.md` do not grow across repeated payee edits, and they do not exercise the six histories in the storage brief.

- `command_completions`: one id, domain, and 32-byte digest per completed command.
- Current entry and draft heads, revision 0, acceptance drafts, and duplicate-review anchors.
- Ordinary ledger operation kinds 1–3, bulk operations, and rule applications, which close removes.
- `session_history` and `review_stack`, which close clears.
- Live transfer versions reduced to the head, one prior restoration input, and generated-match versions.
- One current `review_merges` row per merged transaction, one original per retained import, and one final import-category choice per name.

## Left in place

Help, the manual, icons, packaging, and licensing were outside this pass. The seven-day transfer-link window is the accepted Review policy and was not reopened. `generate_explicit_transfers` was not reread; the transfer engine’s generated-counterpart checks were.
