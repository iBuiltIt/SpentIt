# Release schema version 1

Last updated: 2026-11-10

Status: **waiting for the owner.** Do not carry this out until the owner says
to make the release schema.

There are no SpentIt files to convert. Experimental numbers exist only in this
repository. When this change lands, schema version **1** is the first database
format that matters.

Until the owner says so, a real table change still uses the next experimental
number. Today that pair is **70 with rules / 69 without**. Do not stamp files
as version 1 early.

This change does not add a migrator. Do not create `src/storage/migrations/`.
The first schema change after this one is version 2, and that later app is the
one that upgrades a version-1 file. A file whose number is not 1 is refused,
and its bytes stay the same.

## What becomes version 1

The shipping app is the rules build. Its database schema is numbered 1.

The no-rules schema must not also be numbered 1. It is a different set of
tables. Leave it out of the release format.

Leave these alone. They are not the database schema number:

- The application id `1397770577`.
- The QIF parser name `bank-v6`.
- The SpentIt export file, which is already JSON version 1.
- `manual-v1`.

## Code

Search the tree for `user_version` and for the experimental number in use on
that day. The list below is the layout as of 2026-11-10. Line numbers will
have moved.

On create, the rules schema in `src/storage/import_store.cpp` writes
`PRAGMA user_version=1`. On open, the rules build accepts only that number.
The open still happens after authentication and before any setting that writes.

After a file has opened, the code uses the current rules schema and does not
ask the version number which feature exists. Delete the branches that exist
to recognize an older number. Where a check chooses between two chunks of
SQL, keep the chunk used for the current experimental schema and delete the
older chunk. The older chunk does not match today's tables.

These are the version-number branches:

- `schema3_batch`, `review_stamp_schema`, `categories_format`, and
  `rules_format` in `src/storage/import_store.cpp`. The true side is the
  current schema. Lists of old numbers, including 3, 4, 6, 16, 17, 18, 19,
  20, 69, and 70, go away.
- `expected_format` in the same file. The rules build expects `"1"`.
- Three `user_version=="1"` checks in the same file, on row load, record
  repair, and file check. They treat version 1 as a broken old import.
  Delete them. Version 1 is the good file. Left in place, they block repair
  and fail file check.
- `bulk_supported` in `src/storage/bulk_store.inc`. It is true for an opened
  file.
- `reapply_schema` in `src/storage/rule_reapply_store.inc`. Same.
- The `user_version=="70"` bodies in `src/storage/committed_store.inc`. Keep
  those bodies.
- `committed_changes_supported` in the same file. Drop the
  `user_version=="4"` alternative. Committed editing stays supported.

Keep the open-time table checks: account, transfer, source, preferences,
review, report, session, the common table set, the rules and bulk tables, and
the account-command tables. A file numbered 1 whose tables do not match is
refused, and its bytes stay the same.

The statement cache around `PRAGMA user_version` in `scalar` can stay. It
remembers the number for one statement scope. It does not decide features.

## Tests

A newly created rules file reads back `user_version` 1.

A test that sets a good file's number to 1 and expects refusal must use some
other number.

The catalog of fake historical schemas does not need to remain. One test is
required: a number other than 1 is refused, and the file bytes stay the same.
As of 2026-11-10 those expectations live in `tests/storage/`, including
`rule_store_tests.cpp`, `statement_store_tests.cpp`, `schema_gate_tests.cpp`,
`account_lifecycle_tests.cpp`, `committed_tests.cpp`, `category_tests.cpp`,
and `import_tests.cpp`, plus the schema fixtures those tests include.

## Docs in the same change

State the working database format as version 1. Search `AGENTS.md`, `doc/`,
and `features/` for the experimental pair and for "no migration." This file's
status becomes done.

Do not edit `manual/` unless the owner asks. This change has no user-visible
string. If one appears, log it in
[manual-update-backlog.md](manual-update-backlog.md).
