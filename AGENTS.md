# SpentIt — Where did my money go?

SpentIt is a local, single-user desktop application for understanding spending
through reliable imports, reviewable categorization, and explainable reports.
Data safety, security, and correctness take priority over speed of development.
Losing or silently misrepresenting financial data loses the user's trust.

The application is in use and improved piecemeal. Discuss each new change and
wait for an explicit **Go** before implementation. Launch the local
rules-enabled Release build with `./build/SpentIt`. See the README and
[building](doc/building.md).

## Discover the project before changing it

1. Read this file and [doc/status.md](doc/status.md).
2. Read [doc/project-layout.md](doc/project-layout.md) and use the
   [contract index](doc/README.md) to find the area you are changing.
3. Read the relevant contracts and inspect the implementation and tests.
4. Check the [accepted product scope](doc/status.md#product-scope). Ask about
   material ambiguities before changing financial or security behavior.

## Where information belongs

- `AGENTS.md`: durable contributor instructions.
- `doc/`: current contracts, build and recovery guidance, and [status](doc/status.md).
- `src/`: `engine/`, `storage/`, `app/`, and `ui/`, as in
  [project layout](doc/project-layout.md). UI code does not execute SQL or own
  financial transaction boundaries. The engine does not depend on UI or concrete
  storage.
- `tests/`: automated tests and synthetic fixtures. `cmake/`: build support.
  `third_party/`: dependency metadata and deliberately vendored code only.
- `manual/`: the bundled user manual. Do not edit it, or its pictures, unless
  the owner explicitly asks. Record a future user-visible change in
  [doc/manual-update-backlog.md](doc/manual-update-backlog.md).

Use relative links between repository documents. Do not rely on chat history or
a particular machine for essential requirements. Do not copy personal financial
records into the repository.

## Current file format

The release rules database is version 2. Opening an authenticated version-1
file upgrades it in one encrypted transaction after full verification. Preserve
the original financial history and the current Undo session. Unsupported versions
are refused without changing the file. The rules-off build still writes
version 69 and is outside the release upgrade path. See
[release schema](doc/release-schema.md).

The independent SpentIt exchange file is JSON version 1. It is not the database
format. See [file exchange](doc/file-exchange.md).

## Non-negotiable data invariants

- Use exact monetary arithmetic with an explicit currency and scale. Never use
  binary floating point for authoritative money. Detect overflow and invalid
  precision.
- Splits sum exactly to their effective transaction total. Mixed signs are
  supported. Original imported amounts remain available when a user corrects a
  total.
- Never combine different currencies without an explicit conversion policy and
  recorded conversion inputs. Reports stay separated by currency. No inferred
  exchange rate.
- New and accepted transactions share one balance and the default reports.
  Closing a file does not accept transactions.
- Commit the entire chosen operation atomically, including linked transfers,
  status transitions, and history. Never report success before durable completion.
- A successful close expires ordinary Undo. A failed or cancelled close, or a
  crash, preserves that session for recovery. Originals, recognition, financial
  dependencies, and eligible import Undo survive a successful close. Reversal
  must respect later changes and financial relationships.
- Duplicate recognition must preserve legitimate repeated occurrences. An
  uncertain match is not permission to drop a transaction.
- Reporting totals must reconcile to the exact allocations and filters shown by
  drill-down.
- No silent repair, rounding, truncation, skipped records, fallback to
  plaintext, or destructive migration. Explain and surface errors.

## Security and recovery

- Use established cryptographic libraries. Do not invent encryption or password
  derivation.
- Optional no-password mode is an explicit waiver of confidentiality. It uses a
  documented public format key. Do not describe it as confidential, and do not
  apply it to a password-protected file. There is no forgotten-password recovery.
  Settings can change a file’s password, add one, or remove protection after
  confirmation. A copy made beforehand keeps the password it was made with.
- Keep documents, drafts, source records, and change history encrypted at rest.
  Never log passwords, keys, full financial records, or sensitive query
  parameters. Do not commit real statements, account identifiers, databases,
  credentials, or private fixtures.
- Use synthetic or deliberately sanitized fixtures. The owner has private
  samples and can be asked for access. Do not record their paths, filenames, or
  contents, and do not move the originals.
- Treat imported files and regex patterns as untrusted input. Bound resource
  use. Distinguish invalid input and execution failure from a valid empty result.
- Do not disable integrity or durability to make a benchmark faster or a file
  layout simpler.
- Do not add an in-application backup command. After a successful close, a
  separate copy of the file is the backup. A copy of the open database file
  is not a consistent copy.
- Password changes, schema changes, and document switching need explicit
  failure behavior. Never destroy the last known-good copy on failure.
- Private local datasets use persistent empty locks under the platform
  application-data directory. Never delete a lock during a session or silently
  change namespaces. See [document locking](doc/document-locking.md).

## Implementation discipline

- Keep financial logic testable without the UI. UI validation does not replace
  domain and database validation.
- Prefer small changes with explicit invariants and error handling.
- Test failure paths that the change can hit: disk-full or I/O failure,
  interrupted operations, retries, malformed input, overflow, and inconsistent
  relationships. Verify atomicity on both sides of the durable-commit boundary.
- Test duplicate multiplicity, ambiguous dates, signed splits, transfers,
  currency boundaries, rule ordering, and report drill-down reconciliation where
  the change touches them.
- Use memory and undefined-behavior diagnostics, and parser fuzzing, when C or
  C++ work warrants them.
- Do not claim a platform, a command, or a safety property without a run you
  can point at. Record unavailable checks as unverified.
- Keep heavy work off the UI thread.
- Documentation-only work needs consistency and link checks, not invented
  runtime tests.
- Apply [user-facing language](doc/user-facing-language.md) to every UI string.
  Write the action the user can take and the consequence needed for a decision.
- When the owner asks to use diagnostics hooks, read
  [doc/agent-diagnostics-guide.md](doc/agent-diagnostics-guide.md) first. A
  request to use hooks pre-authorizes native macOS PID-targeted window capture
  and Accessibility inspection when ordinary UI attachment is unavailable.
  Keep the original process identity. Do not resolve the app by name in a way
  that can launch a second instance.

## Product behavior an agent must not regress

These are the current rules. The linked documents are the full contracts.

- **Undo.** One file-wide newest-first Undo/Redo stack. Redo is in memory only,
  is lost on crash, and replays fresh ordinary actions without schema changes.
  Import Undo is not redoable and clears pending Redo. History is a file-wide
  linear stack viewer, not selective branch Undo. The durable chain covers
  transaction edits, opening and currency corrections, and acceptance. An import
  is one source-owned rollback and creates no ordinary edit record for its new rows. Import-created accounts,
  categories, aliases, and the explicit opening remain after that rollback.
  Definition and settings saves are not Undo steps. They preserve Undo and
  clear pending Redo, except unused category/empty account deletion: preserve
  unrelated Undo/Redo, and require confirmation to atomically expire dependent
  history with deletion. Historical identities remain internal.
  Save and apply is atomic; Undo restores the transactions and keeps the saved
  rule. Older-import removal and account Discard require an empty chain and are
  not themselves undoable. Do not resume generic per-row storage capture.
  [Undo and Redo](doc/action-undo-design.md),
  [Change history](doc/action-undo-design.md#change-history).
- **Session close.** [Session history](doc/session-history-baseline.md) and
  [open/close checks](doc/fast-startup.md).
- **Import and acceptance.** Dataset-wide Accept. Explicit zero or reported
  opening on every new-account mapping. Same-account duplicate policy and
  strict bank IDs. [Acceptance and matching](doc/import-commit-discussion.md),
  [QIF accounts](doc/qif-account-imports.md), [import Undo](doc/import-undo.md),
  [QIF profile](doc/qif-profile.md),
  [statement profile](doc/statement-profile.md).
- **Review.** [Review sheet](doc/review-sheet.md).
- **Ledger editing.** [Ledger editing](doc/ledger-editing.md), including Set
  memo, Rename payee, Restore, splits, amount expressions, and amount formatting.
  [Search and selection](doc/search-ledger.md).
  [Manual transactions](doc/manual-transactions.md).
- **Rules.** Category-gated, Uncategorized only, no rule-created transfers.
  Matching uses immutable imported fields. Current-value matching is deferred.
  [rules contract](doc/rules.md),
  [evaluator](doc/rules-engine.md).
- **Categories, transfers, reports, file exchange.**
  [Categories](doc/categories.md), [transfers](doc/transfers.md),
  [reporting](doc/reporting.md), and [file exchange](doc/file-exchange.md).

## Licensing

SpentIt is released under the Unlicense. See [LICENSE](LICENSE). That
dedication applies to SpentIt. Third-party libraries and the currency reference
files keep their own terms. Do not relicense those components, and do not
replace the Unlicense without an explicit decision. See
[licensing requirements](doc/licensing.md).
