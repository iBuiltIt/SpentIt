# QIF account sections and whole-source review

## Import category decisions

See [rules contract](rules.md). After preparation,
missing ordinary names across every mapped account and the complete catalog
appear once in an import-wide checklist. Apply Selected creates checked names
and clears unchecked effective assignments. Use Uncategorized clears all missing
assignments. Original text remains in the source representation. Recognized
categories bind to their existing identity. Transfer aliases stay separate.
The decision, category creation, and automatic rules commit atomically.
Completed preparation awaiting this choice survives close and restart and blocks
acceptance and rules on those rows. Closing the dialog defers it. The sidebar
reopens it. Abort uses the existing import-owned operation. Repaired rows
respect the saved choice. See [release schema](release-schema.md) for the file format.

Incomplete preparation is cleaned up on cancellation or the next open.
Completed uncommitted imports remain. Account Discard is account-owned.
Review repairs import problems and Accept transactions accepts the file.

See the [QIF profile](qif-profile.md) for ordinary records and
[import and review](import-and-review.md) for the file-wide workflow.

## New-account opening

The mapping screen shows currency and an editable opening date for account
creation. The date defaults to
the calendar day before the earliest valid transaction date in that source
account, so the opening sorts before that day's transactions. An account with
no valid transaction date still defaults to 1900-01-01. A day before year 0001
stays on the earliest transaction date. The opening
balance selector defaults to zero and optionally offers the reported balance as a
literal opening amount. It never treats that amount as closing or subtracts imports.
Missing/zero reports supply no second option. Existing mapped accounts are unchanged.

The explicit zero/reported choice is saved in encrypted immutable balance history
within account capture. It confirms the account opening immediately and does not
become stale when transactions are repaired or linked. No later opening setup panel
is shown for those accounts. Omitting the choice refuses before capture.
See [release schema](release-schema.md) for the file format.
Verification checks one fixed opening choice per created mapping against
source and creation facts. Transaction revisions do not participate. The
retired staging API refuses without writing. Import Undo preserves the
opening and the account.

## Accepted source envelope

The bounded UTF-8 profile accepts ISO dates and retains explicit DMY/MDY selection for slash dates, with four-digit years. Ordinary transaction sections use `!Type:Bank`, `!Type:Cash`, or `!Type:CCard`. An optional BOM and LF/CRLF remain supported. Headerless ordinary single-account sources retain their established workflow.

Account exports may contain `!Option:AutoSwitch`, `!Clear:AutoSwitch`, and `!Account` definitions terminated with `^`. Initial definitions require `N` name and a supported ordinary `T` type. Consecutive definitions are indexed separately; a transaction section belongs to the most recently selected account. A later `!Account` record may select an already defined name with `N` alone. Repeated sections for that name retain file order and one source-account identity. Conflicting repeated types or reported balances block the document. Account type is informational; the transaction header determines the supported record grammar.

`B` is retained as reported balance text. Account `D` description, `L` credit limit, `/` statement date, and `$` statement balance are retained as informational metadata and original bytes. They do not create postings. Their presence does not authorize interpreting statement metadata as an opening balance. Names are preserved exactly; suffixes never infer currency. Empty transaction sections and metadata-only account lists are supported.

The index keeps ranges into the complete unchanged file. No synthesized subsection file replaces the evidence. Limits include 64 MiB per source, 1000 named accounts, 10000 sections, 200000 transaction records, and the existing per-field, per-record and per-allocation limits. Cancellation checkpoints run while scanning and parsing. Unknown headers, unsupported types, malformed account lists and account-boundary errors are refused before capture, with an explicit message that no import was saved and the original file is unchanged. Transaction-level unsupported fields remain visible blockers in retained drafts. Resource rejection never masquerades as a valid empty source.

Investment/security/price sections, class catalog imports, two-digit years, locale amount guessing, and unrecognized export structures remain unsupported. An opening-balance pseudo-record using a bracketed self-account reference is not automatically converted into account opening money or stripped from evidence; unresolved self-transfer intent remains blocked. This is a specified ordinary-money profile, not universal QIF compatibility.

