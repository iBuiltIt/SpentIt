# Independent implementation review

2026-11-08 — **APPROVED for the bounded storage prototype and measurement evidence.**
There are zero unresolved blocking findings within this scope. This review does
not approve a new owner storage limit or claim production unified Undo delivery.

## Reviewed scope

The independent reviewer inspected the immutable-body/live-membership adapter,
typed first-before capture and coalescing, payload pins and collection, close
cleanup integration, serial VFS measurement, common financial workload driver,
runner, summarizer and component assembler. The review also covered the actual
SQLCipher lifecycle tests and both prepared-read optimizations. The latter retain
the stored schema and financial checks; typed query comparisons cover fallback,
explicit and absent values, allocation metadata/category-version edge cases and
UNION results. The 1,000-row edit and mixed workloads retain identical non-timing
measurements and byte-identical DBSTAT output across the respective read changes.

Earlier review findings were resolved: mixed workloads now remove imports from
prior completed sessions; durable source/record high-water allocation avoids
reuse against pinned bodies; measurement resets preserve creation/open/verification
peaks; VFS and lifecycle checks are checked-in reproducible tests. The optional
parallel runner isolates documents and subprocesses, rejects duplicate cases,
collects failures and publishes completion only after all requested runs succeed.

## Verification inspected and independently checked

- Recorded macOS Release CTests pass 3/3 for codec/capture, SQLCipher VFS and bounded
  close lifecycle checks. Failed cleanup, cancellation, precommit refusal and a
  first-write disk-full fault preserve complete persistent state; lost
  acknowledgement and process exits around commit retain coherent outcomes.
- The reviewer independently ran the codec and VFS CTests: 2/2 passed. Lifecycle
  assertions and recorded results, including strengthened query checks, were
  inspected rather than represented as a separate reviewer execution.
- All 30 assembled main-matrix runs and 810 physical phases independently
  reconcile: main length equals live plus free pages; cumulative simultaneous
  peaks are monotonic and bound phase/current totals; final main/live sizes match
  summary and DBSTAT allocated/payload sums.
- All assembled raw logs and page reports are byte-identical to their named
  components. Component manifest hashes match. The assembler requires complete
  comparison groups, unchanged driver/analyzer and comparator binaries, and
  identical continuation/mixed prototype binaries. The separately reviewed
  prepared-read variants have explicit provenance and storage-parity checks.
- The six UUID sensitivity runs and 42 physical phases were independently
  reconciled. They remain a distinct import-only component with their original
  read-variant provenance. Timing is excluded from comparison claims.

Across the 36 unique runs, the largest increases relative to unchanged current
storage are **22.0996% retained B-tree payload** (UUID import, 10,000 rows),
**89.5995% main-file length after close** and **117.8813% simultaneous VFS lifetime
peak** (both older-import removal, 10,000 rows). These independently match the
generated comparison tables. The proposed 1.30× / 2.00× / 2.50× regression gates
cover these observed results, but remain an owner decision limited to the declared
benchmark suite. They are not universal session bounds or final-feature estimates.

## Limits preserved

This is a prototype of representative storage and forward/close behavior. It
does not complete the common coordinator, prepared imports, all command/payload
families, action-aware recovery, UI routing or full fault sweeps. The earlier
rename/acceptance inverse verifier mismatch remains explicitly unresolved for
production delivery. Sanitizer, rules-disabled, native UI and other-platform
checks remain unverified for this measurement step.

Successful close expires inverse/action rows and pins and collects unowned
bodies; freed pages can remain reusable in the physical file. Correctness,
recovery, exact financial relationships and full remeasurement of the completed
implementation remain gates. No early expiry, silent truncation or weakened
durability is authorized by the proposed budget. Final Change History layout and
manual text/screenshots remain separate work.

See the [methodology and results](README.md),
[assembled comparisons](completed-matrix/comparisons.csv),
[component provenance](completed-matrix/component-provenance.json) and
[UUID comparisons](uuid/comparisons.csv).
