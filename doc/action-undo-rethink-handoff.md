# Undo rethink — owner handoff

Follow-up, 2026-11-08: the [replacement design](action-undo-design.md) and
[operation inventory](action-undo-inventory.md) now provide the design-only handoff.
The owner accepts keeping import-created accounts/categories. No implementation,
prototype cleanup or reviewer loop was started. The dated rejection account below
is preserved; its requested next-session discussion has now occurred.

Date: 2026-11-08. **Stopped by owner. The generic row-capture architecture is
rejected; do not resume its implementation, storage-budget proposal or reviewer
loop.** This handoff supersedes the execution instructions in the earlier
[rejected design](action-undo-rejected-design.md). That document and its approvals are historical
context, not authority to continue the rejected approach.

## Why work stopped

The prototype treated an import as writes to many reversible database tables and
recorded an inverse entry for each newly inserted storage row. A 50,000-transaction
import produced **350,008 inverse entries** and **22,200,323 bytes of inverse
key/image content**, before database record/index/page overhead. It did not edit
those transactions afterward. Most entries identified inserted rows; these were
not 350,008 user edits or separate payee/amount before-images. That distinction
does not justify the representation.

The owner rejected the underlying model, not merely its measured size. The
assistant compounded the mistake by proposing larger storage limits and then
investigating compaction before challenging why an import required this journal.
Passing tests and independent reviews established only properties of the chosen
prototype; they did not validate that choice against the owner's intended Undo
model. Do not use those approvals to overrule this correction.

## Latest owner direction

- Ordinary Undo is principally for **user-applied transaction changes**.
- Import Undo is **one atomic reversal of the import**. The owner's starting
  model is the pre-import high-water mark/boundary and removal of the import's
  contributions, not individual inverse entries for every imported storage row.
- Rethink the core in another session. Both implementer and reviewer were stopped.
  No reviewer was requested for this handoff; do not automatically restart the
  old review workflow from earlier instructions or skill invocation.
- No new numeric storage budget was accepted. The suggested 1.30× retained
  content / 2.00× final file / 2.50× peak limits are not an approved target.
- Compaction and reporting reusable space are relevant questions, but are not a
  justification for preserving the rejected import representation. No production
  Compact action was approved or implemented.

The earlier design also covered a strict file-wide chain, acceptance, older-import
removal, account/category/rule/report definitions and other commands. The latest
statement narrows the conceptual focus; it does **not** provide a fully specified
replacement scope for every one of those commands. Re-establish that scope with
the owner rather than silently retaining the earlier all-mutations model or
silently removing previously requested behavior.

## Starting point for the next discussion

Start with user operations and their actual reversal needs, before choosing a
database journal or changing schemas. Distinguish:

1. A user edit or bulk rule application: restore the transaction state actually
   changed, with one user operation defining the atomic Undo unit.
2. A new import: identify its boundary/ownership and remove its newly introduced
   data as one operation. Do not presume per-row inverse capture is necessary.
3. Any import effects on pre-existing data: establish exactly what needs to be
   restored, such as a prior transfer relationship or shared source association.
   Record only the necessary prior state; a global numeric cutoff alone must not
   delete unrelated or shared existing data.
4. Removal of an older import: decide its eligibility and whether reversing that
   removal is still required. This differs from undoing the most recent import.

These are discussion prompts, **not an approved replacement design**. Inspect the
existing [source-owned import rollback](import-undo.md) and
`src/storage/account_rollback.inc` before proposing a new mechanism. Existing code
already expresses ownership and transfer restoration, although its current
behavior is not the full previously requested session contract. The
[historical inventory](action-undo-rejected-inventory.md) may help locate code; its proposed table
capture policies are not accepted requirements.

Preserve exact money, splits, currencies, source/transfer relationships, atomic
commands, encryption and durability. Successful close expires ordinary session
Undo; failed/cancelled close and crashes must preserve the required recovery
state. Do not lower these requirements to make a replacement appear cheaper.
Final Change History layout and manual text/screenshots remain separate work.

