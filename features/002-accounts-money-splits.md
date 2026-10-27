# 002 — Accounts, money, and splits

Status: **active**  
Last updated: 2026-10-27  
Implementation: partial; exact in-memory money, validated allocations, and original/effective totals are implemented. The pinned ISO currency catalog and engine remainder editing are also implemented. Production accounts, transfers and UI remain unimplemented; a separate [synthetic sheet experiment](../doc/sheet-prototype.md) exercises the engine. The optional storage prototype now round-trips the engine subset; production account/document persistence remains unimplemented.

The [account-scoped durable workflow](../doc/accounts.md) adds account creation/selection, independent drafts, ordinary financial histories, and the point-4 account commands below. Reports, exchange rates, transfer repair, and committed transfer reversal remain unfinished. Feature 002 stays **active**.

## Accepted requirements

Category hierarchy, naming, neutral income/expense classification, and account destinations in the selector are specified in [feature 010 — Categories and selection](010-categories-and-selection.md). These are future requirements, not implemented category behavior.

- New accounts inherit the document's base/home currency. Creating an account by entering its name in the sidebar adds a special Opening Balance entry dated today with amount zero; the user can edit its date and amount. Opening balances are excluded from income/spending reports.
- Accounts containing transactions can be closed/archived while retaining history and linked transfers. Deletion is reserved for empty accounts. The applied empty-account rule, including an untouched zero opening, is recorded in the 2026-10-27 checkpoint below.
- Allow intentional correction of a mistakenly assigned account currency even after import and commit. This is reassignment, not exchange-rate conversion: for example, an amount entered as CAD 20.00 can be corrected to GBP 20.00. Do not require starting over merely because transactions exist.
- Right-click Select Currency opens a modal showing Current Currency and New Currency (initially the current value). The new-currency selector supports search or incremental typing by code, such as ZAR, and has OK/Cancel. For an account with transactions, warn explicitly that this reassigns currency without conversion before applying the correction. It is not merely a cosmetic symbol change: currency identity used by balances/reports must be corrected consistently, with immutable source evidence preserved and committed changes recorded atomically in history.
- First-run setup asks for a home currency, initially suggesting CAD. The user may choose any supported currency; the supported set must be explicit.
- Support multiple currencies in the model from the beginning, including Wise use cases.
- Initial new-account selection catalog: all current ISO 4217 codes with defined minor-unit scales, selected by the owner on 2026-10-25. Use the numeric-scale entries in SIX List One, including its fund/accounting codes; exclude historical and scale-less codes from that selection catalog. See [snapshot provenance](../third_party/iso4217/README.md).
- Preserve historical currencies in imported transactions. Discontinuation must not make an amount unreadable, change its ISO code or scale, or convert it into a successor currency. Readability of historical amounts is separate from eligibility for new-account selection.
- Transaction-sheet columns are Date, Payee, Category, Withdrawal, Deposit, and **Balance**.
- A transaction may have multiple category allocations, each with its own memo and amount. Multiple allocations may use the same category; a category does not have one global memo.
- Splits may contain mixed positive and negative amounts. Their exact sum must equal the effective transaction total.
- Users may explicitly correct a transaction's total. Preserve and display the original QIF amount and the fact of correction; splits sum to the corrected total.
- Ordinary split editing adjusts only the designated remainder allocation. Explicitly entered allocations must not be silently adjusted.

## Proposed design

Store home currency per document, offering CAD when creating a new document. Reopening an existing document uses its stored setting. New-account inheritance and the deliberate currency-correction dialog are accepted above; the broader setup UI remains to be designed.

The money choices are adopted for the engine subset: signed integer units, explicit currency/scale, and checked arithmetic with precision rejection. Exchange-rate representation remains proposed; store conversion inputs with exact decimal/rational semantics when that scope is designed.

A foreign purchase posted to a CAD account is authoritative in the bank-posted CAD amount. Preserve any supplied foreign amount as additional evidence. Cross-currency transfers preserve both actual account amounts and separately identified fees. Never invent rates or force numerical equality across currencies.

Transfer matching, generated-counterpart provenance, repair, and per-account commit behavior are accepted in [feature 008](008-transfer-matching-and-lifecycle.md). Committing an origin protects its generated destination draft but does not commit that destination account. Apply each chosen operation and its linked state/history changes atomically. Imports from the other account reconcile to the existing movement. Transfer principal is not spending; fees can be spending. The exact posting schema remains undecided.

