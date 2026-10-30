# Agent guide: using SpentIt diagnostics hooks for profiling

Last updated: 2026-10-30

Read this when the owner mentions **hooks**, **diagnostics**, **introspection**, **observing the app**, or **profiling with an agent**. This is the operational entry point; [the runtime contract](application-diagnostics.md) defines the schema, masks, limits and verification evidence. [Feature 012](../features/012-application-diagnostics.md) preserves the requirements and design. No chat history is required.

## Standing owner authorization and attachment discipline — 2026-10-30

When the owner asks an agent to use hooks to observe or investigate SpentIt, the owner authorizes the necessary launch, process discovery, connection and read-only inspection within that requested scope. **The owner explicitly requests and pre-authorizes native macOS PID-targeted APIs as an alternative when the computer-use tool cannot attach, its behavior is unclear, or app-name resolution risks launching another instance.** This includes Core Graphics window discovery/capture and Accessibility inspection against the verified SpentIt process. Do not ask the owner to repeat a special “Go” sentence or re-authorize this method. The authorization is recorded here so a new agent does not depend on chat history.

This authorization covers the inspection method, not unrelated data or financial actions. A request to inspect an owner-selected pair includes reading that pair's retained source facts; a generic timing/profile request does not authorize browsing private records. Preserve unsaved work and selections. Do not modify transactions, merge records, commit, discard, import, restart an active working instance, or implement matching changes without authorization for those actions. Keep private paths, identifiers, screenshots and financial contents out of repository documentation and fixtures.

Owner authorization, tool restrictions, sandbox approvals and macOS permissions are separate. Use normal narrowly scoped escalation when the sandbox blocks native window/socket access; existing owner authorization is the justification, not a reason to ask again. This document does not override higher-priority tool restrictions or OS access controls. If a non-waivable restriction or missing OS permission actually blocks the remaining route, state the precise restriction and the minimum user action needed. Do not weaken protections. Do not treat one tool's attachment failure as proof that the process is missing or inspection is impossible.

## Scope and build variants

The hooks observe operational state locally. They cannot read financial records, execute commands, generate reports, click controls or modify a dataset. There is no MCP server or screenshot interface. Use the normal application UI for owner-authorized interactions. Do not call the model's raw command entry to bypass Save / Discard / Stay, validation or previews. Prefer synthetic workloads; profiling permission alone does not authorize opening private statements, copying a dataset, changing financial data, or implementing optimizations.

The owner accepted the existing build switch as sufficient for now (2026-10-29):

| Build setting | Runtime behavior |
|---|---|
| `SPENTIT_DIAGNOSTICS=OFF` (default for a fresh configuration) | IPC server and reader CLI are excluded; collector enablement is refused. `--diagnostics` exits 2. Inert observation calls and disabled collector code remain. This is not a binary stripped of every diagnostics instruction. |
| `SPENTIT_DIAGNOSTICS=ON`, ordinary launch | Hooks are available but collection, listener and UI sampling are disabled. |
| `SPENTIT_DIAGNOSTICS=ON`, `--diagnostics` launch | Local observation is enabled for this process only; the window indicates enablement. |

Both builds use the same financial logic and document format. Switching editions requires no dataset conversion. CMake caches the option: omitting it later does not reset an existing ON tree. Explicitly set OFF to disable it. Separate build directories and output directories avoid overwriting one edition with the other; a stale CLI left in an old build directory is not proof that an OFF executable has an endpoint. Fully stripping inert code, named distribution presets and future action/reporting interfaces are not implemented or authorized by this documentation update.

## Start or attach to the intended instance

1. Read [current status](status.md), this guide and the runtime contract. Determine the operation the owner wants measured and whether the target is synthetic or an explicitly authorized existing dataset. Preserve any unsaved work in an already running app; diagnostics cannot attach retroactively to a process launched without it.
2. First check whether the intended hooks instance is already running. Attach to it when available. Only when a launch is needed, use the existing configured rules-on Release tree and enable/rebuild before measuring:

   ```sh
   cmake -S . -B build/import-ui-rules-release -DSPENTIT_DIAGNOSTICS=ON
   cmake --build build/import-ui-rules-release -j 2
   ./build/SpentIt --diagnostics
   ```

   These commands assume the established dependency configuration and `build/SpentIt` output location. For a fresh checkout, first follow [building](building.md), [UI setup](import-ui.md), and [the README launch instructions](../README.md). The diagnostics option alone does not enable the UI/storage/rules targets. Do not rebuild during timed comparisons.
