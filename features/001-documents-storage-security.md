# 001 — Documents, storage, and security

Status: **active**  
Last updated: 2026-10-25  
Implementation: partial; optional SQLCipher synthetic-data storage/recovery prototype. Full application document lifecycle, undo, migrations and password changes remain unimplemented.

## Accepted requirements

- Local, single-user operation. One running application per PC and one open financial document at a time.
- Users can select different database/documents. Roommates can have separate documents and passwords; accounts inside one document are not separate users.
- All accounts belonging to a document share its storage so cross-account operations can be atomic. Commit remains explicit per account: an origin commit atomically protects its generated destination draft without committing it. See [transfer lifecycle](008-transfer-matching-and-lifecycle.md).
- Easy backups, preferably a single portable backup file. Safety takes precedence over having no runtime sidecar files.
- Password unlocking must unlock the encrypted data, not merely hide the UI. No initial password reset/recovery service.
- Draft work, committed-change history, and recovery information survive normal restarts.
- Edits and deletions of committed data retain recoverable history. Transfers must remain consistent during changes and reversal.
- Backups require the password applicable when created; changing the current document's password does not rewrite old backups.

## Proposed design

Use SQLCipher with one database per document. Store accounts, transactions, allocations, immutable source evidence, drafts, rules, and history together. Use conventional current-state tables plus a persistent change journal rather than requiring full event sourcing.

Choose and test journaling, synchronous settings, foreign-key enforcement, transaction boundaries, and temporary-storage behavior explicitly. Journals/WAL are recovery mechanisms; their presence is compatible with the single-file-backup goal.

Build an application-managed encrypted snapshot/backup operation using supported database facilities. Write a new backup without overwriting the previous known-good backup, validate it, and publish it only on success. Restore must be tested, including wrong passwords and interrupted writes. Never copy only a live database's main file and assume consistency.

Save each financial mutation and its history atomically. Undo records a reversal; the journal is not erased. Start with recent-action undo and a durable history view. Selective restoration of older changes requires dependency checks and a preview, not blind replacement of newer state. Retain history by default until a deliberate retention policy exists.

Switching documents must finish or cancel the current edit explicitly, close the old document, release keys as supported, and clear old document data from views/caches. Password-only unlocking is the initial design; operating-system credential storage is a possible later feature, not an accepted initial requirement.

Encryption protects stored documents and backups; it cannot protect an unlocked app against a compromised OS. Original bank-export files outside the application remain outside its encryption boundary. Do not silently alter or delete them.

## Acceptance criteria

- Drafts and history reopen correctly after restart; normal reports exclude drafts.
- A second instance cannot independently open another writable session under the one-app policy; document-level locking also protects integrity.
- Each chosen commit, linked state changes (including destination draft protection), history, and draft transitions are all-or-nothing under injected I/O failures and process interruption. This must not implicitly commit the other account. Protected drafts survive restart, destination workspace clearing, and backup/restore while remaining excluded from committed balances and ordinary reports.
- A successful commit followed immediately by interruption reopens as committed; retries cannot duplicate it.
- Wrong-password attempts do not modify the document or create a replacement database.
- Backup/restore preserves balances, allocations, rules, raw sources, drafts, and history.
- Insufficient disk space, damaged backups, failed migration, and failed password changes preserve recoverability and show actionable errors.
- Tests inspect database, relevant sidecars, temporary storage, and logs for known synthetic sensitive markers. This is evidence for tested paths, not a universal absence-of-leaks proof.
- Document switching cannot expose one roommate's data in another document's views or history.

## Implemented prototype — 2026-10-25

The owner approved evaluating SQLCipher Community Edition with synthetic data and an explicitly pinned dependency. The optional storage target implements encrypted saved snapshots/evidence, batch commit with persistent operation identity/history, idempotent retry, integrity checks, and encrypted backup/restore to a new file. It retains currency definition identity, code, scale, original/effective units and ordered allocations. This is a bounded experimental schema, not adoption of the full proposed product design.

Selected prototype settings: SQLCipher 4.19.0 with OpenSSL, SQLCipher 4 compatibility defaults, DELETE rollback journal, EXTRA synchronization, fullfsync, foreign keys, memory-only temporary databases and explicit rejection of unsupported schema versions. Backups are copied using the encrypted database backup API, validated and reopened before exclusive publication; unsuccessful candidates are retained separately. No plaintext fallback exists.

[Prototype design and actual verification](../doc/storage-prototype.md) records the build, failure matrix, restoration procedure and limitations. Debug, Release and ASan/UBSan passed all 17 engine/storage groups, including batch/draft/backup disk-full and I/O injection, interrupted operations, retries, wrong passwords, malformed values, prior-backup preservation and synthetic-marker scans. Exact fault-position counts and platform limitations are recorded there. No application-wide locking, actual financial transfers, committed-edit reversal, migration, password-change workflow, or physical power-loss claim is made. This feature remains **active**.

## Open questions

- Production SQLCipher/provider packaging and whether the prototype compatibility/journal/durability choices should be adopted for the application. Prototype choices and pins are recorded above.
- Lock scope on machines with multiple OS login sessions; stale-lock recovery and instance handoff.
- Backup destination, frequency, retention, and handling of unavailable/full destinations.
- Auto-lock behavior, password-change workflow, migration protocol, history retention, and selective-undo dependencies.
- Whether unfinished keystrokes survive interruption; durable drafts do not automatically promise recovery of every unaccepted field edit.

## References

- [SQLCipher design and temporary-file considerations](https://www.zetetic.net/sqlcipher/design/)
- [SQLite atomic commit assumptions](https://www.sqlite.org/atomiccommit.html)
- [SQLite backup API](https://www.sqlite.org/backup.html)