An ordinary same-sign split starts with one remainder allocation. Entered allocations reduce that remainder. If an edit would reverse its sign, require an explicit mixed-sign choice rather than rejecting a legitimate structure or accepting an accidental reversal. Imported mixed-sign structures remain visible and intact. A zero remainder is valid.

Example: a net deposit of +2,200 CAD can comprise +3,000 salary, -700 tax, and -100 other deductions. The +3,000 allocation exceeds the net total legitimately.

## Acceptance criteria

- Home currency is selectable from a declared supported set and persists per document.
- Historical amounts retain their exact units, currency identity, and applicable scale across import, reload, and catalog updates, even when their currency is unavailable for new-account selection. Unknown or ambiguous historical definitions must produce an explicit diagnostic rather than guessed precision or conversion.
- Money calculations remain exact across imports, edits, rules, balances, reports, backups, and restarts.
- Overflow, excessive precision, and unsupported currencies produce explicit errors.
- Same-sign, mixed-sign, zero-remainder, and corrected-total splits preserve exact sums.
- Invalid temporary editor state cannot be saved as a valid financial transaction.
- Manual allocations remain unchanged when the remainder is recalculated.
- Same-currency and cross-currency transfers preserve both accounts and fees without double counting.
- Foreign amounts never silently contaminate a CAD balance or report total.

## Implemented engine subset and adopted decisions

- `Money` uses signed 64-bit units and a `Currency` value. Current-selection currency constructors enforce the pinned SIX catalog: 165 codes from the 2026-09-17 publication, with scales 0, 2, 3, and 4. `Currency(code)` resolves the scale; `Currency(code, scale)` checks it. Unknown/withdrawn/scale-less codes and incorrect scales fail explicitly. The earlier arbitrary code/scale API policy is superseded. The internal scale ceiling of 9 remains a guard, not permission to override a currency's scale.
- Decimal input is strict ASCII `[+-]?[0-9]+(.[0-9]+)?`. Fewer fractional digits are padded exactly; excess digits are rejected even if zero. Whitespace, exponents, locale separators, and incomplete decimals are rejected. No floating-point ingestion, rounding, rescaling, or conversion exists.
- Addition, subtraction, and negation check signed overflow. Currency codes and scales must match for arithmetic, allocations, and corrections. Errors expose stable `ErrorCode` values through `FinancialError`; messages contain no financial input.
- `Transaction` is a validated in-memory value with at least one allocation, each carrying category text, memo, and amount. Empty category text is allowed pending category-model design. Zero amounts and repeated categories are valid. The transaction itself has no remainder designation; `SplitEditor` adds that editing-session state.
- All allocations must sum exactly to the effective total. Mixed signs require no special domain flag. A linear, opposite-sign-first summation using unsigned magnitudes avoids input-order-dependent intermediate overflow while preserving allocation order; an unrepresentable final sum is rejected. The same helper computes remainders without overflowing intermediate subtotals or negating signed minimum values.
- Imported transactions retain a separate original amount with read-only access. Corrections return a new validated value and preserve that original through repeated corrections; failed corrections leave the prior value unchanged. Manual transactions have no imported amount. The correction indicator compares current and original totals; correcting back clears it. This is not change history, source-byte evidence, or durable immutability.
- `SplitEditor` preserves manual allocations and recalculates only its designated remainder. Ordinary mode rejects introducing mixed signs until the caller explicitly enables them; existing mixed-sign transactions retain their structure. Zero remainders are valid. Edits and corrections return new validated values; failures leave prior values intact. See the [complete editing contract](../doc/split-editing.md).
- The C++20 library has no nonstandard software dependencies; currency reference data is pinned with provenance. Accounts, balances, transfer semantics, source records, durable history, and UI confirmation of mixed-sign editing are deferred.

### Verification — 2026-10-25

All eight test groups passed in Debug, Release, and ASan/UBSan builds on macOS arm64. Tests cover ordinary and mixed-sign splits, zero allocations/remainders, corrections, failed corrections, malformed decimals, precision and scale rejection, currency mismatches, signed limits, true sum overflow, and cancellation across all 24 orderings of four boundary terms. See [build instructions and actual results](../doc/building.md). The additional tests cover all 165 catalog entries, representative unsupported codes, remainder edits/additions/removals, index adjustment, explicit mixed-sign choices, corrections, currency mismatch, and overflow, including 775 small total/entered-amount combinations. The offline generator check matched the pinned snapshot. Linux and Windows remain unverified. No account, persistence, transfer, report, or UI acceptance criterion is claimed complete; this feature remains **active**.

