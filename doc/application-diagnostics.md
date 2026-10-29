# Local application diagnostics

Last updated: 2026-10-29

Diagnostics is opt-in, read-only, process-local observation. It does not change the document format, validation, commands, matching policy, encryption or durable boundaries. It contains workload metadata and should be treated as local sensitive information. There is no telemetry, automatic export, screenshot, MCP server or action interface.

For agent operation and profiling, start with the [agent diagnostics guide](agent-diagnostics-guide.md). It documents endpoint discovery, exact-operation waits, outcome decoding and the accepted build-variant distinction.

## Build and launch

Configure the normal application build with `-DSPENTIT_DIAGNOSTICS=ON`, then rebuild. The option defaults to OFF. The standard-C++ `spentit_diagnostics` target has no Qt, SQLCipher or engine dependency; OFF builds retain inert observation calls and disabled collector code; the IPC server and reader CLI are excluded, and collection cannot be enabled. The owner accepted this separation as sufficient for now on 2026-10-29; fully stripping inert code and packaging two editions remain future work. Both variants share financial behavior and document format, with no dataset conversion. Storage and UI adapters link it; the engine does not.

```sh
cmake -S . -B build/import-ui-rules-release -DSPENTIT_DIAGNOSTICS=ON
cmake --build build/import-ui-rules-release -j 3
./build/SpentIt --diagnostics
```

Normal launch has no diagnostic listener or sampling timer. Enablement is never saved in Settings or a dataset. An OFF build rejects `--diagnostics` with exit 2. An enabled window shows “Diagnostics enabled”. Endpoint startup failure exits 2 without relaxing permissions or opening a dataset.

The enabled application prints its endpoint to stderr. Pass that exact endpoint to the reader built at `build/import-ui-rules-release/src/ui/spentit-diagnostics`:

```text
spentit-diagnostics ENDPOINT status
spentit-diagnostics ENDPOINT events 0
spentit-diagnostics ENDPOINT wait-operation OPERATION_ID 10000
spentit-diagnostics ENDPOINT wait-generation VIEW_GENERATION 10000
spentit-diagnostics ENDPOINT wait-ui UI_REVISION 10000
```

`events` accepts the last consumed event sequence as its cursor. Read the last event's sequence, not the snapshot's newest sequence, when `truncated` is true. `wait-operation` waits for a retained terminal outcome; `wait-generation` requires that exact generation's interaction readiness. `wait-ui` instead targets an exact semantic UI revision. None waits for physical presentation. A superseded/evicted generation or outcome can time out. Waits default to 10 seconds, cap at 60 seconds, and poll at 100 ms. Exit codes: 0 observed, 2 invalid invocation, 3 disconnected/rejected/invalid response, 4 observation timeout, 5 output failure. Disconnect and timeout do not establish a financial result.

Redirect stdout explicitly to save a bounded JSON snapshot or event batch. Each response is independently bounded; there is no automatic trace file. Output failures do not affect the application. Repeated event reads can be collected by a caller, which owns its file retention and disk-space policy.

## Transport and resource limits

Qt local IPC runs on a dedicated event loop. It reads copied collector summaries without requesting work from the GUI or document worker. There is no TCP listener. Each instance gets a randomly created owner-only runtime directory and `QLocalServer::UserAccessOption`; no password/key is used or published. Unix uses a short directory under the sticky `/tmp` root to stay below the Unix socket pathname limit. The directory and endpoint are removed on orderly shutdown; interruption may leave an unused directory. The discovery path is not a diagnostic payload field.

Requests are newline-terminated JSON objects: `{"method":"status"}` or `{"method":"events","after":"0"}`. The cursor is a decimal string. Unknown methods, fields, non-string cursors, malformed and oversized input are rejected with fixed error codes. Limits are 1,024 request bytes, eight connected clients, a two-second connection deadline, 128 events/read, 4,096 retained events and a four-MiB response cap. Producers never perform IPC or filesystem writes. Socket backpressure is contained in the reader thread and deadline. The collector copies fixed-size state under a short mutex, serializes outside that mutex, and drops contended event writes; `health_failures` records such losses as well as contained failures. State/outcome writes use the short mutex so ring pressure cannot discard the current operation. This is bounded synchronization, not a lock-free guarantee.

