# Manual update backlog

Last updated: 2026-11-18

The bundled manual includes MAN-001 through MAN-056 after the owner-authorized
2026-11-14 pass. Both entries previously labelled MAN-045 are covered; the
older History instructions were reconciled with the later linear Undo/Redo
contract. Verification is recorded in [status](status.md#manual).

Two owner-authorized passes on 2026-11-18 completed MAN-057 through MAN-076
and MAN-077 through MAN-087. See [Manual](status.md#manual) for verification.

Owner direction: accumulate the next manual changes here while application work
continues, then update and verify the manual in a later owner-authorized pass.
Logging an item does not authorize editing `manual/` or recapturing pictures.

## How to maintain this log

- After a user-visible change, add an entry with its date, the contract, the
  affected chapters and pictures, and the specific work remaining.
- Distinguish text already updated from outstanding text, screenshots, captions,
  examples, and cross-references. Merge overlapping work so pictures are
  recaptured once against the final UI.
- Keep stable entry IDs. The next entry is MAN-094. Remove an entry from this
  log once the affected content and pictures are in bundled Help.
- Follow [feature 013](../features/013-help-system.md). Use synthetic examples.
  Preserve topic and section identifiers. Never use private records.

## Open items

- **MAN-088 — New transfer date corrections, 2026-11-18.** Update the
  transaction editing and transfer sections to show the Date field, **Edit date
  on original transaction**, bank-match protection, and one-step Undo. Cover
  when a generated counterpart follows the date and when each posting keeps
  its own date. Refresh any affected transfer editor pictures and captions.
  [Ledger editing](ledger-editing.md#correcting-the-date-of-a-new-transfer).

- **MAN-089 — Undo and Redo adding a transaction across accounts, 2026-11-18.**
  In Add and correct transactions, include **Add transaction** as an example of
  an action you can undo and redo from Change history while viewing a different
  account. Include the sequence of undoing a transfer assignment and then
  undoing **Add transaction**. The existing pictures still apply.
  [Change history](change-history-handoff.md).

- **MAN-090 — Edit New manual transfers, 2026-11-18.** Update the transfer
  editor coverage from MAN-088 to show the full original editor,
  **Edit original transaction**, the link-break confirmation, and an amount sign
  correction that refreshes Review. Include the effects on generated and
  independent postings and one-step Undo. Refresh the affected transfer editor
  and Review pictures together.
  [Ledger editing](ledger-editing.md#editing-a-new-manual-transfer).

- **MAN-091 — Undo grouped imports after acceptance, 2026-11-18.** In Import
  transactions, mention that after undoing acceptance, Undo can remove the
  grouped import and clear transfer links between its own batches in one step.
  The existing pictures still apply. [Import Undo](import-undo.md).

- **MAN-092 — Review after a transfer Link is broken, 2026-11-18.** In Review
  duplicates and transfers, explain that a matching pair can reappear after an
  edit breaks its Link. **Not this transfer** continues to hide that pair until
  the decision is undone. The existing pictures still apply.
  [Review](review-sheet.md#one-question-per-transfer--owner-go-2026-11-12-and-2026-11-18).

- **MAN-093 — Redo verification message, 2026-11-18.** In Change history,
  explain that if SpentIt cannot verify the file’s rule history during Redo,
  the Undo remains in effect. [Change history](change-history-handoff.md).

## Recapturing pictures

An owner-authorized pass that changes pictures uses
[tools/capture_manual_images.py](../tools/capture_manual_images.py). Do not
write captures straight into `manual/images`. Do not point
`SPENTIT_GUIDE_CAPTURE` at `manual/images`.

1. Rebuild the help UI test so the pictures show the current interface:
   `cmake --build build/import-ui-rules-release --target spentit_help_ui_tests -j 2`
2. Run `python3 tools/capture_manual_images.py`. It runs the eleven chapter
   walkthroughs in `tests/ui/help_ui_tests.cpp`, writes pictures to a staging
   directory, and fails if a picture linked from `manual/` is missing or
   unexpected. It also refuses a test binary older than `src/ui` QML or the
   walkthrough source.
3. If a walkthrough fails, update that walkthrough so it still reaches the
   pictured screen, rebuild, and rerun. Leave the other chapters alone.
4. Look at the staged pictures. Update the chapter text, captions, and alt
   text where the screen or the example changed. The script does not edit
   those words.
5. After the staging run has passed and the pictures are the ones to publish,
   run `python3 tools/capture_manual_images.py --install`. That copies the
   linked pictures into `manual/images`.
6. Check the changed topics in bundled Help before removing the backlog entry.