## Historical-currency follow-up — implemented bounded engine scope

The owner accepted verified definitions with explicit rejection of unresolved cases on 2026-10-25. `Currency(code[, scale])` still selects only the current catalog. `Currency::from_definition(id)` restores a reviewed retained definition without consulting current eligibility. Amounts carry units, code, scale, and definition identity. Arithmetic also requires matching definition identity, preventing automatic combination of reused codes from different definitions.

The retained registry contains the 165 definitions from the archived 2026-09-17 SIX snapshot, plus BGN (scale 2, amendment 180) and HRK (scale 2, amendment 174). Historical coverage is intentionally bounded to those verified definitions; the historical XML has no minor-unit fields and does not justify guessing other scales. There is no code-only historical lookup or date-based inference. Callers must resolve the applicable definition explicitly; missing identities fail. Definitions are retained independently of the current selection mapping; updates must never remove or reinterpret an existing identity. See [provenance](../third_party/iso4217/README.md).

Verification: nine engine groups passed in Debug, Release and ASan/UBSan on the previously recorded macOS toolchain, without warnings or sanitizer diagnostics. Historical tests cover parsing, arithmetic, mixed-sign-compatible transaction validation, original/effective totals, current-selection exclusion, and unresolved identities. The offline catalog check passed. `python3 tests/tools/currency_refresh.py` removed CAD from a temporary current catalog, rebuilt the money implementation, and verified that its retained definition restored exactly -2931 units at scale 2 while new current selection failed. This is an actual simulated catalog refresh; no repository data was changed by the test. The subsequent [storage prototype](../doc/storage-prototype.md) also verifies encrypted reload and backup/restore of retained historical amounts.

## Open questions

- Additional historical definition coverage and future import UI for selecting applicable definitions. Unknown or ambiguous definitions must remain explicit errors; verified coverage is the accepted policy.
- Catalog refresh procedure, opening-balance ordering relative to same-date/earlier transactions, and account types (cash/bank/credit card initially proposed; investments not specified). The opening entry defaults to today and is editable.
- Changing home currency after setup: display/report preference versus valuation consequences. It must never reinterpret stored amounts.
- Exchange-rate provenance, rounding policy, and consolidated reporting; initially report currencies separately.
- Deterministic balance ordering for same-date records. [Feature 005](005-transaction-sheet-navigation.md) defines unchanged chronological balances under sorting/filtering, filtered ledger totals, and the committed/projected sidebar toggle. An opening balance stays outside that transaction order. The applied opening-correction command is below.

## Applied account-management checkpoint — 2026-10-27