## Schema 1

The authoritative vocabulary is in [the contract](../src/diagnostics/include/spentit/diagnostics.hpp). Only fixed enum names and numeric/boolean fields enter serialization. Unknown command/surface strings map to `unclassified`/`unknown`; arguments, labels, paths, SQL/bindings, financial values/dates, source records, search/regex text, exception messages, credentials and hashes of those values are excluded. No persisted account, transaction or operation identifier is exported.

`session` distinguishes the collector's process session; operation IDs are monotonic within it. `document` increments at lifecycle application; `view` increments for each applied command snapshot, including failed attempts. `ui.revision` separately increments when the observed surface, modal state, controls, pending work or readiness changes. `now_us` and all times use `steady_clock` microseconds. GUI and worker ages/known flags are separate. These observations are not an atomic financial snapshot. Counts in `ui` belong to its view generation; duration and statement counters are process-cumulative. Unsupported page/GPU/physical-display metrics are explicit.

Current operation and 32 recent outcomes survive event-ring eviction. Each outcome records enqueue, start, worker finish and GUI delivery timestamps. Queue wait, execution/refresh and delivery latency can be calculated from those fields; `model_apply` spans separately record GUI application. `cleanup_jobs` counts queued/running retired-view disposal and does not block interaction. The single deferred inspection slot has its own identity; replacement emits superseded, and eventual admission reuses that identity. Busy rejections never masquerade as executed work.

Boolean flags are encoded as numeric 0/1. Enum values are stable for schema 1 and start at zero, in this order:

| Field | Values |
|---|---|
| result | pending, succeeded, failed, cancelled, superseded, not_executed |
| persistence | not_applicable, not_attempted, confirmed_unchanged, confirmed_saved, unknown |
| presentation | awaiting_refresh, model_applied, ui_settled, presentation_observed, unavailable, failed |
| surface | unknown, welcome, setup, unlock, ledger, categories, rules, history, settings, reports |

Cancellation is reported as available/requested/sealed from the existing gate. A seal is not a durable outcome. `durable_stage` events identify retained capture/preparation separately; `saved_stages` also preserves capture (bit 0), preparation (bit 1), and other confirmed save (bit 2) independently of ring eviction. A cancelled multi-stage import may have confirmed saved evidence; that does not mean every stage completed. Saved work with failed presentation retains both facts and `reopen_and_inspect` guidance. A terminal execution failure does not imply rollback. The observer does not perform extra outcome queries; uncertain branches remain unknown.

Spans carry IDs, parent IDs, inclusive/exclusive microseconds and an unwind flag. Verification spans indicate only work actually entered and finished; exception unwinding is not proof of corruption (cancellation can unwind). Cumulative calls/failures support identifying executed checks. Never sum overlapping inclusive spans. Statement prepare/reuse use atomic cumulative counters and currently report counts, with duration zero indicating unavailable timing; high-detail SQL timing is not enabled. No SQL text is collected. Cache events come from actual worker catalog/metadata/ledger/search and group-snapshot branches, including bypass/invalidation. Existing caches and retention policies are unchanged.

## Semantic UI and readiness

QML publishes an explicit allowlist at up to 10 Hz. It includes the current page, modal/menu/chooser presence, pending work, registered control visibility/enabled masks, focus role, sidebar/editor dirtiness, selection, search presence/validity, sort, projected mode, viewport/selected visibility, current delegates and loaded/filtered row counts. No QObject tree, accessibility text, dialog titles or native chooser contents are exported. A zero focus role means unknown/unregistered, not “nothing focused”.

Control/focus indices (mask bit 0 / focus role 1 first): search, projected toggle, transaction date, payee, memo, total, category search, rule label, rule expression, document location, unlock password. The last two expose only state, never text. Other controls are not yet individually registered. Enabled states reflect bindings; unregistered control reasons are unknown.

Pending bits: 0 startup, 1 guard decision, 2 search, 3 account navigation, 4 counterpart reveal, 5 viewport restore, 6 close intent, 7 guard handoff, 8 worker busy, 9 startup probe timer. Modal mask bits:

