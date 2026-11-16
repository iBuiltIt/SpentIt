# Rules storage

Last updated: 2026-11-16

Current rule behavior is [feature 004](../features/004-rules-and-explanations.md).
The evaluator is [rules-engine.md](rules-engine.md).

The release rules database is version 1. A rules-off file is version 69, is
not the release format, and rejects rule commands. A file whose number is not
the number that build accepts is refused, and its bytes stay the same. There
is no migrator.

Save versions document-wide definitions independently of import state.
Discarding an import does not discard saved rules. Preview and Apply are
separate. Apply is atomic for the chosen scope. Manual fields stay unless that
operation replaces them. Amounts, dates, split structure, and transfer links
stay.

Successful close keeps one current ruleset baseline and any definition still
referenced by retained work. It drops full save requests and superseded
snapshots. See the retention section of feature 004.


## Save performance — owner Go, 2026-11-16

A definition-only save preserves financial and import-readiness caches because
it changes neither transaction values nor their relationships. Its durable
receipt and ordinary Undo/Redo policy still apply; the published Review generation
advances with the saved command.

Save and apply prepares and validates the category lookup once per streamed rule
pass, retaining cancellation and per-record input bounds. It retains exact
validated revisions for preview and re-reads changed transactions before writing. Its changed-row set allows unrelated source reads
and accepted ledger entries to survive the operation. Duplicate dependents and
any actual source/transfer cleanup invalidate the relevant reuse. Import readiness
is still refreshed for application. Rollback discards speculative reads, and
acknowledgement failure cannot publish an unsaved result.

No database schema change or durability setting change is involved. See
[status](status.md) for measurements and checks.
