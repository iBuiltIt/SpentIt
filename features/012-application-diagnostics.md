# Application diagnostics and observable UI state

Status: **active**  
Last updated: 2026-10-29  
Implementation state: local diagnostics implemented across the seven stages below, with verification evidence and explicit remaining coverage/overhead limitations. MCP and screenshots are excluded. See [runtime contract and evidence](../doc/application-diagnostics.md).

## Authorization and accepted requirements

The owner authorized investigation and documentation (Steps 1 and 2) on 2026-10-29. The owner subsequently gave explicit Go on 2026-10-29 to implement all seven stages below, excluding screenshots and MCP, preserving the planning edits, and stopping before performance optimization or repeated-transfer matching. A possible MCP adapter (Step 4) is future scope, not authorized implementation. This work precedes the remaining [performance and transfer-matching handoff](../doc/import-performance-handoff.md). Reports remain deferred.

Owner clarification (2026-10-29): the existing `SPENTIT_DIAGNOSTICS` build-time separation is sufficient for now. OFF excludes usable hooks but retains inert code; ON still requires runtime opt-in. Two distributed editions and fully stripping inert code are not implemented. See the [agent guide](../doc/agent-diagnostics-guide.md) for the accepted distinction and operational instructions.

Accepted requirements:

- Expose operation timings, worker queue state, row counts, cache activity, and when the UI becomes usable, for performance and correctness evaluation.
- Let an agent become aware of application state that the user is aware of: current screen, pending work, completed work, failures, and relevant interaction state. Screenshots are excluded.
- Diagnostics are explicitly enabled and read-only by default. The proposed first version has no action interface at all.
- Exclude financial records, passwords and keys. Tests and local profiling must work without MCP.
- Any future action tools must use normal application commands and validation, including editor guards, previews, account-scoped commits and durable-outcome handling.

The concrete contracts and sequence below were the **proposed implementation** reviewed with that Go; the runtime document records actual coverage and limitations. No financial semantics, document format, matching policy or durability settings change.

## Investigation: current implementation and gaps

Inspected implementation and test sources on 2026-10-29; no application, benchmark or private dataset was opened for this investigation. Existing measurement results are historical evidence linked below, not rerun results.

| Existing seam | Verified code behavior | Diagnostic implication |
|---|---|---|
| [ImportModel](../src/ui/import_model.hpp), [command dispatch and worker](../src/ui/import_model.cpp) | Exposes state, busy, selecting, phase, cancellation availability, error and `completed(action, success)`. A private single-thread pool executes commands. Most commands arriving while busy return without execution; inspection during selection is retained in one replaceable pending slot. Retired ledger snapshots are also destroyed on that pool. | Instrument admission, replacement, worker start/end, result delivery and cleanup separately. Pool activity is not equivalent to user-operation activity; there is no general application command backlog to report today. |
| [OperationContext](../src/app/include/spentit/operation_context.hpp) | Shared available/requested/sealed cancellation gate and string progress callback. Some operations seal at dispatch; grouped import can seal and reopen the gate between durable stages. | Sealed does not mean committed. Add typed observation alongside the existing cancellation contract, without changing its races or checkpoints. |
| [Worker execute/snapshot](../src/ui/import_model.cpp) | Executes an action, then obtains a verified presentation snapshot. Failure handling can resolve a durable receipt, close an uncertain session, or report a refresh failure after a saved change. GUI result handling resets rows, clears busy and emits completion before QML deferred work necessarily settles. | A single success boolean cannot explain execution, durable outcome and presentation. Observe each independently and correlate them. |
| [QML application](../src/ui/ImportMain.qml) | Owns currentPage, startup probe/auto-open, dialogs, dirty transaction/category/rule forms, Save / Discard / Stay, pending search/navigation/counterpart reveal, viewport restoration and delayed callbacks. | Export explicit semantic UI state from the owning UI. Model-only introspection misses visible state and pending work. Do not dump the QML tree. |
| [Storage gateway](../src/storage/document_gateway.cpp), [ImportStore](../src/storage/import_store.cpp) | Gateway owns credentials and document lifecycle. Store opens with full verification; verification includes database/cipher/foreign-key integrity, evidence and histories. Transaction helper executes COMMIT. | Instrument safe phase boundaries at their owners. Neither credentials nor arbitrary exception strings may enter observation. A transaction COMMIT is a substage, not necessarily the entire logical operation. |
| [Store caches](../src/storage/import_store.cpp), [group store](../src/storage/import_group_store.inc), worker caches | Scoped SQL statements; a connection-local group snapshot invalidated by writes and bypassed during verification/transactions; page-cache target; worker account/catalog/rules/ledger/view caches. | Observe hits, misses, bypasses, invalidation and sizes at actual cache branches. Do not infer hits from elapsed time or expose cached data/SQL. |
| [Benchmark](../tests/ui/import_benchmark.cpp) | Drives real gateway/model/QML with synthetic data; times completion, samples a UI heartbeat and queued frame notifications, and asserts financial results. Optional phase printing exists. | Reuse fixtures and assertions; replace ad hoc timing boundaries with correlated spans. Retain native/offscreen distinctions. |
| [Navigation tests](../tests/ui/import_navigation_tests.cpp), [recovery tests](../tests/ui/import_recovery_tests.cpp), [model tests](../tests/ui/import_model_tests.cpp) | Tests inspect model maps and named QML controls, invoke normal actions, and often wait for busy to clear. | Add typed waits for the intended operation and UI generation; keep independent domain assertions and pointer/keyboard interaction coverage. |
| [QIF diagnostic](../tools/qif_diagnose.cpp) | Separate local structural parser diagnostic with bounded, value-free reporting. | Reuse its privacy discipline, not its parser-only timing as a measure of application latency. |

