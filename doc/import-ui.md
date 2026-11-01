# SpentIt application UI

Current ledger behaviour (owner Go, 2026-11-01) is described in
[ledger editing](ledger-editing.md). It supersedes Save draft / Cancel edit and
explicit committed-edit activation below: Enter accepts, Escape discards pending
text, double-click toggles, and accepted edits have session Undo.


Settings is available only in the lower-left sidebar; the duplicate upper-right
button was removed at the owner's request on 2026-10-31. Backup/restore remains in
Settings. Show the sidebar first if it is hidden.

The [opening/ledger cleanup and interrupted-import rollback](ui-import-cleanup.md)
is implemented under the owner's 2026-10-31 Go on experimental formats **40/39**.
It adds import routing, Review repair/Commit, account-context Discard and automatic
cleanup of incomplete preparation. Completed uncommitted imports remain durable.
See [current verification and limits](status.md); older Resume and toolbar
descriptions below are historical.

The [dataset-wide acceptance model](import-commit-discussion.md) is implemented. Earlier per-account Commit and permanent-evidence descriptions are historical. Further implementation requires its own owner Go; reports remain deferred.

Last updated: 2026-10-31

The `SpentIt` executable (internal CMake target `spentit_import_experiment`) is the application used for document setup, imports, account management, transaction editing, categories and rules. Feature 009 remains **active** because its wider scope is incomplete. The historical verification sections below use synthetic fixtures; that describes the tests, not a restriction on application use.

