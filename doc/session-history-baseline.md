# Session change history and a close-time baseline

Last updated: 2026-11-07

State: **implemented; lifecycle and storage acceptance verified**. The failed
`f8a1f4b` architecture was corrected in place, preserving unrelated changes.
Permanent revision facts and monetary replay are removed. See
[measurements and verification](session-storage-comparison.md).

## Corrected acceptance boundary — owner Go 2026-11-07

The current state is the baseline. Full verification must validate that baseline,
necessary retained dependencies and subsequent session changes without replaying
all expired ordinary revisions. Do not keep a permanent record, fingerprint,
monetary summary or other substitute for every expired ordinary revision.
Compact command receipts may prevent duplicate execution and conflicting request-ID
reuse; they must not archive transaction history.

Storage is an acceptance criterion. Compare identical import-only, repeated
edit/close and cross-session import-Undo workloads with `cf8f1a5`, before the failed
implementation. Report physical bytes, free pages and actual retained payload/index
bytes. Neither retained storage nor file size may grow relative to that baseline;
no shrinking or padding tricks. If a genuine required tradeoff prevents meeting
this criterion, stop and explain it. No such unavoidable tradeoff has been
established by the measurements so far.

Normal opens and closes, including edited and uncommitted-import sessions, must
use reliable encrypted session state without full verification or full-file
hashing. Retain full checks after abnormal shutdown, for backup/restore and on
explicit request. Preserve operation validation, encryption, locks, atomicity,
durability and error propagation. This is part of the same correction, not a
separate optional follow-up. Manual prose and screenshots remain deferred.

## Accepted product direction

Ordinary Change History and its Undo belong to the current document session.
After a successful close, the current saved state becomes the next session's
baseline and ordinary earlier edits are no longer inspectable or undoable.
Original imported values and **Undo last import** must survive sessions.
Completed uncommitted imports must also survive; closing is not acceptance,
discard, or a change to report inclusion or balances.

Owner-approved verification direction (2026-11-07): remove routine full
verification on normal open and close. Keep full checks after abnormal shutdown,
for backups/restores, and on explicit user request. Maintain financial validation
within operations and exercise independent checks in automated tests. This accepts
reduced independent detection of bugs during otherwise successful sessions.
“Verified baseline” does not mean a fresh full audit on every close. Implementation Go was subsequently given for this scope.

## Earlier implementation and reported delay

The earlier [startup contract](fast-startup.md) reuses full audits only for an
identical file hash and verifier fingerprint. `ImportStore::connect` in
`src/storage/import_store.cpp` checks approval, otherwise audits, then performs
incomplete-import cleanup and review preparation. `ImportStore::certify` reuses
file approval or an unchanged opening audit; otherwise it audits before publishing
approval. `src/storage/document_gateway.cpp` calls certification on normal close.
Writes, recovery journals, missing cache approval and verifier changes can defeat
reuse. Full-file hashing and ledger/review preparation also cost time even when
historical auditing is skipped.

Owner observation: after a long import and quitting without Commit, the next
open and close can both be slow, eventually becoming fast. This has not been
reproduced or diagnosed. Completed uncommitted imports alone should not require
endless audits. A fully audited, unchanged session is intended to reuse that
audit on close. Do not claim the reported delay is necessarily verification or
that its cause is known. Investigate exact phases and invalidation causes under
the diagnostics guide if separately requested; no private dataset was inspected
for this design.

## What a successful SQL commit proves

SQLCipher/SQLite transactions supply atomicity and durability under the configured
storage guarantees and enforce declared constraints. They do not independently
know SpentIt's complete financial semantics. A programming bug can atomically
save the wrong category, omit an intended counterpart, or save mutually
inconsistent application records if the relevant constraint is not enforced.

The existing audit checks relationships, exact allocations and balances, ownership,
and historical replay in addition to database checks. It is additional detection
of certain application bugs and corruption, not a second mechanism to make COMMIT
durable. It cannot prove that an internally consistent transaction reflects the
owner's intent. A clean-shutdown marker also cannot prove semantic correctness or
detect every later external file change. No observed user-facing audit failure is
not evidence that the checks have no detection value; nor does it establish that
their repeated latency is justified for normal use.

