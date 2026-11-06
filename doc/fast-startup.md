# Verified fast startup

The [opening/ledger cleanup and interrupted-import rollback](ui-import-cleanup.md)
is implemented under the owner's 2026-10-31 Go on experimental formats **40/39**.
It includes import routing, Review repair/Commit, account-context Discard and
atomic cleanup of incomplete preparation. Completed uncommitted imports remain
durable. See [current verification and limits](status.md); earlier Resume,
source-retention and toolbar descriptions below are historical.

Last updated: 2026-11-06

## Accepted behavior

Owner Go (2026-10-31): make normal startup as close to instantaneous as practical.
A previous successful audit may be reused when a full-file cryptographic hash
proves the dataset bytes are unchanged. Move expensive auditing to graceful close;
keep the windows visible during quit-time work (owner correction and Go,
2026-11-06). Missing/stale approval means unchecked,
not corrupt. Never repair or discard financial data automatically.

## Implementation

The desktop gateway uses a disposable, owner-private `verification` directory
under Qt's application cache location (macOS: the application's directory under
`~/Library/Caches`; Linux: the platform cache directory). Receipts contain only a
format marker, verifier fingerprint and SHA-256 of the complete encrypted file.
Their filenames hash the canonical document path. No financial values, source
text, passwords, keys or plaintext paths are written. Losing the cache affects
performance only; it is not required to open or recover documents.

The verifier fingerprint includes storage, engine and application C++ sources and
headers, storage include fragments, the rules build option and pinned SQLCipher
source hashes. CMake reconfigures when those inputs change. A verifier change
invalidates old approval even without a schema change. Current experimental schemas are
40/39; the startup-cache change itself originally retained 38/37. The cache is a same-user optimization, not a defense against a compromised
user account that can rewrite both the application and its cache.

Opening retains document locking, authentication, format/schema gates and required
security/durability settings. It holds a SQLite read transaction during hashing.
Only an exact hash/verifier match skips the complete historical audit. Any journal,
WAL or SHM file present before SQLite opens disables this shortcut, even if recovery
later removes it. Current storage uses DELETE rollback journaling; this does not
change journaling mode, HMAC checks, synchronization or backup behavior. Invalid,
missing, unreadable, oversized or unsafe approval files fall back to full verification.
The ordinary headless `ImportStore::open` API always verifies. After either full
verification or cache approval, opening atomically cleans incomplete captured
imports before activation. This invalidates prior file approval through normal
mutation tracking. Completed uncommitted imports are retained; cleanup failure
prevents activation. See the [cleanup contract](ui-import-cleanup.md).

## Closing verification follow-up — owner Go 2026-10-31

A desktop opening that performs a full audit now retains an in-memory approval.
The audit runs under a SQLite read transaction with matching full-file hashes
before and after it. An unchanged close can publish that approval without repeating
the audit. Reuse requires an identical fresh full-file hash and the same connection
write count; rollback and any subsequent verification attempt clear the approval.
Cancellation or publication failure rolls back the closing read transaction and
invalidates it. A process interruption cannot persist an unpublished session
approval. Headless opens and backup validation continue to audit fully.

Full audits reuse already loaded rows and reconstructed committed snapshots only
within that audit. The snapshot cache is capped at 65,536 entries and a 64 MiB
accounted allocation budget, in addition to the existing bounded row cache;
uncached entries follow the original read path. The cache is destroyed on success
or failure. Financial/history ordering, source ownership, references, currency,
allocation and balance checks remain in place. No schema or durability setting
changes are involved. Missing approvals still require a full opening audit.

Graceful Close calls the storage audit on the serial worker, retaining the document
lock and a SQLite read lock. An unchanged approved file can reuse its approval.
Otherwise a matching session approval is reused or the complete audit runs; full
hashes before and after must agree before
publishing approval. Publication uses an owner-only temporary file, file sync,
atomic rename and directory sync. Windows remain visible while quit-time work runs,
with the animated busy indicator and actual worker phases in the status line.
Repeated close requests are ignored while closing. The process exits and closes
its windows only after successful close and queued ledger-snapshot cleanup,
including the explicit Quit anyway path. A second process cannot open the
locked document while the first finishes its audit; it receives the existing
already-open/unavailable error. Forced exit and emergency error cleanup do not
attempt an audit or publish approval. A prior approval remains useful only if the
complete file still matches it.

A failed close-time audit or approval write keeps the window visible and displays the
error, retaining the open document. The user may return or quit without new
approval. Subsequent opening uses the normal hash gate/full verification fallback;
there is no automatic repair or claim that an I/O error proves corruption.

The initial account snapshot defers construction of the full Review suggestion
sheet. It still prepares acceptance validation, loads saved balances and the
selected ledger. Review shows no numeric count until prepared; entering Review or
requesting Commit loads it on the worker. Commit then follows the existing Review
route. Read-only account switching preserves deferral. Mutations retain the
existing Review-refresh behavior. Suggestions remain session caches, not plaintext
persistent caches; saved Review decisions/history remain in the encrypted dataset.

## Verification

The closing follow-up adds `SPENTIT_CLOSE_ROWS=10000` to the approval test executable
for serial synthetic profiling of first unchanged, already-approved unchanged and
written-session closes, including isolated process exit. See the
[measurement record](status.md#closing-measurements) for results and limits.


See [current status](status.md) for commands, measured results and platform limits.
The focused storage test is `import.verification_cache`; focused UI cases are
`cachedStartupReviewAndQuit`, `closingApprovalFailure` and `cachedStartupTiming`.
`SPENTIT_STARTUP_ROWS` sets the synthetic timing case's size (default 120).
Timing includes model commands and their snapshots; it is not a physical-frame
measurement. Tests never use the owner's private dataset.