Current follow-up: [account creation and guarded switching](accounts.md) now provide independent account workspaces. [Combined search, stable multiselection and atomic committed bulk actions](search-ledger.md) now run on the import worker, alongside categories and the rules sheet. Enable `SPENTIT_RULES=ON` for current schema-40 creation and bulk actions (schema 39 without rules); [current build and verification instructions](building.md#search-and-bulk-follow-up--2026-10-26) include the pinned PCRE2 dependency, also required for rules-off search. No pre-release migration/upgrade work is required. Historical test counts and walkthroughs below describe earlier checkpoints; [status](status.md) records current verification and native-window limits.

## Build and run

The normal local Release launch path is now `./build/SpentIt`; see [the application launch instructions](../README.md#run-the-application). `SPENTIT_APPLICATION_OUTPUT_DIRECTORY` selects its output directory. The default is the individual CMake build directory, keeping Debug and sanitizer builds separate.

Prepare the pinned dependency as described in [storage setup](storage-prototype.md), then configure:

```sh
cmake -S . -B build/import-ui-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/opt/homebrew \
  -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3 \
  -DSPENTIT_STORAGE_PROTOTYPE=ON -DSPENTIT_IMPORT_WORKFLOW=ON \
  -DSPENTIT_IMPORT_UI=ON \
  -DSPENTIT_SQLCIPHER_SOURCE="$PWD/build/dependencies/sqlcipher-build"
cmake --build build/import-ui-debug
ctest --test-dir build/import-ui-debug --output-on-failure -j 3
build/import-ui-debug/SpentIt
```

The Qt/OpenSSL prefixes above are macOS Homebrew examples; use the installed dependency locations on your machine. The synthetic sheet executable remains separate. The import UI requires Qt 6.8 or newer; this implementation was exercised with Qt 6.12.0. No Qt dependency was added to the engine or storage contracts.

## Welcome and new datasets

The welcome page now follows [the accepted flow](../features/001-documents-storage-security.md#welcome-and-creation--accepted-2026-10-28). It probes the selected path on the serial worker without activating the ledger or saving that path. At startup, successful public-key verification of the remembered dataset triggers the existing verified open command and skips the welcome screen. Other probes enable Open Dataset; an authentication failure offers a password without showing an error until an unlock is attempted. File → Close Dataset uses the unsaved-edit guard and returns to the welcome screen without automatically reopening. Missing paths and known format/integrity errors are shown below the controls. A successful explicit open still saves the startup path.

New Dataset defaults to Documents/`spentit.spentit`, account SpentIt, CAD and password protection on. Creation normalizes the suffix and asks Cancel/Overwrite for an existing destination. Confirmed overwrite is deliberately destructive at creation time, under the document lock; failure can leave an incomplete new file, which another explicit overwrite can replace. This differs from the unchanged backup/restore operation, which still publishes only to a new destination. Menus and scrollable panes now have persistent draggable scrollbars when needed.

## Owner walkthrough

1. Choose **New dataset**, a new file outside the checkout, an account name and CAD. The suggested location is `spentit.spentit` in the platform Documents folder. Password protection starts checked and requires matching entries. Unchecking it displays the explicit confidentiality waiver. Existing files are replaced only after the creation-time Overwrite confirmation.
2. Set Opening Balance to `100.00`, date `2026-08-01`, and confirm it. Import the [authored synthetic fixture](../tests/fixtures/qif/durable-review-synthetic.qif).
3. Committed balance should remain `100.00`, with projected balance `120.69`. The first source date is 10 August. Inspect record 1, including the original `-29.31` and its two allocations.
4. Change the total to `-30.00` and the second allocation to `-29.86`. Save draft. The projection becomes `120.00`; original evidence remains unchanged. Trying only the total change fails the exact split-sum validation.
5. Close through **Settings / close dataset**, then reopen. Accepted draft edits remain. Leaving an unsaved edit offers Save / Discard / Stay; Discard here affects only the editor buffer.
6. Commit the whole reviewed batch. Its receipt shows change `20.00`, resulting balance `120.00`, two included records and no exclusions. Opening balance is now frozen. Reopen to check the committed ledger and reconstructed receipt.
7. Reimport the same file. Both occurrences are visibly excluded against their prior occurrences; committing this reviewed batch contributes zero. An explicit Include as new override is available. Other possible matches require a chosen prior occurrence or explicit inclusion.
8. Save a backup to a new destination. To restore into a separate working document, open that backup, use **Backup / restore copy** to publish another new file, close it, then open the restored file. Backup preserves the current protection mode and does not replace destinations. Password-protected backups need their original password.

The ledger supports explicit committed/projected modes, combined literal/phrase/regex/date/original-QIF search, six-column sorting, and filtered signed-withdrawal/deposit totals. Chronological balances come from the application ledger calculation and stay attached to their rows. Drafts, duplicate dispositions and unavailable projections are labeled explicitly. Committed inspection is read-only until an explicit committed-edit action; see [committed changes](committed-changes.md). Double-click a contributing ledger row to expand its editor inline, or select an import/record number to inspect it; excluded occurrences remain reachable through the import selector.

Blocked imports retain evidence and display diagnostics. Whole-file or record source previews are read-only, capped at 64 KiB with an explicit byte-count notice; non-UTF-8/NUL-containing previews use hex rather than replacement decoding. The complete bounded source remains in encrypted storage. New schema-2 datasets support explicit date and split-total repair as described below; other errors still require discard/reimport, and blocked batches cannot partially commit. Discarding a batch requires confirmation and retains its evidence/history.

## Execution and failure boundaries

`DocumentGateway` is a plain application boundary. Executable wiring supplies the concrete SQLCipher adapter. The Qt model submits complete repository operations; QML performs neither SQL nor authoritative money calculations. A private worker pool with one thread owns the active gateway. Only one command can be in flight; controls disable switching, closing and additional submissions while it runs. Results are delivered before another command can start, so a late completion cannot populate another dataset. Cooperative cancellation for opening, imports, queries, rules and backups is described in the follow-up sections below. Durable mutations remain non-cancellable after their operation gate closes. Closing waits for work rather than implying rollback.

A successful open is fully validated before the dataset becomes active and its path is saved using QSettings. Settings contains only the dataset path, never the password/key. Failed settings persistence is reported separately from the already-open dataset. Closing clears the model, selected source, receipt and editor fields, and releases the document lock. Parent-directory aliases such as macOS `/var` are resolved; final-file symlinks and multiple hard links remain rejected by storage.

Missing/inaccessible paths and document-lock failures do not invoke a password retry screen. Automatic public-key authentication is tried first. Authentication failure offers password/Browse and says unable to unlock, since damaged encrypted input can look like a wrong key. Authenticated schema, integrity, financial-validation and I/O errors remain errors. No failed open replaces or repairs a file. An empty lock sidecar can remain, as documented in [storage behavior](durable-import.md#storage-and-locking).

Commit IDs are derived from batch identity and expected revision. Receipts are looked up again after restart, covering a prior commit that became durable before its UI acknowledgment. Save/import/discard/commit still use the existing atomic command contracts. A refresh failure closes the session and asks for reopening; it does not acknowledge a partial view as complete. Unaccepted keystrokes are not promised crash recovery.

## Portable credential convention v1

The UI uses the existing experimental application ID `1397770577`, schema version `7` for new rules-enabled datasets and `5` for rules-off datasets, and SQLCipher 4 settings from [the headless report](durable-import.md). It adds a versioned credential convention, without rewriting existing documents:

- No-password mode supplies this exact ASCII raw-key specification to `sqlite3_key`:

  ```text
  x'5370656e744974207075626c696320666f726d6174206b6579207631000000000000'
  ```

  This is a **public 32-byte key**, not a secret, installation token or confidentiality mechanism. SQLCipher retains its per-file salt and integrity machinery. Anyone who has a no-password document or backup can open it.
- Password mode supplies the UTF-8 bytes of `SpentIt/password/v1:` followed immediately by the unmodified password to `sqlite3_key`. SQLCipher's existing PBKDF2 derives the encryption key; there is no new derivation algorithm. Passwords contain 1–1000 UTF-8 bytes without NUL and are never trimmed or normalized. The prefix prevents SQLCipher from interpreting ordinary password input as raw-key syntax, including a password equal to the public specification above.
- Keep both constants compatible in future versions. Protection changes are not implemented. There is no forgotten-password recovery.
- The older direct `ImportStore` test API still accepts its original, unprefixed SQLCipher credential. Those headless test files have not been migrated. Open them with that API and their original credentials; this UI does not guess a legacy credential convention. The older storage-prototype format is also not migrated.

For installation-independent inspection of a no-password **backup copy**, use a compatible SQLCipher 4 tool. The public key is sufficient; original Settings, keychain and application installation are unnecessary. Open the copy read-only where supported, then run these statements before reading application tables:

```sql
PRAGMA key = "x'5370656e744974207075626c696320666f726d6174206b6579207631000000000000'";
PRAGMA cipher_compatibility = 4;
SELECT count(*) FROM sqlite_master;
PRAGMA application_id;
PRAGMA user_version;
PRAGMA cipher_integrity_check;
PRAGMA integrity_check;
PRAGMA foreign_key_check;
```

Expect application ID `1397770577`, schema `40` for a current rules-enabled dataset or `39` for a current rules-off dataset (earlier experimental schemas are rejected unchanged), no cipher/foreign-key error rows, and `ok` from SQLite integrity checking. These SQL checks alone do not establish all financial invariants: use a compatible SpentIt build's full verification before editing, and retain the original backup. A protected document instead needs the password with the exact v1 prefix through the supported key API; do not put private passwords in shell history. This procedure cannot repair damaged/missing files or recover a forgotten password. Do not copy only the main file of a live database; use the application's validated backup operation.

## Verification and remaining work

Actual verification on 2026-10-25: macOS 27.0.1 arm64, AppleClang 21.0.0, Qt 6.12.0, SQLCipher 4.19.0 and OpenSSL 3.5.9. Debug, Release and ASan/UBSan each built and passed **33/33 CTest entries**. After final UI/test refinements, the affected UI and QML checks were rebuilt and passed again in all three configurations. The unchanged synthetic sheet's four Debug checks also passed. No compiler warnings or sanitizer diagnostics were emitted in the final builds/runs. All 89 relative file targets in the changed documentation exist; `git diff --check` passed.

The new UI test executable covers corrected split totals, rejected mismatches, saved draft restart, frozen publication/receipt restart, exact-file duplicate exclusions, backup restoration, password mismatch before file creation, wrong-password versus missing-file/lock errors, document isolation, blocked transfers, invalid-encoding hex evidence, and oversized-file rejection. The QML interaction test exercises field-to-editor binding, Save / Discard / Stay during window closing, failed-save retention and preservation of the last saved revision after discarded keystrokes. An offscreen screenshot of the populated review/editor surface was rendered and inspected. These checks are not a full native pointer/accessibility audit.

A restored no-password backup was opened through the SQLCipher C API using only the documented public key after destroying the original gateway and clearing Settings. That independent connection read the exact opening value. An authenticated schema-version alteration then produced a schema error without a password retry. A protected dataset whose user password literally equals the public raw-key specification rejected automatic opening and opened with that password through the protected convention. This demonstrates installation-independent credentials in a same-machine test, not cross-platform validation.

Tests and smoke launches use temporary synthetic files and isolated Settings; no private exports are accessed. Existing headless fault/crash/backup sweeps remain in the 33-entry suite. No new claim is made for physical power loss, all filesystems, or cancellation at every instruction boundary.

Remaining scope includes repair beyond dates/split-total mismatches, the full multi-account/report navigation shell, broader sheet layout and accessibility review, cancellation of other long operations, packaging, and Linux/Windows validation. The bounded account/Settings shell and cooperative opening/import cancellation are described below. Source previews are bounded rather than a full-file editor. The UI now reuses verified ledger data for read-only queries and builds presentation maps for requested rows. Native 100,000/200,000-row measurements and remaining long operations are documented in the [performance report](import-ui-performance.md); these bounded measurements do not establish general responsiveness. Password changes, legacy credential conversion and migrations remain outside this implementation.

## Walkthrough regression follow-up — 2026-10-25

The automated UI walkthrough now reads the same synthetic fixture linked above. It additionally checks the DMY interpretation, included/excluded receipt counts, rejection of opening-balance changes after commit, filtered withdrawal/deposit reconciliation with stable chronological balances, and backup-to-restored-copy publication followed by reopening and commit. The QML Save / Discard / Stay test was rerun and its rendered review surface inspected. This is automated/offscreen validation, not a native pointer or accessibility audit. Backup interruption behavior and recovery of retained files are documented in the [headless recovery report](durable-import.md#backup-interruption-recovery).

After the final walkthrough assertions were added, `import.ui` was rebuilt and passed again in Debug, Release and ASan/UBSan on the recorded macOS arm64 toolchain, without compiler warnings or sanitizer diagnostics. All 35 relative file targets in this report, project status and the headless report exist; `git diff --check` passed.

## Record repair

In a new dataset, inspect a malformed-date or split-total record. The editor displays the retained diagnostic, original amount and unchanged source view. Enter a valid ISO date and/or explicitly correct the splits/total so they reconcile exactly, then choose **Save repair**. No date or balancing amount is guessed. Invalid corrections remain unsaved. Saved repairs retain a visible original-diagnostic label.

After saving, explicitly select **Include as new** or a candidate to exclude. Saving a repair alone does not resolve duplicates or enable commit. A candidate with an unavailable original date is labeled “same original amount; source date unavailable.” Resolve every remaining record and duplicate decision before committing the whole batch. Save / Discard / Stay applies to the repair editor through the existing save command. Saved repairs survive reopening and backup restoration.

Older schema-1 datasets remain readable but cannot store repairs; the diagnostic explains that a new experimental dataset is needed. Missing/invalid amounts and unsupported constructs remain read-only and block commit. See the [storage contract](durable-import.md#bounded-record-repair).

For synthetic owner review, create a new CAD dataset with confirmed opening `100.00` dated `2026-01-01`, then import the [repair fixture](../tests/fixtures/qif/durable-repair-synthetic.qif) (choose DMY if prompted). Record 1 has an impossible date; explicitly enter `2026-02-28`, Save repair, then Include as new. Record 2 has total `-20.00` and split `-19.00`; explicitly change the split to `-20.00`, Save repair, then Include as new. Projection becomes `70.00`; committed balance remains `100.00` until explicit commit. Reopen before committing to inspect the retained repairs, diagnostics and source bytes.

Verification: the repair model/QML flow passed within the 36-entry Debug, Release and ASan/UBSan suites and was rerun after final refinements. The repair screen was rendered and inspected offscreen. The [headless report](durable-import.md#bounded-record-repair) records failure/recovery coverage and limits.

## Inline editing and source copying

The durable ledger now expands its selected transaction inline. There is one persistent editor outside the recycled delegates, visually placed beneath the matching row by batch/occurrence identity. Sorting retains that identity. A record absent from the displayed ledger (blocked, excluded, discarded, filtered out, or hidden by committed mode) remains accessible through the review panel below the ledger. Whole-file evidence also uses that panel. Committed records initially open read-only; the later [committed-change workflow](committed-changes.md) provides explicit editing. Other split records can now be expanded independently for read-only viewing; see below.

Enable **Show immutable QIF source** to see parsed source fields alongside the retained raw preview. Double-click a parsed value or activate **Copy to edit** to copy into the unsaved editor. Dates are ISO dates interpreted using the original import date order; money is formatted at the account currency scale. An unavailable original date has no copy action. Unsupported malformed records do not expose parsed copy fields. Payee, memo, total, and split category/memo/amount are supported; fields without an editable counterpart remain in the raw evidence view.

Each split copy labels both its source split number and current editor destination number. It copies only that field into that position, never inserts a missing allocation or guesses a mapping after removal. Copy is disabled when that destination no longer exists. Copying a total does not rebalance splits. Copies remain unsaved until the existing atomic save command accepts the complete date/money/allocation interpretation. Rejected saves retain the editor buffer, original evidence, and prior saved revision. Save / Discard / Stay still guards record changes and navigation.

Verification on 2026-10-25: the affected `import.ui` and `import.qml` CTest entries passed in Debug, Release and ASan/UBSan on the previously recorded macOS arm64 toolchain, after rebuilding each configuration. Commands were `cmake --build build/import-ui-<configuration> -j 3` and `ctest --test-dir build/import-ui-<configuration> -R 'import.(ui|qml)$' --output-on-failure`, using `debug`, `release`, and `sanitized`. No compiler warnings or sanitizer diagnostics were emitted. All 84 relative links in the six changed documents, including referenced heading anchors, validated; `git diff --check` passed. This change did not rerun the full 36-entry storage/engine suite.

New UI regressions cover inline placement, sorting/filter fallback, scrolling with recycled rows and unsaved edits, Qt-dispatched row double-clicks, source copies staying unsaved, rejection of mismatched totals, failed-save navigation retention, Discard navigation, reopen/evidence preservation, unavailable source dates and disabled committed-record copying. Existing repair and Save / Discard / Stay regressions also passed. The rendered inline source view was inspected offscreen. An initial new test used QObject ownership lookup for repeater-created controls; it was corrected to traverse the visual item tree before the passing runs. Native platform interaction/accessibility review and Linux/Windows validation remain unverified. Subsequent bounded large-workload measurements are recorded in the [performance report](import-ui-performance.md). No schema or financial-policy change was made.


## Read-only split viewers — 2026-10-25

The Category cell now provides a split-count button for transactions with multiple allocations. Several saved split views can remain expanded while one separate transaction is being edited. Each view shows category, memo and exact signed amount, with an explicit account currency and read-only label. The active editor continues to show its own edit buffer instead of a second saved preview.

Expansion state uses batch/occurrence identity outside recycled delegates. Sorting, filtering and scrolling retain it; closing or opening a dataset clears it. Previewing splits does not select a different transaction, save edits or change balances. Long split lists have a bounded-height, virtualized scroll area. Expansion is session-only. Explicit record inspection still brings the selected editor into view; ordinary scrolling does not follow the current editor automatically.

Verification on 2026-10-25: rebuilt Debug, Release and ASan/UBSan with `cmake --build build/import-ui-<configuration> -j 3`; the affected `import.ui` and `import.qml` entries passed in all three via `ctest --test-dir build/import-ui-<configuration> -R 'import.(ui|qml)$' --output-on-failure`. The final build/test logs contained no compiler, Qt or sanitizer diagnostics. The full 37-entry suite was not rerun for this presentation-only change.

The new regression checks multiple simultaneous previews, Qt-dispatched button clicks, mixed-sign amounts, unchanged balances and unsaved editor content, filtering/sorting, delegate recycling, collapsing and dataset isolation. The rendered two-preview layout was inspected offscreen. Initial test selection collided with the opening row's ordinal; test control names now exclude that row. An intermediate sanitizer run logged a Qt delegate cancellation warning; lazy creation/destruction of the nested preview list removed it in the final runs. Native accessibility and Linux/Windows checks remain unverified.


## Filter return navigation — 2026-10-25

Applying the first nonempty literal filter remembers the selected source occurrence, sort direction/column, ledger mode, and viewport anchor plus its offset. Further filter changes retain that original return point. Clear (or Escape in the filter field) restores the selection and view when no result was explicitly inspected. If a result was inspected, clearing retains that selected occurrence and brings it into view using the current sort/mode. Merely filtering or opening whole-file source evidence does not count as selecting a result.

Viewport restoration uses occurrence identity rather than a row number or absolute content offset, since virtualized layout can change its coordinate origin. If the saved viewport occurrence is no longer present, the view starts at the beginning; this does not settle future committed-deletion selection policy. Search return state is session-only and clears on dataset changes. The query still runs through the worker and uses cached authoritative ledger data. Clear is guarded by Save / Discard / Stay; a failed save retains the filter and edit buffer. This adds no date/regex/query grammar or multiselection behavior.

Verification on 2026-10-25: Debug, Release and ASan/UBSan builds succeeded; affected `import.ui` and `import.qml` CTest entries passed in all three, using `cmake --build build/import-ui-<configuration> -j 3` and `ctest --test-dir build/import-ui-<configuration> -R 'import.(ui|qml)$' --output-on-failure`. Final build/test logs had no compiler, Qt or sanitizer diagnostics. The full 37-entry suite was not rerun.

New regressions cover no-match restoration, stable viewport occurrence despite coordinate-origin changes, restoring sort after changes during search, source-file inspection not counting as result selection, selected-result visibility, invalid identity lookup, Stay, rejected Save, Discard, successful Save, and dataset isolation. Existing split/source/edit/navigation regressions also passed. An intermediate run logged a Qt delegate cancellation warning during a shrinking filtered-model reset; clearing the transient delegate index before reset removed it in the final runs, while preserving the workflow's occurrence identity. Native accessibility and Linux/Windows remain unverified.

## Coordinated navigation and operation recovery — 2026-10-25

The durable UI now has a resizable/hideable account sidebar, persistent navigation controls, and document-local Back/Forward history between the account ledger and active dataset Settings. Both pages stay alive so navigation preserves the ledger view. Navigation uses Save / Discard / Stay; a failed save leaves the editor and current page intact. Closing/opening a document resets navigation history. Settings exposes the current path, validated backup/copy and explicit closing before choosing another dataset. At this historical checkpoint, the one-account experiment did not expose account creation, category, rule or report commands; the current account/category/rule extensions are described above. Narrow windows can scroll the ledger horizontally; this is a bounded fallback rather than the final responsive product design.

Operation feedback identifies the current phase without inventing percentage completion. Opening is cooperatively cancellable between authentication/verification stages and validation units; a cancelled candidate releases its lock, never becomes active and does not replace the startup path. Individual SQL and parser calls are not interrupted by this first opening implementation. A late cancellation racing activation is checked before activation. Saved work remains intact.

Commit remains non-cancellable once dispatched. If its call throws, the worker queries the same deterministic operation identity: a verified receipt is shown as committed; a verified absent outcome leaves the saved draft available; an unreadable outcome closes the session and requires reopening to resolve durable state before retry. No fresh operation identity is substituted for an uncertain attempt.

The separate `import.navigation` regression covers sidebar hiding/restoration, navigation history, selected occurrence retention, Stay, Discard, failed Save, successful durable Save, and navigation reset across datasets. Additional UI regressions cover opening cancellation checkpoints/lock release, late activation cancellation, preservation of an existing saved draft, and before/after/unknown commit acknowledgement failures. All 40 registered tests passed in Debug, Release and ASan/UBSan; the affected tests passed again after final refinements as recorded in project status. Native macOS recovery checks passed 4/4 with the rendered incomplete-import screen inspected.


## Committed corrections, removal and Undo — 2026-10-25

New schema-4 datasets support explicit committed editing with exact before/after previews, removal that preserves evidence/history, and strict Undo that explains dependency conflicts. The Committed changes page discovers removed entries and retains original publications. Guarded navigation waits for preview and durable Apply. Reimporting a removed entry requires an explicit Keep removed or Include decision; stale exclusions require reconfirmation. The C++ worker owns typed commands, freshness tokens and operation identities. See [implementation, recovery and limits](committed-changes.md). Schemas 1–3 remain readable but do not enable these commands.


## Category management and selection — 2026-10-25

New schema-5 datasets expose a guarded Categories editor and allocation picker with active leaves, Uncategorized and unmapped text. Rename/move updates current ledger display and literal filtering without changing retained source or historical labels. Archived assignments remain readable and survive unrelated edits; groups are never assignable. See [behavior, recovery and verification limits](categories.md). Older formats retain their earlier UI capabilities. Combined text/date/source search remains unfinished. The rules page is recorded below.


### Recorded interaction limitation

A rare Qt `DelegateModel::cancel: index out range` warning was observed during the existing filter-reset test, reproducing in one of eight targeted Release repetitions. Occurrence/viewport assertions passed. A proposed workaround was rejected by broader viewport tests and fully reverted; final restored seven-entry UI regression checks passed with clean logs. The warning is not claimed fixed and belongs in the remaining search/ledger interaction review. It does not change the accepted search-return behavior or establish a financial-data failure.

## Rules sheet — 2026-10-25

When built with `SPENTIT_RULES=ON`, current schema-40 documents show the rules page. The page lists, reorders, enables and edits rules; Save/Discard cover the whole image and Preview is separate from Apply. Saved rules are document-wide, while Apply targets only the selected account's pending batch. Rules-off builds use schema 13 and do not register `import.rules_ui`. Earlier experimental layouts are rejected without migration. The UI does not construct a ruleset or execute SQL. See [current verification](status.md#account-scoped-durable-workflow--2026-10-26) and the [rules contract](durable-rules.md). The schema-6 baseline and schema-7 search checkpoint remain historical verification evidence.


## Backup cancellation and progress — 2026-10-26

Backup now shows copying, verification, reopening and publication phases on the existing serial worker. Cancel is available during preparation, between bounded page-copy steps and at verification checkpoints. A cancelled backup leaves the source dataset open and unchanged; the message explains that an encrypted `.partial` candidate may remain and directs retry to a new destination. Such a candidate is not an acknowledged valid backup. Existing candidates and backups are never overwritten or silently deleted.

Cancellation is sealed immediately before exclusive publication. A request accepted before that gate prevents publication; a request after it cannot promise rollback. Success follows directory synchronization and candidate cleanup. Individual SQLite calls and filesystem synchronization remain non-interruptible. No percentage or bounded cancellation-latency guarantee is claimed. See [backup recovery](durable-import.md#backup-cancellation-and-publication--2026-10-26) and [verification](status.md#qif-point-1-independent-backup-progress--2026-10-26).


## Ordinary QIF and unavailable amount review — 2026-10-26

The historical schema-14/15 review added the [v3 ordinary-money profile](../features/009-qif-profile.md). Source classes are displayed read-only and remain separate from categories. For structurally intact records with missing/invalid total or split amounts, the editor starts unknown amount fields blank and labels the original total Unavailable; it never inserts zero. Enter exact effective amounts and balanced allocations, Save repair, then explicitly review Include or a duplicate candidate. Original unavailable amounts stay unavailable through commit and reopening. Invalid transfer split amounts retain their destination and source position, requiring the existing explicit transfer resolution.

Whole-source repair totals/progress and multi-account Abort were not implemented at that schema-14/15 checkpoint. The accepted owner requirement and remaining implementation is in [feature 009](../features/009-durable-import-milestone.md#accepted-ordinary-qif-and-amount-repair-follow-up--2026-10-26); do not describe the partial editor extension as complete QIF workflow delivery. [Project status](status.md#ordinary-qif-and-unknown-amount-repair-checkpoint--2026-10-26) records actual verification.


## Whole-source account review — 2026-10-26

The historical schemas 16/17 introduced [explicit mapping, whole-source repair progress, reported-balance staging and Abort](qif-account-imports.md). The virtualized mapping dialog preserves selections/currency through delegate recreation and defaults to the setup currency. It prompts creation separately for each source account; metadata names remain unchanged. Cancellation after durable capture reports saved source/mappings with Resume or Abort guidance. Unknown outcomes close the session; a known saved operation whose refresh fails is labeled saved.

The review screen separates repairable counts from unsupported structures and shows amount-repair counts before edits. Unknown values remain blank. Source categories/classes and source allocation identities remain visible even before a missing amount is repaired; raw-source rule prefilling does not require inventing money. Staged openings show their exact calculated amount and explicit date before commit. The first account/related transfer commit ends whole-import Abort, with the confirmation stating that remaining accounts still need separate commits. Ordinary source repairs also block external protection until review is complete. Final verification is recorded in [status](status.md#whole-source-qif-account-review--2026-10-26).


## Point-2 interaction hardening — 2026-10-26

The reproduced Qt delegate-reset warning is addressed by disabling asynchronous offscreen cache incubation while keeping the model attached and visible delegates reusable. Twenty repeated warning-fatal navigation/split-view pairs and broader viewport/unsaved-editor tests passed; the paired native cache experiment and remaining performance limits are recorded in [hardening](hardening.md).

The ledger now exposes accessible list/row names, selection and currency/status context. Named category/rule controls and operation/error announcements accompany keyboard/focus tests. The final Cocoa scripted keyboard/accessibility case passed; closed guard controls report invisible. Native accessibility-tree inspection observed named rows/fields, but actual spoken VoiceOver and full manual assistive traversal remain unverified. This supersedes the earlier warning status only for the exercised interactions, not every Qt sequence. The point 8 slice is below.

## Point 8 sheet interactions — 2026-10-27

The ledger columns use the ratios 0.14 date, 0.24 payee, 0.17 category, 0.15 withdrawal, 0.14 deposit, and 0.16 balance. Money text is right-aligned. The user cannot drag a column, and the widths are not saved. The window minimum is 720 by 560. The default size stays 1440 by 850. The ledger content width is at least 1080, so a narrow window scrolls sideways. The sidebar toggle, Back, and Forward stay in the top toolbar.

Bulk removal, bulk category, rule reapply, and history undo capture the visible row after Save / Discard / Stay allows the command. Bulk and reapply put that row back after preview and Apply. History undo puts it back on Apply. Stay does not capture. A missing row opens at the top. The new test checks bulk category. It does not click history undo or reapply.

Sidebar balances and the reports note are in [accounts](accounts.md). Verification is in [status](status.md#point-8-sheet-and-sidebar-interactions--2026-10-27). Feature 005 stays active. On 2026-10-28 the owner said the finished sequence was the work before reports. The note stays until that work has an explicit Go. The open questions are in [feature 006](../features/006-reporting-drill-down.md).