Roadmap point 4 applies the accepted rules above on the durable import worker. New documents use experimental schema 20 with rules and schema 21 without rules. Schemas 18 and 19, and every other unexpected `user_version`, are rejected with `Unsupported experimental format; no migration was attempted`. The application id remains `1397770577`. One schema shape is shared by both versions. Verification is in [status](../doc/status.md#point-4-account-management--2026-10-27).

Each command is atomic and historied. An identical operation-id retry returns the stored receipt. Conflicting reuse is rejected. The worker reports success only after durable completion. Cancellation and an unknown outcome follow the existing durable-command rules.

- Rename changes the current display name only. `account_creations.name` stays the original name, so a later import that repeats the creation name does not retarget the renamed account. An empty or whitespace-only name is rejected. A name longer than the existing account-name limit is rejected. A name that is not blank is stored as entered, including interior spaces. Balances, currency, openings, evidence, and transfer links stay unchanged.
- Archive and reactivation keep the account, its history, evidence, balances, and transfer links. Archived accounts stay in a separate sidebar section and remain readable. They are not targets for new imports, new commits, new transactions, or new transfer matches. Archive is refused while that account has a pending import; the import stays. An unsaved editor stays with the existing Save / Discard / Stay guard. Reactivation restores ordinary use and changes no amounts, dates, evidence, or links. If the selected account is archived, selection moves to another active account when one exists. When none exists, the active area stays readable and can still open the archived account. Archive does not delete the last active account.
- Empty-account deletion retires the integer identity permanently. It is allowed only when the account has no batches, records, transfer participation, source evidence, or financial history beyond creation, the opening amount is still zero, the opening date is still the creation opening date, and another account remains. Confirming that untouched zero does not block deletion. Any other case refuses with `Empty-account deletion requires an untouched zero opening, the creation opening date, no imports, no transfer participation, and another account remaining in the document.` and changes nothing. A creation retry cannot resurrect the retired identity. Encrypted history of other accounts stays. A failed deletion leaves the last known-good database in place.
- Opening-balance correction is the later command for an account whose opening is already confirmed or frozen. It can change the current opening amount, the current opening date, or both. Transaction amounts, allocations, source bytes, and source facts stay unchanged. The previous opening stays replayable. Balances are that opening plus the existing committed effective totals in the existing order. A new opening date later than any committed transaction date on that account is refused. Overflow is refused. Undo restores the prior opening exactly when no later financial operation on this account, and no later operation on a linked transfer, has happened since. Direct updates of a frozen opening outside this command still abort. The unfrozen reported-balance preview remains the separate first-import workflow and does not replace a frozen opening from a QIF `B` record.
- Currency reassignment corrects a mistaken account currency. The numeric decimal is preserved. CAD 20.00 may become GBP 20.00. Each current opening, ledger effective total, allocation, and draft effective total and allocation on that account is represented as its exact decimal and rescaled to the new currency's scale. Extra fractional digits are kept only when they are zero. A non-zero digit that would disappear refuses the whole operation. Scale 2 to scale 3 may pad a trailing zero. Scale 2 to scale 0 refuses when any fractional digit is non-zero. Overflow of any rescaled minor-unit value or resulting balance refuses the whole operation. `account_creations.definition` stays the original definition. Source bytes, `source_occurrences.currency`, and `source_occurrences.original_units` stay in the old currency. The editable original imported amount stays that source money. Effective totals and allocations use the new currency. Validation accepts that pair when each rescaled amount equals its own previous decimal at the new scale and a recorded reassignment is in effect. A previously corrected effective total is rescaled from its own value and need not equal the original imported decimal. No rate is stored.
- A linked transfer changes only the reassigned account's amounts. The other side is left in place. The two amounts are not forced equal, and no fee or rate is invented. A former same-currency link may remain as two preserved opposite decimals in different currencies. `preserved_amounts` records that preservation. New transfer generation, proposal, and commit still reject a new cross-currency match. This is the applied reading of the existing link value. It is not a new match policy and not transfer repair. Drafts on the reassigned account are rescaled in the same transaction. The other account's drafts and source facts stay unchanged. A later prepare whose statement currency differs from the new account currency is refused. An in-progress import keeps its captured source facts; its effective interpretation is rescaled with the account.
- Before OK, the currency dialog warns when the account has an opening, a draft, or a transaction: `This reassigns the account currency without conversion. Numeric amounts stay the same decimal. It does not apply an exchange rate.` The selector lists current-catalog codes and accepts search or incremental code typing. Cancel changes nothing.
- Undo of a reassignment restores the prior definition and the prior minor units when no later financial operation on this account or a linked transfer has happened since. Ordinary committed Undo continues to exclude transfer reversal. After a currency reassignment, that ordinary Undo stays unavailable for the rescaled rows because their entry heads advanced with the account command.

The cross-currency preservation case did not need a separate financial policy. The existing link records the preserved opposite decimals, and new matches stay same-currency. Points 5–8 were not started.

## Reference

- [Real-world mixed-sign paycheck splits](https://info.quicken.com/sim/setting-up-a-paycheck-series-with-splits)

## Accepted durable-import experiment — 2026-10-25

The owner accepted the bounded policies in [feature 009](009-durable-import-milestone.md), including its explicit deferrals. Those decisions govern that experiment only; broader open questions in this feature remain unless explicitly settled there. The [headless experiment](../doc/durable-import.md) implements one experimental account, opening-balance confirmation/freeze and exact reconciliation; production account behavior remains incomplete.


## Category metadata extension — 2026-10-25

The bounded schema-5 workflow adds optional stable category bindings, allocation identities, retained source associations and explicit field ownership to the existing exact allocation values. Amount/currency/sum invariants are unchanged. Current category paths are resolved separately from retained labels. See [category implementation](../doc/categories.md). This does not implement multi-account transfers, currency reassignment or category reports. The optional [rules preview adapter](../doc/rules-engine.md) preserves dates, amounts, allocation structure and identities while proposing only classification/display changes. Schema-6 pending Apply and the rules sheet are recorded in [feature 004](004-rules-and-explanations.md). Committed reapplication remains deferred.