## Mapping and evidence

The mapping dialog requires an explicit choice for every named source account, including empty accounts. When more than one source account is listed, a line separates each account's choices. Choose an existing account or explicitly create one using its exact imported name, a visible currency code, and an opening date. New accounts default to the initial setup account's currency. Every source account maps to a distinct target account; combining multiple differently named source accounts into one pending account is refused visibly. Repeated sections of the same source account already share its mapping.

Capture atomically stores one whole-file source, the complete mapping, explicit account creations and every captured account draft. Each batch references that source and its stable source-account index. Resume atomically materializes all account drafts. Cancellation or failure before that transaction completes leaves the captured group available for Resume or Abort. Reopening and restored backups retain the same group and evidence.

An unchanged operation identity is retryable; changed source, profile, mapping or creation inputs conflict. A new request with the same bytes, profile and mapped existing accounts navigates to its already pending group. Deliberately creating different accounts is a different mapping. Byte-identical committed-file recognition is scoped to both target account and source-account index and preserves one-to-one occurrence multiplicity. All other candidates remain reviewable; no uncertain record is silently dropped.

## Repairs and the publication gate

Review shows the total repairable-record count before editing, completed saved repairs, the number requiring amount repair, unsupported records/sections, and outstanding review items. Amount repair includes unavailable totals, unavailable split amounts, and exact split-total discrepancies. Date-only repairs count separately through the total. Unsupported structures are not labeled repairable. Saving a repair preserves unknown original money and source identities; duplicate disposition still requires explicit review.

No grouped account can commit or acquire transfer protection until every source account's repairs, blockers, duplicate decisions and required new-account balance decisions are resolved. External transfer publication checks the same gate. Ordinary single-account imports also block protection while their source still requires repairs or duplicate review, so partially repaired imports remain fully abortable.

Account commit remains explicit and separate. The first related financial publication atomically records that the group has started and links it to the durable financial operation. The confirmation explains that whole-import Abort ends at this boundary; other accounts remain drafts. Afterwards their ordinary protected-discard rules apply. Discarded members no longer block remaining accounts, while retained protected portions must still satisfy their financial checks.

## Abort, recovery and retained identities

Before first publication, Abort discards all group drafts and earlier saved repairs in one transaction. Encrypted source bytes and append-only histories remain. Accounts explicitly created by that import disappear from the active account list when unused; retained internal identities keep evidence references valid. Unrelated accounts or independent work survive. An aborted creation cannot be resurrected through an old creation retry, and new account identity allocation includes retired identities. If Abort removes the selected account, selection returns to the initial account.

Group outcome queries reject a transaction that has not returned to autocommit. The worker distinguishes known saved state, known unperformed work and unknown outcomes. On an unreadable outcome it closes the session and asks for reopening and inspection. Capture requests retain their mapping identities across acknowledgement failure; Resume and Abort are retryable, and repeating an unchanged balance decision does not append another decision. A saved operation whose UI refresh fails is reported as saved, with explicit reopen guidance.

## Verification

Use the account-group and opening-mapping suites in
[storage tests](../tests/storage/account_tests.cpp). Cover mapping, repeated
sources, exact opening choices, incomplete capture, durable Resume/Abort,
transfer publication and retry after both durable boundaries. Authenticated
verification must reject mismatched mappings, publication and retired-account
state.

## Staged category catalog

A complete QIF category catalog belongs to the whole import, not to one account. The encrypted immutable whole source is its durable staging store; no category-head, category-history or committed catalog change occurs during capture, materialization, repair or review. The review dialog lists every imported name and its source metadata, including unused definitions. Transactions retain their imported category labels during review. Pending categories are not offered to unrelated work as committed identities.

Whole-import Abort removes the entire staged catalog from active review, regardless of draft references. It does not decide category rollback using an unused-category test. Original encrypted evidence and audit history remain, as with discarded transactions. Pre-existing committed categories are unchanged. Restart and restored backups reconstruct the staged catalog from the same source and group state.

