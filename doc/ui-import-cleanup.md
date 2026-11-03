# Opening, ledger cleanup and interrupted imports

Settings is available only in the lower-left sidebar; the duplicate upper-right
button was removed at the owner's request on 2026-10-31. Backup/restore remains in
Settings. Show the sidebar first if it is hidden.

Status: **implemented under owner Go; verification recorded in [status](status.md)**  
Last updated: 2026-11-03

A prepared import waiting for the [missing-category decision](qif-account-imports.md)
is complete preparation, not an interrupted import. Closing that dialog or the
application retains its rows and choices for reopening; startup cleanup must not
remove it. The decision and automatic rules are one atomic operation.

## Priority and authorization

This is the owner's next work, ahead of the previously listed handoff work in
[the roadmap](roadmap.md). The owner approved the decisions below and requested
that they be documented and committed. The owner subsequently gave implementation Go on 2026-10-31. Reports and unrelated work
remain deferred. Do not resume the older sequence automatically.

## Opening presentation

While the dataset opens, show a centered vertical group: the bundled
`assets/images/SpentIt_light_1024.png` logo, some space, “Where did my money go?”,
then Cancel. Put opening status/progress in the bottom status line. The light
1024px image already belongs to the application resources; no new artwork is
needed. Preserve truthful cancellation availability and existing opening safety.

## Feedback placement — owner Go 2026-10-31

Routine operation errors and success messages use the permanent bottom status
line. A modal dialog that displays the operation error owns it while open: suppress
the duplicate bottom message and Details button, then restore them on dismissal
if the error still exists. Only one dialog displays the global error at a time.
The user-requested Details popup follows the same rule. Welcome errors remain on
the welcome screen when no workspace is active.

Keep substantive previews, history, record diagnostics, transfer candidate guidance
and balance-impact details in their views. Rules Save/Apply/reapply results and
transfer completion feedback use the status line. The dedicated closing-verification
failure dialog retains its recovery explanation and explicit choices. This changes
presentation only, not validation, persistence or the dataset format.

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

### Duplicate decisions belong in Review — owner Go 2026-11-02

The ledger transaction editor no longer shows the import disposition row
(Include as new, Include as separate transaction, matching candidate and Already
imported). A visible included transaction already counts independently; this is
not a command for reversing Merge. Review Undo retains that responsibility.

Review inspection retains explicit import decisions for unresolved records,
including reimports of previously removed transactions (Keep removed or Include
as separate transaction). Correct and import-blocker record navigation open the
inspector inside Review, including records requiring repair. The existing saved
decision commands and financial behavior are unchanged.

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
and cleanup command. The desktop no longer exposes Resume; opening removes durably captured preparation before the workspace becomes writable. This accepted
scope supersedes the older preservation of import-created accounts/categories
only for cleanup of the interrupted import described here.

## Implementation and verification obligations

Inspect single-account and mapped multi-account capture/preparation paths, source
ownership, generated transfers, account/category creation and durable outcomes
before choosing the storage design. Keep worker-owned transactions and exact
financial invariants. Experimental formats have no migration obligation. The original requirements
checkpoint chose no schema; the implementation below uses 40/39.

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

## Implemented storage boundary — 2026-10-31

Experimental formats are now **40 with rules / 39 without**. The format change
allows deletion of account-creation receipts only inside the guarded cleanup
command; older experimental formats are rejected unchanged, without migration.

Capture saves source bytes, mappings and creation-time opening choices. Preparation
publishes all rows, source associations, categories and generated transfer effects
in one transaction, together with the batch materialization/group completion
marker. A durably captured import therefore has no partially published transaction
relationships to reverse. Cleanup rejects inconsistent stages, removes only the
captured source roots and their capture history, and removes exclusively created
accounts unless retained work or account changes require them. Source categories
have not yet been published at this boundary. Completed imports, including failed
records and duplicate-only completion, are outside cleanup.

Cleanup runs after authentication and verification even when startup approval is
reused. Its deletes and full retained-state verification share one transaction;
failed cleanup prevents desktop activation or closes the worker connection.
Missing source roots make retries harmless, and durable absence resolves a lost
cleanup acknowledgement. External files are never modified. Account-context
Discard continues to use the existing account-owned rollback command.

The worker resolves sidebar/account-context routing before capture. Unnamed
sources get an explicit destination prompt for sidebar import; known identities
can route directly; unknown/mismatched/multiple source accounts use mapping.
OFX-family routing also checks prior confirmed bank identity mappings without
adding bank identifiers to transfer-name aliases. The status summary totals all
mapped source accounts. Full-file source storage and inline record inspection
remain unchanged.

## Completed import undo — owner Go 2026-11-03

[Undo import](import-undo.md) adds source-scoped reversal after preparation has
completed, through the account context menu and Review. It is separate from both
account-wide Discard and the incomplete-preparation cleanup above. Existing
formats remain readable; import labels use type, dates, count and order because
original filenames were not persisted.
