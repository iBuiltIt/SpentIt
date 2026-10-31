# Verified fast startup

Last updated: 2026-10-31

## Accepted behavior

Owner Go (2026-10-31): make normal startup as close to instantaneous as practical.
A previous successful audit may be reused when a full-file cryptographic hash
proves the dataset bytes are unchanged. Move expensive auditing to graceful close;
hide the window during quit-time work. Missing/stale approval means unchecked,
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
invalidates old approval even without a schema change. Experimental schemas stay
38/37. The cache is a same-user optimization, not a defense against a compromised
user account that can rewrite both the application and its cache.

Opening retains document locking, authentication, format/schema gates and required
security/durability settings. It holds a SQLite read transaction during hashing.
Only an exact hash/verifier match skips the complete historical audit. Any journal,
WAL or SHM file present before SQLite opens disables this shortcut, even if recovery
later removes it. Current storage uses DELETE rollback journaling; this does not
change journaling mode, HMAC checks, synchronization or backup behavior. Invalid,
missing, unreadable, oversized or unsafe approval files fall back to full verification.
The ordinary headless `ImportStore::open` API always verifies.

Graceful Close calls the storage audit on the serial worker, retaining the document
lock and a SQLite read lock. An unchanged approved file can reuse its approval.
Otherwise the complete audit runs; full hashes before and after must agree before
publishing approval. Publication uses an owner-only temporary file, file sync,
atomic rename and directory sync. The window hides before quit-time work begins,
and the process exits after successful close. A second process cannot open the
locked document while the first finishes its audit; it receives the existing
already-open/unavailable error. Forced exit and emergency error cleanup do not
attempt an audit or publish approval. A prior approval remains useful only if the
complete file still matches it.

A failed close-time audit or approval write restores the window and displays the
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

See [current status](status.md) for commands, measured results and platform limits.
The focused storage test is `import.verification_cache`; focused UI cases are
`cachedStartupReviewAndQuit`, `closingApprovalFailure` and `cachedStartupTiming`.
`SPENTIT_STARTUP_ROWS` sets the synthetic timing case's size (default 120).
Timing includes model commands and their snapshots; it is not a physical-frame
measurement. Tests never use the owner's private dataset.
