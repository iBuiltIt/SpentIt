# Rules storage

Last updated: 2026-11-09

Current rule behavior is [feature 004](../features/004-rules-and-explanations.md).
The evaluator is [rules-engine.md](rules-engine.md).

Working formats are **70 with rules / 69 without**. A rules-off file rejects
rule commands. Older experimental files are rejected unchanged. There is no
migration.

Save versions document-wide definitions independently of import state.
Discarding an import does not discard saved rules. Preview and Apply are
separate. Apply is atomic for the chosen scope. Manual fields stay unless that
operation replaces them. Amounts, dates, split structure, and transfer links
stay.

Successful close keeps one current ruleset baseline and any definition still
referenced by retained work. It drops full save requests and superseded
snapshots. See the retention section of feature 004.
