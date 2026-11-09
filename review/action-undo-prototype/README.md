# Unified Undo implementation investigation

**Owner stop, 2026-11-08:** the generic row-capture Undo architecture was rejected.
See the [rethink handoff](../../doc/action-undo-rethink-handoff.md). This material is historical investigation
evidence, not authority to resume implementation, review or budget approval.

2026-11-08. **Development-only prototype; no production Undo delivery.**

**Owner follow-up:** the initial hard-gate stop below is historical. The owner
subsequently authorized [broader storage-budget investigation](broader-storage/README.md)
before deciding a final limit, indicating that roughly 8% lasting overhead might
be acceptable. This is permission to measure further, not a blanket storage waiver.
The earlier measurements and independent checkpoint review remain preserved below.

The approved [historical design](../../doc/action-undo-rejected-design.md) requires a representative
storage prototype before broad conversion. The [historical inventory](../../doc/action-undo-rejected-inventory.md)
records the current 94-table conversion surface.

`SPENTIT_ACTION_JOURNAL_PROBE=ON` builds a test executable that calls actual
`ImportStore` rename, acceptance and prior-source removal commands. It installs
TEMP capture triggers; the first-before records themselves are persisted in the
same SQLCipher transaction as the actual writes. Typed encoding preserves NULL,
TEXT/BLOB distinction and exact integer values and refuses REAL values.

The initial capture/replay experiment has demonstrated canonical table equality,
FK checking and exact money after reversal on synthetic fixtures. It deliberately
leaves operation identities monotonic. The legacy full verifier fails with
`SQLITE_CORRUPT` after rename/accept reversal; this is **not** recorded as a passing
full verification. Existing registry-count/orphan checks require conversion in
the new format. Removal's full verifier passed in the first 1,000-row round trip.
These initial results precede the final prototype and do not establish financial,
recovery, fault-injection, concurrency or storage acceptance.

Capture regressions found and corrected two concrete SQLite issues:

- BEFORE INSERT can see an unallocated integer key (`-1`), so absence capture
  happens AFTER actual insertion. Existing keys are captured beforehand to cover
  the existing same-primary-key replacement path.
- An outer INSERT OR REPLACE overrides a trigger's INSERT OR IGNORE policy.
  Capture now uses explicit `ON CONFLICT (...) DO NOTHING`, preserving the first
  image. Arbitrary alternate-unique-key replacement is not covered by this probe.

The fixture regressions cover ignored inserts, same-primary-key replacement,
key-change refusal, rollback of data and journal together, net no-op coalescing,
integer extrema, NULL/empty/binary values, malformed/REAL rejection, original
same-domain registry restoration and atomic missing/wrong-domain refusal. Root,
child and request-digest ownership are still coordinator work, not established
by these domain-only checks.

The source-only conversion experiment also exposed implicit source `rowid`
ordering. Its revised membership records source order explicitly; the fixture
now removes the earlier of two sources. A separate isolated compilation of the
actual store was built to initialize immutable bodies and memberships before
imports, retain source order, move outgoing live FKs onto membership and pin bodies
without copying them into inverse images. Production targets do not use that SQL
adapter. Full snapshot-body coverage, collector correctness and paired physical /
DBSTAT / peak measurements remain gates, not inferred successes.

No application UI, final History layout or manual text/screenshots is changed.

## Hard storage gate — stable final comparison

The byte-identical `session_storage_measure.cpp` import-only driver used 1,000
synthetic CAD -1.00 transactions, the same Release toolchain, pinned SQLCipher
and OpenSSL settings against `cf8f1a5` and the isolated payload representation.
Both runs completed their exact-balance and existing full-verification checks,
successfully closed, reopened and closed again. The candidate begins directly
with split tables; these results do not include a runtime conversion copy.

| Stable checkpoint / metric | `cf8f1a5` | Prototype | Excess |
| --- | ---: | ---: | ---: |
| Initial close, file bytes | 1,556,480 | 1,683,456 | 126,976 |
| Reopened/closed, file bytes | 1,556,480 | 1,683,456 | 126,976 |
| Reopened/closed, live page bytes | 1,556,480 | 1,667,072 | 110,592 |
| Final retained B-tree payload bytes | 602,151 | 663,049 | 60,898 |

