# 014 — Busy feedback and operation progress

Status: **active**
Last updated: 2026-11-09

The bottom status line is shared by every workspace. A rotating indicator is
visible during work. It has no extra caption from the indicator itself.

## In place

- Graceful quit shows **Closing SpentIt…** after unsaved-edit and acceptance
  choices are resolved. The main window stays visible. The dialog has no
  buttons. Escape and outside clicks do not dismiss it. A check failure offers
  the existing Return and Quit anyway choices. Quit anyway uses the same
  progress while closing without a new audit.
- Import mapping shows **Importing transactions…**. Apply Selected and Use
  Uncategorized show **Applying imported categories…**. Cancel requests
  cancellation while it is still available, then the control becomes
  **Cancelling…** and **Finishing…**. Completion closes the dialog and shows
  the next required decision. A saved operation whose refresh failed keeps the
  existing recovery message.
- The shared calendar is used for report dates, opening dates in import
  mapping, opening-balance corrections, and manual transfers, as well as
  transaction dates. Choosing a date updates the field. It does not submit the
  enclosing operation. Typing remains available. Date-format settings are not
  calendar dates.
- Export dialogs show their own progress and errors and disable Export while
  work is in progress.

Financial, cancellation, and close policies are unchanged by these dialogs.
See [open/close checks](../doc/fast-startup.md).

## Not done

The broader readiness audit: feedback lasts until the resulting screen is
usable, and conflicting actions stay unavailable. That audit is first-release
work and needs its own Go. It is separate from the usage-driven UI list.
