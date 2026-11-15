# Manual update backlog

Last updated: 2026-11-15

The bundled manual includes MAN-001 through MAN-056 after the owner-authorized
2026-11-14 pass. Both entries previously labelled MAN-045 are covered; the
older History instructions were reconciled with the later linear Undo/Redo
contract. Verification is recorded in [status](status.md#manual).

Owner direction: accumulate the next manual changes here while application work
continues, then update and verify the manual in a later owner-authorized pass.
Logging an item does not authorize editing `manual/` or recapturing pictures.

## How to maintain this log

- After a user-visible change, add an entry with its date, the contract, the
  affected chapters and pictures, and the specific work remaining.
- Distinguish text already updated from outstanding text, screenshots, captions,
  examples, and cross-references. Merge overlapping work so pictures are
  recaptured once against the final UI.
- Keep stable entry IDs. The next entry is MAN-059. Remove an entry from this
  log once the affected content and pictures are in bundled Help.
- Follow [feature 013](../features/013-help-system.md). Use synthetic examples.
  Preserve topic and section identifiers. Never use private records.

## Open items

- MAN-057 (2026-11-15) — Password length. Contract:
  [001](../features/001-documents-storage-security.md). Chapters:
  [Create or open a file](../manual/create-open-file.md) (Choose password
  protection), [Backup and recovery](../manual/backup-recovery.md) (Change or
  remove the password), and [Messages and problems](../manual/messages-problems.md).
  A password is 1 to 64 characters. An empty password says “Enter a password.”
  A longer password says “Use a password of 1 to 64 characters.” Help for that
  sentence says the password is stored as UTF-8 text. No picture change.
- MAN-058 (2026-11-15) — Open a `.spentit` file from Finder. Contract:
  [007](../features/007-licensing-and-distribution.md). Chapter:
  [Create or open a file](../manual/create-open-file.md) (Open your file).
  Include macOS with the Windows and Linux sentence. Double-clicking a
  `.spentit` file starts SpentIt and opens it. A password-protected file stays
  on the welcome screen with that file selected and asks for the password.
  The file opened last time stays closed until this one opens. If SpentIt
  already has a file open, the window comes forward and that file stays open.
  No picture change. `create-file-unlock.png` already shows the password field.

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