The [summary](final-import/summary.csv), [baseline output](final-import/baseline-import.txt),
[candidate output](final-import/candidate-import.txt),
[baseline DBSTAT](final-import/baseline-import-pages.csv),
[candidate DBSTAT](final-import/candidate-import-pages.csv),
[binary/driver hashes](final-import/binary-hashes.txt) and
[comparison runner](final-import/measure.py) preserve exact evidence. The runner
exited 2 because the size gate failed. It does not itself establish semantic,
recovery or peak-journal acceptance. No VACUUM, padding, altered durability,
plaintext sidecar or omitted financial dependencies was used.

The [read-only payload audit](final-import/payload-audit.txt) checked all ten
split tables: **zero payload bodies lack live memberships**. Thus the import-only
excess is present without removed bodies waiting for collection. The structural
measurement executable does not install the inverse journal or action coordinator;
the representation exceeds the existing bound before adding those records.
The [isolation test](final-import/production-refusal.txt) confirms normal production
opening rejects the prototype with `SQLITE_SCHEMA` and leaves every file byte
unchanged. Prototype initialization uses application ID 1397770578; production
continues using 1397770577.

**Stop decision:** broad conversion did not begin. The approved design requires
a stop on measured excess. This representation fails that gate; the experiment
does not prove that all encodings or a different architecture must fail. The
recommended next decision is to rework the representation under the unchanged
cap, then repeat the prototype and independent review. Changing the storage
requirement is a separate explicit owner choice; no relaxation is inferred from
implementation Go.

## Representative command verification

The final actual-store probes use 1,000 rows per source and the initialized split
schema. [Rename](rename.txt), [acceptance](accept.txt) and
[earlier-source removal](remove-pinned.txt) passed canonical equality across
non-monotonic tables, foreign-key checks and exact balance after inverse replay.
The removal fixture contains two sources, removes the first, keeps its body pinned
once while its live membership/ledger rows are absent, restores it without an
external file, preserves source order 1/2 and rejects deletion of both inactive
pinned and active bodies. Successful inverse replay releases its pins and collects
unreachable bodies created by the action. The collector is prototype-only, not
full close/recovery integration.

Removal passed the existing full verifier after restoration. Rename and acceptance
record a full-verifier `SQLITE_CORRUPT` failure separately, not a pass; the old
verifier requires a live legacy event for every operation-registry identity. The
prototype keeps identities monotonic as required, while canonical comparison and
FK checks provide separate bounded evidence. The broad action-aware verifier has
not been implemented.

[Codec/capture CTest](codec-test.txt) passed **1/1**. Independent pure-schema checks
also exercised all ten actual table definitions, missing bodies/live-parent FK
refusals, immutable body/member updates, same-ID/different-body refusal, stable
source order and allocator exhaustion rollback; these used system SQLite in
memory, not SQLCipher recovery tests.

Measured action-phase file sizes in the logs are point-in-time readings, **not**
a proven worst-case transient journal/pin peak. The source-only conversion probe
is exploratory and is not the stable storage comparator. No full mixed chain,
prepared-import protocol, root/child/digest authorization, complete high-water
allocation, complete rule/transfer/Review payload-family conversion, independent-process recovery, fault sweep, backup restoration,
sanitizer or rules-disabled/platform/native-UI verification is claimed. The
prototype's per-table metadata currently originates during schema initialization;
standalone recovery/replay requires the planned durable manifest.

## Independent implementation review

The independent Reviewer returned **APPROVED — bounded prototype, verification
evidence and storage-gate stop; zero unresolved blockers for this checkpoint**.
Review covered capture conflicts, stable source ordering, membership integrity,
immutable payloads, registry-domain checks and production-format isolation. The
Reviewer independently ran the codec/capture regression executable successfully
and checked the final paired measurement, zero-orphan audit and unchanged-file
refusal evidence. Full unified Undo is incomplete and **not approved as delivered**.
The storage excess triggers the design's stop gate; it does not prove every
representation must fail. Modified-document relative file links and
`git diff --check` passed.
