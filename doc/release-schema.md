# Release schema version 1

Last updated: 2026-11-11

Status: **done.** The owner said to make the release schema on 2026-11-11.
The application version is 1.0.0.

The rules database is version 1. A new rules file reads back `user_version` 1.
A file whose number is not 1 is refused, and its bytes stay the same. There is
no migrator and no `src/storage/migrations/`. The next schema change is version
2. That later application is the one that upgrades a version-1 file. It must
not refuse a version-1 file.

The rules-off build still writes version 69. That schema is a different set of
tables. It is not the release format and it is not numbered 1.

These are not the database schema number:

- The application id `1397770577`.
- The QIF parser name `bank-v6`.
- The SpentIt export file, which is JSON version 1.
- `manual-v1`.

Open still checks that the tables match. A file numbered 1 whose tables do not
match is refused, and its bytes stay the same. After a file has opened, the
code uses the current tables and does not ask the version number which feature
exists.
