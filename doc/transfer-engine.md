# Transfer lifecycle engine and durable workflow

See [transfer lifecycle](transfers.md) for product behavior and the
[release schema](release-schema.md) for storage compatibility.

## Implemented contract

`spentit::transfers` identifies each endpoint by account, entry and stable allocation identity. Generation selects an allocation by identity, including within mixed-sign splits or repeated equal payments. It creates an exact opposite amount in the same retained currency definition, using the origin date provisionally. Cross-currency inputs fail explicitly; negating the signed minimum fails with the existing checked-money overflow error. Generated provenance is distinct from independent bank evidence.

Pure commit proposals change only the explicitly selected account. The other draft becomes protected by the relationship to that committed side, in either commit order. Clearing a protected draft preserves it. Clearing an ordinary imported origin removes its purely generated uncommitted counterpart. Deleting only a generated destination draft is refused: the origin controls that draft. If the counterpart is an independent draft, clearing the origin unlinks it and leaves that counterpart. Explicit replacement of an independent or committed counterpart unlinks it, leaves its amount, and applies the origin change in the same operation.

Complete-set validation rejects duplicate link identities, reused allocation endpoints, inconsistent states or dates of shared entries, conflicting account currency definitions and reuse of a generated entry. Whole-account proposal functions return all resulting links without mutating their input. When any allocation protects an origin entry, clearing its account preserves that entire entry and its sibling links. Persistence must supply the complete relationship set and perform revision checks and the complete storage transaction; the engine cannot establish completeness or durability on its own.

Match validation checks account, exact money and a valid independent posting date. Explicit proposals retain the evidence identity separately without changing the existing entry date, amount or commit state. Publishing a proposal changes only its account to committed and attaches the evidence; the opposite side keeps its prior state. Attaching evidence to an already committed entry leaves its amount unchanged. The engine does not suppress imported occurrences or calculate durable balance effects; storage performs the atomic integration described below. Candidate ranking remains unresolved. Grouped evidence is resolved at allocation level: only equal opposite allocations link, and repeated equal allocations stay distinct.

`merge` and `retarget` are pure transitions in the same engine. Merge of a generated duplicate on the same account retires that duplicate, including a purely generated committed duplicate, and links the independent allocation. A generated row that has its own import evidence is not removed. Merge of two independent opposite allocations, including two committed rows, records the link and removes neither. Unequal amounts in one currency, and ambiguous endpoints, refuse. An explicit link of two independent allocations in different currencies keeps each posted amount and records no rate. One amount must be outgoing and the other incoming. Generation and automatic matching still refuse a different currency. Retarget to a category or another account follows the same disposable-versus-kept counterpart rule: a purely generated uncommitted counterpart is removed, and a committed or independent counterpart stays with its amount. A committed date-only edit of one linked allocation keeps the link and the other posting date. An amount edit, category rewrite, or removal of one side ends the link and leaves the other amount. A link with `preserved_amounts` is refused rather than repaired. A link that still has attached or proposed evidence refuses an amount, category, date, or removal edit, and it also refuses a payee-only correction. Replacing that evidence is not this command. The source files are `src/engine/include/spentit/transfer.hpp` and `src/engine/transfer.cpp`; tests are `tests/engine/transfer_tests.cpp`. They belong to the existing dependency-free engine target.

For a New ledger posting, a date correction updates each linked allocation's
posting date. A purely generated New destination follows a correction to its
origin. Independent and committed counterpart dates stay as recorded. Any
attached or proposed bank match on an affected relationship blocks the edit.
Storage writes the draft revision and transfer versions in one transaction
using the editor's existing Undo journal. The verifier replays the date change
from the recorded draft revisions and checks the resulting link snapshots.

A correction to a New manual ledger posting can also resize or reverse its
purely generated New counterpart and update its text. A link change that
detaches an existing posting requires confirmation against the same transaction
revision and Review generation. Independent and accepted postings keep their
financial values. The same editor journal records the draft and relationship
changes for atomic Undo, Redo, and verification replay.

## Accepted evidence-matching and unmatched-entry policy

The owner accepted provisional matching: a confirmed match from a pending import remains a proposal until that importing account explicitly commits. Discarding that import drops the proposal and retains the original evidence, leaving the existing transfer's financial state and protection unchanged.

The owner clarified that manual and unmatched entries must be committable even when no bank record will ever arrive. Provide explicit acceptance as valid without a bank match, followed by separate account commit. Acceptance must preserve provenance and allow later matching. It does not override exact-money checks: −100 and +95 may each be committed as valid unmatched entries, but linking stays blocked until the difference is explicitly accounted for. See [transfer lifecycle](transfers.md).

The engine now supports manual-origin generation, explicit unmatched acceptance, provisional evidence proposals, proposal discard and explicit publication. Unmatched manual/generated sides require acceptance before ordinary commit; acceptance alone changes neither state nor money and creates no bank evidence. Whole-entry acceptance applies consistently across sibling allocations. Creation provenance and the original generated date survive later matching, including when the bank posting date differs. Proposal publication requires the exact expected evidence snapshot. Whole-import helpers act only on the selected account and named import and reject reused bank allocations across links.