## Retention boundary

Retain across sessions:

- Current active transactions, exact amounts/currencies/splits, dates/text,
  acceptance state, accounts/openings, categories, rules, preferences and reports.
- Original imported facts and their source associations needed for original-value
  display/restore and duplicate recognition. Do not invent imported originals for
  manual entries or introduce a new permanent archive of forgotten source files.
- Current transfer relationships, generated-counterpart ownership, saved Review
  decisions, exclusions and their validity/conflict tokens.
- Removed-transaction identity/original matching facts where needed to keep a
  reimport unresolved instead of silently restoring or excluding it. A retained
  recognition record is not ordinary historical Undo eligibility.
- Source-scoped import rollback records, prior transfer arrangements, ownership
  and dependency/conflict information required by [import Undo](import-undo.md).
  Retain sufficient prior state to reverse a later imported match or Review Link
  without overwriting newer work, including changes made in an earlier session.
- Durable outcome identities needed to resolve retries or lost acknowledgements.
  Baseline publication must not make an old request accidentally execute twice.

Owner decision (2026-11-07): after successful close, old command retries may
return a compact durable **already completed** result and require refreshing
current data instead of returning expired before/after snapshots. Retain enough
request information to reject reuse of an operation ID with different contents.
Never execute an already-completed operation again, and never present current
data as its historical result. Import Undo retains all required snapshots.

Expire at successful session close: ordinary intermediate edit snapshots and
their user-facing history/Undo entries, once none of the retained purposes above
references them. Undoing an edit within the session need not preserve the edit
forever. Records retained solely for import rollback must not appear as old
ordinary edits that the user can undo from Change History.

Import Undo keeps its current eligibility: an import containing accepted
transactions is not eligible for whole-import Undo. Keeping it across sessions
does not authorize undoing arbitrary committed imports. Earlier imports and edits,
shared sources, multi-account scope, and conflict refusals retain their existing
semantics. Retention must be based on explicit dependencies, not just dates or the
last displayed import. Do not reset conflict generations when compacting history.

## Accepted baseline representation and transaction boundary

Use an explicit baseline generation and a durable current-session journal in the
encrypted document. Current state is authoritative; the journal supplies session
Undo and recovery, not an indefinitely growing requirement to reconstruct every
past interpretation. Baseline validation checks current financial state plus all
retained import/recognition/relationship obligations. Financial identities and
original values survive unchanged; never synthesize a balancing transaction or
alter account openings to make replay shorter.

Prefer an in-document SQL transaction for logical baseline publication: publish
the new baseline generation, retain required dependency records, prune eligible
session history, and mark the document clean atomically. Resolve unsaved edits and
the existing acceptance decision first. Hold the document lock through completion.
Do not copy the entire dataset into a second baseline on every close merely to
rename it; concrete schema and query choices require implementation review.

Deleting history makes database pages reusable; it need not shrink the physical
file. Physical compaction is a separate cost and must not imply routine full-file
rewrites at quit. No storage-saving or speed figure is promised without measurement.

## Accepted normal and recovery verification policy

Validate complete affected invariants within each mutation transaction. Normal
open and close must not run a full historical replay or full-dataset audit;
publish the baseline on normal close using the checked current state. This also
applies after a large import closed without Commit. Missing disposable audit-cache
approval or an ordinary application rebuild must not by itself reinstate routine
full verification. In this policy, “verified” means validation
is maintained through checked operations from a trusted starting baseline; it does
not mean an independent full scan just ran. Existing mutation coverage must be
inventoried and gaps closed before relying on that claim. Dependency checks may
span accounts or sources and must not be narrowed to the selected row for speed.

After locking/authenticating and checking the supported format, inspect a durable
session marker. Before exposing an editable session, durably mark it open/unclean.
Normal close publishes the clean marker in the same transaction as the baseline.
This marker lives in the encrypted document and is separate from persistent empty
OS lock files; never delete or repurpose those locks. Failure to establish the
marker prevents writable activation. An open marker with no live lock means a
recovery path is required, not that corruption has been proven.

