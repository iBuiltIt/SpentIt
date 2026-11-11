# 012 — Application diagnostics

Status: **active**
Last updated: 2026-11-09

Local diagnostics are implemented for operation timing, worker state, row
counts, cache activity, and semantic UI state. They are off unless the build
includes them and the process opts in. They are read-only. They exclude
financial records, passwords, and keys. There is no action interface, no
screenshot feature, and no MCP adapter.

`SPENTIT_DIAGNOSTICS` OFF keeps inert code and excludes usable hooks. ON still
requires runtime opt-in. Two distributed editions that strip the inert code
are not implemented.

When the owner asks to use hooks, follow
[doc/agent-diagnostics-guide.md](../doc/agent-diagnostics-guide.md). That
request pre-authorizes native macOS PID-targeted window capture and
Accessibility inspection when ordinary UI attachment is unavailable. Keep the
original process. Do not launch a second instance by resolving the app name.

The runtime contract is [doc/application-diagnostics.md](../doc/application-diagnostics.md).
Diagnostics do not change financial semantics, the file format, matching, or
durability.