```text
0: diagnosticFileMenu
1: importBlockersDialog
2: transferDialog
3: transferCommit
4: protectedCommit
5: ledgerContext
6: accountContext
7: overwriteDialog
8: openDialog
9: createDialog
10: importMapping
11: importDialog
12: backupDialog
13: unsaved
14: changePreview
15: mergePreview
16: bulkDialog
17: reapplyDialog
18: rulesUnsaved
19: matchedRuleDialog
20: categoryContext
21: categoryPicker
22: renameDialog
23: deleteAccountDialog
24: correctOpeningDialog
25: currencyDialog
26: importCategoriesDialog
27: discardDialog
28: commitDialog
```

The modal stack records visibility transitions in order, using the same fixed IDs. Bit/stack code 29 represents a combo-box popup by role, without inspecting its items. Native file chooser bits identify only purpose/open state. Blocker displayed and total counts come from the existing import summary; reason text and candidate identities are excluded.

Model application, logical interaction readiness and native presentation are distinct. Readiness requires settled pending work and visible ledger delegates where relevant; an idle modal is ready for its decision. Dirty fields alone are not a blocker. Hidden/minimized/offscreen windows and native chooser surfaces outside the Qt frame proxy cannot claim native presentation. The native frame proxy captures the ready generation before scene synchronization, checks both the model generation and semantic UI revision again at frame delivery, and rejects stale frames (including a frame for an earlier surface on the same model). A single update is requested when a new UI revision becomes logically ready. This is not a GPU/input-latency or physical-display assertion. The application never injects input. UI sample gaps measure scheduling, not rendering latency.

## Workloads and verification

The synthetic import benchmark opts in with `SPENTIT_BENCHMARK_DIAGNOSTICS=1`. It checks the exact operation's delivered outcome and its applied generation, then observes logical readiness. When the existing close/startup-probe chain replaces a generation it explicitly prints `readiness=superseded`, rather than accepting the newer generation as evidence for the old one. Existing independent money, evidence, transfer multiplicity, commit/reopen and edit assertions remain in place. Scroll loops still measure a prescribed interaction duration; their sampling sleeps are not operation-completion criteria.

UI and selected storage test runners accept `SPENTIT_TEST_DIAGNOSTICS=1`; normal execution leaves collection disabled. Diagnostics-specific tests cover fixed vocabulary, disabled builds, bounds/eviction, serialization allocation failure, cancellation-observer exceptions, stale generations/frames, concurrent writers/readers, retained outcomes, saved-but-failed-refresh, semantic privacy, deferred replacement/busy rejection, malformed/oversized IPC, instance isolation, permissions, stalled clients, orderly shutdown and interrupted reader processes. See the verification record below for actual commands/results, not just available tests.

## Known coverage limits

No screenshots, MCP, automatic uploads, performance optimization or repeated-transfer policy change is included. Native/Linux/Windows validation is separately recorded. The UI surface is useful but not an exhaustive accessibility/control model: every control's reason, delegate lifetime/reuse totals and detailed preview subphases remain unknown/uninstrumented. Default collection has no high-detail SQL/frame profiling mode. Current spans group some evidence reconstruction and association checks rather than tracing every subroutine. The application startup metric begins after Qt application/endpoint initialization; loader and process-creation time are unavailable. Collection observes existing verification; it cannot replace exact financial assertions or recovery.

Cross-user denial with a second OS account, physical power loss, and every process-shutdown interleaving require additional validation. The fixed ring cannot grow under event pressure, serialization catches allocation failure, and output writes happen only in readers; these properties do not establish exhaustive fault coverage. No private dataset was used. Large 100k/200k durable-import runs are not inferred from the smaller workloads or historical sheet benchmarks.

## Action-outcome inventory

Observation is attached to the existing dispatch branches; it never issues a new durable lookup. On exceptions, unknown is retained unless a confirmed stage/receipt was already observed. `confirmed_saved` can mean retained capture in a cancelled multi-stage operation; consult its stage events and execution result.

