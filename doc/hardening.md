# Current-format hardening

Last updated: 2026-10-26

Roadmap point 2 is authorized after the completed QIF checkpoint. Linux/Windows builds and native interactions are explicitly excluded by the owner for this checkpoint; they remain unverified. This document records reproducible checks, not production readiness. The delivered checkpoint and its explicit remaining verification limits are recorded below.

## Coverage-guided QIF fuzzing

The harness covers ordinary parsing, whole-document indexing and each admitted account's parsing with DMY/MDY and JPY/CAD/KWD scales. It checks retained byte ranges, diagnostic bounds, original/effective total agreement and exact allocation sums. Invalid indexes may retain a partially scanned section and are never passed to account parsing. The separate `amount_repair` diagnostic bit is not a guarantee of structural repairability; `repairable` remains the admission gate.

The corpus is synthetic. Small seeds cover ordinary/class/transfer records, missing money, repeated account sections, malformed account boundaries and extreme signed amounts. The generator adds field/split/account count boundaries, invalid UTF-8 and BOM/CRLF. Generated/mutated corpus and crash artifacts remain under ignored `build/`, never routine private samples.

AppleClang's bundled compiler lacks libFuzzer. The optional pinned [compiler-rt runtime](../third_party/compiler-rt/README.md) provides it without a system installation. Python 3.12 or later is required for safe archive extraction:

```sh
curl -L --fail https://github.com/llvm/llvm-project/releases/download/llvmorg-17.0.6/compiler-rt-17.0.6.src.tar.xz -o /tmp/compiler-rt-17.0.6.src.tar.xz
python3 tools/build-libfuzzer.py /tmp/compiler-rt-17.0.6.src.tar.xz
cmake -S . -B build/qif-fuzz -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSPENTIT_SANITIZERS=ON -DSPENTIT_QIF_FUZZER=ON -DSPENTIT_FUZZER_RUNTIME="$PWD/build/libfuzzer-runtime/libFuzzer.a"
cmake --build build/qif-fuzz --target spentit_qif_fuzzer -j 2
python3 tools/prepare-qif-fuzz-corpus.py build/qif-fuzz/boundary-corpus
mkdir -p build/qif-fuzz/artifacts
build/qif-fuzz/tests/engine/spentit_qif_fuzzer build/qif-fuzz/boundary-corpus -dict=tests/fixtures/qif/fuzz.dict -seed=20261028 -max_total_time=300 -max_len=1048576 -len_control=0 -timeout=10 -rss_limit_mb=2048 -artifact_prefix=build/qif-fuzz/artifacts/
```

A bundled compiler runtime can instead omit `SPENTIT_FUZZER_RUNTIME`. Engine and harness use coverage instrumentation and ASan/UBSan. The standalone runtime uses ASan (matching libc++ container annotations), without coverage or UBSan instrumentation. No sanitizer option suppresses checks in the parser. A time-bounded campaign is not proof of absence of defects; retain and minimize any genuine parser failure, add a deterministic regression, then rerun.

Initial bootstrap failures were harness/runtime issues: dictionary escapes require `\x0A` rather than `\n`; upstream's small build script uses an obsolete language standard; mixed ASan/non-ASan libc++ templates produced a runtime corpus-enumeration container warning; and instrumenting sanitizer-internal pointer arithmetic with UBSan is unsupported. The checked helper uses C++17 and consistent runtime ASan. Two overstrict proposed assertions were corrected to the parser's actual contracts described above. These are not reported as product defects.

## Combined recovery

The synthetic account workflow combines whole-source capture/materialization, unavailable-amount repair, explicit duplicate review, category/rule application, staged opening interpretation, commit, committed correction/Undo, overlapping export exclusion, category rename and verified backup restoration. Rules-disabled builds exercise the same workflow without rules. Each explicit before/after commit exit checks reopened durable state before retry; the resumed test completes the remaining workflow and restores a backup. First/middle/last write/sync positions additionally inject alternating disk-full/I/O failures at each step (45 rules-enabled cases, 39 rules-disabled cases). Reopening verifies the surviving state before the remainder runs. Current-format full per-operation fault coverage from point 1 remains separately recorded; do not substitute a combined boundary test for all write/sync positions.

