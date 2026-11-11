# Local application diagnostics

Last updated: 2026-11-04

Diagnostics is opt-in, read-only, process-local observation. It does not change the document format, validation, commands, matching policy, encryption or durable boundaries. It contains workload metadata and should be treated as local sensitive information. There is no telemetry, automatic export, screenshot, MCP server or action interface.

For agent operation and profiling, start with the [agent diagnostics guide](agent-diagnostics-guide.md). It documents endpoint discovery, exact-operation waits, outcome decoding and the accepted build-variant distinction. Its [standing owner authorization and attachment procedure](agent-diagnostics-guide.md#standing-owner-authorization-and-attachment-discipline--2026-10-30) covers PID-targeted native macOS inspection when UI tools cannot attach to the standalone build. That external inspection route does not expand this diagnostics endpoint or add financial data to its payload.

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

## Import phase timings — 2026-10-30

The fixed vocabulary now appends `import_capture` (source/mapping capture),
`import_prepare` (whole grouped resume), `import_populate` (one account's draft
materialization), `import_resolve_transfers` (mapped reciprocal matching), and
`import_generate_transfers` (unresolved explicit counterpart generation).
These are function-scoped observations of existing work, including exceptional
exits; they add no queries or per-record events. Existing enum indices remain
unchanged. The prepare span includes its child spans: use exclusive time or
compare children, never add their inclusive durations to the parent.

The UI publisher preserves the removed projected-toggle index as an empty slot.
It no longer references the removed QML object, which previously prevented all
semantic UI snapshots with a `ReferenceError`.

### Imported-category resolution timings — 2026-11-04

`resolve_import_categories` now has a fixed action ID, so Apply selected reports
its own execution, saved outcome, delivered model and readiness rather than
leaving the previous import operation visible. New enum values append to schema 1;
existing values retain their indices. `import_category_resolution` includes the
explicit resolution command and its final review/commit; `import_category_assign`
and `import_category_rules` measure each account's assignment and automatic-rule
passes. Assignment/rules spans also cover automatic resolution during preparation.
These spans nest: do not add their inclusive times to their parent. They expose
no category names, account names, source text or financial values.

### Review operation timings — 2026-10-31

The fixed action vocabulary now includes `review_answer`, `review_undo` and
`review_batch`; successful receipts mark confirmed saved before refresh. The
`review` workspace has its own surface value, visible-group indices and delegate
readiness; deferred viewport restoration participates in pending bit 5. Review
survivor/difference and status Details dialogs share modal bit 28. Existing enum
indices are unchanged.
`review_build` times Review construction (including cached reads), `review_write`
times answer/batch command execution, and `review_refresh` times mutation-maintained
acceptance review. Nested span durations must not be added together. These hooks
add no financial queries or record payloads. The synthetic storage runner's
`decisions 1000` mode prints phase deltas and compares 34 individual decisions
with one batch, retaining exact-balance, row-count, verification and Undo assertions.

## Semantic UI and readiness

QML publishes an explicit allowlist at up to 10 Hz. It includes the current page, modal/menu/chooser presence, pending work, registered control visibility/enabled masks, focus role, sidebar/editor dirtiness, selection, search presence/validity, sort, projected mode, viewport/selected visibility, current delegates and loaded/filtered row counts. No QObject tree, accessibility text, dialog titles or native chooser contents are exported. A zero focus role means unknown/unregistered, not “nothing focused”.

Control/focus indices (mask bit 0 / focus role 1 first): search, reserved removed projected-toggle slot, transaction date, payee, memo, total, category search, rule label, rule expression, document location, unlock password. The last two expose only state, never text. Other controls are not yet individually registered. Enabled states reflect bindings; unregistered control reasons are unknown.

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
12: unused (removed backup dialog; do not renumber later bits)
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
| `save`, `decide`, `opening`, `commit`, `discard` | Complete command return or existing commit receipt; unknown if the existing failure path cannot establish durability |
| `import` | Known capture/preparation from the controlled store; a mapping-only preview has no new saved outcome |
| `map_import`, `resume`, `group_balance`, `group_abort`, `group_resolve_transfers` | Existing capture/preparation outcome or completed group command; capture and later stages remain distinct |
| `transfer_accept`, `transfer_clear`, `transfer_clear_row`, `transfer_commit`, `transfer_from_draft`, `transfer_manual`, `transfer_match`, `transfer_merge`, `transfer_merge_apply`, `transfer_retarget`, `transfer_unmatch` | Existing transfer receipt; Merge can return a preview before any save and then remains unknown rather than claiming persistence |
| `probe`, `open`, `unlock`, `close`, `select`, `select_account`, `inspect`, `query`, `evidence`, `history`, `entry_history`, `category_list`, `category_select`, `rules_load`, `rules_discard`, `rules_prefill`, `rules_preview`, `rules_reapply_preview`, `preview_change`, `preview_bulk`, `preview_category`, `transfer_candidates`, `prepare_create`, `cancel_change`, `cancel_bulk`, `cancel_merge`, `cancel_reapply`, `cancel_category_preview`, `cancel_import_mapping` | Successful read/session/preview operation: persistence not applicable; exceptions conservatively unknown |
| Unrecognized action | Fixed unclassified/rejected-invalid observation; no diagnostic action handler |


The POSIX reader failure/timeout check is retained in [reader_checks.py](../tests/diagnostics/reader_checks.py). Run it with the built transport-test binary and CLI binary as its two arguments. It simulates an output-file quota failure using `RLIMIT_FSIZE`, expects reader exit 5, checks that the server's latest operation is unchanged, and expects exit 4 from a bounded wait for a nonexistent operation. It does not simulate physical media failure.

The macOS native proxy check is retained in [native_smoke.py](../tests/diagnostics/native_smoke.py), with the application executable as its argument. `--smoke` isolates Settings and uses a missing temporary document path. The script reads only diagnostic JSON and asserts an exposed surface and matching-generation/revision presentation observation. It does not capture screenshots.
