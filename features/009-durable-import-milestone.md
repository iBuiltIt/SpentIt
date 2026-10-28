# 009 — First durable import workflow

Status: **active**  
Last updated: 2026-10-28  
Implementation: partial, headless foundation plus optional durable UI experiment. The owner accepted the five policy groups and explicitly authorized implementation on 2026-10-25. The bounded parser, encrypted draft/review/commit commands and ledger reconciliation now exist; a bounded [setup/review UI](../doc/import-ui.md) connects them and schemas 2 and 3 support date/split-total repair. The review UI now includes inline editing, parsed source-field copying, simultaneous read-only split previews and combined-search return navigation; see the [bounded scope and verification](../doc/import-ui.md#inline-editing-and-source-copying). Remaining scope is incomplete. See [implementation and verification](../doc/durable-import.md).

Subsequent bounded extensions now include committed changes, categories/rules, combined search, multiselection and atomic committed bulk/group Undo. See [current status](../doc/status.md) and [search/ledger scope](../doc/search-ledger.md). Historical deferrals in the original milestone below describe its first slice, not current absence. OFX, QFX, QBO, and ASO bank and credit-card statements are implemented. New documents use schema 22 with rules and schema 21 without. Schema 20 is rejected without migration. Remaining QIF dialects and the full product remain incomplete. On 2026-10-28 the owner said the finished eight-point sequence was the work before reports. Feature 006 holds the open questions. Committed transfer edits and selected-account rule reapplication are implemented. Allocation-level Merge and unlink are in [feature 008](008-transfer-matching-and-lifecycle.md#accepted-transfer-management-decisions--2026-10-27). Experimental formats need no migration or upgrade work until the owner announces a release. The first import commit still freezes a newly confirmed opening. A later explicit opening correction is the separate account command in [feature 002](002-accounts-money-splits.md#applied-account-management-checkpoint--2026-10-27).

## Objective and existing evidence

Accepted milestone outcome: import synthetic QIF into one account, review duplicates and edits, restart with accepted work intact, explicitly commit, and reconcile the exact committed total against its contributing records. This is a bounded precursor to the full import → rules → transfers → reports workflow, not completion of features 001–008 or readiness for personal financial data.

Existing engine values support exact money, retained currency definitions, original/effective totals, and validated splits. The [storage experiment](../doc/storage-prototype.md) supports immutable staged snapshots and idempotent batch publication. Inspection of `src/storage/prototype_store.cpp` confirms its triggers prohibit draft editing and deletion; its history records operation membership, not full edit history. The [sheet experiment](../doc/sheet-prototype.md) saves immutable in-memory snapshots and has no persistence contract. Their tests cover these bounded behaviors, not the workflow proposed here. No existing runtime checks were rerun to prepare this design.

## Accepted requirements carried forward

These requirements already belong to [documents](001-documents-storage-security.md), [accounts and money](002-accounts-money-splits.md), [imports](003-qif-import-review.md), [sheet behavior](005-transaction-sheet-navigation.md), and [transfers](008-transfer-matching-and-lifecycle.md):

- Immutable original evidence remains separate from editable interpretations, including original versus corrected amounts.
- Accepted draft work survives restart and does not enter committed balances or ordinary reports.
- Exact money, currency identity, split sums, and overflow validation apply outside the UI as well as within it.
- Uncertain duplicate matches cannot silently remove transactions; legitimate repeated occurrences must survive.
- Commit is explicit per account, durable and atomic with history and draft transitions; retries cannot publish twice.
- Save / Discard / Stay resolves an active editor; discarding draft work cannot change committed financial state.
- Opening balances start at zero dated today, can be edited, and are excluded from spending/income reports.
- Transfer behavior remains governed by feature 008. A smaller milestone cannot reinterpret transfer principal as ordinary spending or implicitly commit another account.

## Accepted experimental scope — 2026-10-25

Use a separately identified experimental document format and executable with synthetic inputs. Continue evaluating Qt Quick and SQLCipher without declaring final product adoption. Existing prototype documents remain untouched; unsupported formats fail explicitly rather than migrate automatically.

Support one account per experimental document, with explicit home/account currency selected from the current catalog and retained definition identity stored with amounts. Currency is supplied by account setup, never inferred from a symbol in the file. A zero Opening Balance entry is created as setup state; its editable date/amount must be confirmed before the first import commit, which freezes it for this milestone. Later opening-balance changes, account currency reassignment, archive/delete, multiple accounts and consolidated totals are deferred.

Provide create/unlock/reopen, explicit close, durable draft edits, source inspection, duplicate review, commit receipt, and backup/restore into a new document. Start with macOS verification; Linux and Windows remain unverified until actually tested. Retain a single writer per dataset and one active dataset per running application, with tested document locking before a UI can write. Settings specifies the dataset path to load; the user can close that dataset and open another. There is no application-wide instance limit. Document switching requires resolving the editor and closing the current document. The owner subsequently accepted [optional-password setup and unlock/recovery behavior](001-documents-storage-security.md#accepted-setup-unlock-and-recovery-behavior--2026-10-25), now implemented in the bounded UI gateway. No changes to existing dataset passwords/protection modes or schema migrations are included.

Rules, bulk categorization, category hierarchy/editor, general search grammar, saved reports, report-creation dialogs, transfer matching, committed editing/deletion/reversal, and multi-account files are deferred. Manual category text and uncategorized allocations remain available. Committed rows are read-only in this milestone. The reconciliation view is an account ledger check, not a category-spending report.

### QIF profile and incomplete input

Use one explicitly selected profile: a single-account ordinary banking QIF subset, UTF-8 with optional BOM, LF or CRLF, four-digit years, and user-selected DMY or MDY dates. The existing Wise-shaped example, including split memos without categories, must be supported. A confirmed format applies to the whole import and is stored with parser version and diagnostics; do not infer an ambiguous date from locale. Unsupported encodings, record types, date forms or numeric syntax produce actionable diagnostics. No replacement-character decoding or guessed precision.

The [implemented profile contract](009-qif-profile.md) specifies supported headers/fields, repeated-field behavior and split grouping against synthetic fixtures and a primary QIF reference. Other dialects remain unsupported. Unknown fields retain their bytes and receive diagnostics; they cannot be treated as harmless if they could change financial meaning. Investment and multi-account sections are unsupported.

Use limits of 64 MiB per file, 200,000 records per import, 1 MiB per record, 64 KiB per field and 1,000 allocations per record, checked before unbounded allocation. These are accepted experimental import-resource limits, not account-history limits. Confirm them against representative fixtures and measured memory use; exceeding any limit is an explicit failed import, never truncation. The prototype's evidence-size limits do not support these proposals unchanged.

Preserve bounded source bytes atomically in encrypted storage before presenting an import as saved. Parse failure leaves a visible blocked import with evidence and diagnostics, not a partially successful ledger. If evidence persistence fails, report failure and leave no acknowledged import. Over-limit files are rejected before ingestion; the external original is untouched. Cancellation before persistence leaves no import; cancellation afterwards retains a blocked/cancelled evidence record that can be reviewed or deliberately discarded.

Allow no partial commit of a malformed import. Missing amounts/dates, invalid precision, repeated conflicting fields, split-total discrepancies and unsupported financial constructs block the batch until explicitly resolved or the import is discarded. Interpretation corrections preserve the diagnostic, source value and user decision. No automatic balancing allocation repairs imported discrepancies.

### Duplicate review

Separate command retry from a new import of the same bytes. Retrying the same durable import identity returns the recorded outcome; a new import attempt has its own evidence identity even when its content hash matches.

Allow automatic recognized-duplicate exclusion only for a byte-identical file previously committed to the same account under the same profile, with one-to-one source-record occurrence mapping. Compare bytes as well as hashes. Show every exclusion, its prior occurrence and the override to include a legitimate new transaction. An identical file still pending returns navigation to that pending import rather than creating a second workspace.

All overlapping or reformatted exports produce possible-match candidates, not automatic exclusions. Candidate generation compares immutable interpreted source date, exact original amount/currency, payee and reference; it may offer broader candidates with an explanation. It never compares only edited committed values. Persist the candidate basis, user disposition and referenced occurrence. Do not claim bank references are unique unless a future profile establishes that contract.

Multiplicity is explicit: two identical source records remain two occurrences. One existing occurrence cannot silently consume two incoming records. When identities are uncertain, require Include as new or Exclude as duplicate against a chosen occurrence. Unresolved candidates block commit. An exclusion preserves its evidence and decision but contributes zero to the projection.

### Draft and commit lifecycle

Use one pending batch per account; a different import waits until it is committed or explicitly discarded. No arbitrary subset commit: publish all included, valid, reviewed entries together. Excluded records remain in the import's audit record. A batch with no included entries can be finalized as reviewed without creating monetary postings.

Save/OK durably stores a validated draft revision and its review dispositions; it does not commit. Cancel or navigation Discard abandons only the current unsaved editor buffer. Clearing the batch is a separate confirmed operation: mark it discarded and remove its financial projection while retaining encrypted source evidence and audit disposition. Every keystroke is not promised durable; after an interrupted edit, reopen the last acknowledged saved revision. The owner accepted these retention and save-boundary choices for this milestone.

Commit receives an operation identity, expected batch revision and complete selected content. In one storage transaction, revalidate account/currency, splits, diagnostics and duplicate decisions; record the operation and immutable publication snapshot/history; publish included entries; finalize excluded dispositions and batch state. A stale revision fails with refresh/review, not overwrite. An identical operation retry returns its recorded result; reuse with different content fails. Membership alone is insufficient once drafts are editable.

After an uncertain commit outcome, reopen/query the durable operation identity before retrying. UI success follows durable completion only. Cancellation after commit starts resolves its durable outcome; it does not imply rollback. Failure before commit leaves the previous saved draft available. Backup/restore must preserve pending batches, evidence, revisions, decisions and operation identities as well as postings.

### Transfer boundary

Historical first-slice boundary (superseded for bracketed destinations by [explicit QIF transfer resolution](../doc/qif-transfers.md)): account-reference categories and other supported-profile transfer indications block the whole batch with an explicit unsupported-transfer diagnostic. Transfer selectors are unavailable. Do not strip brackets, convert these to category text, fabricate counterparts, or drop their amounts to make commit succeed. Unknown financial fields also block rather than bypass this boundary. Unmarked bank movements cannot be reliably identified from payee/amount alone; this experiment therefore makes no claim of transfer recognition or spending-report correctness. Supporting transfers later requires feature 008's protected-draft lifecycle and unresolved policies to be designed first.

### Balances and reconciliation

Use stable chronology by financial date, then persisted account-entry sequence; the Opening Balance precedes other entries on the same date. Earlier-dated entries remain earlier, rather than making the opening entry an implicit date cutoff. Sequence survives edits, restart, import retry and sorting. This policy is accepted for this experiment; it does not adopt synthetic fixture IDs.

Committed balance equals opening balance plus committed effective totals. Projection adds included valid drafts exactly once. A blocked import is visibly incomplete and has no authoritative projected ending balance; an optional valid-row subtotal must be labeled incomplete. The accepted ledger balance mode is explicit Committed or Projected, with excluded rows marked as having no contribution. Filtering/sorting does not recalculate chronological balances. Withdrawal/Deposit totals cover exactly the displayed contributing records in the selected mode and reconcile separately from the whole-account balance.

The commit receipt shows operation identity, included/excluded counts, prior committed balance, signed committed change and resulting balance, with the same snapshot's contributing rows and exact allocations available for inspection. No category report, refund policy or conversion policy is implied.

## Proposed implementation sequence and boundaries

1. Resolve the decisions below; specify the QIF grammar and command/state contracts with synthetic fixtures. Record accepted choices in the owning feature documents.
2. Implement engine parsing/diagnostics and occurrence-aware duplicate candidates independently of Qt and SQL. Add account/batch/revision domain validation as needed.
3. Add narrow application commands for ingest, save draft, decide duplicate, discard and commit. Storage implements those contracts using a new explicit schema; do not weaken existing prototype immutability triggers in place. Store evidence blobs separately from mutable draft revisions and immutable commit history.
4. Verify headless lifecycle, recovery and backup/restore before wiring the UI. Keep existing engine/prototype tests intact.
5. Adapt the sheet to application commands and background work. QML performs no money calculations or SQL; executable wiring supplies concrete storage. Tag async results with document/session identity so stale results cannot populate a reopened or different document.
6. Measure the durable workflow at 100,000 account entries and 200,000 stress entries, including storage, queries and visible refresh. Record actual results and any revised scope; do not inherit in-memory prototype performance claims.

## Milestone acceptance matrix — full workflow remains incomplete

| Case | Required observable result |
|---|---|
| Opening CAD 100.00; import -29.31 split -0.14 fee/-29.17 transport and +50.00 | Before commit: committed 100.00, projection 120.69. After commit/restart: committed 120.69; receipt change +20.69; allocations reconcile. |
| Wise date `10/08/2026` | No guessed date; DMY means 10 August, MDY means 8 October; selected profile survives restart. |
| Save corrected -30.00 with -0.14/-29.86 splits | Original -29.31 and source bytes remain; projection becomes 120.00 in the first fixture; failed correction changes nothing. |
| Net +2,200.00 with +3,000.00/-700.00/-100.00 allocations | Exact valid mixed-sign structure survives save, reopen, commit and restore. |
| Two identical -10.00 occurrences; reimport the identical committed file | Original commit contributes -20.00; two separate explained exclusions on reimport; inclusion override remains explicit. |
| Overlap containing two identical purchases against one prior occurrence | Neither silently disappears; user resolves mapping and preserves the extra legitimate occurrence. |
| Committed payee/amount differs from source in a future compatible fixture | Duplicate evidence uses original fields; edited interpretation is not a substitute identity. No committed-edit UI is required here. |
| Malformed date, invalid UTF-8, unsupported header, excess precision, overflow, mismatched splits, resource limit | Explicit diagnostic; no partial monetary publication or silent repair; bounded evidence policy honored. |
| Transfer category in a parent or split | Batch blocked; no ordinary spending reclassification, counterpart or implicit other-account commit. |
| Save, close/reopen, Cancel edit, clear batch | Last acknowledged save survives; Cancel removes only buffer changes; clear removes projection but preserves committed state and retained evidence. |
| Stale draft revision or operation identity reused with changed content | Explicit conflict; no lost updates or duplicate posting. |
| Disk-full/write/sync failure and interruption around ingest, edit, discard and commit | Reopen yields previous or fully completed operation, never a partial acknowledged state; retry resolves using durable identity. Test immediately before and after durable commit. |
| Wrong password, second writer, cancelled open, stale worker completion | No document replacement, concurrent write session, or cross-document data exposure. |
| Backup pending and committed states; restore into a new document | Exact evidence, balances, decisions, history and retry identities survive; previous backup remains usable after failed publication. |
| Sort/filter and reconciliation drill-down after restart | Chronological balances stay stable; displayed totals equal contributing exact amounts in the same snapshot/mode. |

Also scan relevant encrypted files, sidecars, temporary storage and logs for synthetic private markers; fuzz the bounded parser and run appropriate sanitizers. Preserve uncertainty about physical power loss and unavailable platforms. A test passes only when executed and recorded; this matrix is not verification evidence. The [headless verification record](../doc/durable-import.md) identifies the implemented subset and remaining gaps.

## Owner decisions — accepted 2026-10-25

The owner explicitly accepted all five groups:

1. One-account experimental scope; rules, transfers and category reports deferred; transfer-bearing imports blocked.
2. UTF-8, four-digit-year, explicit DMY/MDY banking profile and the resource limits above.
3. Reviewable byte-identical committed-file exclusion, manual resolution of other candidates, one pending batch and whole-batch commit.
4. Durable Save/OK, encrypted evidence retention after discard, and opening balance frozen after first commit.
5. The chronology/balance modes above and continued Qt Quick/SQLCipher evaluation for this experiment.

These are bounded milestone decisions, not final architecture choices or completion of the larger features. The bounded parser grammar and headless command/schema implementation are now recorded in the linked profile and implementation report; setup/unlock credential details and bounded writable UI integration are now recorded in the [UI report](../doc/import-ui.md); full milestone scope remains incomplete. The owner clarified that the limit is one active dataset per running application, with a Settings path selecting what to load; the earlier application-wide instance-lock question is superseded. Exclusive document locking remains required. The owner explicitly selected QIF first. Future support for QFX, ASO, QBO and OFX is required, but deferred beyond this milestone; see [import format scope](003-qif-import-review.md#accepted-future-import-formats). No private sample was inspected to record these decisions.

## Private sample availability

The owner has actual private QIF samples and a separate real-world bank dataset exported in each of QFX, ASO, QBO and OFX. The latter four exports represent the same account activity and may be useful for cross-format comparison; no equivalence with the QIF samples is asserted. The bank does not offer QIF. The owner may be asked to provide access again when format investigation or testing requires it. Format contents and dialects have not been inspected or verified. Correction 2026-10-27: an in-place structural inspection identified the owner's private ASO export as OFX 1.0.2 SGML credit-card statement syntax. No path, filename, or contents were recorded. QFX, QBO, and OFX behavior in this checkpoint comes from the public OFX 2.3 specification, not from those private files. Do not record their filenames, paths, account identifiers, contents or other identifying details in repository documents, fixtures, logs or commits. Do not move or modify the originals. Current permission permits in-place inspection; it does not permit copying the files into the project or treating them as routine automated test inputs. Prefer wholly synthetic fixtures that reproduce relevant structural cases. Future agents should request access when needed rather than search for private files.

## Original design verification

The original design was a documentation-only change. Check relative link targets, whitespace and consistency with the owning feature documents. Record actual results in project status after running the checks. Subsequent implementation is tracked in the linked verification record; the bounded writable UI and date/split-total repair are implemented, while the broader milestone remains incomplete.

## Accepted bounded record repair — 2026-10-25

The owner selected explicit repair of malformed dates and split-total mismatches first. Missing/invalid amounts, missing required fields, unsupported financial constructs, encoding errors and other structural errors remain blocked for discard/reimport. No automatic date guess or balancing split is permitted.

The implemented schema-2 experiment offers editable interpretations only when **all** errors in that record belong to the selected repair scope, even when the first displayed diagnostic masks later errors. Save validates the supplied date and exact allocation sum, preserving the raw bytes, original parsed amount, first diagnostic and append-only correction history. An invalid original date stays unavailable in original-date evidence; the corrected date belongs to the interpretation. The first saved repair requires a separate Include as new or explicit duplicate choice. Same-original-amount candidates with unavailable original dates are labeled as uncertain; no inferred date or automatic exclusion is introduced. File-level errors and other unresolved records still block the entire batch.

Date/split-total repair was introduced in experimental schema 2 and remains supported in schemas 3–5. Schema-1 datasets remain readable and support their existing commands, but repairs require a new dataset; no migration is attempted. Backup preserves the source schema. Older executables reject schema 2. See the [repair contract and verification](../doc/durable-import.md#bounded-record-repair) and [UI workflow](../doc/import-ui.md#record-repair).

## Durable workload measurement follow-up — 2026-10-25

The [native UI performance report](../doc/import-ui-performance.md) records the representative and stress workloads, measured bottlenecks, UI caching/presentation changes, verification and remaining limits. Reopen/save latency remains dominated by verification and authoritative ledger reloads. Feature 009 remains active; a bounded benchmark does not complete its remaining UI, recovery or platform obligations.


## Read-only split review follow-up — 2026-10-25

The durable ledger now allows multiple saved split previews alongside one active editor. Occurrence-based expansion state survives sorting/filtering/row recycling without modifying the editor or financial data, and clears on document changes. See [behavior and verification](../doc/import-ui.md#read-only-split-viewers--2026-10-25). Broader navigation, cancellation and recovery UX remain incomplete; this feature remains active.


## Filter return follow-up — 2026-10-25

The bounded review UI now restores selection and view after clearing its literal filter, or retains an explicitly inspected result. Clear/Escape respect unsaved-edit navigation and dataset isolation. See [behavior and verification](../doc/import-ui.md#filter-return-navigation--2026-10-25). Broader navigation, cancellation and recovery UX remain incomplete.


## Coordinated cancellation and navigation follow-up — 2026-10-25

The [coordinated roadmap](../doc/roadmap.md) authorizes continuing routine implementation with delegated agents. The durable UI now has bounded account/Settings navigation, a resizable/hideable sidebar, guarded Back/Forward, operation phases and commit-outcome recovery. Experimental schema 3 adds retained evidence with explicit Resume/Discard when controlled import processing does not finish; captured imports contribute no financial rows or authoritative projection. Schemas 1 and 2 remain readable without automatic migration. See the [storage contract](../doc/durable-import.md#retained-evidence-and-cooperative-cancellation--2026-10-25) and [UI follow-up](../doc/import-ui.md#coordinated-navigation-and-operation-recovery--2026-10-25). The implementation and verification records distinguish this bounded scope from remaining product work. This does not implement multi-account navigation, committed editing, rules, transfers, general search or reports.


## Subsequent bounded extensions — 2026-10-25

The original milestone deferrals above describe its initial scope. Subsequent schema-4 ordinary committed correction/removal/Undo is recorded in [feature 011](011-committed-editing-and-reversal.md), and schema-5 category management/assignments in [feature 010](010-categories-and-selection.md). Rules-disabled new datasets use schema 5. Rules-enabled new datasets use schema 7 and keep that catalog plus bulk group receipts. Schema-6 Save/Apply and the rules sheet are recorded in [feature 004](004-rules-and-explanations.md). Schemas 1–4 remain readable without migration. Combined search, multiselection and atomic committed bulk changes are implemented in the bounded follow-up. Transfers, reports and multi-account documents are still deferred.


## Sequential QIF completion follow-up — 2026-10-26

The owner authorized point 1 of the [sequential eight-point roadmap](../doc/roadmap.md#current-authorization-and-delivery-checkpoints--2026-10-26). Independent [backup cancellation/progress](../doc/import-ui.md#backup-cancellation-and-progress--2026-10-26) is implemented with cancellation before exclusive publication, retained encrypted candidates, source preservation and retry-to-new-destination guidance; [verification](../doc/status.md#qif-point-1-independent-backup-progress--2026-10-26) records the actual checks. This does not complete feature 009 or point 1.

The owner subsequently settled the ordinary-money dialect, multi-account mapping/class treatment and repair policies recorded below. The [point-1 handoff](../doc/roadmap.md#point-1-completion-handoff--2026-10-26) records the accepted policies, completed implementation and later dependencies; no broader parser acceptance or new financial behavior is implied by the backup change.


## Accepted ordinary QIF and amount repair follow-up — 2026-10-26

The owner accepted ordinary UTF-8 Bank/Cash/CCard records with explicit DMY/MDY and four-digit years, and class labels retained separately as source metadata without financial classification effects. The owner accepted explicit repairs when original amounts are unavailable: original evidence must remain unknown, effective amounts must be entered exactly, and duplicate disposition must be reviewed. No zero original, automatic exclusion or inferred currency is permitted. The [v3 parser contract](009-qif-profile.md) states the implemented grammar and precise repair envelope.

The owner also requires the total number of repairs upfront, progress such as 5/90, and Abort for the entire import including earlier saved repairs. Source evidence/history retention still applies. For multi-account imports, all repairs must finish before any commit or protection; Abort removes unused accounts explicitly created for this import, while preserving unrelated accounts and retained evidence/history. This was unfinished at the nullable-amount checkpoint; the whole-source implementation below now supplies these controls and also gates external protection of ordinary single-account imports while repairs or duplicate decisions remain. Source evidence and history remain retained after Abort.

Source account definitions should offer explicit account creation with preserved imported names and a visible currency choice; do not infer currency from name suffixes. Account type is informational. Treat exported account `B` as reported balance evidence, not an automatically applied opening balance. For newly created accounts only, after all repairs and duplicate decisions are settled, compare the exact included reviewed change from zero with `B`. Equality leaves opening zero. On mismatch, ask whether `B` is opening or closing: opening uses `B`; closing derives opening as `B` minus reviewed change with checked exact arithmetic. Preserve the reported value and explicit decision. Confirm the opening date explicitly using the opening-balance workflow. Never overwrite the opening balance of an existing mapped account. Synthetic fixtures must not reproduce private names, account suffixes or balances from discussion. See the [current checkpoint](../doc/status.md#ordinary-qif-and-unknown-amount-repair-checkpoint--2026-10-26).

## Whole-source account review implementation — 2026-10-26

The accepted group policy is implemented and verified in the bounded scope: [account-section contract, explicit mapping, repair progress, staged reported-balance choices, whole-import Abort and recovery](../doc/qif-account-imports.md). Experimental schemas 16/17 retain one unchanged complete source and stable per-account associations. Existing account openings stay unchanged; new-account opening proposals publish only with the first account commit. Class and unavailable-money evidence remain separate from interpretations. Unsupported structural sections fail visibly before capture; accepted structures retain transaction blockers for review. Point 1 is complete within this accepted envelope; selected current-format verification and limitations are recorded in [status](../doc/status.md#whole-source-qif-account-review--2026-10-26).


## Current-format hardening follow-up — 2026-10-26

Point 2 verification is recorded in the [hardening guide](../doc/hardening.md): actual coverage-guided parser campaigns, combined current-format crash/I/O/retry/restore workflows, accessible ledger semantics and keyboard/focus regressions, and representative native workload measurements. This keeps financial behavior, bank-v3 acceptance and experimental schemas 16/17 unchanged. Linux/Windows verification was explicitly excluded from this checkpoint. Feature lifecycle remains active because later format/transfer/product requirements still lie outside this QIF scope.

## Statement import follow-up — 2026-10-27

Point 3 specifies OFX 1.x SGML and OFX 2.x XML bank and credit-card statement lists. QFX, QBO, and ASO are file labels of that grammar. The [statement profile](003-ofx-profile.md) records admitted messages, refusals, currency, `FITID`, and the shared source representation. QIF imports write the same occurrence and association tables, so a later OFX-family import can associate with a QIF-originated transaction. ASO was identified by in-place structural inspection as OFX 1.0.2 SGML credit-card statement syntax and is admitted through `ofx-sgml-1` with format `aso`. No path or contents were recorded. Selected macOS verification for that checkpoint is in [status](../doc/status.md#point-3-statement-imports--2026-10-27). Current documents use schema 22 with rules and schema 21 without, and reject schema 20 and schemas 18 and 19. Feature 003 and this milestone stay active. Point 4 account commands are in [feature 002](002-accounts-money-splits.md#applied-account-management-checkpoint--2026-10-27). Point 5 transfer management is in [feature 008](008-transfer-matching-and-lifecycle.md#accepted-transfer-management-decisions--2026-10-27). Point 6 committed edits and point 7 selected-account rule reapplication are in [status](../doc/status.md#point-7-committed-rule-reapplication--2026-10-27). Point 8 sheet interactions are in [status](../doc/status.md#point-8-sheet-and-sidebar-interactions--2026-10-27).