| Existing actions | Successful outcome source |
|---|---|
| `create`, `create_account`, `rename_account`, `archive_account`, `reactivate_account`, `delete_account`, `correct_opening`, `reassign_currency`, `undo_account` | Existing gateway/complete repository return or validated existing outcome; marked before presentation work where available |
| `apply_change`, `apply_bulk`, `apply_category` | Existing command receipt, including strict/group Undo through the same action |
| `rules_save`, `rules_apply`, `rules_reapply_apply` | Existing receipt; explicit unchanged/no-row-change result maps to confirmed unchanged |
| `save`, `decide`, `opening`, `commit`, `discard`, `backup` | Complete command return or existing commit receipt; unknown if the existing failure path cannot establish durability |
| `import` | Known capture/preparation from the controlled store; a mapping-only preview has no new saved outcome |
| `map_import`, `resume`, `group_balance`, `group_abort`, `group_resolve_transfers` | Existing capture/preparation outcome or completed group command; capture and later stages remain distinct |
| `transfer_accept`, `transfer_clear`, `transfer_clear_row`, `transfer_commit`, `transfer_from_draft`, `transfer_manual`, `transfer_match`, `transfer_merge`, `transfer_merge_apply`, `transfer_retarget`, `transfer_unmatch` | Existing transfer receipt; Merge can return a preview before any save and then remains unknown rather than claiming persistence |
| `probe`, `open`, `unlock`, `close`, `select`, `select_account`, `inspect`, `query`, `evidence`, `history`, `entry_history`, `category_list`, `category_select`, `rules_load`, `rules_discard`, `rules_prefill`, `rules_preview`, `rules_reapply_preview`, `preview_change`, `preview_bulk`, `preview_category`, `transfer_candidates`, `prepare_create`, `cancel_change`, `cancel_bulk`, `cancel_merge`, `cancel_reapply`, `cancel_category_preview`, `cancel_import_mapping` | Successful read/session/preview operation: persistence not applicable; exceptions conservatively unknown |
| Unrecognized action | Fixed unclassified/rejected-invalid observation; no diagnostic action handler |


The POSIX reader failure/timeout check is retained in [reader_checks.py](../tests/diagnostics/reader_checks.py). Run it with the built transport-test binary and CLI binary as its two arguments. It simulates an output-file quota failure using `RLIMIT_FSIZE`, expects reader exit 5, checks that the server's latest operation is unchanged, and expects exit 4 from a bounded wait for a nonexistent operation. It does not simulate physical media failure.

The macOS native proxy check is retained in [native_smoke.py](../tests/diagnostics/native_smoke.py), with the application executable as its argument. `--smoke` isolates Settings and uses a missing temporary document path. The script reads only diagnostic JSON and asserts an exposed surface and matching-generation/revision presentation observation. It does not capture screenshots.


## Verification record — 2026-10-29

All datasets/fixtures were synthetic; no private export or installed financial document was opened. No screenshot capture/export facility was added to diagnostics, and its native check reads JSON only. The broad existing UI regression retains its pre-existing in-memory render assertion; screenshot-export options were not enabled.