3. Read the exact `Diagnostics endpoint: ...` line from that application's stderr. Retain the **PID, exact executable path, managed terminal/process handle, endpoint and returned diagnostics session together** in the working session. Keep its terminal/process available. Each launch has a new endpoint; do not guess its name or choose another instance's socket. If attaching to an existing enabled app, obtain that instance's startup endpoint. Do not relaunch merely to discover it without accounting for the owner's current work.
4. In another terminal/tool invocation, set these shell variables, replacing the placeholder with the observed path:

   ```sh
   diag_reader="$PWD/build/import-ui-rules-release/src/ui/spentit-diagnostics"
   diag_endpoint='PASTE_EXACT_ENDPOINT_FROM_STARTUP'
   "$diag_reader" "$diag_endpoint" status
   ```

   Check `schema == 1`, `enabled == 1`, `session`, `ui_known`, and `ui_age_us`. Record the session with all later observations. Numeric flags are 0/1. A new session invalidates all previous IDs, generations and event cursors. An unknown schema requires inspecting its contract before interpreting values.

The endpoint is same-user local IPC, not TCP. Native app/window/socket access may require the tool environment's normal approval. If the sandbox refuses access, use the normal approval/escalation mechanism under the standing authorization above; report a blocker only if it remains unresolved; do not weaken permissions or invent a replacement network endpoint. Startup failure exits 2 without opening a dataset or relaxing permissions.

## Inspect the existing macOS window without launching another app

**Do not call an app-name resolver such as `getApp("SpentIt")` to attach to the standalone `./build/SpentIt --diagnostics` process.** That operation can launch the installed `.app` instead. A running-app inventory may omit the standalone executable, and an exact executable path may be rejected by a tool that expects an application bundle. Neither result means the original process has exited. Do not retry by name, create a second instance, switch datasets or ask the owner to repeat their selection.

Before asking the owner to select transactions and say Ready:

1. Establish the instance identity from the launch handle and endpoint. If necessary, use process metadata (`ps -p "$diag_pid" -o pid=,comm=,args=`) and socket ownership (`lsof -a -p "$diag_pid" -U -Fn`) to verify the process. Never reuse a previous session's PID or window ID without rechecking its identity.
2. Check the exact endpoint with the diagnostics reader. A live endpoint establishes that process's diagnostics connection, not that the UI tool can read its window or selected records. `ui_known == 0` is an observation limitation, not proof of a closed app or no selection.
3. Establish a working window-inspection route for that same PID. Use a tool's documented attach-only capability if available. Otherwise proceed directly with the **pre-authorized native macOS route**: enumerate windows using `CGWindowListCopyWindowInfo`, filter by `kCGWindowOwnerPID`, and retain the intended window's `kCGWindowNumber`. Verify the process/window identity before capture. Use `screencapture -x -o -l "$diag_window" "$diag_capture"` or the supported Core Graphics capture API for that window only. Do not capture the whole desktop or other apps.
4. Native Accessibility inspection can use `AXUIElementCreateApplication(diag_pid)`. Check access and actual returned controls; Qt may expose only a limited tree. Accessibility denial does not establish that window capture is unavailable: test the independent capture route. Do not claim click/navigation capability from a successful screenshot alone.
5. Verify that the captured window belongs to the intended process and shows the expected working screen. Then ask the owner to prepare their selection if it is not already ready. Preserve an existing selection and compare the selected records read-only.

Use a private temporary location outside the repository for any necessary capture, with restrictive file permissions, and delete it after inspection. Do not persist private screenshots or source excerpts in test fixtures, logs or handoffs. Do not relaunch/rebuild the active process just to make a particular UI tool recognize it.

### Hooks, window inspection and source evidence are separate

The hooks endpoint deliberately excludes transaction contents and persistent record IDs. A screenshot can identify the owner-selected pair, but displayed fields are not proof of original imported facts. For a request to investigate that pair, inspect its source views or use a narrowly scoped read-only storage query if UI access is insufficient. Use supported decryption credentials already authorized for that document; the documented public format key applies only to explicit no-password documents. Never retrieve a password from process memory or bypass document protection.

