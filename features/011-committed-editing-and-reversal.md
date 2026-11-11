# 011 — Committed editing and reversal

Status: **active**
Last updated: 2026-11-09

Accepted transactions stay editable. Removing one changes the current balance.
It does not add an offsetting payment. A bank refund is its own imported
transaction. Original source bytes and original amounts remain.

Undo appends the restored state. It is refused when that transaction or a
dependent relationship has changed since. Undo does not un-accept an import,
does not invent an opposite payment, and has no force override. The file-wide
chain and Change history are in [doc/action-undo-design.md](../doc/action-undo-design.md)
and [doc/change-history-handoff.md](../doc/change-history-handoff.md).

Reimport of a removed transaction stays unresolved. It is not automatically
included or excluded.

Ordinary corrections and removals of a live transfer link are refused. The
locked transfer cases are in [feature 008](008-transfer-matching-and-lifecycle.md).
After the link has ended, the unresolved imported counterpart is an ordinary row.

Moving a committed row to another account, and rewriting source-occurrence
links, must fail in domain and storage validation. Hiding the command is not
enough.

The storage revision model is in [doc/committed-changes.md](../doc/committed-changes.md).
Ledger commands are in [doc/ledger-editing.md](../doc/ledger-editing.md).
