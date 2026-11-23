# Rules storage

Last updated: 2026-11-16

Current rule behavior is [rules contract](rules.md).
The evaluator is [rules-engine.md](rules-engine.md).

The format and upgrade policy are in [release schema](release-schema.md).
A rules-off file uses version 69 and rejects rule commands.

The rules-enabled build supports 4,096 saved rules in the existing release
database layout. Disabled rules count toward the limit. Use the updated build
to open files saved with more than 128 rules. The previous Release storage
build rejected a synthetic 129-rule file during verification and left its
bytes intact. The updated build opened that file and a 128-rule file created
by the previous build.

Save versions document-wide definitions independently of import state.
Discarding an import does not discard saved rules. Preview and Apply are
separate. Apply is atomic for the chosen scope. Manual fields stay unless that
operation replaces them. Amounts, dates, split structure, and transfer links
stay.

Automatic rules skip transactions already recognized as duplicates when the
import runs. Verification counts the transaction versions eligible at the
application's recorded starting revision. A later Review decision does not
change that count. The explicit pending-batch Apply command keeps its own scope.

Successful close keeps one current ruleset baseline and any definition still
referenced by retained work. It drops full save requests and superseded
snapshots. See [rules retention](rules.md#retention).


## Save performance

A definition-only save preserves financial and import-readiness caches because
it changes neither transaction values nor their relationships. Its durable
receipt and ordinary Undo/Redo policy still apply; the published Review generation
advances with the saved command.

Save and apply prepares and validates the category lookup once per streamed rule
pass. Bounded lookups reuse import category decisions and account alias results
within that pass. The matcher evaluates the same scope with its existing
cancellation and input bounds. Save and apply keeps exact validated revisions
for preview and re-reads changed transactions before writing. It calculates the
committed balance once per affected account and checks that each rule write
preserves it.

When no transactions change, Save and apply retains prepared import readiness
and Review results and advances their generation after commit. Applications that
change transactions refresh import readiness. Their changed-row set allows
unrelated source reads and accepted ledger entries to survive. Duplicate
dependents and source or transfer cleanup invalidate the relevant reuse. Rollback
discards speculative reads. A lost acknowledgement follows the durable receipt.

No database schema change or durability setting change is involved. See
[diagnostics guide](agent-diagnostics-guide.md) for measuring current behavior.