The existing `state`, `rowRecord`, accessibility labels, dialog titles, error text and command arguments contain financial or identifying information. They are **not** safe diagnostic payloads. Removing a few sensitive keys from those maps is insufficient; the observation schema must be constructed from an allowlist.

The [latest performance report](../doc/import-ui-performance.md#application-performance-work--2026-10-29) records improved synthetic switching but opening above target. Owner-observed native opening/switching differs from the offscreen workload. New hooks must locate that difference rather than assume a rendering multiplier or label all waiting as storage work.

## Proposed observation contract

### Enablement and privacy

Use an optional diagnostics build target and an explicit per-process launch option, provisionally `--diagnostics`. Normal launch emits no diagnostic stream, starts no endpoint and runs no diagnostic sampling timer. Do not persist enablement in dataset or normal Settings. Show a small enabled indicator in the UI. A compiled-out build reports the option unsupported rather than silently enabling something else.

Build a typed schema with enum names and bounded numbers/booleans. Allow build/version/configuration identifiers, platform/backend, monotonic times, operation kinds, safe phase/error codes, scoped counts, cache statistics, control roles and readiness reasons. Never export amounts, balances, financial dates, payees, memos, references, account/category/rule names, source bytes, filter/regex text, SQL/bindings, file paths, passwords, keys, raw exceptions or user-derived labels. Do not hash these values as a substitute for exclusion. Unknown phases/errors map to a fixed `unclassified` code.

Use process-local operation IDs and document-generation tokens, not persisted financial identifiers. If distinguishing selected accounts or rows is necessary, assign bounded session-local opaque tokens with no exported lookup table. Reset them on close/switch; never derive them from names or record hashes. Aggregate counts and timings still disclose workload shape: treat exported diagnostics as local sensitive metadata, not anonymous public telemetry. No automatic upload or crash attachment.

### Correlation, freshness and bounded collection

A snapshot carries schema version, process/session token, document generation, monotonically increasing snapshot/event sequence, capture time, component freshness, current operation, last terminal outcomes and UI/view generation. Use one monotonic clock domain for elapsed times. Publish immutable component summaries from their owning threads; a reader must not wait behind document verification or query storage to answer a diagnostic read.

Represent worker and GUI freshness separately. A last-known UI snapshot remains readable during a GUI stall but must say how old it is. Do not imply that concurrently sampled components form an atomic financial snapshot. Stale generations cannot satisfy waits for a new document/view. A timeout is a reader's observation, not proof that the operation failed; a disconnected process has unknown outcome until the normal application resolves it.

Use a bounded event ring plus cumulative counters and bounded duration summaries. Provisional limits: 4,096 events, 4 MiB serialized snapshot/response cap, at most 128 events per read, and rate-limited UI/queue sampling at no more than 10 Hz. Record drops, truncation, unsupported metrics and sampling mode explicitly. Preserve the current active operation and latest outcome independently of ring eviction. No per-row event stream; aggregate high-frequency SQL/delegate activity per span. Readers cannot delay a financial operation; overflow drops diagnostic history rather than blocking or growing indefinitely. Collector failures are contained and surface as diagnostic health failures without changing command success or durable behavior.

### Operation and worker state

Record command attempts as accepted, rejected-busy, deferred, superseded or rejected-invalid, without storing arguments. Track the deferred inspection slot separately from accepted queued commands and cleanup jobs. Include queue age, running job kind, queue wait, worker duration, result-delivery delay, snapshot-application time and deferred UI-settle time. A task's enqueue/start/end events must use the same identity.

Keep independent axes rather than one ambiguous status:

- Execution: admitted, queued, running, finished; result succeeded, failed, cancelled, superseded or not executed.
- Persistence: not applicable, not attempted, confirmed unchanged, confirmed saved, or unknown. Multi-stage capture/preparation reports stage outcomes separately. Cancellation can leave retained evidence; never translate it automatically into unchanged.
- Presentation: awaiting refresh, model applied, UI settled, presentation observed, unavailable or failed.
- Cancellation: available, requested or sealed, directly reflecting the existing gate.

A saved operation with failed refresh must expose `confirmed saved` plus `presentation failed`, safe recovery guidance such as `reopen_and_inspect`, and no invitation to retry blindly. Emit confirmed durable success only from existing command outcomes/receipts after the durable boundary; do not infer it from a progress label, a sealed cancellation gate or a low-level transaction event. Where existing code cannot establish an outcome, report unknown until an authoritative normal outcome lookup resolves it. Do not add extra database probes solely for observation.

### Timings, counts and cache activity

| Layer | Initial hooks | Intended question |
|---|---|---|
| Startup/lifecycle | Entry, QML creation, startup probe, auto-open chain, unlock wait, verified open, initial model and UI readiness | Is delay before open, in verification, or after data reaches the UI? User password entry is a separate wait, not execution time. |
| Storage verification | Integrity checks, account/group validation, evidence parse/reconstruction, source associations, category/rule/committed/transfer history validation; counts processed and total only where known | Which existing validation dominates, and did the required phases actually finish? |
| Commands | Import capture/preparation, resolution, commit/save/Undo, account switching, backup publication and refresh | How much time belongs to each stage and durable outcome? |
| View construction | Account/catalog/group metadata, ledger load, search/sort, selection lookup, snapshot build/apply/reset, old-view disposal | Is the worker recalculating data or the GUI applying/rendering it? |
| Caches | Named cache hit/miss/bypass/invalidation reason, entries/bytes where cheaply known; statement prepare/reuse counts and aggregate time; page metrics only if supported safely | Is reuse working, and did a write or failure invalidate the correct cache? |
| UI | Visible/filtered/loaded row counts, delegate creation/reuse counts, event-loop gaps, model resets, pending restoration, first matching frame observation | Is a slow switch caused by loading, GUI work, or presentation? |

Every count names its scope and generation: dataset/account/import/current filtered view/visible viewport. Loaded rows, filtered rows and instantiated delegates are different counts. Unknown totals remain unknown rather than zero. No money totals are included. Nested spans carry parent IDs; report inclusive and exclusive costs explicitly and do not add overlapping spans as if serial. High-detail SQL timing and frame sampling are separately enabled profiling modes; the default observer remains light.

### Semantic screen state and usable UI

Register stable semantic identifiers for welcome/setup/unlock, ledger, categories, rules, history, settings, and the existing placeholder/report state. Inventory all dialogs, menus and native file choosers. Report the active surface and modal stack, focused control role, visible/enabled control states with bounded reason codes, sidebar visibility, editor kind/dirty flags, selection count, filtered/projected mode, sort key/direction, viewport range and selected-item visibility. Report search presence/validity and pending application without search text. Register only safe static control IDs, not arbitrary QObject names or accessibility text.

Include pending startup, guard decision, search, account navigation, counterpart reveal, preview, viewport restore and close intent. Expose blockers as reason codes and counts, including displayed/total/truncated counts; exclude candidate records and names. Native chooser state is limited to chooser purpose/open/closed; never inspect its filesystem listing. Unsupported or uninstrumented surfaces must be marked unknown, not quietly omitted as if absent.

Define readiness for a specific UI generation and surface:

1. **Model applied:** the requested operation's snapshot has reached the GUI.
2. **Interaction ready:** the intended surface has applied its state, required visible delegates/editor are ready, relevant deferred navigation/restore has settled, and the controls appropriate to that surface can accept input. A modal prompt may be ready for a decision while the underlying ledger is blocked. Dirty edits do not themselves make the screen unusable.
3. **Presentation observed:** a frame associated with that applied generation has been observed while the native window is exposed. This is a presentation proxy, not proof of physical display or human perception. Do not count an old frame delivered late as the new generation's frame.

Record blocker reasons rather than relying on `busy == false`, an empty worker queue, a fixed sleep or an arbitrary settling delay. Separate background cleanup from work that blocks interaction. Hidden/minimized/offscreen windows can establish logical readiness but cannot claim native presentation; return unavailable with reason. Event-loop heartbeat gaps are scheduling observations, not input/GPU latency. No synthetic input is injected into a user's live session by diagnostics.

### Correctness observations

Observe existing validation with named check, scope/generation, running/passed/failed/not-run state and safe failure code. Report only checks actually executed. Cache invalidation and view-generation observations help detect stale presentations; they do not replace source-evidence, exact-money, multiplicity, transfer or history assertions. Tests retain independent expected values inside synthetic fixtures and verify those through normal engine/repository APIs, without exporting them through diagnostics.

Diagnostic snapshots are ephemeral observations, not durable history, commit receipts, backups or recovery evidence. If an event is lost at process death, absence proves nothing about commit. Recovery continues through existing application paths.

## Stepwise implementation plan — authorized 2026-10-29

Each step includes implementation, focused tests and updated evidence. Follow dependencies in order; no broad performance optimization or transfer-policy change is included.

### 1. Define and test the safe schema and collector

Add a small standard-C++ observation contract/collector with no Qt, SQLCipher or MCP dependency, composed at executable boundaries. Keep the engine independent; use app/storage/UI adapters where observations belong. Wire an optional CMake target and null/disabled sink; avoid a wholesale worker or repository rewrite. Define schema enums, clocks, generation tokens, ring limits, fixed safe errors and immutable snapshots. Add synthetic contract/serialization tests for bounds, sequence gaps, overflow, stale generations, concurrency, disabled behavior and hostile values. Exit: unapproved strings cannot enter serialized fields, and headless collection needs no GUI or MCP.

### 2. Instrument dispatch, worker scheduling and outcomes

Instrument `ImportModel::command`, deferred inspection, retired-view jobs, result delivery, `OperationContext` and authoritative command outcome branches. Preserve existing admission/cancellation behavior. Inventory every existing action and classify whether its durable outcome is observable; use unknown where necessary. Test rejected busy attempts, superseded inspection, cleanup backlog, cancellation before/after sealing, multi-stage retained capture, durable success with refresh failure and unknown outcomes. Exit: a reader can reliably distinguish pending, finished, failed and saved-but-not-displayed work by ID.

### 3. Add storage, verification and cache spans

Instrument the gateway, verification stages, transaction/command boundaries, worker snapshot/search and actual cache branches. Aggregate statement metrics without SQL text; avoid per-record allocations or progress events. Include correct cache bypass/invalidation and rollback behavior. Use existing synthetic validation, cache regressions and relevant fault/recovery cases to compare enabled/disabled results. Exit: opening and switching can be decomposed without skipping checks, retaining new data caches or changing the document schema.

### 4. Publish semantic UI snapshots and readiness

Add a narrow QML/C++ observer interface populated from explicit bindings and registered controls, including every existing page, modal, guard, pending navigation and file chooser. Implement generation-aware model/interaction/presentation milestones, freshness and optional heartbeat/frame sampling. Extend navigation/QML tests with real pointer/keyboard cases, unsaved edits, failed saves, search clearing, smaller model resets, blocker reveal, deferred account restore, modal decisions, hidden windows and delayed/absent frames. Exit: an agent can explain the current screen and what it is waiting for without record content or screenshots.

### 5. Expose local read-only access without MCP

Provide in-process snapshot/event reads for tests, plus a small local CLI reader and optional bounded JSON trace export for profiling. Proposed desktop transport: explicitly enabled Qt local IPC, per-process endpoint under an owner-only runtime directory with same-user access checks, no TCP listener, no credentials in discovery metadata, no automatic filesystem export. The CLI supports status, bounded events-after-cursor and bounded wait-for-operation/generation; each request has size/time limits. The server reads published summaries independently of a blocked document worker or GUI; it does not run SQL or request QML traversal. A disconnected/slow client cannot stall producers. Reject unknown methods and all mutations. Surface startup/access failures without weakening permissions. Test multi-instance isolation, malformed/oversized requests, stale cursors, access denial, disconnect/backpressure and shutdown. Exit: local tools can inspect a running enabled application and report stale UI during a stall. Transport and permission details require platform validation; do not claim portability from a macOS result.

### 6. Integrate correctness and profiling workloads

Update the existing import benchmark and focused tests to wait for the exact diagnostic operation/UI generation rather than arbitrary sleeps or generic completion. Keep commands on existing APIs and financial assertions independent. Cover small deterministic CI fixtures, grouped 18,972-row and ordinary 19,000-row workloads, then 100k/200k where appropriate and practical. Include draft and committed reopen, switching, inspection, query/scroll, save/refresh, commit, resolution, Abort, retry and post-write invalidation. Run repeated serial enabled/disabled Release comparisons with no competing builds; report distributions, workload/build/backend and sample counts. Distinguish warm reopen from OS-cold cache; do not claim disk-cold without controlling it. Provisional observer overhead budget: median operation-time increase at most 5% on representative long operations, bounded memory within configured collector limits, and no new material UI stalls. Report noise and raw ranges; do not enforce flaky wall-time thresholds in CI. Higher-detail profiling overhead is separately measured. Exit: hooks attribute costs and preserve exact results, without claiming that this diagnostics project fixes the outstanding performance target.

### 7. Harden, document and hand back

Run privacy canaries through every output surface using synthetic financial text, paths, passwords, keys, search patterns and exception messages; verify none escape. Exercise collector/export disk-full, allocation/capacity pressure, stalled readers, shutdown races and interrupted processes. Select storage I/O/interruption and durable-outcome tests for touched paths; compare enabled/disabled outcomes. Run appropriate sanitizer/thread diagnostics where supported, rules-on/off configurations and compiled-out/runtime-disabled builds. Verify native macOS readiness separately from offscreen tests; record unavailable Linux/Windows checks. Document launch/CLI/schema/limits/privacy/overhead and coverage gaps, update status, and rebuild the local Release application only during authorized implementation. Exit: all required checks have evidence or explicit unresolved limitations; diagnostics cannot acknowledge or alter financial work. Stop after handing back diagnostics. Remaining performance optimization and repeated-transfer matching each require subsequent authorization.

## Possible later MCP adapter

Keep the schema and observation API independent of MCP. A later adapter can offer read-only status, operation lookup, bounded events and bounded waits using the same local interface, with no second database connection or alternative UI state model. Advertise schema/capabilities and retain freshness, unknown outcomes, bounds and privacy filtering. No MCP dependency or server is necessary for Steps 1–7.

MCP transport, authentication, discovery, installation and agent permissions require a separate design/Go. Any later action API is another explicit scope: it must enter the same UI/application intent path, including Save / Discard / Stay, validation, previews, stale-state checks, cancellation and durable-outcome resolution. Calling the current model's raw command entry directly can bypass QML guards; it is not sufficient merely because it is named `command`. No SQL, arbitrary property mutation, arbitrary code evaluation or diagnostic backdoor should become an action tool.

## Implementation choices and boundaries

No unresolved financial policy is needed for this plan. The implementation uses the proposed launch/build switches, local IPC/CLI delivery, process-local tokens and bounded retention. The 5% overhead budget remains an empirical target, not a guarantee. If a hook cannot meet the no-record-content rule, omit that payload and report limited coverage rather than quietly broaden access. Screenshots remain excluded. The plan does not authorize retrieving private datasets for tests or automatically exporting diagnostic files.

## Verification of this planning change

Source inspection and repository documentation consistency/link checks only. No runtime tests, native performance measurements, diagnostics implementation or MCP verification are claimed. See [project status](../doc/status.md) for the actual planning-check result.

## Implementation evidence — 2026-10-29

The seven stages now have a standard-C++ collector, dispatch/worker and authoritative outcome adapters, storage/verification/cache spans, semantic QML state, an independent local IPC reader/CLI, correlated synthetic workloads, and hardening tests/documentation. The feature remains **active**: the runtime contract explicitly identifies limited semantic/control and profiling coverage and platform/fault checks that have not been established. This is not a claim that every proposed payload or validation combination is complete.

See [launch, schema, bounds, command coverage and verification](../doc/application-diagnostics.md). The original investigation and plan remain above as the acceptance/reference baseline. No performance optimization, repeated-transfer matching, screenshot or MCP implementation is part of this change.
