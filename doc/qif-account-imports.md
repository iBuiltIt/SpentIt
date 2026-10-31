# QIF account sections and whole-source review

The [opening/ledger cleanup and interrupted-import rollback](ui-import-cleanup.md)
is implemented under the owner's 2026-10-31 Go on experimental formats **40/39**.
It adds import routing, Review repair/Commit, account-context Discard and automatic
cleanup of incomplete preparation. Completed uncommitted imports remain durable.
See [current verification and limits](status.md); older Resume and toolbar
descriptions below are historical.

Last updated: 2026-10-31

Implemented and verified within the accepted roadmap point-1 scope; exact selected checks and limitations are recorded in status. This extends the [ordinary QIF profile](../features/009-qif-profile.md), [account workspaces](accounts.md), and [transfer lifecycle](qif-transfers.md). It does not implement general account deletion, account currency reassignment, transfer repair, or new import formats.

## Current mapping opening setup — 2026-10-30

The mapping screen now completes new-account opening setup. Currency and editable
opening date appear only for creation. The date defaults to the earliest valid
transaction date in that source account, falling back to 1900-01-01. The opening
balance selector defaults to zero and optionally offers the reported balance as a
literal opening amount. It never treats that amount as closing or subtracts imports.
Missing/zero reports supply no second option. Existing mapped accounts are unchanged.