## UI verification boundaries

Qt interface checks inspect accessible roles, names, selection and currency context, and dispatch keyboard navigation, Tab/Backtab, Enter save and modal/search Escape. Native screen-reader behavior is separate evidence. The ledger retains model attachment and stable occurrence view restoration; disabling its asynchronous offscreen cache is being checked against warning-fatal repeated regressions and native scrolling. Visible delegates remain virtualized and reusable.

The current workload executable uses the real document gateway directly. The old partial timing decorator silently omitted newer repository capabilities; its component timings are historical. Current timings include the complete command plus result delivery and separately measured UI heartbeat/scroll frame intervals, with no weakened storage/verification settings. Native 100,000/200,000-row measurements must run serially without builds, fuzzing or tests competing for resources.

## Coverage campaign results

On the available macOS arm64 host, two final ASan/UBSan-instrumented target campaigns completed without parser crashes or sanitizer diagnostics:

| Corpus / settings | Seed | Duration | Executions | Covered edges | Features | Peak reported RSS |
|---|---:|---:|---:|---:|---:|---:|
| Small seeds plus evolving corpus; max length 65,536 | 20261027 | 301 s | 723,392 | 3,330 | 15,915 | 634 MiB |
| Generated boundary seeds; max length 1,048,576; length control disabled | 20261028 | 301 s | 26,812 | 3,326 | 17,330 | 612 MiB |

The total was 750,204 executions. Coverage numbers belong to each campaign, not a deduplicated combined total or percentage of all parser code. Both used a 10-second per-input timeout and 2,048 MiB RSS limit. The first campaign's evolving length limit reached 976 bytes; the second deliberately exercised larger seed inputs. Campaigns overlapped ordinary builds/tests, so execution rates are not performance benchmarks. The checked runtime build helper was then reproduced with Python 3.13 and the resulting harness replayed the ordinary and account-section seeds successfully.

## Intermediate verification — before workload optimization

Debug and Release broad functional selections each passed 75/75 (94.61 and 83.78 seconds). The initial ASan/UBSan selection passed 74/75 in 226.56 seconds; the new combined scenario runner encountered a SIGKILL in a deliberately interrupted child, without an ASan/UBSan diagnostic. A diagnostic rerun identified the overlap-commit child at the pre-commit boundary. The OS kill reason was not established. The runner now isolates each scenario before preparing SQLCipher state, avoiding inherited allocator/quarantine state from prior scenarios. It preserves every boundary check, retry, completed history and restored backup; it disables no sanitizer checks. The final sanitizer combined/crash and combined-I/O cases passed 2/2 in 171.80 seconds (73.04 and 98.75 seconds).

After that harness isolation and a deterministic opening-date refinement in the keyboard fixture, Debug and Release focused combined/recovery/UI selections each passed 3/3 (78.76 and 65.65 seconds). Rules-off passed combined, combined-I/O, UI, QML and schema-gate checks 5/5 in 57.20 seconds. The final sanitizer keyboard/accessibility slot passed three QtTest checks including setup/cleanup in 0.967 seconds. Final builds and inspected detailed test logs were clean. These selections do not constitute a full 103-entry suite.

The original Release search-return test reproduced `DelegateModel::cancel: index out range`. With asynchronous offscreen caching disabled, 20 repeated Release navigation/split-view pairs passed with that warning configured to fail the test; broad functional tests also passed without it. This is exercised regression evidence, not a proof about every Qt interaction. Native scrolling measurements remain required before accepting the cache tradeoff.

## Current-format read optimization

A native Release 100,000-row pre-optimization run completed import in 324,732 ms (maximum UI heartbeat gap 334.3 ms, peak RSS 453.97 MiB). A two-second CPU sample during that command found the worker predominantly loading ledger rows, including repeated SQL preparation in row/metadata readers. The UI thread was waiting in its event loop in that sample. Pending reopen was deliberately stopped after more than 110 seconds; no completed baseline reopen, full-workflow success, 200,000-row baseline or baseline scrolling result is claimed. Sampling can perturb timing, so this baseline is diagnostic rather than a controlled comparison.