## Useful measurements, with their limits

The owner reports approximately **19,000 transactions accumulated since 2017
across all accounts**. The 50,000-row case is an exceptional scale probe, not a
typical editing session. The owner may apply rules to most transactions following
initial import, potentially across sessions. A future representative workload
should distinguish initial import, bulk rule changes, small later edits/imports,
successful close, and optional compaction. The completed 50,000-row run was
**import-only**; the repeated-edit cases were not a 50,000-row bulk rule test.

The [broader report](../review/action-undo-prototype/broader-storage/README.md)
contains 36 completed comparison runs and bounded codec, VFS and close-lifecycle
checks. Its measurements describe the rejected prototype, not an unavoidable
cost of Undo or acceptance of the architecture.

For the UUID-length 50,000-row prototype:

- Final main file: 105,791,488 bytes, comprising 72,089,600 allocated live-page
  bytes and 33,701,888 bytes of entirely free pages.
- Stored B-tree payload: 59,831,026 bytes. Payload is not physical file length.
- Peak total VFS file lengths: 143,726,072 bytes, reached during close when a
  37,934,584-byte rollback journal accompanied the main file. This is total peak
  space, not additional space on top of that total.
- Successful close left no prototype action/inverse/pin rows or unreachable
  payload bodies. The file retained free pages for reuse.

The subsequent [compaction experiment](../review/action-undo-prototype/compaction/README.md)
reported, on disposable copies of the same closed synthetic files:

| Physical bytes | Current implementation | Rejected prototype |
| --- | ---: | ---: |
| Before compaction | 61,399,040 | 105,791,488 |
| After VACUUM | 54,345,728 | 66,138,112 |
| After ordinary backup page-copy instead | 61,399,040 | 105,791,488 |

Both compacted files still differ by about 21.7%; compacting only the prototype
is not a fair representation comparison. The helper reported exact typed table,
rowid and schema equality, integrity/FK/key checks and unchanged originals. It
exercised the backup API mechanism, not the full application backup/publication
workflow. Compaction peak space, fault recovery, live-Undo handling and production
UI were not tested. Final independent compaction review was stopped; a reviewer
had raised a source/log provenance question. Do not claim final review approval
or silently resolve that uncertainty in this handoff.

The application can query entirely free pages (`freelist_count × page_size`);
that is reusable space, not all potentially reclaimable space. VACUUM can also
repack partially filled pages. Active Undo data is still live and cannot simply
be discarded by a Compact action. None of this establishes a shipped feature.

## Workspace and verification state

- Original implementation Go was recorded in commit `b4b65df`; it is not renewed
  authority for the now-rejected architecture.
- Production `src/` code was not changed by this investigation. Unified Undo and
  Compact have not been delivered. Existing production behavior remains.
- Uncommitted experimental code is in `tests/storage/action_*`,
  `tests/storage/measure_vfs*` and the opt-in CMake targets guarded by
  `SPENTIT_ACTION_JOURNAL_PROBE` (default OFF).
- `review/action-undo-prototype/` holds scripts, source snapshots, raw text
  measurements and historical reviews. Earlier incomplete attempts are retained
  and labeled; use the assembled complete matrix for the 36-run figures.
- Documentation changes include this handoff, status/discovery pointers, the
  inventory and prior evidence. No manual chapter or screenshot was updated.
- Preserve the working tree as investigation history until the owner decides
  what to retain or discard. Do not merge the prototype into production, delete
  it wholesale, or rerun the large experiments merely to restart the discussion.
- This handoff received only documentation consistency/link checks. No reviewer,
  new implementation, new runtime verification or new approval is implied.

Next session: read this handoff and current status, discuss the operation-level
model with the owner, then establish a replacement design and its authorization.
Do not start by asking the owner to accept the previous storage multipliers.