A storage inspection must open read-only, enforce query-only access, preserve normal database locking, and constrain the query to the selected records and necessary comparison checks. Do not copy the live database, use an unsafe immutable/no-lock mode, expose full records or scan unrelated private data. Prefer comparison results and counts over printing identifying text. No financial mutation is authorized by an inspection request.

### Observed attachment failure and successful route — 2026-10-30

During owner-authorized inspection, app-name resolution launched the installed application while the standalone hooks process remained alive. The exact executable path was not accepted by that UI tool, and its app inventory did not show the standalone process. PID/socket verification identified the original instance. Native Accessibility inspection was denied, but PID-filtered window capture succeeded; a narrowly scoped read-only source comparison then completed the investigation. No financial changes were made, and temporary inspection files were removed. This establishes the observed fallback on this macOS setup, not universal Accessibility access or portability. No private record contents or dataset paths are retained here.

## Observe one operation without confusing its completion

Take a baseline `status` immediately before the authorized UI interaction. Save the baseline session, event `sequence`, current `view`, UI revision and cumulative `metrics`. Start bounded event/status polling before the interaction when per-operation spans matter; the ring retains only 4,096 events, 128 per response and 32 outcomes. Polling around 100 ms matches the existing reader cadence. Bound the observation session's duration and any output file size yourself.

Perform the intended UI interaction through the ordinary UI. Find its new operation ID using subsequent `events` and `active`/`outcomes`, matching the action and baseline sequence. Do not assume `latest` still refers to your action: startup probes, deferred selection and other work can supersede it. IDs are observation tokens, not database IDs.

Use the exact IDs obtained from responses (replace the uppercase placeholders):

```sh
"$diag_reader" "$diag_endpoint" events LAST_CONSUMED_SEQUENCE
"$diag_reader" "$diag_endpoint" wait-operation OPERATION_ID 60000
"$diag_reader" "$diag_endpoint" wait-generation MODEL_VIEW 60000
"$diag_reader" "$diag_endpoint" wait-ui UI_REVISION 10000
```

`wait-operation` means a retained terminal execution result is available, not that it succeeded, saved, reached the GUI, or became interactive. Inspect that operation in `outcomes` and require its nonzero `delivered_us` for GUI delivery; its `view` then identifies the applied model. `wait-generation` requires interaction readiness for exactly that model generation. `wait-ui` targets the separate semantic revision (surface/modal/pending/control changes can occur without a new model). Neither wait promises a native frame. If a generation is superseded, record the replacement separately instead of accepting it as completion of the original one.

For events, advance the cursor to the **last returned event's `sequence`** and drain further batches while `truncated == 1`. Do not jump to the response's newest `sequence` when there are unread batches. If no events are returned, retain the cursor. `cursor_stale`, `dropped`, increased `health_failures`, or non-contiguous sequences mean evidence is incomplete; they are not permission to reconstruct missing events. Retained outcomes can still establish their own facts.

CLI exit codes: **0** observed, **2** invalid invocation, **3** disconnected/rejected/invalid response, **4** observation timeout, **5** output failure. Timeouts default to 10 s and cap at 60 s per call; a longer authorized measurement can use further bounded observations. Timeout/disconnection/output failure does not imply rollback or justify retrying a financial operation. Never retry a write because an observation failed.

## Decode the three independent outcome axes

Schema 1 serializes these enums as integers:

| Field | Values |
|---|---|
| `result` | 0 pending; 1 succeeded; 2 failed; 3 cancelled; 4 superseded; 5 not executed |
| `persistence` | 0 not applicable; 1 not attempted; 2 confirmed unchanged; 3 confirmed saved; 4 unknown |
| `presentation` | 0 awaiting refresh; 1 model applied; 2 UI settled; 3 presentation observed; 4 unavailable; 5 failed |

For example, failed execution with confirmed saved capture is not a rolled-back import. `saved_stages` preserves capture at bit 0, preparation at bit 1, and other confirmed save at bit 2. Saved work with failed refresh retains both facts. Follow `recovery` guidance and normal application recovery instead of repeating the write. A low-level transaction span ending does not acknowledge a durable command.

