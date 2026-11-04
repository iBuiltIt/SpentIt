# Encrypted storage and recovery prototype

Last updated: 2026-10-25

## Scope and decisions

The owner approved SQLCipher Community Edition for an isolated synthetic-data prototype after historical-currency engine support. The `spentit_storage_prototype` target is optional, independent of UI, and depends on the engine and a private SQLCipher target. The engine does not link storage. This is **not** the application document schema or a completed storage feature.

The prototype saves immutable imported evidence and a validated transaction snapshot (original amount, effective amount, ordered allocations, category/memo, units, definition identity, code, scale). A saved draft survives reopen and is excluded from `committed_amounts()`. This query returns individual amounts, never an implicitly converted or mixed-currency total. It is a verification API, not a report engine.

`commit(ids, operation)` publishes the entire selected batch, including postings, draft transitions and persistent operation history, inside `BEGIN IMMEDIATE` / `COMMIT`. Each draft belongs to at most one operation. Retrying the same operation with the same unordered member set returns successfully without another posting/history entry; different membership or duplicate identifiers is rejected. All snapshots are revalidated through the engine before commit and on reopen. Callers receiving an error or losing the response must reopen and retry the **same** identity and member set, not invent a new operation. The observer hook is exclusively a prototype testing seam at either side of durable commit.

There is no editing/deletion/reversal API for committed records, no transfer matching, and no account model. Committing two opposite synthetic amounts tests batch atomicity, not financial transfer semantics. History contains immutable operation membership over immutable snapshots; it is not the final change-journal or undo design. Draft editing and discarding also remain future use cases.

## Encryption and durability configuration

- Pinned SQLCipher 4.19.0 / SQLite 3.53.4, using OpenSSL. See [dependency provenance](../third_party/sqlcipher/README.md).
- Passwords are passed through `sqlite3_key`, never interpolated into SQL or logged. Empty passwords are rejected. SQLCipher 4 compatibility settings retain 4096-byte encrypted pages, PBKDF2-HMAC-SHA512 with 256000 iterations and page HMAC-SHA512, without a plaintext header. Settings are read back and checked. Memory wiping is enabled in SQLCipher; this does not promise wiping caller-owned C++ strings, swap or core dumps.
- Existing documents open without `CREATE`, authenticate by reading the schema before any configuration writes, and require the prototype application ID and schema version 1. Unknown versions fail without migration. Plain SQLite cannot act as a fallback. New files use exclusive creation and mode 0600. Paths must be real paths without symlink components under SQLCipher's `NOFOLLOW` policy.
- Rollback journal `DELETE`, `synchronous=EXTRA`, `fullfsync=ON`, foreign keys, secure deletion, and memory-only temporary databases (`SQLITE_TEMP_STORE=3` at build time; `temp_store=MEMORY` on connections). Journal and durability mechanisms are retained. WAL is not used or evaluated by this prototype.
- A transaction is acknowledged only after successful SQLCipher commit. Successful new-file creation and backup publication also synchronize the containing directory. These are OS/filesystem durability requests, not proof against every physical power-loss or device-cache failure.
- Loadable extensions are disabled. SQL length is bounded at 64 KiB, a SQLite row/value at 2 MiB, evidence and each text field at 1 MiB, snapshot text/evidence at 8 MiB, allocations and commit members at 10000, identifiers at 128 ASCII identifier characters, and password input at 1024 bytes. No query, input record, key or path is included in storage error messages.
- Engine validation and strict schema/foreign-key/immutability constraints complement one another. Reopen also runs SQLite, cipher, relationship, definition and split validation. The prototype rejects inconsistent values rather than repairing them. This full scan is intentionally conservative and is not a responsiveness claim.

## Backup and recovery behavior

`backup_to(new_path, password)` exclusively creates `new_path.partial`, copies through the supported encrypted-to-encrypted SQLite backup API, validates the result, closes it, and reopens it with the backup password. It then publishes using a POSIX hard link that cannot replace an existing destination, synchronizes the directory, removes the temporary link, and synchronizes again. A live main-file copy is never used.

On failure, the source and prior backups remain intact. An unpublished candidate may remain at `.partial`, with an encrypted journal if interrupted. It is not a completed backup. A retry uses a different new destination or requires deliberate handling of the abandoned candidate; the API never automatically deletes it. If failure occurs after publication, a valid destination may already exist: inspect it before retrying. Directory-sync and publication errors are surfaced; these filesystem boundaries are not covered by the SQLite VFS injection tests.

Restoration is exercised by opening a backup, validating it, then using the same backup API to materialize a **new** restored document and reopening that file. There is no in-place overwrite restoration. A backup can use a different password; that does not change the source password. There is no password-reset mechanism. In-place password changes and migrations are intentionally absent, so they cannot silently destroy a known-good document; their future workflows still need design and failure tests.

## Build

