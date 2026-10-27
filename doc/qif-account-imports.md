# QIF account sections and whole-source review

Last updated: 2026-10-26

Implemented and verified within the accepted roadmap point-1 scope; exact selected checks and limitations are recorded in status. This extends the [ordinary QIF profile](../features/009-qif-profile.md), [account workspaces](accounts.md), and [transfer lifecycle](qif-transfers.md). It does not implement general account deletion, account currency reassignment, transfer repair, or new import formats.

## Accepted source envelope

The bounded UTF-8 profile retains explicit DMY/MDY selection and four-digit years. Ordinary transaction sections use `!Type:Bank`, `!Type:Cash`, or `!Type:CCard`. An optional BOM and LF/CRLF remain supported. Headerless ordinary single-account sources retain their established workflow.

Account exports may contain `!Option:AutoSwitch`, `!Clear:AutoSwitch`, and `!Account` definitions terminated with `^`. Initial definitions require `N` name and a supported ordinary `T` type. Consecutive definitions are indexed separately; a transaction section belongs to the most recently selected account. A later `!Account` record may select an already defined name with `N` alone. Repeated sections for that name retain file order and one source-account identity. Conflicting repeated types or reported balances block the document. Account type is informational; the transaction header determines the supported record grammar.

`B` is retained as reported balance text. Account `D` description, `L` credit limit, `/` statement date, and `$` statement balance are retained as informational metadata and original bytes. They do not create postings. Their presence does not authorize interpreting statement metadata as an opening balance. Names are preserved exactly; suffixes never infer currency. Empty transaction sections and metadata-only account lists are supported.

The index keeps ranges into the complete unchanged file. No synthesized subsection file replaces the evidence. Limits include 64 MiB per source, 1000 named accounts, 10000 sections, 200000 transaction records, and the existing per-field, per-record and per-allocation limits. Cancellation checkpoints run while scanning and parsing. Unknown headers, unsupported types, malformed account lists and account-boundary errors are refused before capture, with an explicit message that no import was saved and the original file is unchanged. Transaction-level unsupported fields remain visible blockers in retained drafts. Resource rejection never masquerades as a valid empty source.

Investment/security/price sections, category/class catalog imports, two-digit years, locale amount guessing, and unrecognized export structures remain unsupported. An opening-balance pseudo-record using a bracketed self-account reference is not automatically converted into account opening money or stripped from evidence; unresolved self-transfer intent remains blocked. This is a specified ordinary-money profile, not universal QIF compatibility.

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
