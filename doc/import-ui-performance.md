# Durable UI workload measurements

Last updated: 2026-10-31

This is a bounded synthetic benchmark of the experimental durable UI, not a production readiness or portability claim. The owner selected 100,000 transactions as representative and 200,000 as the stress case. These are workload sizes, not new record limits.

These native durable-UI measurements predate both the schema-7 search checkpoint and the current account-scoped formats. Separate 2026-10-26 Release application-search measurements covered 100,000/200,000 synthetic rows and are recorded in [status](status.md#search-selection-and-atomic-bulk-ledger-changes--2026-10-26). Those measurements exclude database/source-file loading and Qt rendering; do not substitute them for current native UI or replay-performance evidence.

Current-format point-2 evidence and updated measurement boundaries are recorded in [hardening](hardening.md). The implementation and component-timing descriptions below describe the historical checkpoint; current benchmarks use the complete production gateway without the obsolete timing decorator.

## Diagnostics-guided startup and switching follow-up — 2026-10-29

The owner authorized a further performance pass and read-only opening/account switching on an existing private dataset. No dataset copy, financial export, screenshot, record content, account names or private filesystem paths were saved to the repository. Synthetic fixtures remain the regression and failure-test inputs.

Diagnostics identified two complete verifications during automatic startup: the welcome probe opened and verified the document, closed it, then the automatic Open repeated the work. Startup now submits a single controlled Open using the public credential. Authentication failure still presents the password form without an initial error. Welcome browsing and returning after Close still probe without activating a document; explicit Open remains required there. Corrupt, missing and locked documents still fail before activation.

Storage now reuses successfully validated row heads across the independent evidence, category, rules and history checks within one verification. Each verification starts with an empty cache, including explicit verification after opening; failed or cancelled verification cannot publish partial cache results. Successful verification seeds the connection's read-only row cache. Account ownership is checked before lookup. Any connection write, including rolled-back/trigger writes, invalidates it via SQLite's total-change counter; transactional reads discard and bypass the connection cache. Candidate enumeration is not cached. Neither the cache nor any financial record is persisted separately. Admission stops at 65,536 heads or 128 MiB of accounted object/string/vector capacity, with a node allowance; allocator overhead and transient copies are additional. On a miss or admission limit, the ordinary validated read still runs. Closing destroys the cache.

Repository-scoped format checks reuse the immutable format version during that operation. QIF parsing constructs its category-name lookup only when slash notation needs it; literal slash/category-class ambiguity rules are unchanged. No schema change, weakened verification, deferred integrity check, matching-policy change or durability-setting change is included.

Verification so far on macOS arm64:

- Broad Release selection: 91 checks passed in the sandbox; the two IPC checks failed there because the sandbox denies their socket access, then passed **2/2** with the required local IPC access. The original 93-check run took 118.97 s; the IPC rerun took 1.83 s.
- ASan/UBSan financial/storage/UI selection: **26/26 passed**, 148.33 s, with no sanitizer diagnostic. Rules-disabled Debug account validation/group/basic-import selection: **3/3 passed**, 21.00 s.
- Startup tests cover public, protected, missing, invalid and locked datasets; they now assert one startup Open completion. Final startup-only Release and sanitizer runs each passed **7/7 including init/cleanup**, 2.922 s and 4.203 s. The automatic startup has no path-edit debounce; welcome path editing retains its 350 ms debounce.
- The added basic-import regression exercises cached ledger reads, transactional read-your-writes, injected pre-commit rollback, successful retry, explicit verification and account isolation. Existing validation suites still reject corrupted evidence/history/ownership; backup and transfer/account/rule tests exercise their existing independent financial assertions.

- Ordinary-import injected disk-full/write-failure and process-interruption sweeps: **2/2 passed**, 317.44 s with both running in parallel. Existing restart/retry, before/after durable-boundary and encrypted-marker assertions passed. The larger grouped-import fault sweeps were not repeated in this follow-up.
- Rules-disabled final startup tests also passed **7/7 including init/cleanup**, 2.714 s.

The final rules-enabled Release native app (SHA-256 `006d63a0e7d2e34971cc84713863bbe4bc28a672b474cf53354902b981835f36`, Qt 6.12.0 Cocoa) was measured with diagnostics enabled, after all builds/tests finished. One process launch on the owner-authorized dataset reached observed matching-generation native readiness in **7.715 s**. Its single Open worker took **6.805 s**, including **6.375 s** verification and **0.341 s** snapshot construction. Initial GUI application took **0.165 s**. The earlier baseline executed two verifications totaling **25.003 s**, with probe plus open worker time **27.637 s**; the owner's launch-to-usable estimate was about 30 s. The baseline was not captured with the final external launch timer, so these are not identical timing boundaries.

Two switches to the owner-requested large account took **0.282/0.287 s** on the worker, including **0.145/0.146 s** ledger loading; GUI application was **0.183/0.184 s**. Observed native readiness was **0.617/0.567 s** after enqueue. An intervening smaller-account switch took **0.150 s** worker / **0.499 s** observed native readiness. The intermediate build before connection-cache retention took **1.809 s** worker, of which **1.670 s** was ledger loading. The owner previously observed about 2.5 s before this pass; that is not an instrumented baseline. All final observed operations succeeded, with zero reported event drops/health failures. Readiness is sampled via the 10 Hz semantic feed and a matching Qt native-frame proxy, not physical display/input latency. Warm filesystem cache, same-machine observations and the small native sample count limit generalization; no disk-cold, Linux or Windows performance claim is made.

Startup remains well short of near-instant: full integrity/evidence/financial-history verification remains the dominant cost. These changes reduce repeated work; they do not defer required checks until after activation.

Owner acceptance (2026-10-29): the owner measured startup at approximately **7 seconds**, accepted the current performance for now, and considers the application usable. This closes the current performance pass without claiming near-instant startup. A redesigned startup/loading screen is deferred; its design and implementation require a later discussion and explicit Go.

Final synthetic measurements used the rules-enabled Release/offscreen benchmark with diagnostics enabled and SHA-256 `2274c2b9b58d880a275f0a5265e78b370b8c06df20cfd9d03f79aecf48b1f939`. Runs were serial with no concurrent builds/tests/native measurements. All three 18,972-row grouped runs and the separate 19,000-row ordinary full run passed their independent correctness assertions.

| Grouped phase | Median | Range | Samples |
|---|---:|---:|---:|
| Verified reopen through model completion | 5.958 s | 5.831–6.027 s | 3 |
| Account switch through model completion | 0.410 s | 0.387–0.492 s | 36 (12 per run) |
| Preparation | 7.560 s | 7.389–7.622 s | 3 |
| Save and refresh | 2.349 s | 2.318–2.373 s | 3 |
| Resolve existing mapped transfers | 0.835 s | 0.816–0.848 s | 3 |
| Abort | 2.879 s | 2.789–2.957 s | 3 |

An earlier instrumented intermediate build in this pass, before row-cache reuse, measured grouped reopening at 13.397 s and switches at 0.703–0.785 s (one run). This is not the original pre-pass baseline; the historical prior-pass results below remain separate evidence.

The ordinary full run measured pending reopening **5.666 s**, saved reopening **5.463 s**, commit **8.435 s**, and committed reopening **9.389 s**. It passed exact signed-split totals, saved-edit/source-evidence preservation, search, both scroll paths, commit and reopen assertions; peak RSS was **298.422 MiB**. This one-run committed result remains substantial and is not a broad responsiveness guarantee. Offscreen frame samples do not establish native rendering speed. No 100k/200k durable workload or additional native platforms were tested in this follow-up.

Reproduction: use the existing commands below with `SPENTIT_BENCHMARK_DIAGNOSTICS=1`; run the grouped workload three times, serially, then the ordinary 19,000-row workload. The broad/fault/sanitizer selections below were used with the scope differences stated above. The earlier measurements below remain historical evidence, not results of these changes.

## Application performance work — 2026-10-29

The owner authorized performance before repeated-transfer matching. Matching policy remains mutual-unique-only; the accepted repeated-group extension is still pending. Existing schema-30/29 datasets remain usable without reimport or a format change.

The added `spentit_import_benchmark 18972 --groups` workload uses six newly mapped accounts, 18,972 synthetic transactions, 942 unique reciprocal transfer links and twelve deliberately unresolved repeated transfer allocations. It exercises the real gateway, serial worker, model and QML. It checks occurrence counts, exact projected amounts, unchanged committed balances, retained blockers, a saved ordinary edit with unchanged source evidence, rerunning resolution and whole-import Abort. `import.group_benchmark_smoke` uses 120 rows. `SPENTIT_BENCHMARK_PHASES=1` prints only phase labels and elapsed times for profiling.

CPU samples identified repeated SQL compilation in group status, repeated committed-balance reconstruction during draft linking, and duplicate-candidate probes scanning pending records. Opening also repeatedly validated and serialized the growing transfer set during history replay. Changes:

- Repository-operation-scoped SQL plans are reset and rebound, including insert plans during materialization/commit. The cache admits up to 256 plans with a 4 MiB retained-plan limit; financial query results are not retained in it.
- Candidate probes start from committed batches in the selected account, with the same candidate predicates and occurrence rules.
- One account-independent group snapshot survives read-only switching. Any connection write, including trigger writes or rolled-back writes, invalidates it through SQLite's total-change counter. Transactional reads and verification bypass and discard it. It never crosses a document connection.
- Independent draft linking and the draft-only portion of Abort reuse each account's starting committed balance within the command. Every event remains separately recorded in the same atomic transaction.
- A prefix of independent draft Merge history checks each reconstructed link and payload separately, then validates the accumulated set before another transition or final head acceptance. Append-only set validation covers every prefix; general operations and currency-reassignment histories keep ordinary chronological replay.
- Committed-source verification reads each batch’s evidence associations once, checking missing, repeated and incorrect mappings, instead of rescanning that table for every occurrence.
- A 32 MiB connection page-cache target reduces repeated decryption. Cipher memory security, HMAC, journal mode, synchronous EXTRA, fullfsync, foreign keys, cache spill and complete opening verification remain enabled.
- Identical progress labels no longer emit repeated UI-change notifications. Cancellation checkpoints remain unchanged.

The six-account Release/offscreen baseline measured preparation 75.821 s, opening 34.966 s, twelve switches 13.441–14.573 s, rerunning mapped resolution 16.271 s and Abort 31.167 s. The final matching workload measured preparation 8.212 s, opening 14.504 s, twelve switches 0.719–0.756 s, an ordinary save/refresh 2.849 s, resolution after that edit 1.627 s and Abort 3.025 s. Application startup measured separately at 0.393 s. All correctness assertions passed. Baseline preparation overlapped one storage compile; some baseline switches overlapped short builds/tests, and CPU sampling added overhead to selected intermediate runs. These are observations, not controlled distributions or a claimed speed ratio for the owner's export. Final timing runs had no concurrent builds/tests. The final association-read change affects committed verification, not these all-draft group timings.

A separate 19,000-row ordinary-import workload passed import, exact signed-split reconciliation, saved edit/evidence checks, search, two offscreen scroll traversals, successful commit, and committed reopen. Commit measured 8.921 s. Committed reopen dropped from 39.912 s before the association-read change to 26.409 s afterward. This is a comparison between two intermediate optimized builds, not the original application baseline. Full verified opening remains the principal unfinished performance target: neither the grouped nor committed workload opens in a few seconds. Further profiling should focus on repeated row reconstruction and category/committed-history verification while retaining every check.

Final verification is recorded in [project status](status.md). Runs use synthetic temporary encrypted datasets only. Qt offscreen timings measure application work and UI-thread scheduling, not native Cocoa rendering or physical disk-cold performance. A reopened connection is distinct from a cold operating-system page cache. Linux/Windows and the owner's actual dataset remain unmeasured.

### Reproduction and verification commands

Run timing workloads serially, without builds or tests competing for CPU/I/O:

```sh
cmake --build build/import-ui-rules-release -j 2
QT_QPA_PLATFORM=offscreen build/import-ui-rules-release/src/ui/spentit_import_benchmark 18972 --groups
QT_QPA_PLATFORM=offscreen build/import-ui-rules-release/src/ui/spentit_import_benchmark 19000
```

The broad Release selection excludes the long fault sweeps, which are selected separately. Sanitizer and rules-off checks are selected coverage, not full-suite claims. Final counts/results are in [status](status.md#performance-first-application-pass--2026-10-29).

```sh
ctest --test-dir build/import-ui-rules-release --output-on-failure -j 2 -E '(_io$|\.io$|_crashes$|\.crashes$|\.faults$|_faults$|backup_failures|backup_publication|backup_cancellation)'
ctest --test-dir build/import-ui-rules-release --output-on-failure -R '^(accounts.group_(io|crashes)|import.(io|crashes))$'
cmake --build build/import-ui-rules-sanitized -j 2
ctest --test-dir build/import-ui-rules-sanitized --output-on-failure -j 2 -R '^(accounts.(groups|validation|combined)|import.(basics|review|repairs|amount_repairs|validation|cancellation|ui|navigation|recovery_ui|group_benchmark_smoke|qml)|transferstore.(imports|management|validation|qif)|statement.review|committed.(basics|history|review|validation)|rulestore.(application|reapply)|category.assignments)$'
cmake --build build/import-ui-debug --target spentit_account_tests spentit_statement_store_tests -j 2
ctest --test-dir build/import-ui-debug --output-on-failure -R '^(accounts.(groups|validation)|statement.review)$'
```

## Current-format point-2 observations — 2026-10-26

The production-gateway benchmark now uses bank-v6/schema 40 with rules; the measurements in this section used bank-v3/schema 16. Run `build/import-ui-rules-release/src/ui/spentit_import_benchmark 100000 --interactions` (or 200000) serially from a desktop session. This mode covers import, verified pending reopen, queries, inspection, unsaved scrolling and save/source/exact-balance assertions; default mode additionally covers saved/committed reopening and commit. See [full evidence, diagnostics and limitations](hardening.md).

Final individual native Release command times in milliseconds:

| Operation |100k final attempt |200k interaction run |
|---|---:|---:|
| Import |195517 |140846 |
| Verified pending reopen |151032 |85432.2 |
| Six sorts |222.61–364.17 |98.21–131.05 |
| Match/empty/clear filters |220.75–919.84 |79.62–358.65 |
| First/last inspection |228.97–233.48 |94.16–96.05 |
| Save and refresh |Not reached |12427.2 |
| Peak RSS observed (MiB) |569.48 before failure |1014.44 |

The 200k interaction run completed all exact correctness assertions, but produced no frame samples; its rendering measurements are unavailable. The 100k final attempt produced 220 cache-zero frames (p95 20.43 ms, maximum 33.09 ms), preserving the unsaved edit, then failed because the default-cache pass produced no frames. It did not reach save or final correctness completion. A separate 100-row native smoke produced 228 frames in each cache mode, validating the paired harness on a small dataset. The benchmark now checks exposure and refuses to report empty native frame sets as zero latency.

An earlier full 100k diagnostic completed all source, balance, save/reopen, commit/receipt and committed-reopen assertions, but was sampled and partially overlapped a delayed native accessibility wrapper. No build/test/profiler/native fixture overlapped the final pair. Their non-monotonic timings and differing callback cadence have no established cause; they are not a scaling or causal speedup claim. Large paired-cache rendering and 200k rendering remain unverified. Long imports/verified opening and occasional event-loop gaps remain performance limitations.

Read improvements remove repeated SQL preparation within bounded read operations, avoid whole-file reparse during selected-record inspection, avoid empty-filter/string-sort copies, and reuse the already validated head in category-history verification. Exact row/history checks, encryption, journaling and durable writes remain intact. The historical implementation descriptions below predate those changes.

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