On interrupted shutdown, recover through SQLite first, then audit the baseline,
retained import dependencies and surviving session journal before activation.
Retain the unfinished session's recoverable Undo rather than pretending a clean
close occurred. A successful recovery establishes reusable validation for that
session so an unchanged close does not repeat the full audit. An error is surfaced;
there is no automatic repair, deletion or financial rewrite.

Retain encryption/authentication, locking, schema gates, transaction constraints,
durable writes and error propagation on all paths. Full backup/restore validation
and an explicit Check file action are required full-audit routes. An on-request
audit must report its actual result without silently repairing data or ending the
ordinary Undo session. Independent financial/recovery checks remain part of
automated tests; they must not become mandatory normal-open/close work.
Unknown/incompatible formats, invalid session markers and detected storage errors
need explicit refusal or recovery handling; they cannot silently inherit clean
status. Any future exceptional validation-version policy must be justified
separately, rather than reintroducing routine audits through cache invalidation.

Owner decision (2026-11-07): use the encrypted clean-session marker without
routine full-file hashing. The owner explicitly accepts reduced detection of
changes made while SpentIt is closed. Keep full verification after abnormal
shutdown, for backups/restores, and on explicit request. Authentication, database
and I/O errors must still surface whenever encountered; never silently repair
these errors. Missing disposable cache receipts and ordinary rebuilds do not
trigger audits. No exceptional validation-version policy is introduced here.

Not selected: independently validate every new baseline at close. This would still
scan the live dataset and preserve the routine cost the owner rejected. Do not
move that same mandatory full scan to another phase of normal open/close or make
every session wait for a background audit before it can close.

## Failures, sessions and backups

An interrupted baseline transaction must reopen as either the old baseline with
its journal or the complete new baseline, never a partially pruned document. SQL
commit success followed by lost acknowledgement is resolved from the persisted
generation/marker. Failure before commit retains history and saved financial
changes; do not report successful pruning. A forced exit may require recovery
verification next time. A close cancelled before publication keeps the session.

Accepted session boundary (2026-11-07): every successful document close, including
File → Close file, switching documents, and quitting. Failed or cancelled closes
and crashes retain recoverable session history. Closing must
not accept uncommitted transactions implicitly. An unchanged session needs only
the minimal session-finalization writes, not a rebuilt full snapshot.

Backups retain current state, originals and import-Undo capability. Existing backup
validation stays. Older backups may still contain older ordinary history; pruning
the live document is not secure erasure of backups or filesystem snapshots. Define
restored-session handling explicitly: a backup taken with an open session requires
validation and a consistent baseline/journal, not false clean status.

## Losses and tradeoffs

After successful close there is no ordinary cross-session Undo, old payee/memo
inspection, or complete reconstruction of the discarded edit sequence. This also
reduces troubleshooting information. Backups become the route to older ordinary
states. Current report results and financial values must remain unchanged.

Removing routine full audits reduces independent detection of semantic bugs after
apparently successful operations. It does not weaken SQL atomicity/durability,
and the owner accepts that detection tradeoff in favor of fast normal open/close.
Operation validation remains required, with full checks after abnormal shutdown,
for backups/restores and on request. Crash auditing alone will not catch bugs that
occur during otherwise clean sessions. The verifier itself can share bugs or
mistaken assumptions with the writer; agreement is not proof of user intent.

## Acceptance obligations and follow-up verification

- Reopen after clean close: current data, originals, reports, balances and saved
  decisions identical; ordinary history empty; eligible import Undo still works.
- Exercise earlier/later imports, shared ownership, duplicate-only imports,
  multi-account imports, generated counterpart restoration, Review Link reversal,
  and later conflicting edits across several baseline generations.
- Preserve removed-entry reimport behavior, manual/imported distinctions, exact
  signed splits, currency boundaries and durable retry/acknowledgement behavior.
- Inject I/O/disk-full failures and process exits at marker creation, baseline
  publication/pruning and both sides of commit. Retry and reopen must yield one
  complete valid state without silent loss or double application.
