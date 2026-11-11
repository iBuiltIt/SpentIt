# Session-aware startup and verification

Last updated: 2026-11-10

Open and close follow [session history](session-history-baseline.md). The
release rules database is version 1 and uses an encrypted `document_session`
row. A file whose number is not 1 is refused, and its bytes stay the same.
There is no migrator. There is no full-file hash cache.

## Normal open and close

Open holds the document lock, authenticates SQLCipher and checks the format and
required schema. A clean marker permits activation without a full audit or file
hash. Indexed checks reject a clean marker accompanied by unfinished snapshot,
history or completion rows. Before exposing the document, a transaction durably
marks it unclean. Failure prevents activation. Review preparation and bounded
cleanup of incomplete capture still run; completed uncommitted imports survive.
Neither missing disposable approval files nor a rebuild triggers a full audit.
The obsolete cache publication, file-fingerprint functions and build-generated
verifier ID have been removed; recovery-sidecar detection remains. Legacy cache
path arguments are ignored by storage opening/finalization.

Successful close prunes obsolete ordinary snapshots and their operation records,
expires session Undo, advances the baseline cutoffs and marks the file clean in
one SQL transaction. It does not accept pending transactions, run verification,
hash the file, VACUUM or copy the database. Existing locks, encrypted journals,
full synchronization and durable commit handling remain in use. Failed/cancelled
close rolls back; a lost acknowledgement is resolved against the saved generation.

A crash or error-path disconnect does not certify a close. The next open performs
SQLite recovery and full verification before writable activation, retaining the
recoverable session journal. A successful later close expires that journal.

## Full checks

Full verification starts with retained baseline payloads and relationships, then
checks the subsequent session journal. It validates exact money, allocations,
source/acceptance/duplicate dependencies, currencies and provenance. It does not
reconstruct expired ordinary history or rely on permanent per-revision hashes.

- An unclean marker or an existing recovery journal requires a full check.
  That check reports errors and does not repair data.

The encrypted clean marker deliberately accepts reduced detection of offline
changes. SQLCipher authentication and encountered database/I/O errors still apply.
A clean marker is not proof that an internally consistent edit reflects user
intent.

Normal startup still prepares ledger and Review views. This does not promise
constant-time startup or close on every file.