Ledger loading and full verification now reuse prepared read statements within the current operation. They still bind and execute every query, validate every row, clear bindings/results after each use, and finalize all plans before the operation returns. The cache is bounded to 64 SQL plans and 4 MiB of idle statement memory, does not retain result data, and changes no write, encryption, journal or synchronization setting. Record inspection obtains exact immutable stored occurrence bytes instead of reparsing the entire import for every selected row; unavailable materialized rows retain the whole-source parsing fallback. Search avoids constructing folded copies when no text filter is present, and string sorting compares views of retained strings.

Exact-record regressions compare the accessor against original indexed byte ranges across missing-amount repair, separate source accounts, committed reopening and backup restoration. They reject wrong-account access and unavailable ordinals. Full history/corruption and recovery tests continue to check reads against authoritative state.

Post-optimization Release passed the 75-case functional selection in 62.50 seconds, followed by the added exact-source `accounts.groups` assertions in 4.02 seconds. Debug passed seven focused groups/combined recovery/UI/QML/search/ledger checks in 78.88 seconds; rules-off passed those plus its schema gate (eight checks) in 69.00 seconds. These runs share build/test resources and are correctness evidence, not performance comparisons.

The broad functional selection is reproducible with:

```sh
ctest --test-dir build/import-ui-rules-release --output-on-failure -j 3 -E '(^storage\.|\.(io|crashes|group_io|group_crashes|combined_io|backup_failures|backup_crashes|bulk_io|bulk_crashes)$|import.benchmark_smoke)'
ctest --test-dir build/import-ui-rules-debug --output-on-failure -j 2 -R '^(accounts.groups|accounts.combined|accounts.combined_io|import.ui|import.qml|import.schema_gate|import.ledger|import.search)$'
```

Use the corresponding `import-ui-rules-sanitized` or rules-disabled `import-ui-debug` tree for those configurations; test numbering differs by configured optional targets.

The post-optimization ASan/UBSan broad functional selection passed 75/75 in 178.25 seconds, including the exact-record evidence assertions. No sanitizer diagnostics were reported.

The final post-optimization sanitizer combined I/O selection passed 1/1 in 84.60 seconds; this includes 45 injected write/sync cases with durable reopening, retries and backup restoration.

## Native accessibility evidence

On the available macOS host, a synthetic held test window exposed the Transactions list through the native accessibility tree, including row date, payee, category, amount, currency and Draft status; editable fields and controls had names, and focus was on Ledger search. This is native accessibility-tree evidence, not a spoken VoiceOver session. The native automation request stalled for approximately 50 minutes before returning; further native traversal and actual spoken output were not verified. Its synthetic wrapper subsequently hit QtTest’s 300-second timeout because the optional inspection hold itself was 300 seconds. The hold is now 240 seconds; no successful held native test run is claimed. Offscreen/interface keyboard checks cannot replace that missing manual evidence. Linux/Windows remain explicitly excluded and unverified.

## Native 100,000-row diagnostic run

The read-plan/evidence optimization completed the full synthetic workflow and printed `correctness=passed`: exact projected/committed balances, pending isolation, all six sorts, matching/empty/cleared filters, first/last inspection, unsaved edit retention while scrolling, saved-edit reopening, source preservation, commit receipt count and committed reopening. Import took 245,676 ms, pending reopen 213,378 ms, save with refresh 6,434.75 ms, saved reopen 41,315.4 ms, commit with refresh 39,532.5 ms and committed reopen 89,830.9 ms. Sort commands ranged 67.57–99.85 ms, filters 48.90–201.56 ms, and first/last inspection 52.63–55.99 ms. Peak RSS was 623.78 MiB. Visible-only scrolling reported 228 frames for 240 timer steps, p95 17.91 ms, maximum 31.03 ms; the unsaved edit survived.

This was a diagnostic run, before the final redundant category-head read was removed and before the benchmark gained its paired default-cache traversal. A two-second sample during pending reopening showed category/history verification executing repeated row/metadata reads and SQLCipher secure allocation work. The final change reuses the already validated head instead of loading that same head a second time, while retaining historical fields/allocations/provenance checks. The long pending-open timing and much shorter saved-open timing must not be presented as a controlled speedup: callback scheduling differed substantially, an outstanding native automation request overlapped part of the run, and the pending-open phase was sampled. A delayed native accessibility wrapper also overlapped part of the diagnostic; no build or fuzz campaign overlapped it. The interrupted baseline's temporary directory was removed only after verifying every byte of its generated 100,000-record fixture; the completed diagnostic cleaned up its own temporary files.

