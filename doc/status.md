# Project status

Last updated: 2026-10-25

SpentIt has a buildable C++20 financial engine, an optional isolated encrypted-storage/recovery prototype, and an optional synthetic Qt Quick transaction-sheet prototype. **There is no production financial-document application yet.**

## Implemented engine

Exact signed integer money with explicit currency/scale, strict decimal input, checked arithmetic, validated mixed-sign allocations, original imported amounts retained separately from effective-total corrections, and designated-remainder editing with explicit mixed-sign choice. Manual allocations and prior values survive rejected edits unchanged.

The current account-selection catalog contains all 165 numeric-scale ISO codes in the pinned SIX List One publication of 2026-09-17. Retained amount definitions are separate: those 165 definitions plus verified BGN and HRK definitions preserve exact units, code, scale, and identity independently of current eligibility. Unknown definitions fail explicitly; no historical scale or conversion is guessed. Broader historical coverage and date-based resolution remain unimplemented. See [feature 002](../features/002-accounts-money-splits.md), [currency provenance](../third_party/iso4217/README.md), and the [split contract](split-editing.md).

The engine has no UI, concrete storage, or third-party software dependencies. Nine engine test groups passed in Debug, Release, and ASan/UBSan on macOS 27.0.1 arm64 with AppleClang 21.0.0, CMake 4.4.4, and Ninja 1.13.2. The offline catalog check and a compiled simulated CAD-withdrawal regression passed: its original amount remained readable after CAD was removed from a temporary current selection catalog.

## Encrypted-storage prototype

The owner approved SQLCipher Community Edition for this bounded experiment. The optional `spentit_storage_prototype` target uses pinned SQLCipher 4.19.0, with the tested OpenSSL 3.5.9 provider. It saves immutable synthetic source evidence and validated draft snapshots, restores historical money, excludes drafts from committed-amount queries, and atomically publishes a batch with operation history and draft transitions. Stable operation identities make identical retries idempotent; conflicting reuse is rejected.

Encrypted backups use the supported database backup API, validation and reopen before exclusive publication. Restore is tested into a new document. Existing backups are not overwritten. Unknown schemas and invalid definitions/amount relationships fail explicitly. See [prototype decisions, build instructions, verification and recovery limits](storage-prototype.md).

Debug, Release and ASan/UBSan verification passed all 17 engine/storage test groups, including injected disk-full and write/sync failures, abrupt exits around commit and backup I/O, both commit boundaries, retry checks, wrong passwords, malformed authenticated documents, writer contention, backup restoration and scans for synthetic private-data markers. No compiler warnings or sanitizer diagnostics were emitted. The source-preparation helper also reproduced the pinned hashes from a fresh extraction. These are bounded test results, not proof of safety under physical power loss or all filesystems.

## UI requirements recorded — 2026-10-25

The owner-approved main layout is documented in [feature 005](../features/005-transaction-sheet-navigation.md): resizable/hideable navigation, Accounts/Reports sections, persistent toolbar, ledger sorting and filtered totals, committed/projected balance toggling, and Save / Discard / Stay navigation. Search will combine whole-record text matching, period filters, and a QIF-source mode; grammar remains unresolved. [Feature 002](../features/002-accounts-money-splits.md) records opening-balance defaults, account archiving, and deliberate currency reassignment without conversion. Scale/transfer handling for reassignment still needs design. Settings, category/rules editors, and report-generator contents remain future discussions. These are requirements only; no UI or account behavior was implemented by this documentation update.

## Transfer requirements recorded — 2026-10-25

[Feature 008](../features/008-transfer-matching-and-lifecycle.md) records accepted transfer matching, provenance, Fix match, and removal behavior. Commit is explicit per account: committing checking protects its generated Visa draft without committing Visa. That draft survives destination workspace clearing, affects projected balance only, and requires editing/reversing its origin to remove the purely generated movement. Independent bank evidence is preserved. Per-operation relationship/history updates remain atomic. Matching in either import order, attention for unmatched generated entries, and match-icon navigation are specified. [Feature 005](../features/005-transaction-sheet-navigation.md) records search selection/view restoration. Later committed reversal, independent-evidence reconciliation, and detailed match policies still need design. No transfer, search, or UI implementation or runtime verification is claimed by this documentation update.

## Synthetic transaction-sheet prototype — 2026-10-25

The owner authorized a bounded sheet experiment. The optional `spentit_sheet_prototype` executable now implements the shell, resizable/hideable sidebar, two synthetic account views, exact totals and balance toggles, virtualized ledger, sorting/literal filtering, selection, inline split/total editing and Save / Discard / Stay navigation. Temporary edits and selections use stable identities outside recycled delegates. All data is synthetic; saves last only for the running session. It does not use the storage prototype or implement production account/history behavior.

Plain C++ session use cases remain independent of Qt. The Qt adapter performs generation, queries and saves in background workers; QML does no money arithmetic. All 13 engine/session/model/QML/interaction CTest entries passed in Debug, Release and ASan/UBSan on macOS arm64. Native scrolling was measured separately. The owner selected 100,000 transactions/account as representative and 200,000 as the stress workload, not record limits. See [prototype scope, build, measurements and limitations](sheet-prototype.md). Qt Quick remains a product candidate, not a final architecture decision.

## Remaining product work

All [feature documents](../features/README.md) remain **active**. Features 001, 002 and 005 have bounded implemented subsets. Production accounts, balance ordering, transfer semantics, actual QIF parsing/duplicate review, rules, reports and durable application/UI use cases do not exist. The synthetic sheet conventions do not settle those designs. The prototype does not implement full document lifecycle, application instance locking, draft editing/discard, undo, password changes, migrations, or their recovery workflows. It is not intended for personal financial data.

The [project layout](project-layout.md) is realized for engine, optional storage, synthetic app/UI, their tests, build support and reference/dependency metadata. Qt Quick/QML is used only in the sheet experiment; final adoption and PCRE2 remain proposals. SQLCipher is selected for the prototype only. Linux is unverified; the storage prototype is POSIX-only and explicitly does not build on Windows. No portability or general responsiveness claim is made; bounded macOS sheet measurements are recorded separately.

The owner's licensing intent remains permanent free access, prohibition on selling SpentIt or derivatives, and free internal business use. Exact terms remain unresolved; no project license has been selected or added. Third-party notices are retained separately. See [licensing and distribution](../features/007-licensing-and-distribution.md).

Next product work requires choosing the next bounded milestone and settling its material questions. The engine and storage experiments do not settle the final application schema, UI or distribution design.

## Version control baseline — 2026-10-25

The project is tracked in a local Git repository on `main`. The initial snapshot includes source, tests, requirements, verification records, dependency notices and pinned currency references. Ignore rules exclude generated builds, local settings and common sensitive/runtime files; attributes define text line endings, binary assets and byte-preserved upstream XML. See [repository conventions](../README.md). No remote publication or project-license selection is part of this setup.
