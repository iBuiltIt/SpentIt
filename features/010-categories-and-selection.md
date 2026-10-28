# 010 — Categories and selection

Status: **active**  
Last updated: 2026-10-27  
Implementation: bounded catalog, allocation assignments and editor are implemented and verified on available macOS configurations. Current documents use experimental schema 20 with rules and 21 without; both include this catalog. The durable category selector lists real accounts as transfer destinations and stores the account identity. Category reports remain unimplemented. See [implementation and recovery](../doc/categories.md) and [transfer management](008-transfer-matching-and-lifecycle.md#accepted-transfer-management-decisions--2026-10-27).

The 2026-10-26 follow-up adds committed bulk assignment to every selected allocation, with exact preview, one atomic operation, and group Undo. See the [search/ledger contract](../doc/search-ledger.md). Experimental-format migration and compatibility obligations start only at an owner-announced release.

## Purpose and roadmap position

The owner discussed categories ahead of their implementation milestone. Preserve these decisions for category editing, selection, rules, and reporting work; implementation is now authorized under the coordinated roadmap, subject to the accepted lifecycle policy below. Read alongside [accounts and splits](002-accounts-money-splits.md), [rules](004-rules-and-explanations.md), [transaction navigation](005-transaction-sheet-navigation.md), [reporting](006-reporting-drill-down.md), and [transfers](008-transfer-matching-and-lifecycle.md).

The organizing principle is useful distinctions that the owner can reliably make from statements at a sustainable review effort. Review may happen only every three months. Do not require receipt reconstruction or fine-grained purchase classification merely to satisfy a theoretically uniform taxonomy.

## Accepted requirements and decisions

### Shallow hierarchy and familiar names

- One grouping level with assignable leaves: parent → leaf, with no deeper nesting. Parents cannot receive transaction allocations.
- The hierarchy has reporting meaning: a parent groups its descendant allocations for filtering, totals, and drill-down. A parent subtotal and its children represent the same allocations and must not be added together again.
- Category names are user classifications, not triggers for financial behavior. Renaming Travel to Holidays must not change financial treatment. An ordinary category named Transfers does not establish an account relationship.
- Preserve the owner's Auto terminology. Typing Auto must reveal its choices, including Parking, without requiring the owner to remember or type Transportation first.
- Utilities is the intended home for phones, TV, streaming, cable, electricity, and similar services. Do not relocate TV/Streaming to Entertainment merely for taxonomic consistency.
- Uncategorized is special: absence of classification, distinct from a deliberate Other/Miscellaneous assignment. It must remain visible in reporting; exact storage and UI representation remain to be designed.

### Amazon is intentional

Keep Amazon as a category in the owner's proposed set. Amazon statement entries do not reliably reveal the purchased goods. The owner makes frequent purchases and does not intend to reconstruct roughly 90 purchases during quarterly review to distinguish washing powder from darts. Amazon represents useful miscellaneous household/general spending, and its total is meaningful on its own.

Do not replace it with a merchant-only filter or require item-level splits. Category and payee may intentionally overlap. Automatic assignment by a rule is a useful proposed convenience, subject to the existing rule design; this discussion does not implement or settle rule precedence.

### Income and expense

Categories do not require an expense/income designation. A category can receive allocations of either sign: an Amazon purchase and its refund can share Amazon. Salary can be a category without another mandatory type setting.

Direction alone does not identify income or spending. Transfers, opening balances, and refunds need explicit reporting treatment. Keep transfer/opening-balance identity separate from category names. Refund/net-spending and income-report policy remain open in feature 006; neutral categories do not settle those policies. Optional reporting groups were discussed as a possible later convenience, not an accepted requirement to implement now.

### Accounts in the category selector

Accounts appear as distinguishable transfer destinations in the same selector. From the user's perspective they can be selected where a category is selected; internally selecting an account establishes a typed account reference, not an ordinary category-name match.

For example, choosing the actual CIBC: Visa account on a Savings payment initiates a transfer. Its account identity survives renaming; a same-named ordinary category does not trigger a transfer. The colon in the account's name does not by itself establish a category hierarchy.

The durable selector lists each other account with its name, currency, and identity. Choosing it on an unlinked allocation creates a generated destination draft. Choosing it on an existing transfer origin replaces the destination under the [2026-10-27 replacement policy](008-transfer-matching-and-lifecycle.md#accepted-transfer-management-decisions--2026-10-27). It does not auto-link an existing independent row. Candidates remain an explicit confirmation step. A category whose name matches an account stays an ordinary category assignment.

Use the accepted [transfer lifecycle](008-transfer-matching-and-lifecycle.md): preserve evidence, avoid counting payment principal as spending again, and require explicit commit per account. Committing Savings protects a generated Visa draft but does not commit Visa. Automatic matching confidence and cross-currency links remain deferred there. Allocation-level links, including a fee split beside a transfer allocation, are accepted.

### QIF-compatible category names

Category names must respect the supported QIF format. In particular, do not allow a literal slash in an editable category name: it has structural meaning in QIF category/class notation. Validate at category creation/rename with an explanation; do not defer errors until export or silently strip/replace characters. Preserve immutable imported source evidence.

The current bounded implementation in `src/engine/qif.cpp` rejects category text containing `[]/|`; this is an implementation fact about a restricted parser, not a completed naming specification. Before implementation, verify the full naming contract against the supported QIF profile, including reserved account-transfer notation, hierarchy delimiters, control characters, and import/export behavior. Do not interpret the slash-separated labels in the discussion list below as approved literal names.

## Owner's proposed category set — retained for discussion

This preserves the supplied grouping and labels, including alternatives separated by slashes. It is a discussion inventory, not a finalized seed catalog or directly importable QIF. The owner rejected suggestions to remove Amazon, deepen Auto under Transportation, relocate streaming, distribute general insurance across purposes, separate Gifts/Donations or Fees/Interest, or rename Coffee/Fast Food. Do not treat those suggestions as accepted changes.

| Discussion heading | Supplied categories |
|---|---|
| Housing | Rent / Mortgage; Home Maintenance; Home Improvement; Furniture & Appliances |
| Utilities | Electricity / Gas / Heating; Water / Sewer / Garbage; Internet; Phone / Mobile; TV / Streaming |
| Transportation | Auto: Gas / Fuel; Auto: Maintenance / Repair; Auto: Insurance; Auto: Parking & Tolls; Auto: Payment / Other; Public Transit; Rideshare / Taxi |
| Food | Groceries; Restaurants / Dining; Coffee / Fast Food |
| Shopping | Amazon; Clothing; Electronics / General Shopping |
| Health & Wellness | Medical: Doctor / Hospital; Medical: Pharmacy; Gym / Fitness; Personal Care (hair, beauty, toiletries, etc.) |
| Entertainment & Hobbies | Entertainment (movies, events, streaming beyond the TV category if wanted, games, etc.); Hobbies |
| Travel | Travel: Flights; Travel: Hotels / Lodging; Travel: Other (car rental, activities, etc.) |
| Family & Education | Child Expenses / Childcare; Education |
| Other / Miscellaneous | Gifts & Donations; Pets; Insurance (general — health, life, home, etc.; or split further if desired); Fees / Interest / Bank Charges; Miscellaneous / Other |

The owner's later clarification prefers names such as Utilities: Phones and Utilities: TV & Streaming. Travel is an explicit grouping parent with no direct allocations. Auto must be discoverable directly. The original heading-plus-prefix arrangement is not permission to introduce three levels.

## Implementation direction and deferred options

- Store stable category identities and explicit parent relationships; render a path such as Travel: Flights without making mutable text the identity.
- Match picker searches against both parent and leaf names: Auto reveals its leaves; Parking finds Auto: Parking & Tolls. Show enough context to distinguish identically named leaves, and make parents non-assignable.
- Starter categories remain optional and editable; this bounded implementation starts with an empty catalog so unresolved starter names do not become financial assumptions.
- Optional trip/project tags could collect meals, taxis, shopping, and flights for one holiday across categories. This was discussed but not adopted; do not add tags or require them as part of category implementation.

## Open questions for the implementing agent

- Reconcile the original headings with the accepted depth limit. Is Auto a top-level parent alongside Transportation? Similarly, how should Medical relate to Health & Wellness? Do not silently add an extra hierarchy level or a separate presentation-section system. The suggestion to make Auto top-level was not explicitly resolved.
- Finalize QIF-safe leaf spellings without changing their intended meaning. Standalone assignable categories are accepted; their presentation beside groups is an implementation choice.
- Implement the accepted lifecycle policy below with stable identities, retained evidence/history, and explicit mapping of existing free text. No automatic document migration is authorized.
- Define parent expansion/filter behavior and Uncategorized representation, while preserving exact allocation-level reconciliation and currency separation.
- Settle refund/net-spending and income reporting policy separately; do not infer it from a name or amount sign alone.
- Mortgage and Auto Payment labels do not settle loan-principal, interest, or cash-flow reporting policy.

## Acceptance criteria — future verification obligations

- Parent nodes cannot receive allocations; the supported category model rejects deeper nesting.
- Parent totals reconcile exactly to their included leaf allocations and drill-down without double counting or combining currencies.
- Typing Auto finds its available categories, including Parking; actual picker details remain to be designed.
- Amazon remains an ordinary usable category without mandatory item reconstruction.
- Both signs are accepted without mandatory income/expense typing; Uncategorized remains distinct from Other.
- Account selections use account identity and initiate the specified transfer lifecycle; category names alone never do so.
- Invalid QIF category names are explained and rejected without silent modification or loss of imported evidence.

## Verification and implementation limits

The original discussion was documentation-only. Configuration-specific verification is recorded in [project status](../doc/status.md). The durable selector's account entries are part of the point-5 transfer commands. Category-report reconciliation remains a future acceptance obligation.

Documentation validation: all 93 relative link file targets across the eight changed documents resolved, and `git diff --check` passed. Runtime tests were not run because this change only records requirements and discussion.

## Accepted lifecycle policy for coordinated implementation — 2026-10-25

Use stable category identities with optional standalone assignable leaves and one non-assignable parent level. Siblings have unique names; top-level leaves and groups share a namespace, while different parents may have equally named leaves. Renaming or moving a category preserves its identity and allocation assignments. Current ledger/report views for any financial period use the current names and hierarchy, while original import evidence and recorded change history retain their earlier labels/definitions.

For example, moving Parking & Tolls from Auto to Travel would move last year's parking allocations into Travel in a newly generated current report. The owner accepted this current-taxonomy behavior. Refund/income and historical-as-of reporting remain separate policies.

Archive removes a category from new assignment choices but keeps existing allocations, balances, history and report visibility. Delete only never-referenced empty entries, retaining definition audit; otherwise use archive. Do not merge or reclassify allocations implicitly. Existing free-text classifications remain visibly unmapped until deliberate mapping; do not silently convert them to Uncategorized. A committed allocation reassignment uses the accepted committed-correction history rather than catalog renaming.

The starter catalog remains optional/proposed so its unresolved headings and final spellings do not prevent the model and editor from being implemented. This accepted lifecycle policy does not settle transfer behavior or reporting sign/refund policy.
