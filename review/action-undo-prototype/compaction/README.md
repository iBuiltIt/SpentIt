# Closed-copy compaction and backup investigation

**Owner stop, 2026-11-08:** the generic row-capture Undo architecture was rejected.
See the [rethink handoff](../../../doc/action-undo-rethink-handoff.md). This material is historical investigation
evidence, not authority to resume implementation, review or budget approval.

2026-11-08. Owner requested an explanation of compaction, backup size and reusable
space before deciding any storage budget. This is a bounded experiment on closed
synthetic copies, **not** implementation or approval of production compaction.
The owner's ordinary dataset is roughly 19,000 transactions; the measured 50,000-row
fixture is an exceptional scale, not a representation of their actual dataset.

## Actual results

Both completed UUID-length 50,000-row import fixtures were copied to fresh paths.
The original files were opened read-only and their complete SHA-256 hashes remained
unchanged. The helper links the same SQLCipher static library as the current
measurement build, applies the application's cipher compatibility/memory security,
DELETE journal, synchronous EXTRA, fullfsync ON, foreign keys ON, memory temporary
store, secure_delete ON and cache settings, then runs ordinary `VACUUM` on each copy.

| After successful close / operation | Current bytes | Prototype bytes |
| --- | ---: | ---: |
| Original physical main file | 61,399,040 | 105,791,488 |
| Original reusable freelist pages (4,096 bytes each) | 656 | 8,228 |
| Original live-page bytes | 58,712,064 | 72,089,600 |
| Compacted physical main file | 54,345,728 | 66,138,112 |
| Compacted freelist pages | 0 | 0 |
| Backup page-copy physical main file | 61,399,040 | 105,791,488 |
| Backup page-copy freelist pages | 656 | 8,228 |

Compaction reduced the prototype by **39,653,376 bytes (37.48%)**, from 100.89 MiB
to 63.07 MiB. Current storage also shrank by **7,053,312 bytes (11.49%)**, from
58.55 MiB to 51.83 MiB. Comparing both compacted files leaves **11,792,384 bytes
(21.70%)** prototype overhead, rather than the uncompacted 72.30%. Decimal sizes
are 66.138112 MB versus 54.345728 MB. Compaction removes whole unused pages and
packs partially filled B-trees; therefore its reduction exceeds freelist bytes
alone. The remaining size includes real stored rows/indexes/schema and page
overhead. It is neither zero-cost Undo nor proof that all remaining overhead is
unavoidable.

The encrypted SQLite backup mechanism copied the original freelist and physical
size unchanged in both cases. It does **not** compact simply by making a backup.
The helper uses the same `sqlite3_backup_init` / 128-page `sqlite3_backup_step` /
`sqlite3_backup_finish` loop as `ImportStore::backup_to`; it measures that mechanism,
not the full application's candidate publication, document-lock, clean-marker or
backup UI workflow. A future compact-backup action would need an explicit design;
none is implemented here.

## Verification and limits

Before/after VACUUM, after backup, and after reopening each output, the probe checks
SQLCipher integrity, SQLite integrity and foreign keys. It compares every persistent
table's values exactly by SQLite type, including INTEGER, NULL, text/blob bytes and
floating-point bits, and compares implicit rowids for rowid tables. It also checks
all stored schema SQL, application ID and user version. Physical B-tree root page
locations are deliberately excluded from schema equality. Current has 95 tables /
350,018 rows; prototype has 109 tables / 600,023 rows. All comparisons passed.
Both outputs refuse unkeyed and incorrect-key reads. No financial records are
printed. Original SHA-256 equality is recorded in the [manifest](results-final/manifest.json).

These are successful operations on copies of two synthetic fixtures. They do not
prove production compaction's interruption/disk-full/cancellation behavior, temporary
disk requirement, atomic publication, backup recovery workflow, responsiveness or
all application ordering assumptions. VACUUM can change implicit rowids in general;
the equality check passed for these specific fixtures, not every possible document.
No compaction peak-space measurement, fault sweep, UI change, private-data inspection
or 19,000-row rerun was performed. Production Undo remains incomplete and no proposed
storage limit has been accepted.

## Reproduction and evidence

From the repository root on the verified macOS build:

```sh
c++ -std=c++20 -Wall -Wextra -DSQLITE_HAS_CODEC \
  -Ibuild/dependencies/sqlcipher-build \
  review/action-undo-prototype/compaction/probe.cpp \
  build/import-ui-rules-release/src/storage/libspentit_sqlcipher.a \
  /opt/homebrew/opt/openssl@3/lib/libcrypto.dylib \
  -framework Security -framework CoreFoundation \
  -o /private/tmp/spentit-compaction-probe
python3 review/action-undo-prototype/compaction/run.py \
  --probe /private/tmp/spentit-compaction-probe \
  --source-root /private/tmp/spentit-action-broader-uuid-1 \
  --work /private/tmp/spentit-compaction-check-2 \
  --evidence review/action-undo-prototype/compaction/results-final
```

Use new work/evidence paths for another run; the runner refuses existing directories.
The source-root layout is the broader measurement runner's completed synthetic
`import-50000-current/import.spentit` and `import-50000-prototype/import.spentit`.
The synthetic fixture key is 28 bytes including its terminating NUL, matching the
original analyzer/fixtures; it is not a user credential. Compiler warnings were
empty. The linked runtime reports SQLite 3.53.4 and SQLCipher 4.19.0 community.

Raw [current](results-final/current.txt) and [prototype](results-final/prototype.txt)
logs, [build/source hashes](results-final/build.json), and original hashes/exit codes
are retained. The initial `results/` attempt stopped before VACUUM because this
production-linked SQLCipher library does not expose DBSTAT; the optional payload
query was removed and both fresh final runs passed. No database copies are committed.
Independent review of these final findings was stopped by the owner. No final
review approval is claimed; see the rethink handoff for the recorded provenance
question. Do not resume review merely to complete this experiment.