The benchmark's optional `--interactions` mode stops after save/source/exact-balance assertions and a normal close, covering the point-2 import → verified reopen → search/sort → inspect/edit/scroll work. It does not measure committed publication or saved/committed reopen; the default full mode retains all those assertions. Both modes now compare identical scroll traversals with cacheBuffer zero and this Qt build's actual default value, on the same loaded dataset, restoring zero before subsequent mutations. These are individual runs, not distributions or guarantees.

## Final focused checks

After category-head reuse, the paired-cache benchmark, exact source checks and closed-modal accessibility regression, Debug/Release/ASan configurations each passed six focused checks; rules-off passed five (without rules validation). Elapsed times were 86.45, 61.42, 148.23 and 59.41 seconds respectively. The selection was `^(category.validation|committed.validation|rulestore.validation|accounts.combined|import.ui|import.benchmark_smoke)$`. These are additional focused checks, not another full suite. Compiler/test logs were inspected separately from the intentionally timed-out held native fixture.

A final Release Cocoa run of `spentit_import_ui_tests accessibilityAndKeyboard`, without the optional hold, passed all three QtTest setup/test/cleanup checks in 960 ms. It dispatched native Qt keyboard events for ledger navigation, Tab/Backtab, modal Escape/focus restoration, Enter save and search Return/Escape. The guard’s Save button was accessible/visible while open and marked invisible when closed. This resolves the native tree’s enumeration of hidden guard nodes; no actionable hidden control was demonstrated. Actual VoiceOver speech remains unverified.

## Final native workload observations and limits

The final Release 200,000-row interaction run completed and printed `correctness=passed scope=import-verified-reopen-edit-search-scroll`. Import took 140,846 ms, verified pending reopen 85,432.2 ms and save/refresh 12,427.2 ms. Sorts ranged 98.21–131.05 ms, filters 79.62–358.65 ms and first/last inspection 94.16–96.05 ms. Peak RSS was 1,014.44 MiB. No build, test, profiler or native accessibility fixture overlapped this run. Both scroll phases produced no frame notifications: their printed zeros are **unavailable rendering measurements**, not zero latency. Unsaved-edit and exact-money/source assertions still passed.

The benchmark was then hardened to raise/request activation, verify exposure, and require nonempty native frame samples. Offscreen smoke remains explicitly exempt from native rendering claims. A native 100-row smoke completed with 228 frames in each traversal: cache zero p95 17.95/max 33.70 ms and default 320 p95 17.70/max 36.00 ms. This validates the harness and a small paired comparison, not large-dataset rendering.

A final isolated 100,000-row interaction run imported in 195,517 ms and verified reopened in 151,032 ms. Sorts ranged 222.61–364.17 ms, filters 220.75–919.84 ms and first/last inspection 228.97–233.48 ms. Cache-zero scrolling retained the unsaved edit and produced 220 frames, p95 20.43/max 33.09 ms (peak RSS through that phase 569.48 MiB). The subsequent default-cache pass produced no frames. The harness correctly **failed** with “No native frames observed; scrolling measurement unavailable”; save and final correctness completion were not reached in this final run. Its temporary synthetic dataset was removed by exception unwinding. No extra large rerun was performed.

The isolated pair is non-monotonic, and callback scheduling differed substantially. The reason was not established; do not infer scaling or a percentage speedup from these individual runs. The earlier full 100k diagnostic did complete all correctness assertions, and the final 200k interaction run completed its stated scope, but neither substitutes for a successful paired large native rendering comparison. Visible-only 100k frames plus warning-fatal repeated resets and broad viewport/unsaved-edit assertions support the bounded cache fix. Large paired-cache comparison, 200k native rendering, actual spoken VoiceOver/manual assistive traversal, and owner-excluded Linux/Windows checks remain unverified. Import and verified opening remain substantial long operations; no general responsiveness guarantee is made.