`ui.surface`, modal/pending/control masks and focus roles describe the screen without text. Decode them using the [semantic UI contract](application-diagnostics.md#semantic-ui-and-readiness) and [enum definitions](../src/diagnostics/include/spentit/diagnostics.hpp). An idle decision dialog can be interaction-ready; readiness does not mean the ledger is available. Check surface, modal stack and pending mask. Unknown or unsupported values do not mean zero work. Native presentation is only a matching-generation/revision Qt frame proxy; hidden/offscreen windows and native file choosers cannot supply it. No physical display or GPU/input latency claim follows.

## Attribute elapsed time

All `*_us` timestamps/durations are monotonic microseconds, not wall-clock dates. Use only matching-session data and nonzero timestamps:

| Question | Observation |
|---|---|
| Time waiting for the worker | `started_us - queued_us` for the exact operation |
| Worker execution including its refresh work | `ended_us - started_us` |
| Worker finish to GUI delivery | `delivered_us - ended_us`; inspect `model_apply` spans separately |
| Why the screen is still waiting | UI generation/revision, pending/modal masks, readiness and `ui_age_us` |
| Verification, snapshot, search or ledger costs | Operation-correlated span events; process-cumulative metric deltas across the interval |
| Statement preparation/reuse and cache behavior | Counter deltas and actual cache hit/miss/bypass/invalidation events; SQL text/timing are unavailable |

Span events have `operation`, `span_id`, `parent`, `duration_us` and `exclusive_us`. Do not add nested inclusive times or attribute all process-cumulative metrics to the last operation. `running` identifies unfinished spans; absence of a finished duration is not zero cost. `failed` includes unwinding and does not itself prove corruption. Worker age reflects the last published worker observation, not proof that a long verification stopped executing. UI age/sample gaps indicate scheduling freshness, not rendering/input latency. Identify uninstrumented costs explicitly.

## Reproduce synthetic workloads

These use isolated synthetic fixtures and independent financial assertions. They do not use the reader endpoint; the benchmark reads the same collector in process. It prints phase timing/readiness and correctness results rather than a complete per-operation JSON trace.

```sh
# Small grouped check; requires the diagnostics-enabled benchmark build.
SPENTIT_BENCHMARK_DIAGNOSTICS=1 QT_QPA_PLATFORM=offscreen \
  build/import-ui-rules-release/src/ui/spentit_import_benchmark 120 --groups

# Serial three off/on pairs for each 18,972-grouped and 19,000-ordinary workload.
# Creates an explicit temporary destination outside the source tree.
diag_output=$(mktemp -d /tmp/spentit-profile.XXXXXX)
python3 tests/diagnostics/compare_workloads.py \
  build/import-ui-rules-release/src/ui/spentit_import_benchmark "$diag_output"
```

The comparison runner records binary SHA-256, phase times, exit codes and synthetic logs. Runtime-disabled/enabled comparisons use the same ON-build executable; they do not compare two compiled editions. Run serially with no builds/tests/other benchmarks competing. Report median and range, sample count, workload, backend, build fingerprint, failures, and warm/cold conditions. Offscreen logical readiness is not native rendering. Current pending-reopen comparisons exceeded the provisional 5% observer budget amid substantial noise; do not assume zero overhead or that the budget is met. See [recorded measurements and limitations](application-diagnostics.md#serial-workload-observations).

For a native readiness plumbing check only, run `python3 tests/diagnostics/native_smoke.py ./build/SpentIt`. This launches an isolated smoke instance and reads JSON; it is not a workload performance test. [Other verification commands](application-diagnostics.md#verification-record--2026-10-29) cover faults, transport and sanitizers.

## Hand back useful evidence

Report the operation and synthetic/authorized-data scope; executable/build/backend; observed session and matching operation/view/revision; queue, worker, GUI delivery and readiness observations; dominant spans/counter deltas; missing events and unknown outcomes; independent correctness result; repeated timing distributions and overhead caveats. Keep operational trace files local unless the owner requests export; never add private financial paths, values, records or credentials to repository reports. Separate measured facts from hypotheses and proposed optimizations. Profiling does not authorize changing verification, financial behavior, repeated-transfer matching or starting report implementation.
