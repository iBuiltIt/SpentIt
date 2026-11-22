# Release schema version 2

Last updated: 2026-11-19

The owner approved version 2 on 2026-11-18 for restoring merged transactions
after acceptance. The application version is 1.0.3.

New rules-enabled databases use `user_version` 2. Opening an authenticated
version-1 file runs the upgrade under its normal document lock. The upgrade
installs the restoration journal and its integrity rules and verifies the
result before committing the version change. The single encrypted transaction
also marks the session open. Cancellation before the commit seal or an error
before durable completion rolls back the upgrade. Probing a file does not
upgrade it. Existing posting identities, source bytes, publication membership
and eligible Undo survive the upgrade. An older SpentIt version requires a
copy made before upgrade to open the file.

The rules-off build still writes version 69. That schema is a different set of
tables. It is not the release format and it is not numbered 1.

These are not the database schema number:

- The application id `1397770577`.
- The QIF parser identity. New imports use `bank-v7`. Retained `bank-v6` imports
  keep their original interpretation. A build that supports only `bank-v6`
  refuses files containing `bank-v7` imports.
- The SpentIt export file, which is JSON version 1.
- `manual-v1`.

Open checks for the expected tables and rejects unsupported or inconsistent
versions before the upgrade can write. See [Review](review-sheet.md#unmerge-after-acceptance--2026-11-18).

Changing the file's date format updates the existing encrypted preference row.
On the first change to an older file, the same transaction replaces the
original update trigger with one that still protects the singleton and the
starter-category choice. The delete guard stays in place. The schema version
and stored dates are preserved. Opening a file does not perform this change.