| Check | Actual result |
|---|---|
| macOS rules-on Release broad regression | **91/91 passed**, 153.95 s; enabled runners where wired, with existing financial assertions retained |
| Release ordinary-import injected `SQLITE_FULL`/write failures and process interruptions | **2/2 passed**, 355.77 s in parallel; all eight operation sweeps, reopen/retry, evidence/leak checks, and explicit before/after durable-boundary cases |
| ASan/UBSan selected regression | Initial **15/16**: startup UI assertion failed while its probe was still busy; no sanitizer memory diagnostic. After replacing the fixed wait with a probe-completion wait, the rebuilt five-check UI/diagnostic selection passed **5/5**, 88.09 s. Subsequent semantic/correlation checks passed **3/3**, 10.16 s; the final failure-axis correction and regression passed **4/4**, 82.66 s |
| Final Release focused semantic/correlation checks | **4/4 passed**, 54.13 s after the failure-axis correction, including retained capture followed by preparation failure and saved rule refresh failure; preceding semantic selection **3/3**, 4.81 s and full UI/rules selection **5/5**, 55.32 s |
| Standalone collector under ThreadSanitizer | **1/1 passed**, 0.68 s; concurrent collection/reads, bounds, stale revisions and allocation-failure containment. This does not claim Qt-wide race verification |
| Rules-off Debug with diagnostics enabled | Collector/navigation **2/2 passed**, 2.30 s after the final semantic changes (earlier run 2.99 s) |
| Compiled-out Debug UI | Collector/QML **2/2 passed**, 1.28 s; `--diagnostics --smoke` refused with exit 2. Final standalone OFF collector **1/1 passed**, 0.12 s |
| Final IPC/diagnostic QML | **2/2 passed**, 2.22 s, outside the filesystem/socket sandbox; permissions were not weakened. In-sandbox startup failed closed as expected |
| Native Cocoa readiness | 65 sampled responses; exposed native window and matching model-generation/UI-revision presentation observed. Hidden/offscreen/stale-frame cases are separately covered by semantic/collector tests |
| Large ASan/UBSan replay | **19,000 rows passed**, exit 0: import, verified reopen, edit/evidence preservation, search and both scroll paths. No sanitizer diagnostic. A concurrent rules-off build makes this correctness evidence only, not timing evidence |
| Reader output failure and timeout | Injected output-file quota failure returned **5**, retained outcome unchanged; nonexistent-operation wait returned **4**. No financial action was sent |

A missing `QSignalSpy` include caused one intermediate build failure; it was corrected before the successful rebuilt selections above. Final build logs were checked for compiler warnings/errors. Linux/Windows execution, native chooser interactions on those platforms, a second-user OS permission test and physical media/power-loss behavior remain unverified.

Reproduction selections (after configuring the existing build trees as described in the repository build documentation):

```sh
SPENTIT_TEST_DIAGNOSTICS=1 ctest --test-dir build/import-ui-rules-release --output-on-failure -j 2 -E '(_io$|\.io$|_crashes$|\.crashes$|\.faults$|_faults$|backup_failures|backup_publication|backup_cancellation|diagnostics.transport|diagnostics.qml)'
SPENTIT_TEST_DIAGNOSTICS=1 ctest --test-dir build/import-ui-rules-release --output-on-failure -j 2 -R '^import.(io|crashes)$'
SPENTIT_TEST_DIAGNOSTICS=1 ctest --test-dir build/import-ui-rules-sanitized --output-on-failure -j 2 -R '^(diagnostics.(collector|workload)|accounts.(groups|validation|combined)|import.(basics|review|validation|cancellation|ui|navigation|recovery_ui|qml)|rulestore.(application|reapply)|category.assignments)$'
ctest --test-dir build/import-ui-rules-release -R '^diagnostics.(transport|qml)$' --output-on-failure
python3 tests/diagnostics/native_smoke.py ./build/SpentIt
python3 tests/diagnostics/reader_checks.py build/import-ui-rules-release/src/ui/spentit_diagnostic_transport_tests build/import-ui-rules-release/src/ui/spentit-diagnostics
```

The native/IPC checks need OS socket/window access; a sandbox refusal is not a reason to relax endpoint permissions. The ThreadSanitizer target is a separate headless build using `-DSPENTIT_DIAGNOSTICS=ON -DCMAKE_CXX_FLAGS=-fsanitize=thread -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread`, building only `spentit_diagnostics_tests`. The Qt-enabled sanitizer tree uses the existing ASan/UBSan configuration.


The serial workload runner is [compare_workloads.py](../tests/diagnostics/compare_workloads.py). Pass the benchmark executable and an explicit output directory. It records the executable SHA-256, three enabled/disabled pairs per workload by default, raw synthetic logs, phase durations and every exit status; it stops on a failure and rejects an executable changed during the run. Do not run builds or other benchmarks concurrently. The normal application still performs no automatic export.


## Serial workload observations

