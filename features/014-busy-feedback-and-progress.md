# 014 — Busy feedback and operation progress

Status: **active**  
Last updated: 2026-11-05  
Implementation: the owner gave a narrow Go on 2026-11-05 for a more noticeable
animated indicator without added text. That visual first pass is implemented;
the broader operation-state and progress-dialog work below still needs Go. This work does not
replace or complete the in-flight [help viewer and manual](013-help-system.md).

## Indicator visual first pass — owner Go 2026-11-05

The owner found the pale dotted indicator too subtle and asked to try a clearly
animated, more noticeable treatment first, without additional activity text.
The title-row indicator now uses a 36-pixel ring with a five-pixel, high-contrast
blue arc and an 850 ms continuous rotation. Theme colours cover light and dark
appearances; a render-thread RotationAnimator rotates the painted arc. The
accessible name is Working. Existing worker busy timing is unchanged.

Native synthetic tests compare successive rendered frames in both themes and
check hidden idle state and hiding after stopping. This verifies the visual
control's animation, not responsiveness during the owner's heavy workload.
The broader readiness, progress-dialog and cancellation requirements remain open.
See [status](../doc/status.md) for verification. Extra title-row text is not part
of this first pass and must not be added solely from the earlier proposal below.

## Problem and owner observations

Selecting a large account can leave the application apparently idle for about
three seconds before its ledger is usable. The owner reported this for an account
with transactions from December 2017 onward; its row count is unknown. This is an
owner observation, not a measured benchmark.

The owner also described an import file of roughly 116,000 lines, creating about
five accounts and importing categories. After confirming account mappings, the
choice dialog remains visible and appears editable while processing continues.
Applying the subsequent category choices likewise leaves the checklist on screen
instead of clearly showing work in progress. File lines are not transaction counts.
No private file content or location belongs in this work item or test fixtures.

These are required regression scenarios, not the complete scope. Busy feedback
must work consistently whenever the application is carrying out work the user
must wait for.

## Accepted requirements — owner direction 2026-11-05

- Put a visible animated busy indicator at the far right of the top title row,
  above the ledger's **+ New transaction** button. It must work across workspaces,
  not just on the ledger, and remain understandable during modal operations.
- Start feedback when an operation is accepted and keep it accurate until the
  resulting screen is usable, or a failure/cancellation outcome is presented.
  Worker completion alone is not sufficient if the view is still being prepared.
- Distinguish processing from waiting for a user decision. A choice dialog waiting
  for input is not busy. Idle screens must not retain a spinning indicator.
- After account mappings or category choices are submitted, replace the choice
  interface with a dedicated progress dialog. Show the current activity and a
  Cancel control when cancellation is supported. Do not leave apparently editable
  choices on screen while their submitted operation runs.
- Make the general behavior reliable, including animation, control availability,
  transitions, cancellation, errors and recovery. Test the reported scenarios and
  the shared behavior rather than merely checking that a spinner exists.

## Current implementation facts

Source inspection on 2026-11-05 found a `BusyIndicator` in
`src/ui/ImportMain.qml` at the requested title-row location, bound to
`workflow.busy`. The bottom status line also shows the worker phase and offers
cancellation when available. These facts do not establish that feedback is visible
or animated throughout the owner's delay.

`ImportModel::command` in `src/ui/import_model.cpp` sets busy before starting an
asynchronous worker, then clears it after applying the result to the model. QML
layout, rendering and readiness may extend beyond that boundary; the cause of the
reported missing feedback is not yet established. Some immediate selection work
bypasses the asynchronous busy path. Do not indiscriminately turn every click,
keystroke, paint or animation into a busy operation.

The account-mapping dialog remains visible while its import plan exists. Its
submit/cancel buttons are gated by busy, but its mapping fields are not generally
disabled. The category dialog disables its choices while busy but retains the
checklist presentation. Both need explicit choice-to-progress transitions.

## Proposed interaction and implementation

Use one shared presentation of operation state. The initial proposal included
activity text beside the top indicator, such as **Opening Visa…**; the owner’s
subsequent visual-first direction above defers that addition. Preserve the existing bottom status
line and keep its message consistent with the top indicator and progress dialog.
Use indeterminate animation unless a meaningful total and completed count exist;
never invent percentages or estimated time remaining.

For account switching, keep the existing screen visible with **Opening [account]…**
and disable actions that would conflict with the pending switch. Do not relabel
old account rows as belonging to the new account. Finish only after the destination
view and its restored selection/position are usable. No extra modal dialog is
proposed for ordinary account switching.

