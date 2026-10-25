# Durable UI workload measurements

Last updated: 2026-10-25

This is a bounded synthetic benchmark of the experimental durable UI, not a production readiness or portability claim. The owner selected 100,000 transactions as representative and 200,000 as the stress case. These are workload sizes, not new record limits.

## Reproduction and measurement boundary

Build the Release configuration described in [the durable UI guide](import-ui.md), then run from a desktop session:

```sh
build/import-ui-release/src/ui/spentit_import_benchmark 100000
build/import-ui-release/src/ui/spentit_import_benchmark 200000
```

Run one process at a time without overlapping builds or tests. The executable creates its own temporary encrypted dataset, synthetic QIF and isolated Settings; normal completion removes them. It accepts a positive multiple of ten up to 200,000. Its fixed password belongs only to generated synthetic data. It never opens personal datasets or uses normal application Settings. A terminated process can leave its temporary synthetic files; it does not acknowledge the interrupted command as successful.

The fixture spans 28 dates, unique references/payees and alternating CAD withdrawals/deposits. Every tenth transaction has two mixed-sign allocations. Opening balance is explicitly confirmed. The benchmark checks exact projection, committed isolation, filtered counts/totals, unsaved editor survival while scrolling, saved-edit reopening, unchanged original evidence, commit receipt count and committed reopening. It uses the actual model, QML view and SQLCipher gateway. No journal, synchronization, encryption or verification setting is weakened.

Each command's `ms` runs from submission through the model's completion signal, including worker work and result delivery. Instrumented repository-boundary times identify ledger reads, balance reads, evidence/row retrieval, saves, ingestion, commit and verified opening. Those components overlap neither each other nor command completion, except that internal calls within the concrete repository are included in their outer operation. They are not CPU profiles or SQL-statement timings.

`ui_gap_ms` is the largest observed interval between 5 ms UI-thread heartbeat callbacks, including a 30 ms settling period after completion. It measures event-loop scheduling delays, not direct user-input or GPU latency. Native scrolling traverses 120 logical pixels every 16 ms for 240 timer events with an unsaved inline edit. Frame intervals come from queued `frameSwapped` notifications on the UI thread; they are a presentation proxy, not precise GPU timings. RSS is resident memory sampled after a phase; peak RSS is the process high-water mark. macOS reports both; other POSIX systems currently report only peak RSS.

The `import.benchmark_smoke` CTest entry uses 100 rows with Qt's offscreen platform to check the harness and correctness assertions. Offscreen results cannot establish native rendering performance. Large runs are deliberately manual and are not unit-test timing thresholds.

## Implementation boundaries

The UI worker retains verified account metadata and one ledger mode's source rows for read-only view changes. Sort/filter queries still use the existing exact application ledger calculation. Inspecting a record or viewing source evidence reuses the current view. Changing committed/projected mode reloads that mode's ledger. Every mutation, document open/close and command failure invalidates cached reads; writes use the unchanged atomic repository commands and reload authoritative state. A refresh failure still closes the session instead of acknowledging a partial view.

The GUI model holds an immutable typed ledger view and formats QVariant records only when delegates request them. It retires replaced views on the serial worker instead of destroying large row trees on the UI thread. Selection lookup also runs on the worker and stays keyed by batch/occurrence identity. Committed/discarded records no longer build duplicate-action labels, since their editor is read-only; pending duplicate review is unchanged.

## Scope limits

Opening still performs full encrypted-document verification. Imports and successful writes still reload the ledger; record inspection still parses retained source evidence. Those operations can take seconds at these sizes. This change does not implement cancellation, database pagination, a new schema, a different financial policy, or a general responsiveness guarantee. Full native keyboard/pointer/accessibility auditing, Linux/Windows validation and physical power-loss testing remain separate work.

## Verification

The affected `import.ui`, `import.qml` and new `import.benchmark_smoke` CTest entries passed in Debug, Release and ASan/UBSan after rebuilding each configuration on macOS arm64. Commands were `cmake --build build/import-ui-<configuration> -j 3` and `ctest --test-dir build/import-ui-<configuration> -R 'import.(ui|qml|benchmark_smoke|view)$' --output-on-failure`, using `debug`, `release` and `sanitized`; the expression selected three entries. Compiler output and test logs contained no compiler warnings, Qt warnings or sanitizer diagnostics. CMake reported unavailable optional Vulkan headers; native runs use the existing macOS Qt renderer. The full 37-entry engine/storage/UI suite was not rerun for this UI-only change.

