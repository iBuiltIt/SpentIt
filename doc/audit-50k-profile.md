# 50k full-audit profiling

Date: 2026-11-06. Status: **profiling and proposals; no further optimization implemented**.

The preceding Undo/audit improvements are committed as `0edf528`. This investigation
uses synthetic data only; it neither opens private datasets nor changes application
validation. See the [earlier report](undo-audit-profile.md) for the 15k comparison
and the initial 73.7659-second 50k observation.

## Method and reproduced result

The same driver and final rules-enabled macOS ARM64 Release libraries created a
fresh SQLCipher account with 50,000 distinct ordinary transactions, accepted all,
deleted the whole selection, undid deletion and ran full verification. It asserted
zero rows/balance after Delete and the original row count/exact balance after Undo.
No transfers, category definitions or currency reassignments occur in this workload.
Setup and UI refresh are excluded from phase times; operating-system caches were
not flushed. No agent builds or tests ran concurrently with a timed run.

The committed-build driver SHA-256 was
`f9c1ff7b40f5919a37c8b83249d4d243c2131df3830911c19c133e51101dfbf6`.
One diagnostics-enabled run with three five-second native samples from its exact
PID reproduced Delete / Undo / audit at **28.9854 / 22.0495 / 74.4080 seconds**.
Every assertion passed. Sampling is observational overhead; this is a reproduction
and phase attribution, not a new uninstrumented median.

| Audit phase | Seconds | Share |
|---|---:|---:|
| Committed-history replay | 29.761496 | 40.0% |
| Category/provenance validation | 27.989840 | 37.6% |
| Imported evidence | 9.206485 | 12.4% |
| Rules | 3.741120 | 5.0% |
| Integrity/cipher/FK | 2.168204 | 2.9% |
| Transfers | 1.502034 | 2.0% |

The transfer phase still performs reconciliation on this no-transfer workload;
its time is not a measurement of transfer-heavy behavior. Other phase time is small.
There were 146 statement preparations and 14,374,035 reuses, versus 146 and
2,174,979 in the preceding final 15k profile. Rows grew 3.33× but statement uses
6.61×. Preparation is not the dominant remaining problem.

The five-second samples began approximately 0, 20 and 45 seconds into the audit.
The middle sample had 3,701 main-thread samples: 3,699 were within category
validation and 2,459 within `entry_snapshot`. The last sample had 3,671 main-thread
samples, all within committed replay: 2,976 were within `entry_snapshot`, including
1,213 within `load_row`. These are nested inclusive counts and must not be added.
They identify reconstruction/read paths, not individual SQL costs.

## Observation-only cache/query instrumentation

A temporary copy of the storage translation unit counted audit-only source-row
and snapshot cache hits/misses/admission failures, and summed each SQL template's
`sqlite3_step` duration, VM instructions and full-scan steps. It used the same
compiler optimization, SQLCipher and dependency libraries. No query, cache budget,
validation predicate or transaction boundary was changed. Only the temporary
synthetic driver enabled collection, and logs contain SQL templates and aggregate
counts, never bound parameters. This instrumentation was not added to the app.

The SQL count means step calls (including row and done results), not executions.
Step timings exclude statement binding, construction, copying and C++ validation;
instrumentation overhead prevents substituting their wall times for baseline
medians. The temporary instrumentation's SQL counters reset on each statement
checkout, avoiding accumulation from prepared-statement reuse.

Both observation-only runs passed every financial assertion and full validation.
Their audits took 11.3449 s at 15k and 73.6044 s at 50k. The committed application
run's phase attribution was reproduced closely; these timings are not independent
performance baselines because of instrumentation.

| Audit cache/work | 15k rows | 50k rows |
|---|---:|---:|
| Source-row cache hits | 135,000 | 442,372 |
| Source-row cache misses/reconstructions | 15,000 | 268,192 |
| Source rows retained | 15,000 | 36,363 |
| Source-row estimated bytes | 55,365,000 | 134,215,833 |
| Source-row admission refusals | 0 | 231,829 |
| Snapshot cache hits | 105,000 | 139,436 |
| Snapshot misses/reconstructions | 45,000 | 360,564 |
| Distinct snapshots in workload | 45,000 | 150,000 |
| Snapshots retained | 45,000 | 59,758 |
| Snapshot estimated bytes | 50,535,000 | 67,108,234 |
| SQL step calls | 2,880,136 | 17,671,944 |
| SQL VM instructions | 74,377,171 | 389,147,934 |
| SQL full-scan steps | 614,993 | 2,049,993 |
| Time inside SQL step calls | 7.003628 s | 46.244308 s |

**Measured cause: bounded caches stop admitting entries, rather than replacing
older entries.** [VerificationRows](../src/storage/import_store.cpp) has a 128 MiB /
65,536-row budget; [entry_snapshot](../src/storage/committed_store.inc) has a
64 MiB / 65,536-snapshot budget. Both hit their byte limits first in the 50k
fixture. The first rows remain resident throughout verification; later rows remain
uncached however often they are revisited. The 15k fixture fits both budgets.

Consequently, 50k has 210,564 snapshot reconstructions beyond its 150,000 distinct
snapshots, and 218,192 source-row reconstructions beyond its 50,000 distinct rows.
`load_row` reconstructs the complete RowView, including current draft, source
associations, ownership, metadata and state. Historical consumers often need only
its source facts. These misses amplify many individually targeted queries. The
counts explain the observed change in slope; they do not establish an asymptotic
quadratic algorithm or the exact speedup of a proposed cache policy.