Prerequisites: the engine toolchain, a C compiler, make, Python 3.9+ for the helper, and OpenSSL development headers/library. Downloads and compilation are explicit; ordinary engine builds remain dependency-free. Generate the reviewed amalgamation in an ignored directory:

```sh
curl --fail --location https://github.com/sqlcipher/sqlcipher/archive/refs/tags/v4.19.0.tar.gz -o /tmp/spentit-sqlcipher-4.19.0.tar.gz
python3 tools/prepare_sqlcipher.py /tmp/spentit-sqlcipher-4.19.0.tar.gz
cmake -S . -B build/storage-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DSPENTIT_STORAGE_PROTOTYPE=ON \
  -DSPENTIT_SQLCIPHER_SOURCE="$PWD/build/dependencies/sqlcipher-build"
cmake --build build/storage-debug
ctest --test-dir build/storage-debug --output-on-failure -j 3
```

Point `OPENSSL_ROOT_DIR` at your installed provider if CMake cannot discover it. The tested macOS installation used `/opt/homebrew/opt/openssl@3`; this is not hardcoded in the build. SQLCipher's generation script encountered a sandbox temporary-file ownership error in this environment and completed when the same generation command was allowed outside the sandbox. No upstream changes were needed.

For Release use a separate `build/storage-release` with `-DCMAKE_BUILD_TYPE=Release`. For sanitizer coverage use `build/storage-sanitized`, Debug and `-DSPENTIT_SANITIZERS=ON`; this instruments both first-party code and the SQLCipher amalgamation, but not the system OpenSSL library. The generated-source hashes are enforced by CMake; unexpected regeneration changes require review, not removal of the check.

## Verification and limitations

Actual verification on 2026-10-25: macOS 27.0.1 arm64, AppleClang 21.0.0, CMake 4.4.4, Ninja 1.13.2, SQLCipher 4.19.0 / SQLite 3.53.4 and OpenSSL 3.5.9.

| Configuration | Actual result |
|---|---|
| Debug | Configure/build succeeded; 17/17 engine/storage groups passed. The final added plaintext-rejection check also passed in a targeted validation rerun. |
| Release | Configure/build succeeded; 17/17 groups passed. |
| ASan/UBSan Debug | Configure/build succeeded; 17/17 groups passed; no sanitizer diagnostics. |

No compiler warnings were emitted. The test log scan found none of the synthetic private-data markers. The dependency preparation helper succeeded both with existing sources and from a fresh temporary extraction using Python 3.9.6; generated hashes matched the pin. The offline currency check and compiled simulated catalog-withdrawal regression also passed. Local Markdown links and anchors were checked. Git status/diff remains unavailable because this workspace has no Git metadata.

The successful I/O traces yielded the following fault positions; each was exercised with a fresh synthetic fixture in each configuration:

| Operation | Disk-full write failures | I/O write/sync failures | Abrupt process exits |
|---|---:|---:|---:|
| Save draft | 30 | 33 | Not separately enumerated |
| Commit batch | 26 | 29 | 29, plus immediately before and after durable commit |
| Backup | 59 | 65 | 65 |

Additional checks cover original/effective amounts, retained HRK/BGN definitions, signed integer extremes and mixed-sign allocations, conflicts and duplicate operation membership, missing drafts, immutable evidence/history constraints, wrong/empty passwords, plaintext rejection, altered authenticated values, unsupported schema, corrupt backups, alternate backup passwords, restoration, resource rejection, and writer contention. Failed backup candidates never replaced the previous known-good file.

Tests are synthetic and use isolated private temporary directories. The test-only VFS proxy delegates to the real OS VFS except for a chosen write/sync returning an error or terminating the subprocess. It is never linked into the storage library.

The harness enumerates write and sync positions observed on successful draft/batch/backup operations, then injects one failure per fresh fixture. It separately checks abrupt exits throughout commit and backup I/O and immediately before/after the batch COMMIT. Prior backups are compared byte-for-byte and reopened. Raw database/journal/candidate files are scanned for synthetic evidence, category, memo and password markers before recovery where relevant. Absence of those markers is evidence for these paths, not a universal leakage proof.

The prototype is single-owner and single-thread. SQLite writer contention is tested, but application-wide instance locking, stale-lock recovery, UI document switching, OS credential storage, exports, physical disk exhaustion, actual power removal, network filesystems, migration, password-change interruption, report correctness and performance are not validated. Windows and Linux developer builds of the storage library are described in [Windows development setup](windows-development-setup.md) and [Linux development setup](linux-development-setup.md). The crash-injection harness recorded above stays Unix-only. Do not use the prototype as an application for personal financial documents.

## References

- [SQLCipher API and encryption settings](https://www.zetetic.net/sqlcipher/sqlcipher-api/)
- [SQLCipher encrypted backup API support](https://discuss.zetetic.net/t/using-the-sqlite-online-backup-api/2631)
- [SQLite synchronization semantics](https://www.sqlite.org/pragma.html#pragma_synchronous)
- [SQLite backup API](https://www.sqlite.org/backup.html)