The explicit zero/reported choice is saved in encrypted immutable balance history
within account capture. It confirms the account opening immediately and does not
become stale when transactions are repaired or linked. No later opening setup panel
is shown for those accounts. Schema 38/37 adds these fixed choices to the existing
balance decision representation; older experimental datasets are rejected unchanged.
The legacy staged-review descriptions below apply only to internal calls that omit
a creation-time choice. Capture, resume, Commit and Abort were verified with injected
I/O failures, durable-boundary interruptions, retries and backup restores; see
[the verification record](status.md#opening-setup-during-import-mapping--2026-10-30).
[Requirements and rationale](../features/003-qif-import-review.md#opening-setup-during-account-mapping--owner-go-2026-10-30).

## Accepted source envelope

The bounded UTF-8 profile accepts ISO dates and retains explicit DMY/MDY selection for slash dates, with four-digit years. Ordinary transaction sections use `!Type:Bank`, `!Type:Cash`, or `!Type:CCard`. An optional BOM and LF/CRLF remain supported. Headerless ordinary single-account sources retain their established workflow.

Account exports may contain `!Option:AutoSwitch`, `!Clear:AutoSwitch`, and `!Account` definitions terminated with `^`. Initial definitions require `N` name and a supported ordinary `T` type. Consecutive definitions are indexed separately; a transaction section belongs to the most recently selected account. A later `!Account` record may select an already defined name with `N` alone. Repeated sections for that name retain file order and one source-account identity. Conflicting repeated types or reported balances block the document. Account type is informational; the transaction header determines the supported record grammar.

`B` is retained as reported balance text. Account `D` description, `L` credit limit, `/` statement date, and `$` statement balance are retained as informational metadata and original bytes. They do not create postings. Their presence does not authorize interpreting statement metadata as an opening balance. Names are preserved exactly; suffixes never infer currency. Empty transaction sections and metadata-only account lists are supported.

The index keeps ranges into the complete unchanged file. No synthesized subsection file replaces the evidence. Limits include 64 MiB per source, 1000 named accounts, 10000 sections, 200000 transaction records, and the existing per-field, per-record and per-allocation limits. Cancellation checkpoints run while scanning and parsing. Unknown headers, unsupported types, malformed account lists and account-boundary errors are refused before capture, with an explicit message that no import was saved and the original file is unchanged. Transaction-level unsupported fields remain visible blockers in retained drafts. Resource rejection never masquerades as a valid empty source.

Investment/security/price sections, class catalog imports, two-digit years, locale amount guessing, and unrecognized export structures remain unsupported. An opening-balance pseudo-record using a bracketed self-account reference is not automatically converted into account opening money or stripped from evidence; unresolved self-transfer intent remains blocked. This is a specified ordinary-money profile, not universal QIF compatibility.

## Mapping and evidence

The mapping dialog requires an explicit choice for every named source account, including empty accounts. Choose an existing account or explicitly create one using its exact imported name, a visible currency code, and an opening date. New accounts default to the initial setup account's currency. Every source account maps to a distinct target account; combining multiple differently named source accounts into one pending account is refused visibly. Repeated sections of the same source account already share its mapping.

Capture atomically stores one whole-file source, the complete mapping, explicit account creations and every captured account draft. Each batch references that source and its stable source-account index. Resume atomically materializes all account drafts. Cancellation or failure before that transaction completes leaves the captured group available for Resume or Abort. Reopening and restored backups retain the same group and evidence.

An unchanged operation identity is retryable; changed source, profile, mapping or creation inputs conflict. A new request with the same bytes, profile and mapped existing accounts navigates to its already pending group. Deliberately creating different accounts is a different mapping. Byte-identical committed-file recognition is scoped to both target account and source-account index and preserves one-to-one occurrence multiplicity. All other candidates remain reviewable; no uncertain record is silently dropped.

## Repairs and the publication gate

Review shows the total repairable-record count before editing, completed saved repairs, the number requiring amount repair, unsupported records/sections, and outstanding review items. Amount repair includes unavailable totals, unavailable split amounts, and exact split-total discrepancies. Date-only repairs count separately through the total. Unsupported structures are not labeled repairable. Saving a repair preserves unknown original money and source identities; duplicate disposition still requires explicit review.

No grouped account can commit or acquire transfer protection until every source account's repairs, blockers, duplicate decisions and required new-account balance decisions are resolved. External transfer publication checks the same gate. Ordinary single-account imports also block protection while their source still requires repairs or duplicate review, so partially repaired imports remain fully abortable.

Account commit remains explicit and separate. The first related financial publication atomically records that the group has started and links it to the durable financial operation. The confirmation explains that whole-import Abort ends at this boundary; other accounts remain drafts. Afterwards their ordinary protected-discard rules apply. Discarded members no longer block remaining accounts, while retained protected portions must still satisfy their financial checks.

## Reported balances and staged openings

Only accounts explicitly created by this import participate in reported-balance opening review. An existing mapped account's opening is never overwritten.

After repair and duplicate review, calculate the exact reviewed change from zero. Include effective source records once, exclude reviewed duplicates, and include generated/local transfer effects once. A matched bank counterpart replaces its generated contribution rather than adding another movement. Use cancellation-safe exact summation and checked subtraction.

When reported balance equals reviewed change, zero opening is the supported proposal. Otherwise explicitly choose whether the reported value is opening or closing: opening uses that value; closing derives opening as reported balance minus reviewed change. An explicit Ignore choice keeps the new account's zero opening and can bypass invalid reported balance text without changing the evidence. Invalid precision or overflow cannot silently round. The date is explicitly confirmed.

The UI calculates and stages the proposal, displays its exact amount, and leaves committed balances unchanged. The proposal publishes atomically when that account first commits, including transfer-first publication. The reported text, choice, date, source batch revision, exact reviewed change, and financial sequence are retained in immutable decision history. Replay reconstructs included source values and transfer snapshots. A change in the exact reviewed monetary basis requires another review; harmless sequence or protection changes do not repeatedly invalidate the same proposal.

## Abort, recovery and retained identities

Before first publication, Abort discards all group drafts and earlier saved repairs in one transaction. Encrypted source bytes and append-only histories remain. Accounts explicitly created by that import disappear from the active account list when unused; retained internal identities keep evidence references valid. Unrelated accounts or independent work survive. An aborted creation cannot be resurrected through an old creation retry, and new account identity allocation includes retired identities. If Abort removes the selected account, selection returns to the initial account.

Group outcome queries reject a transaction that has not returned to autocommit. The worker distinguishes known saved state, known unperformed work and unknown outcomes. On an unreadable outcome it closes the session and asks for reopening and inspection. Capture requests retain their mapping identities across acknowledgement failure; Resume and Abort are retryable, and repeating an unchanged balance decision does not append another decision. A saved operation whose UI refresh fails is reported as saved, with explicit reopen guidance.

## Verification

Use the current [status record](status.md) for exact results. Synthetic `accounts.groups` covers capture/reopen/resume/cancellation, repairs, reported-balance choices, generated transfers and both commit orders, empty accounts, duplicate multiplicity/mapping, Abort, retired identities and restored backups. `accounts.group_io` and `accounts.group_crashes` exercise capture, materialization, balance staging, Abort after repair, staged-opening account/transfer publication and unknown-original repair. Explicit before/after durable-boundary assertions inspect the reopened state before retrying. Authenticated synthetic corruption tests also require reopen to reject mismatched mappings, balance replay/current-proposal divergence, invalid group history/publication references and invalid retired-account state, including corruption hidden from the active account list.

The model/QML regression uses mapping selections, Prepare, balance staging and the Abort dialog, plus acknowledgement and refresh failures. Offscreen rendering is distinct from native interaction, accessibility and Linux/Windows validation, which remain unverified and belong to subsequent hardening.

## Staged category catalog — 2026-10-28

Accepted and implemented: a complete QIF category catalog belongs to the whole import, not to one account. The encrypted immutable whole source is its durable staging store; no category-head, category-history or committed catalog change occurs during capture, materialization, repair or review. The review dialog lists every imported name and its source metadata, including unused definitions. Transactions retain their imported category labels during review. Pending categories are not offered to unrelated work as committed identities.

Whole-import Abort removes the entire staged catalog from active review, regardless of draft references. It does not decide category rollback using an unused-category test. Original encrypted evidence and audit history remain, as with discarded transactions. Pre-existing committed categories are unchanged. Restart and restored backups reconstruct the staged catalog from the same source and group state.

The first explicit account commit publishes the entire catalog in the same durable transaction as financial publication and group transition. Transfer-first publication uses that same boundary. Exact identity matches use the existing case-insensitive/canonical-colon policy; similar names are separate categories. Matching an archived category requires explicit reactivation before commit. The implementation preserves catalog order for newly created definitions and records each creation in category history. It does not infer report behavior from descriptions or income/expense markers.

Source-owned allocations resolve to published category identities when their account commits; manual/rule replacements and transfer destinations remain unchanged. The first publication's immutable catalog revision determines that mapping, so a later rename or name reuse cannot redirect a still-pending account's source assignments. Reopening verifies the entire catalog at that revision, including unused definitions, and verifies the initial entry bindings against source evidence and publication history. Subsequent editing uses the usual stable identity/history behavior.

Commit confirmations state that the complete catalog publishes with the first account and whole-import Abort ends. Other accounts stay uncommitted. Current source/profile limits and slash ambiguities are specified in [QIF v6](../features/009-qif-profile.md#category-catalog-and-literal-slash-names--accepted-2026-10-28). No private exports were copied or opened for this work. See [status](status.md) for actual verification.

## Reusing confirmed mappings for transfer pairs — accepted 2026-10-28

The owner approved automatic destination resolution for complete named-account QIF exports. The confirmed source-account mapping is the authority: a bracketed or unbracketed source name resolves through that mapping and the confirmed account alias list even when the target account has a different display name. This supersedes the requirement to choose that destination again for every record in this bounded whole-export case. Standalone statement imports retain explicit transfer resolution.

The 2026-10-29 alias extension recognizes unbracketed account names as well as bracketed references, including split allocations, through current names, rename history and confirmed import mappings. Account-named catalog entries produce an import-review warning and no ordinary category. During materialization, current included, structurally valid draft allocations with reciprocal account references, the same currency definition, exactly opposite signed amounts and the same posting date are eligible. Equivalent reciprocal groups must have equal eligible occurrence counts. Each endpoint is consumed exactly once, in stable source-account, record-ordinal and allocation order; this association does not claim source evidence identifies a unique historical pair. Already-linked endpoints are excluded. Unequal groups are left wholly unresolved rather than partially paired. This repeated-group extension was implemented on 2026-10-29. Class metadata remains separate. Unknown destination names, self references, currency mismatches, different dates, unavailable/unequal amounts, excluded or unresolved rows and ambiguous candidates remain for review. No date tolerance or currency conversion is inferred.

A match writes the existing independently evidenced Merge relationship and history. It preserves both records, their allocations, dates, memo/category text and source evidence; it generates no counterpart or extra posting. All matches and draft materialization share one transaction. Cancellation or failure rolls them all back; retry uses the retained capture. Later per-account commit and protection remain explicit. Whole-import Abort releases both uncommitted independent sides and discards the entire group atomically, including unused newly created accounts. It records the releases using the existing transfer history and replay machinery.

Existing drafts can use **Resolve mapped transfers**. This scans only current account-named allocations (bracketed or unbracketed), uses the same mapping and matching criteria, and atomically records any new matches. Repeating it is idempotent; linking alone does not change the reviewed opening amounts. It creates no migration and needs no schema bump: existing schema-30/29 drafts and existing transfer/history representations are reused. Resolution failures never permit partial commit.

The whole-import readiness counter now includes unresolved transfer allocations. **What’s blocking commit?** lists up to 200 account/record-specific blockers and navigates to the relevant account or record. Opening decisions, repairs, transaction inclusion/duplicate decisions and unresolved transfers are distinct reasons. All blockers still contribute to the count even when the list is capped. Unmatched transfers can be reviewed from their now-locatable record; the list does not invent missing counterparts. The earlier general storage/status latency remains outside this fix.

## Actionable blocker details and navigation — 2026-10-28

The owner approved replacing the generic unresolved-transfer message with diagnostic reasons, candidate inspection, a usable scrollbar, and reliable record reveal. This is read-only analysis and UI behavior; the matching criteria, amounts, evidence, schema and commit policy do not change.

For each displayed transfer blocker, the group snapshot distinguishes a missing mapped destination, self reference, archived/unavailable source or destination, incompatible currency, an unrepresentable opposite amount, no reciprocal draft reference, unequal amounts, differing dates, unavailable candidates, unequal eligible occurrence counts, or a unique pair/equivalent balanced repeated group awaiting the resolve command. Current draft allocations provide candidate facts; committed and discarded imports are not presented as available draft counterparts. Excluded, stale and already-linked draft candidates retain their explicit status. Analysis is bounded to 400,000 transfer allocations; exceeding that bound yields an explicit analysis-limit reason rather than claiming no counterpart exists.

The dialog displays up to ten candidate allocations per blocker, with the total count, account, record number, date, exact amount/currency and review status. Same-date/opposite-amount candidates are displayed first as a group; absent those, opposite-amount candidates on other dates are shown, then reciprocal allocations with different amounts. These are inspection links, not match confirmations. Following either a blocker or candidate clears the ledger filter, selects the projected account view, expands the requested record and reveals it instead of restoring the old account viewport. Opening-only blockers navigate to their account. The virtualized list reserves space for a visible draggable scrollbar; the aggregate blocker count and 200-entry display cap remain unchanged. No private dataset is needed to verify this behavior; see [status](status.md).