These transitions now have persisted commands, acceptance history, evidence retention, per-account financial publication and cancellation/recovery integration. Destination-only cancellation of a generated draft is an explicit refusal. Linking two already committed independent allocations records the relationship without changing money. Retiring a purely generated committed duplicate changes only that account's committed balance and is previewed before it is written. Rewriting a committed origin's category, or removing that origin, ends the link and leaves a committed counterpart's amount. A date-only correction keeps the link. Undo of those operations restores the prior link when no later edit, transfer relationship, or source association blocks it. Acceptance cannot bypass an unequal or cross-currency refusal. A link that already preserves amounts in two currencies is not repaired by these commands. A committed row is not moved to another account, and source-occurrence links are not rewritten.

## Verification scope

Engine tests cover both commit orders, unchanged opposite committed state, protected clearing, ordinary origin discard, repeated equal allocations, multiple destinations, reordered allocations, mixed signs, whole-entry protection, independent posting dates, invalid identities/states/dates, mismatched amounts/currencies and signed-minimum overflow. Additional acceptance/proposal/manual cases cover required explicit acceptance, commit without bank evidence, later matching, manual provenance retention, proposal discard in both draft and committed states, whole-import publication/discard, stale expected evidence, and evidence multiplicity. Do not treat this description as a passing test run.

Pure value transitions alone do not establish durability. Linux and Windows remain unverified until run.

## Durable storage contract

`TransferRequest` carries an operation identity, exact expected financial sequence, stable link/allocation identities and, when applicable, the reviewed import revision. Commands create a manual pair, assign an imported draft allocation, accept an unmatched side, commit a local side, propose/drop a bank match, clear an ordinary manual origin, Merge two allocations, or retarget an origin to a category or another account. Every command validates the complete relationship set. Exact retry returns the original receipt; changed content or stale previews fail explicitly. A zero-change Merge or retarget leaves committed local totals unchanged. Retiring a purely generated committed duplicate changes only that account's committed balance, is previewed, and is recorded on the financial timeline. Any other committed-money change is refused before durable commit. A live linked import is edited through the committed correction, removal, or Undo path. Undo of that correction restores the prior link when nothing later blocks it, and it still does not un-commit an import. An unresolved non-local endpoint does not keep occupying that entry for an ordinary non-transfer command.

Encrypted immutable transfer events and version snapshots share the existing financial operation ordering. Heads select the current snapshot; authenticated open replays transitions, evidence identities/amounts/dates, local movement effects and retained-source membership. A match proposal leaves committed amounts unchanged. Projection counts the bank allocation once in place of its local generated/manual amount. Committing that import attaches evidence and removes the redundant local projection in the same SQLCipher transaction, without erasing the bank record. An already committed local side therefore produces a zero net balance change when equal bank evidence is published. Different posting dates survive, alongside original generated provenance.

The storage primitives publish a local side without implicitly accepting its counterpart. The UI submits [file-wide acceptance](import-commit-discussion.md), including any recorded unmatched decisions. Committing either side protects the opposite draft without committing it. Clearing an import retains any protected whole source row and its sibling links, excludes ordinary cleared rows, and permits another import. Retained rows have a separate explicit commit control. Dropping a pending bank import drops its proposal while preserving source evidence and the existing transfer. Complete operations and their history are one transaction; no UI loop owns a financial boundary.

## UI entry points

Use the ledger category picker to generate or replace a destination. Review
and the ledger actions handle matching and linking existing allocations.
The [Review contract](review-sheet.md) defines candidate presentation and
the [ledger editing contract](ledger-editing.md) defines preview and
confirmation for changes to accepted postings.

## Atomic category transfer assignment

The owner's follow-up Go extends the right-click account category operation to
1–4096 selected transactions. `assign_transfer_batch` validates complete allocation
facts and revisions, then composes existing retarget commands inside one storage
transaction. All child commands share the enclosing commit and cancellation gate.
The receipt binds the full ordered request for idempotent retry. Relationships
may occur only once in the batch. Each row names its own source account, so
one save can include several accounts. A row already in the destination
account, or a source amount in another currency, refuses the batch before any
write. Invalid members, stale facts, unsupported
currencies and pre-commit cancellation leave no partial result.

The group is recorded in the existing Review history as `transfer-assignment`,
with original/result link snapshots and one mutation-generation barrier. Undo
reuses that stack, restores prior links and retires purely generated drafts for
new links; independent imported postings remain. Replay applies the same rule.
The ledger exposes the group Undo even before loading Review suggestions. No new
schema or migration is involved. Existing matching policies are unchanged.
See [category behavior](categories.md).

### Reverse category assignment

The group command also accepts category-release requests (retarget mode 0).
All caller facts are validated before any sibling advances a source batch's
revision. Child writes then use the current internal revision inside the same
transaction. Mixed ordinary rows are validated and recorded as Review row
snapshots; linked rows use the existing release command. The group records both
row and relationship before/after snapshots for one Undo. Counterpart retention
continues to follow the engine's disposable-draft policy.

Pending Undo restores the exact original metadata and ruleset provenance.
Category and rule audits recognize such a reversal only with a durable Review
Undo receipt and matching original/result snapshots, including the immediately
preceding draft state. Snapshot comparison permits stacked Undo without dropping
ownership checks on ordinary edits. Existing source history stays encrypted.