The first explicit account commit publishes the entire catalog in the same durable transaction as financial publication and group transition. Transfer-first publication uses that same boundary. Exact identity matches use the existing case-insensitive/canonical-colon policy; similar names are separate categories. Matching an archived category requires explicit reactivation before commit. The implementation preserves catalog order for newly created definitions and records each creation in category history. It does not infer report behavior from descriptions or income/expense markers.

Source-owned allocations resolve to published category identities when their account commits; manual/rule replacements and transfer destinations remain unchanged. The first publication's immutable catalog revision determines that mapping, so a later rename or name reuse cannot redirect a still-pending account's source assignments. Reopening verifies the entire catalog at that revision, including unused definitions, and verifies the initial entry bindings against source evidence and publication history. Subsequent editing uses the usual stable identity/history behavior.

Commit confirmations state that the complete catalog publishes with the first account and whole-import Abort ends. Other accounts stay uncommitted. Current source/profile limits and slash ambiguities are specified in [QIF v6](qif-profile.md#category-catalog-and-literal-slash-names).

## Reusing confirmed mappings for transfer pairs

For a complete named-account QIF export, the confirmed source-account mapping is the authority: a bracketed or unbracketed source name resolves through that mapping and the confirmed account alias list even when the target account has a different display name. Standalone statement imports retain explicit transfer resolution.

Matching recognizes unbracketed account names as well as bracketed references, including split allocations, through current names, rename history and confirmed import mappings. Account-named catalog entries produce an import-review warning and no ordinary category. During materialization, current included, structurally valid draft allocations with reciprocal account references, the same currency definition, exactly opposite signed amounts and the same posting date are eligible. Equivalent reciprocal groups must have equal eligible occurrence counts. Each endpoint is consumed exactly once, in stable source-account, record-ordinal and allocation order; this association does not claim source evidence identifies a unique historical pair. Already-linked endpoints are excluded. Unequal groups are left wholly unresolved rather than partially paired. Class metadata remains separate. Unknown destination names, self references, currency mismatches, different dates, unavailable/unequal amounts, excluded or unresolved rows and ambiguous candidates remain for review. No date tolerance or currency conversion is inferred.

A match writes the existing independently evidenced Merge relationship and history. It preserves both records, their allocations, dates, memo/category text and source evidence; it generates no counterpart or extra posting. All matches and draft materialization share one transaction. Cancellation or failure rolls them all back; retry uses the retained capture. Later per-account commit and protection remain explicit. Whole-import Abort releases both uncommitted independent sides and discards the entire group atomically, including unused newly created accounts. It records the releases using the existing transfer history and replay machinery.

Existing drafts can use **Set up imported transfers…**. This scans only current account-named allocations (bracketed or unbracketed), uses the same mapping and matching criteria, and atomically records any new matches. Repeating it is idempotent; linking alone does not change the reviewed opening amounts. Resolution failures never permit partial commit.

The storage readiness snapshot counts all unresolved work, including transfer
allocations, and retains up to 200 account/record-specific blocker details.
Review presents import problems and their available actions. See the
[Review contract](review-sheet.md#user-interaction).

## Actionable blocker details and navigation

For each displayed transfer blocker, the group snapshot distinguishes a missing mapped destination, self reference, archived/unavailable source or destination, incompatible currency, an unrepresentable opposite amount, no reciprocal draft reference, unequal amounts, differing dates, unavailable candidates, unequal eligible occurrence counts, or a unique pair/equivalent balanced repeated group awaiting the resolve command. Current draft allocations provide candidate facts; committed and discarded imports are not presented as available draft counterparts. Excluded, stale and already-linked draft candidates retain their explicit status. Analysis is bounded to 400,000 transfer allocations; exceeding that bound yields an explicit analysis-limit reason rather than claiming no counterpart exists.

The diagnostic snapshot retains up to ten candidates per blocker and their
total count. Same-date, opposite-amount candidates take priority, followed
by opposite amounts on different dates, then reciprocal allocations with
different amounts. Candidate evidence does not authorize a match.
