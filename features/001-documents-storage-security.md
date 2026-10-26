# 001 — Documents, storage, and security

Status: **active**  
Last updated: 2026-10-26  
Implementation: partial; optional SQLCipher synthetic-data storage/recovery prototype and [headless durable-import experiment](../doc/durable-import.md). Bounded document switching, backup/restore, ordinary strict Undo and atomic group Undo are implemented; full application lifecycle and password changes remain incomplete.

Accepted format policy, 2026-10-26: no datasets exist in the wild. Experimental formats need no migration, upgrade or backward-compatibility work until the owner explicitly announces a release. Migration requirements below apply only once that gate makes them relevant; encryption, current-format atomicity and recovery remain required. Current group-history behavior is described in [search and ledger](../doc/search-ledger.md).

## Accepted requirements

- Local, single-user operation. Each running application has one active financial dataset at a time. The owner clarified on 2026-10-25 that there is no one-application-per-user rule; the earlier one-running-application-per-PC wording is superseded. Do not introduce an application-wide instance limit.
- Settings contains the path specifying which dataset to load. A user may have multiple datasets; close the active dataset before opening another. This is a dataset-selection setting, not a new in-app user/account system.
- Users can select different database/documents. Roommates can have separate documents and passwords; accounts inside one document are not separate users.
- All accounts belonging to a document share its storage so cross-account operations can be atomic. Commit remains explicit per account: an origin commit atomically protects its generated destination draft without committing it. See [transfer lifecycle](008-transfer-matching-and-lifecycle.md).
- Easy backups, preferably a single portable backup file. Safety takes precedence over having no runtime sidecar files.
- Password protection is enabled by default during setup, but the user may explicitly disable it and knowingly waive confidentiality. When enabled, password unlocking must unlock the encrypted data, not merely hide the UI. No initial password reset/recovery service.
- Draft work, committed-change history, and recovery information survive normal restarts.
- Edits and deletions of committed data retain recoverable history. Transfers must remain consistent during changes and reversal.
- Password-protected backups require the password applicable when created; changing the current document's password does not rewrite old backups. No-password datasets/backups use the documented public unlock mechanism and provide no confidentiality.

## Accepted setup, unlock and recovery behavior — 2026-10-25

- Initial setup asks for home currency, dataset location and password protection. Protection defaults to checked; if enabled, enter and confirm the password. Suggest `~/Documents/SpentIt/` on macOS and the platform Documents-folder equivalent on Windows/Linux, with Browse to choose another location. This is a configurable suggestion, not a hardcoded requirement or permission to relocate existing files.
- Subsequent launches use the dataset path from Settings. A dataset requiring a password shows an unlock screen identifying the selected dataset/location, with password entry and Browse to choose another dataset. No-password datasets open without that prompt after successful verification.
- Choosing no password is an explicit waiver of confidentiality, not a promise of hidden protection. Continue using the encrypted database format with a fixed, public, reserved unlock key. The key is a format constant, not random per installation, machine-bound, or stored only in local Settings/keychain. Keep its definition and compatible recovery instructions publicly documented and available independently of an installed app. Any future format-version key change must retain recovery of older files.
- The reserved no-password mechanism must be distinct from ordinary user-password input, so a user's chosen password cannot accidentally become an automatic-unlock password. This is a separation of modes, not a secret users are prohibited from knowing or reproducing with recovery tools. Exact key bytes, the supported SQLCipher interface and format identification remain implementation details to specify and verify; no custom cryptographic scheme is authorized.
- Losing/reinstalling the app or moving intact no-password files to another machine must not lose access. A compatible app or documented SQLCipher-based recovery tool must open them without the old installation. Verify this with restored synthetic copies and no original Settings/credentials. This does not repair corrupt/missing data or provide forgotten-password recovery for protected datasets.
- Opening first handles missing/inaccessible paths and other identifiable file errors. Attempt the reserved no-password unlock/verification path; if authentication cannot be established, offer password entry or Browse. A successful unlock is followed by format/schema, integrity and financial validation before activating the dataset. A known unsupported format, corruption after unlocking or I/O failure follows error reporting, not a password retry loop. Some authentication failures cannot distinguish a wrong key from damaged encrypted input: say unable to unlock rather than claim the password is definitely wrong. Never modify or replace files merely because opening failed.

The direct headless API retains its original explicit credential convention. The optional [durable UI gateway](../doc/import-ui.md) now implements setup/unlock, a reserved public raw key, password-domain separation and installation-independent recovery checks. Changing an existing dataset's protection mode remains part of the separately designed password-change/recovery workflow; setup choice does not authorize an unsafe in-place conversion.

## Proposed design

Use SQLCipher with one database per document. Store accounts, transactions, allocations, immutable source evidence, drafts, rules, and history together. Use conventional current-state tables plus a persistent change journal rather than requiring full event sourcing.

Choose and test journaling, synchronous settings, foreign-key enforcement, transaction boundaries, and temporary-storage behavior explicitly. Journals/WAL are recovery mechanisms; their presence is compatible with the single-file-backup goal.

Build an application-managed encrypted snapshot/backup operation using supported database facilities. Write a new backup without overwriting the previous known-good backup, validate it, and publish it only on success. Restore must be tested, including wrong passwords and interrupted writes. Never copy only a live database's main file and assume consistency.

Save each financial mutation and its history atomically. Undo records a reversal; the journal is not erased. Start with recent-action undo and a durable history view. Selective restoration of older changes requires dependency checks and a preview, not blind replacement of newer state. Retain history by default until a deliberate retention policy exists.

Switching documents must finish or cancel the current edit explicitly, close the old document, release keys as supported, and clear old document data from views/caches. The owner-approved optional-password setup above supersedes the earlier password-only design. Operating-system credential storage is not required for no-password recovery.

User-password encryption protects stored documents and backups; it cannot protect an unlocked app against a compromised OS. Public-key no-password mode deliberately provides no confidentiality. Original bank-export files outside the application remain outside its encryption boundary. Do not silently alter or delete them.

## Acceptance criteria

- Drafts and history reopen correctly after restart; normal reports exclude drafts.
- A dataset has at most one writable session at a time, enforced by document-level locking. Each running application has at most one active dataset; this does not impose an application-wide instance limit.
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
- User-facing handling when a dataset is already open elsewhere. The current experiment uses a kernel-owned document lock that releases on process exit and never overrides a live owner; no application-wide lock is required.
- Backup destination, frequency, retention, and handling of unavailable/full destinations.
- Auto-lock behavior, password-change workflow, migration protocol, history retention, and selective-undo dependencies.
- Whether unfinished keystrokes survive interruption; durable drafts do not automatically promise recovery of every unaccepted field edit.

## References

- [SQLCipher design and temporary-file considerations](https://www.zetetic.net/sqlcipher/design/)
- [SQLite atomic commit assumptions](https://www.sqlite.org/atomiccommit.html)
- [SQLite backup API](https://www.sqlite.org/backup.html)

## Accepted durable-import experiment — 2026-10-25

The owner accepted the bounded policies in [feature 009](009-durable-import-milestone.md), including its explicit deferrals. Those decisions govern that experiment only; broader open questions in this feature remain unless explicitly settled there. The headless subset and bounded setup/review UI are implemented; full lifecycle and milestone scope remain incomplete.