New regressions verify current balances and filtered rows across committed/projected toggles, opening changes, failed/successful saves, commit, duplicate include/exclude, discard and switching between two documents. Read-only records no longer expose unused duplicate choices. Existing repair, restart, source preservation, inline recycling and Save / Discard / Stay checks still pass. Storage commands, schema, durable boundaries and failure-injection code were not modified. All 94 relative links in the seven changed documents, including referenced heading anchors, validated; `git diff --check` passed. The two interrupted baseline datasets were removed after verifying that they contained this benchmark’s generated synthetic files.

## Native results — 2026-10-25

One baseline and one optimized Release process per workload, run serially on an Apple M2 (8 CPU cores, 16 GiB RAM), macOS 27.0.1 arm64, Qt 6.12.0 Cocoa, SQLCipher 4.19.0 and OpenSSL 3.5.9. Synthetic files used the system temporary directory. No builds/tests overlapped the native measurements. These single-run observations are not distributions across machines or repeated trials.

Both baseline processes were deliberately terminated during their post-commit selected-record refresh, after the saved-edit reopen measurements below. Inspection was constructing duplicate-action labels for a read-only record, with a new candidate query for every candidate. This redundant work scales poorly for the fixture's repeated original date/amount groups. No completed baseline commit or committed-reopen timing, nor final baseline correctness success, is claimed. The optimized processes both completed the entire harness and printed `correctness=passed`.

Command times in milliseconds (sort/filter ranges cover different queries, not repeated trials):

| Operation | 100k baseline | 100k optimized | 200k baseline | 200k optimized |
|---|---:|---:|---:|---:|
| Import | 33369.5 | 33520.8 | 68066.4 | 65041.0 |
| Reopen pending | 9621.5 | 9433.6 | 19217.0 | 19397.0 |
| Sort (six columns) | 4112.4–4219.0 | 38.3–86.6 | 8347.0–8765.9 | 68.3–113.7 |
| Filter (match, no match, clear) | 3964.7–4250.9 | 20.8–34.1 | 7987.5–8263.8 | 47.0–55.7 |
| Inspect (first/last occurrence) | 4250.9–4264.7 | 133.0–138.9 | 8651.0–8681.5 | 261.3–270.9 |
| Save plus refresh | 4313.3 | 4181.4 | 8797.2 | 8537.3 |
| Close pending | 99.9 | 19.0 | 179.7 | 19.4 |

The baseline repeated roughly 3.7–3.9 seconds / 7.6–7.9 seconds of repository ledger loading for every sort and filter. The instrumented optimized sort/filter commands made no repository ledger or balance calls. Full reloads remain intentional after writes; the actual repository save took 11.9 ms / 12.9 ms, while its verified UI refresh accounts for most of the total save latency. Import/verified-open times remain substantial; the small differences in these single runs are not evidence of an ingestion or verification optimization.

| Observation | 100k baseline | 100k optimized | 200k baseline | 200k optimized |
|---|---:|---:|---:|---:|
| Peak RSS through saved-row reopen (MiB) | 813.30 | 452.59 | 1473.00 | 742.19 |
| Largest sort/filter UI heartbeat gap (ms) | 79.95 | 19.94 | 138.90 | 17.24 |
| Scroll frame interval p95 (ms) | 17.70 | 17.48 | 17.46 | 18.14 |
| Scroll frame interval maximum (ms) | 21.37 | 19.82 | 26.97 | 21.25 |

Scrolling was already reasonably paced in this bounded script; these frame measurements show no consistent improvement and do not prove a frame-rate guarantee. Startup still produced a 109.1 ms / 102.9 ms heartbeat gap. The new cache avoids blocking the UI on bulk view destruction, but this does not mean every interaction is free of stalls.

The optimized full commit plus refresh completed in 5534.7 ms / 11281.9 ms; committed reopen completed in 9550.3 ms / 19161.0 ms. Full-run peak RSS remained 452.6 MiB / 742.2 MiB. Native benchmark logs contained no Qt warnings. Release native runs were not sanitizer-instrumented; the separate affected tests were.