For import mapping and category application, retain submitted choices internally
while replacing the choice dialog with progress. On success, show the next required
decision or finished view. On a recoverable failure, restore the relevant choices
without resetting them and explain what the user can do next. Do not automatically
resubmit. Cancellation must follow the existing operation's actual rollback and
retention contract; it must not imply that previously completed import steps were
undone. Show **Cancelling…** after a request and prevent repeated cancellation.
When cancellation is no longer available, communicate that processing is finishing.
Closing the progress window or pressing Escape must not silently abandon work.

Track accepted work, required follow-up work and view readiness without a false
idle gap. Stale progress/completion callbacks must not hide a newer operation's
indicator or restore an obsolete dialog. An error, cancellation, document change
or teardown must not leave stuck busy state. Preserve the distinction between a
saved change whose refresh failed and a change that was not saved.

Audit operation entry points across opening/unlocking/closing files, account
switching, imports and repairs, search/sort, rules, Review, reports, saving edits,
acceptance, Undo and backup. Record which need feedback and how completion is
established. Include work outside the existing worker busy flag where it causes
user-visible waiting. Welcome/unlock screens need equivalent feedback even though
the normal account title row is absent. Background housekeeping that does not
block the user's activity must not create a misleading blocking dialog.

Keep the UI event loop responsive. A spinner that freezes during expensive model
application/layout is a failure, not an acceptable implementation. Locate and
address blocking work as needed for this scope without weakening financial checks.
No general account-loading speed target or unrelated storage optimization is
promised by this work item.

## Verification and acceptance

Use synthetic fixtures and controlled delayed operations for repeatable transition
tests, plus representative real workloads for responsiveness. Artificial delays
alone cannot prove that animation survives actual heavy work.

| Scenario | Required checks |
| --- | --- |
| Large account switch | Feedback appears promptly after acceptance, before the waiting period; animation continues; old/new account content is not confused; indicator lasts through usable destination layout and viewport restoration. |
| Large multi-account import | Synthetic input near the reported 116,000-line scale with about five accounts and categories; record actual line, transaction, split and category counts. Mapping choices transition to progress, cannot be edited/resubmitted, and the next decision appears only when ready. |
| Category application | Apply Selected and Use Uncategorized both transition to progress; completion advances correctly; failed submission retains choices for correction. |
| Shared operation coverage | Exercise representative operations from each audited family, including work with no active ledger and a modal dialog; record omissions explicitly. |
| Completion boundaries | Delayed worker, model application, view layout and chained follow-up work; no premature idle, stale callback takeover or stuck indicator. Immediate/rejected operations do not leave busy state behind. |
| Cancellation | Supported request, repeated click, rollback/cleanup delay and request near the commit gate; truthful terminal outcome, no duplicate submission or silent loss of earlier completed steps. |
| Failure and recovery | Validation failure, worker exception, saved-but-refresh-failed outcome and safe retry; correct controls, retained choices where applicable, and accurate saved outcome. |
| Presentation | Light/dark themes, minimum window size, keyboard/focus behavior and accessible activity text. Modal progress does not obscure all feedback or allow conflicting actions. |
| Idle and responsiveness | Waiting for input and completed work stop animation; native observation confirms animation advances during heavy work and Cancel remains responsive where supported. |

Measure action-to-first-visible-feedback and action-to-usable-view separately,
and observe animation continuity during the intervening work. Record platform,
build, workload and actual timings. Define any quantitative responsiveness budgets
before claiming them passed. A busy property assertion or one screenshot cannot
prove timely rendering or continuous animation; include native interaction/frame
observation alongside automated state tests. Test financial/cancellation boundaries
through existing domain/storage checks where the implementation touches them.

Read [the diagnostics guide](../doc/agent-diagnostics-guide.md) before profiling
or using hooks. Use synthetic data by default and preserve any running owner
instance. The owner's private dataset is not a routine automated test input.

## Decisions to settle during implementation planning

Determine why the existing indicator does not cover the reported delay, the exact
view-readiness signal, and handling of extremely short operations without flicker.
Choose precise phase wording and non-cancellable presentation after inspecting
actual cancellation gates. These details must preserve the accepted behavior above;
material changes to cancellation or financial semantics need owner discussion.

## Boundaries and handoff

This is an independent work item alongside Help. Reuse applicable Help modality
and focus conventions without expanding its renderer, chapters or context links.
Preserve existing unsaved-edit guards, operation atomicity, import retention,
privacy and cancellation boundaries. No schema change is proposed.

Implementation must update [project status](../doc/status.md) with actual results,
remaining gaps and unavailable platform checks. Do not mark this feature landed
based solely on a spinner binding or happy-path import test.
