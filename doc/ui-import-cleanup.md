# Opening, ledger cleanup and interrupted imports

Status: **accepted requirements; implementation pending**  
Last updated: 2026-10-31

## Priority and authorization

This is the owner's next work, ahead of the previously listed handoff work in
[the roadmap](roadmap.md). The owner approved the decisions below and requested
that they be documented and committed. This checkpoint changes documentation
only; implementation still requires explicit Go. Reports and unrelated work
remain deferred. Do not resume the older sequence automatically.

## Opening presentation

While the dataset opens, show a centered vertical group: the bundled
`assets/images/SpentIt_light_1024.png` logo, some space, “Where did my money go?”,
then Cancel. Put opening status/progress in the bottom status line. The light
1024px image already belongs to the application resources; no new artwork is
needed. Preserve truthful cancellation availability and existing opening safety.

## Ledger and navigation

Below Hide Sidebar / forward / back / account navigation, leave only Search,
Apply and Clear above the ledger. Remove all other controls and labels in that
area, including dataset path, balance, shortcut hints, import/record selectors,
record-number spinner, Review record, Source file, import status, selected count,
Mode: Projected, delete/category toolbar and Commit/Discard. Inline transaction
inspection and existing ledger context actions remain. Move operational feedback
to the permanent bottom status line; immediately after import show its added,
already-present and failed counts there. Persistent problems remain in Review.

- Put one Import button immediately above Review in the sidebar, and Import in
  each account's context menu.
- Sidebar import: one unambiguously identified existing account is the destination.
  With no account identity, ask for the destination using an account dropdown and
  OK/Cancel. A named but unknown account opens the existing scrolling account
  creation/mapping dialog. Multiple source accounts always open that dialog.
- Account-context import: use that account as the destination when the file has
  no account identity or identifies that account. A different identified account,
  or multiple source accounts, opens the existing creation/mapping dialog.
  Preserve account identity, currency and mapping validation; never guess an
  ambiguous identity. Keep existing new-account opening choices.
- Backup/restore controls belong in Settings, not the ledger toolbar.
- Window title: `SpentIt — ~/Documents/spentit.spentit` for that example dataset.
  Abbreviate paths under the user's home with `~/`; use absolute paths elsewhere.
- Change category in the ledger context menu opens a separate category-list
  popup/dialog. Remove the toolbar category selector. Preserve current assignment
  validation, bulk preview/atomicity and transfer restrictions.
- Commit provisionally belongs in Review, to the left of Merge selected matches
  and Clear selection. Preserve dataset-wide acceptance and quit prompting.
- Discard uncommitted work belongs in the account context menu, not in Review as
  a global button. This remains account-owned rollback with explicitly described
  cross-account effects. It must not acquire dataset-wide semantics.

## Review and source visibility

Remove the opaque import-ID/record-number navigation. Normal rows already open
inline. Failed records without a ledger row still need actionable Review entries.
Review owns outstanding import failures, supported repairs, repair progress and
commit blockers, including mapped-transfer resolution. Live processing progress
belongs in the status line. Do not simply hide the only route to an unresolved
problem. Completed imports with malformed records retain their successful rows
and expose failures in Review; they are not interrupted imports.

Remove the whole-file display button. Storage currently retains both complete
source bytes and per-record snippets; full sources also support parsing and
verification. The owner explicitly agreed to retain this representation. This
presentation change does not authorize deleting retained source bytes or changing
inline original-record inspection.

## Cancel and interrupted-import cleanup

Replace user-facing Resume with discard-and-reimport for incomplete preparation.
An incomplete import is different from a completed import awaiting Commit.
Completed uncommitted imports must survive restart unchanged.

- Cancel during preparation atomically removes material owned exclusively by
  **this import across all accounts**, including generated counterparts. It does
  not discard earlier uncommitted imports or accepted transactions. Preserve
  pre-existing matched/linked transactions and material required by other owners;
  undo only this import's contributions and relationships.
- Remove accounts and categories created exclusively by that incomplete import
  when no retained work requires them. Preserve existing accounts, their opening
  values, categories and all previously retained work.
- After opening/authenticating/verifying the dataset, detect durably incomplete
  preparation and perform the same cleanup before allowing further changes.
  Use durable import identity/stage, not a blanket unclean-shutdown flag or the
  presence of uncommitted rows. Startup approval reuse must not bypass this gate.
- A durable completion marker decides whether preparation finished. If completion
  was saved just before a crash, retain the import even if success was never shown.
  If Cancel arrives after completion, report completion, not successful cancellation.
- Cleanup is atomic and safely retryable. If shutdown interrupts cleanup, it stays
  pending and is retried at next opening. If cleanup fails, report the failure and
  prevent further changes until resolved. Never announce removal before durable
  completion; never silently repair unrelated corruption.
- After successful startup cleanup, explain: “The previous shutdown interrupted
  an import. That incomplete import has been removed. You can import the file again.”
- Original external files are untouched. Reimport is an explicit user action.

The current account-wide Discard is **not** an implementation substitute: it can
remove earlier pending imports. Implement an import-specific ownership boundary
and cleanup command. Existing Resume/captured-source behavior remains the current
implementation until this replacement is implemented and verified. This accepted
scope supersedes the older preservation of import-created accounts/categories
only for cleanup of the interrupted import described here.

## Implementation and verification obligations

Inspect single-account and mapped multi-account capture/preparation paths, source
ownership, generated transfers, account/category creation and durable outcomes
before choosing the storage design. Keep worker-owned transactions and exact
financial invariants. Experimental formats have no migration obligation; this
checkpoint changes no schema and chooses no new schema number.

Use synthetic imports covering existing and new accounts/categories, multiple
prior pending imports, shared source ownership, existing matched postings,
generated counterparts, malformed rows and duplicate-only completion. Verify
exact balances, retained history/relationships and reimport after cleanup.
Exercise cancellation and process exit before/after capture, preparation completion
and cleanup commit, lost acknowledgements, disk/I/O failures, cleanup retry and
restart with completed-but-uncommitted work. Verify no partial cleanup or false
success. Check startup approval interaction and failure gating.

Verify opening layout, title paths inside/outside home, every import-routing case,
category dialog, Review repair access, account-context discard scope and settings
backup/restore access. Keep the ledger search-only and status layout stable.
Record actual checks and platform limits in [status](status.md); documentation
review is not runtime verification.