The revision-to-history map introduced by `0edf528` is a separate cache: this
workload has 100,000 change/Undo revisions, within its 262,144-member and 64 MiB
budgets. Its budget-exhaustion fallback is not the cause of this cliff.

### Where SQL time goes

Top targeted reads in the instrumented 50k audit:

| Query purpose | Step calls | Seconds | Full-scan steps |
|---|---:|---:|---:|
| Snapshot allocations (`entry_allocation_values`) | 721,128 | 7.2864 | 0 |
| Historical currency timeline | 978,759 | 4.6309 | 0 |
| Revision-zero acceptance/history | 380,480 | 2.0599 | 0 |
| Associated source IDs | 268,192 | 1.9871 | 0 |
| Snapshot fields (`entry_values`) | 360,564 | 1.4664 | 0 |
| Draft allocation metadata | 836,384 | 1.3797 | 0 |
| Committed currency-reassignment markers | 410,564 | 1.3239 | 0 |
| Pending deletion state | 268,192 | 1.2875 | 0 |

The allocation query took 0.8810 s for 90,000 steps at 15k. Its step count grew
8.01× and time 8.27×; the main growth is repeated work, not a newly expensive single
lookup. Historical currency performs almost a million timeline queries even though
this fixture has no currency changes; reading account creation currency adds
another 978,759 steps / 0.8040 s. Empty and unchanged facts still cost SQL calls.

Total full-scan steps grew 3.33×, proportional to row count. Leading scans are
whole-history counts, ordered snapshot enumeration and ordering reconciliation.
The hot point reads above reported zero full scans and bounded VM work per use.
This does **not** prove every query is optimal, but it argues against another
blanket index addition as the first response. The preceding OR-query scan problem
has been replaced here by redundant reconstruction and read amplification.

## Proposed next improvements (not implemented)

1. **Use bounded audit working sets that follow the current transaction.** Keep
   source facts and the current/prior/original snapshots together while checking
   a transaction or receipt. Category validation already reads a head before
   calling `entry_snapshot`; pass that known source rather than reconstructing it
   again. Keep the head through consecutive revisions in the ordered category
   cursor. Carry the same source through committed before/after and original-Undo
   receipt validation. Replace audit-only first-entry retention with a bounded
   rolling/LRU policy where reuse spans calls. Retain the byte/member limits and
   recompute on eviction. Do not cache mutable source associations or heads across
   commands. This targets the measured cache cliff without merely moving it by
   increasing memory budgets.
2. **Read snapshot data in ordered batches/streams.** Supply fields, allocations,
   allocation identities, ownership and rule metadata from bounded ordered cursors
   or chunks instead of several point queries for each reconstructed snapshot.
   Feed the same engine validators and exact before/after/dependency/balance checks.
   Preserve order/position checks, orphan/extra-row detection and exact counts;
   do not use inner joins that silently hide missing metadata or allocations.
   This targets the remaining mandatory reads as well as repeated reads, and
   should be measured separately from the working-set change. Keep all data in
   memory within explicit limits or use existing encrypted storage; no plaintext
   spill files.
3. **Reuse the audited historical currency timeline.** Load each account's creation
   currency and ordered currency-change/Undo facts once per audit, then resolve
   currency at a history order from that timeline. Retain explicit per-revision
   reassignment markers and all validation of changes, reversals and definitions.
   An empty timeline can be reused only after it is actually read/validated, not
   assumed from the current currency. The measured timeline-plus-creation reads
   alone cost 5.43 s at 50k; that is a work opportunity, not a promised wall-time
   saving. A bounded indexed fallback must remain correct for large histories.

Prioritize the working-set change, then streamed reconstruction; the currency
lookup is a smaller, independently measurable follow-up. Do not skip the full
history pass, omit source validation, weaken integrity checks, certify early, or
replace exact comparisons with unchecked hashes. The SQLite integrity/cipher/FK
phase is only 2.9% of the reproduced audit and should remain intact. Transfer-heavy
optimization remains separately unmeasured.

## Acceptance checks for a future Go

- Repeat serial 15k and 50k runs, adding sizes around both observed byte-budget
  thresholds (about 20k rows for these snapshots and 36k for these RowViews).
  Vary split counts/text lengths: thresholds are bytes, not universal row limits.
- Report cache admissions/evictions, reconstructions, SQL steps/VM work, memory,
  phase timings, medians and ranges. Compare each change separately; do not promise
  an elapsed-time target from these counts alone.
- Force eviction and budget fallback while validating deep histories, currency
  changes/Undo, categories/rule ownership, split provenance, source associations,
  duplicate dependencies and linked transactions.
- Reject missing, duplicate, orphaned and out-of-order facts in every stream,
  including facts on either side of a chunk/eviction boundary. Compare rejection
  behavior with the current verifier on synthetic damaged files.
- Preserve cancellation checkpoints and unchanged file bytes on cancelled audits;
  retry cleanly. Repeat financial recovery, strict Undo and sanitizer coverage for
  any shared reader changed. No changes to durable/atomic command boundaries.

Only profiling documentation changed after the implementation commit. No additional
application optimization or schema change was made. Measurements cover synthetic
macOS repository execution, not native UI response, private data, other platforms
or transfer-heavy accounts.