- Cover no-Commit clean close, unchanged reopen/close, dirty recovery, stale
  validation version, external file changes under the chosen policy, and restored
  backups. Demonstrate that recovery validation is not repeated unnecessarily.
- Assert that normal open/close, including after edits or large uncommitted imports,
  does not invoke full verification. Missing disposable cache approval and ordinary
  rebuilds must not trigger it. Demonstrate that abnormal shutdown, backup/restore
  and explicit Check file do invoke their required checks and surface failures.
- Measure audit time separately from hashing, review preparation, SQL close,
  baseline work and process teardown. Measure retained rows/file growth and
  operation latency over repeated sessions, not only a single small example.
- Verify memory/resource bounds and supported platforms before claiming them.

The implementation uses experimental formats **64 with rules / 63 without**;
no old-format migration is provided. Preserve the existing reject-unchanged policy. Update affected
contracts and contributor rules only under implementation Go. Manual chapters on
editing/history, file closing/recovery, acceptance and import Undo will need a
deferred backlog entry when the behavior actually changes; no manual prose or
screenshots have changed in this implementation task.

## Corrected implementation — 2026-11-07

Current draft/entry metadata carries its currency definition and a baseline flag
and ordering reference. Only retained payloads have these fields; there is no
`snapshot_facts` or allocation-origin tombstone table. Intrinsic `source-N`
allocation identities bind source ordinals, and retained metadata prevents rebinding.

Close computes the reference closure of current heads, original/acceptance rows,
duplicate targets/review origins, and required relationship/import restoration
snapshots. Ordinary linked edits retain only the live restoration input; superseded
editor restoration chains expire. Transfer versions retain current heads and source
match restoration references. Obsolete operation records, deletion subcommands, group memberships and
ordinary monetary summaries (including conversion marks for expired snapshots) are deleted, along with unreferenced payloads.
Autoincrement high-water marks preserve ordering across pruning and import rollback.

Full verification seeds current baseline heads and balances from retained payloads
and baseline relationships, reverses only subsequent opening changes to establish
the starting opening, and validates subsequent journal transitions. It still checks
source facts, exact splits, provenance, acceptance and current-head reconciliation.
Retained dependency snapshots are validated intrinsically, not against fingerprints
of removed revisions. Historical financial replay before the baseline is unnecessary.

Request receipts contain an ID, command domain, request digest and expiration bit.
They contain no transaction identity, revision, amount or before/after result.
Direct, editor, selection, transfer-assignment, bulk and rule commands reject changed
request reuse; expired requests return completion without re-execution. Reserved
internal child IDs cannot be reused as new commands. Source recognition, import
acceptance and configuration receipts remain only for their separate purposes.

Payload tables use their existing composite keys as WITHOUT ROWID storage, avoiding
a second B-tree for the same key. No VACUUM, padding or weakened encryption/durability
is used. Physical free pages and actual retained payload are measured separately.

## Operation validation inventory

- Draft/editor/selection changes canonicalize ownership/source allocation identities,
  validate exact totals/splits and currencies, check current revision/generation,
  and commit all members and affected links atomically.
- Committed changes validate tokens and dependency generations, exact monetary delta,
  current links and strict Undo preconditions before saving their session journal.
- Transfer commands validate both endpoint facts, currency/account ownership,
  relationship-set consistency and cross-account effects in one transaction.
- Import preparation validates parsed records and durable review state; acceptance
  consumes mutation-maintained review with stale-state checks. Source rollback
  checks ownership/protected relationships and retains its independent full audit.
- Rule application validates rule definitions, evaluated scope, category/provenance
  and conflict tokens; it cannot change financial totals. Account/currency operations
  retain their existing exact conversion and cross-relationship validation.
- Incomplete-capture cleanup proves there are no published financial rows before
  deletion, uses one transaction and preserves completed imports. It no longer runs
  a routine full audit after deleting only incomplete preparation.

SQL constraints, encrypted temporary/journal storage, durable synchronization,
operation cancellation gates and error propagation remain enforced. Independent
recovery/fault tests and explicit audits check the maintained invariants; the clean
marker does not substitute for operation validation.
