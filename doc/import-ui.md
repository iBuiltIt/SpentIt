# Durable QIF UI experiment

Last updated: 2026-10-25

The optional `spentit_import_experiment` executable connects setup/unlocking and a writable Qt Quick review screen to the existing durable import commands. It is for synthetic inputs. Feature 009 remains **active**; this is not a production financial application or completion of the full sheet design.

## Build and run

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
build/import-ui-debug/src/ui/spentit_import_experiment
```

The Qt/OpenSSL prefixes above are macOS Homebrew examples; use the installed dependency locations on your machine. The synthetic sheet executable remains separate. The import UI requires Qt 6.8 or newer; this implementation was exercised with Qt 6.12.0. No Qt dependency was added to the engine or storage contracts.

## Owner walkthrough

1. Choose **New dataset**, a new file outside the checkout, an account name and CAD. The suggested location is the platform Documents folder under `SpentIt`. Password protection starts checked and requires matching entries. Unchecking it displays the explicit confidentiality waiver. Existing files are never overwritten by creation.
2. Set Opening Balance to `100.00`, date `2026-08-01`, and confirm it. Choose **DMY** and import the [authored synthetic fixture](../tests/fixtures/qif/durable-review-synthetic.qif).
3. Committed balance should remain `100.00`, with projected balance `120.69`. The first source date is 10 August. Inspect record 1, including the original `-29.31` and its two allocations.
4. Change the total to `-30.00` and the second allocation to `-29.86`. Save draft. The projection becomes `120.00`; original evidence remains unchanged. Trying only the total change fails the exact split-sum validation.
5. Close through **Settings / close dataset**, then reopen. Accepted draft edits remain. Leaving an unsaved edit offers Save / Discard / Stay; Discard here affects only the editor buffer.
6. Commit the whole reviewed batch. Its receipt shows change `20.00`, resulting balance `120.00`, two included records and no exclusions. Opening balance is now frozen. Reopen to check the committed ledger and reconstructed receipt.
7. Reimport the same file. Both occurrences are visibly excluded against their prior occurrences; committing this reviewed batch contributes zero. An explicit Include as new override is available. Other possible matches require a chosen prior occurrence or explicit inclusion.
8. Save a backup to a new destination. To restore into a separate working document, open that backup, use **Backup / restore copy** to publish another new file, close it, then open the restored file. Backup preserves the current protection mode and does not replace destinations. Password-protected backups need their original password.

The ledger supports explicit committed/projected modes, literal whole-record filtering, six-column sorting, and filtered signed-withdrawal/deposit totals. Chronological balances come from the application ledger calculation and stay attached to their rows. Drafts, duplicate dispositions and unavailable projections are labeled explicitly. Committed rows are read-only. Double-click a contributing ledger row or select an import/record number to inspect it; excluded occurrences remain reachable through the import selector.

Blocked imports retain evidence and display diagnostics. Whole-file or record source previews are read-only, capped at 64 KiB with an explicit byte-count notice; non-UTF-8/NUL-containing previews use hex rather than replacement decoding. The complete bounded source remains in encrypted storage. New schema-2 datasets support explicit date and split-total repair as described below; other errors still require discard/reimport, and blocked batches cannot partially commit. Discarding a batch requires confirmation and retains its evidence/history.

## Execution and failure boundaries

`DocumentGateway` is a plain application boundary. Executable wiring supplies the concrete SQLCipher adapter. The Qt model submits complete repository operations; QML performs neither SQL nor authoritative money calculations. A private worker pool with one thread owns the active gateway. Only one command can be in flight; controls disable switching, closing and additional submissions while it runs. Results are delivered before another command can start, so a late completion cannot populate another dataset. There is no mid-operation cancellation UI. Closing waits for work rather than implying rollback.

A successful open is fully validated before the dataset becomes active and its path is saved using QSettings. Settings contains only the dataset path, never the password/key. Failed settings persistence is reported separately from the already-open dataset. Closing clears the model, selected source, receipt and editor fields, and releases the document lock. Parent-directory aliases such as macOS `/var` are resolved; final-file symlinks and multiple hard links remain rejected by storage.

Missing/inaccessible paths and document-lock failures do not invoke a password retry screen. Automatic public-key authentication is tried first. Authentication failure offers password/Browse and says unable to unlock, since damaged encrypted input can look like a wrong key. Authenticated schema, integrity, financial-validation and I/O errors remain errors. No failed open replaces or repairs a file. An empty lock sidecar can remain, as documented in [storage behavior](durable-import.md#storage-and-locking).

Commit IDs are derived from batch identity and expected revision. Receipts are looked up again after restart, covering a prior commit that became durable before its UI acknowledgment. Save/import/discard/commit still use the existing atomic command contracts. A refresh failure closes the session and asks for reopening; it does not acknowledge a partial view as complete. Unaccepted keystrokes are not promised crash recovery.

## Portable credential convention v1

The UI uses the existing experimental application ID `1397770577`, schema version `2` for new datasets (`1` remains readable), and SQLCipher 4 settings from [the headless report](durable-import.md). It adds a versioned credential convention, without rewriting existing documents:

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

Expect application ID `1397770577`, schema `1` or `2`, no cipher/foreign-key error rows, and `ok` from SQLite integrity checking. These SQL checks alone do not establish all financial invariants: use a compatible SpentIt build's full verification before editing, and retain the original backup. A protected document instead needs the password with the exact v1 prefix through the supported key API; do not put private passwords in shell history. This procedure cannot repair damaged/missing files or recover a forgotten password. Do not copy only the main file of a live database; use the application's validated backup operation.

## Verification and remaining work

Actual verification on 2026-10-25: macOS 27.0.1 arm64, AppleClang 21.0.0, Qt 6.12.0, SQLCipher 4.19.0 and OpenSSL 3.5.9. Debug, Release and ASan/UBSan each built and passed **33/33 CTest entries**. After final UI/test refinements, the affected UI and QML checks were rebuilt and passed again in all three configurations. The unchanged synthetic sheet's four Debug checks also passed. No compiler warnings or sanitizer diagnostics were emitted in the final builds/runs. All 89 relative file targets in the changed documentation exist; `git diff --check` passed.

The new UI test executable covers corrected split totals, rejected mismatches, saved draft restart, frozen publication/receipt restart, exact-file duplicate exclusions, backup restoration, password mismatch before file creation, wrong-password versus missing-file/lock errors, document isolation, blocked transfers, invalid-encoding hex evidence, and oversized-file rejection. The QML interaction test exercises field-to-editor binding, Save / Discard / Stay during window closing, failed-save retention and preservation of the last saved revision after discarded keystrokes. An offscreen screenshot of the populated review/editor surface was rendered and inspected. These checks are not a full native pointer/accessibility audit.

A restored no-password backup was opened through the SQLCipher C API using only the documented public key after destroying the original gateway and clearing Settings. That independent connection read the exact opening value. An authenticated schema-version alteration then produced a schema error without a password retry. A protected dataset whose user password literally equals the public raw-key specification rejected automatic opening and opened with that password through the protected convention. This demonstrates installation-independent credentials in a same-machine test, not cross-platform validation.

Tests and smoke launches use temporary synthetic files and isolated Settings; no private exports are accessed. Existing headless fault/crash/backup sweeps remain in the 33-entry suite. No new claim is made for physical power loss, all filesystems, or cancellation at every instruction boundary.

Remaining scope includes repair beyond dates/split-total mismatches, full row-expanded editing and source-field copying, the full resizable navigation shell, multiselection/search-return behavior, accessibility review, mid-operation cancellation, packaging, and Linux/Windows validation. Source previews are bounded rather than a full-file editor. The UI currently rebuilds a complete presentation snapshot after commands; large durable-UI workloads and memory/frame latency have not been measured, and the earlier synthetic/headless timings do not certify it. This is a functional review surface, not a responsiveness claim. Password changes, legacy credential conversion and migrations remain outside this implementation.

## Walkthrough regression follow-up — 2026-10-25

The automated UI walkthrough now reads the same synthetic fixture linked above. It additionally checks the DMY interpretation, included/excluded receipt counts, rejection of opening-balance changes after commit, filtered withdrawal/deposit reconciliation with stable chronological balances, and backup-to-restored-copy publication followed by reopening and commit. The QML Save / Discard / Stay test was rerun and its rendered review surface inspected. This is automated/offscreen validation, not a native pointer or accessibility audit. Backup interruption behavior and recovery of retained files are documented in the [headless recovery report](durable-import.md#backup-interruption-recovery).

After the final walkthrough assertions were added, `import.ui` was rebuilt and passed again in Debug, Release and ASan/UBSan on the recorded macOS arm64 toolchain, without compiler warnings or sanitizer diagnostics. All 35 relative file targets in this report, project status and the headless report exist; `git diff --check` passed.

## Record repair

In a new dataset, inspect a malformed-date or split-total record. The editor displays the retained diagnostic, original amount and unchanged source view. Enter a valid ISO date and/or explicitly correct the splits/total so they reconcile exactly, then choose **Save repair**. No date or balancing amount is guessed. Invalid corrections remain unsaved. Saved repairs retain a visible original-diagnostic label.

After saving, explicitly select **Include as new** or a candidate to exclude. Saving a repair alone does not resolve duplicates or enable commit. A candidate with an unavailable original date is labeled “same original amount; source date unavailable.” Resolve every remaining record and duplicate decision before committing the whole batch. Save / Discard / Stay applies to the repair editor through the existing save command. Saved repairs survive reopening and backup restoration.

Older schema-1 datasets remain readable but cannot store repairs; the diagnostic explains that a new experimental dataset is needed. Missing/invalid amounts and unsupported constructs remain read-only and block commit. See the [storage contract](durable-import.md#bounded-record-repair).

For synthetic owner review, create a new CAD dataset with confirmed opening `100.00` dated `2026-01-01`, then import the [repair fixture](../tests/fixtures/qif/durable-repair-synthetic.qif) using DMY. Record 1 has an impossible date; explicitly enter `2026-02-28`, Save repair, then Include as new. Record 2 has total `-20.00` and split `-19.00`; explicitly change the split to `-20.00`, Save repair, then Include as new. Projection becomes `70.00`; committed balance remains `100.00` until explicit commit. Reopen before committing to inspect the retained repairs, diagnostics and source bytes.

Verification: the repair model/QML flow passed within the 36-entry Debug, Release and ASan/UBSan suites and was rerun after final refinements. The repair screen was rendered and inspected offscreen. The [headless report](durable-import.md#bounded-record-repair) records failure/recovery coverage and limits.