All **12 recorded runs passed** their independent financial assertions: three disabled/enabled pairs for grouped 18,972-row imports and three pairs for ordinary 19,000-row imports. The executable SHA-256 stayed identical throughout; [the numeric samples and build fingerprint](diagnostics-measurements-2026-10-29.json) are retained. A subsequent diagnostics-only failure-tagging correction separates preparation failure from presentation failure; these timings predate that correction and identify their exact measured executable rather than claiming to measure the final rebuilt binary. Backend: Qt 6.12.0 offscreen, macOS rules-on Release. No builds or other tests were run concurrently. Other OS activity and thermal/cache conditions were not controlled. Reopens were warm; no disk-cold claim is made.

The command `ms` boundary is completion/model delivery in **both** modes. Enabled runs separately observe `interaction_ready_ms`; a replaced view is reported superseded. A preliminary implementation included readiness waiting on only the enabled side; those earlier timings are not used below. Statement counts use atomic counters; no application query/cache/matching optimization was made.

Seconds below are **median (minimum–maximum)**. Switch samples pool twelve account switches per run. Negative differences are observations amid run variation, not evidence that diagnostics accelerates the application.

| Workload / phase | Samples | Disabled seconds | Enabled seconds | Median difference |
|---|---|---|---|---|
| 18,972 / group_prepare | 3 each | 8.018 (7.953–12.489) | 7.705 (7.695–12.309) | -3.9% |
| 18,972 / open_group | 3 each | 14.076 (14.030–25.071) | 13.882 (13.819–18.355) | -1.4% |
| 18,972 / switches | 36 each | 0.729 (0.709–5.291) | 0.722 (0.698–0.749) | -1.1% |
| 18,972 / save_group_edit | 3 each | 2.840 (2.760–2.859) | 2.912 (2.717–7.455) | +2.5% |
| 18,972 / resolve_existing | 3 each | 0.843 (0.843–0.844) | 0.833 (0.826–0.863) | -1.2% |
| 18,972 / abort_group | 3 each | 2.945 (2.874–2.962) | 2.830 (2.830–2.898) | -3.9% |
| 19,000 / import | 3 each | 11.111 (10.963–11.236) | 6.351 (6.348–8.781) | -42.8% |
| 19,000 / reopen_pending | 3 each | 15.937 (11.357–18.438) | 18.367 (15.787–28.224) | +15.2% |
| 19,000 / save | 3 each | 2.283 (2.131–3.207) | 2.267 (2.144–2.987) | -0.7% |
| 19,000 / reopen_saved | 3 each | 18.396 (16.043–18.428) | 16.709 (11.173–17.307) | -9.2% |
| 19,000 / commit | 3 each | 13.239 (13.168–14.435) | 13.678 (13.107–15.377) | +3.3% |
| 19,000 / reopen_committed | 3 each | 41.273 (39.830–42.855) | 38.191 (30.347–39.874) | -7.5% |

Grouped medians are within the provisional 5% increase budget. Ordinary pending reopening measured **+15.2%**, with wide run variation in several phases; the observer overhead budget is **not established across the ordinary workload**. No timing threshold is enforced in CI. These measurements do not satisfy the outstanding application-opening responsiveness target and do not authorize proceeding to performance optimization.

One earlier disabled 19,000-row run aborted after scrolling with an uncaught `std::bad_alloc`; its other two disabled/enabled pairs had passed. All three final disabled and enabled repeats passed. The earlier abort is not silently counted as a pass or claimed fixed without a root cause. The final measurements cover draft/committed reopen, switching, inspection, search/scroll, save/refresh, commit, transfer resolution and Abort. Fault tests independently cover retries and interruption. 100k/200k durable-import runs and high-detail profiling modes were not measured; no result is inferred for them.


Whole-process peak RSS across the final trials was 378.9/364.3 MiB (disabled/enabled) for the grouped workload and 250.1/255.2 MiB for the ordinary workload. These are whole-process observations, not collector allocation measurements. The collector itself uses fixed-size ring/outcome/metric arrays and bounded responses. Maximum sampled GUI scheduling gaps were approximately 4.5–4.6 seconds in **both** modes; this is not an input/GPU-latency measurement or a claim of responsive UI. It reinforces the need for a separately authorized performance investigation.

Documentation verification: all 595 relative links/anchors across 40 repository Markdown documents passed, as did Python syntax checks for the retained diagnostic scripts, measurement JSON parsing and `git diff --check`.
