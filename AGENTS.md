# SpentIt — Where did my money go?

SpentIt is a local, single-user desktop application for understanding spending through reliable imports, reviewable categorization, and explainable reports. Data safety, security, and correctness take priority over speed of development. Losing or silently misrepresenting financial data loses the user's trust.

## Discover the project before changing it

1. Read this file and `doc/status.md`.
2. Read `doc/project-layout.md`, `features/README.md`, and the feature documents relevant to the task.
3. Inspect existing implementation and tests when they exist. Documentation is not evidence that code exists or works.
4. Identify accepted requirements, provisional designs, and open questions before making changes. Ask about material ambiguities; do not invent financial or security behavior.

## Where information belongs

- `AGENTS.md`: durable contributor instructions, discovery, and engineering discipline. Keep detailed feature designs elsewhere.
- `doc/`: actual project status, verified implementation facts, architecture decisions once adopted, and project-wide verification/recovery guidance. Keep status current when work changes it.
- `features/`: feature requirements, design proposals, acceptance criteria, unresolved questions, and feature-specific implementation/verification notes. Start at its index.
- `src/`: first-party implementation, separated into `engine/`, `storage/`, `app/`, and `ui/` as described in `doc/project-layout.md`.
- `tests/`: automated tests and synthetic fixtures; `cmake/`: build support; `third_party/`: dependency metadata and deliberately vendored code only.
- Create implementation directories when they receive real content. Keep generated builds and dependency downloads out of the source tree and version control.

Use relative links between repository documents. Do not rely on chat history, private attachment paths, or a particular developer's machine for essential requirements. Preserve meaningful decisions and rationale. Do not copy personal financial records into the repository.

## Status and evidence

- Use the feature lifecycle in `features/README.md`: `active`, `landed`, or `abandoned`.
- Lifecycle is separate from decision certainty. Label accepted requirements, proposed implementation, and unresolved decisions explicitly.
- `active` does not mean implemented. Mark a feature `landed` only when its stated scope is implemented and the required verification is recorded. Split out deferred scope rather than concealing it.
- Record what was tested, how, and the actual result. Never claim a command ran, a platform passed, or a safety property was verified without evidence.
- On incomplete work, state what remains and any limitations. Do not describe scaffolding or happy-path tests as complete financial correctness.
- Update a feature document's last-updated date only when its content changes meaningfully; use ISO dates.

## Non-negotiable data invariants

- Use exact monetary arithmetic with explicit currency and scale. Never use binary floating point for authoritative money values; detect overflow and invalid precision.
- Splits sum exactly to their effective transaction total. Mixed signs are supported. Original imported amounts remain available when a user corrects a total.
- Never combine different currencies without an explicit conversion policy and recorded conversion inputs.
- Preserve immutable import evidence separately from editable transaction interpretations and rule results.
- Draft imports persist across restarts but do not alter committed balances or ordinary reports.
- Commit the entire chosen operation atomically, including linked transfers, draft-state transitions, and history. Commit is explicit per account: protecting a linked destination draft does not commit it (see [transfer lifecycle](features/008-transfer-matching-and-lifecycle.md)). Never report success before durable completion.
- Preserve persistent history for committed changes. Reversal must respect later changes and financial relationships; it must not silently overwrite newer work.
- Duplicate recognition must preserve evidence and legitimate repeated occurrences. An uncertain match is not permission to drop a transaction silently.
- Reporting totals must reconcile to the exact allocations and filters shown by drill-down.
- No silent repair, rounding, truncation, skipped records, fallback to plaintext, or destructive migration. Explain and surface errors.

## Security and recovery discipline

Pre-release format policy (owner decision, 2026-10-26): no datasets have been released or exist "in the wild." Current experimental formats are disposable; format changes need no migration, upgrade path, or backward-compatibility work. The owner will explicitly announce the first release, which is the gate for introducing those obligations. Do not add compatibility work solely to preserve earlier experimental schemas. Current-format atomicity, financial correctness, encryption, and failure/recovery verification remain required. Migration guidance below applies when migrations become relevant after that release gate.

- Use established cryptographic libraries and supported interfaces. Do not invent encryption or password derivation schemes.
- Optional no-password mode is an explicit owner-approved waiver of confidentiality, using a documented public format key recoverable without the original installation. Do not describe it as confidential or silently apply it to password-protected data; see [document setup and recovery](features/001-documents-storage-security.md#accepted-setup-unlock-and-recovery-behavior--2026-10-25).
- Keep documents, drafts, source records, and change history encrypted at rest. Review journals, temporary storage, logs, diagnostics, exports, and backups for leakage.
- Never log passwords, keys, full financial records, or sensitive query parameters. Do not commit real statements, account identifiers, databases, credentials, or private fixtures.
- Use synthetic or deliberately sanitized fixtures that preserve the relevant edge cases.
- The owner has private QIF samples and matching QFX/ASO/QBO/OFX exports of one real-world dataset and can be asked for access when needed. Do not record their paths, filenames or contents, move the originals, or commit them. Prefer synthetic structural fixtures; private samples are not routine automated test inputs. See [sample handling](features/009-durable-import-milestone.md#private-sample-availability).
- Treat imported files and regex patterns as untrusted input. Bound resource consumption; distinguish invalid input and execution failure from a valid empty result.
- Do not disable integrity or durability mechanisms to make benchmarks faster or the file layout simpler.
- Test backups by restoring them. Do not assume a live copy of a database's main file is a valid backup.
- Password changes, schema migrations, and document switching must have explicit failure/recovery behavior. Never destroy the last known-good copy on failure.
- The initial product has no password-reset mechanism. Do not imply forgotten-password recovery exists.

## Implementation and verification

- Keep financial logic independently testable from UI presentation. UI validation does not replace domain/database validation.
- Enforce the dependency boundaries in `doc/project-layout.md` through build targets when implementation begins. UI code must not execute SQL or own financial transaction boundaries; the engine must not depend on UI or concrete storage.
- Prefer small, reviewable changes with explicit invariants and error handling. Do not rewrite unrelated working code.
- Test relevant failure paths as part of financial/storage work: disk-full or I/O failure, interrupted operations, retries, malformed input, overflow, and inconsistent relationships.
- Verify atomicity and retry behavior around commit. Test interruptions on both sides of the durable-commit boundary.
- Test duplicate multiplicity, ambiguous dates, signed splits, transfers, currency boundaries, rule ordering/resource limits, and report drill-down reconciliation where relevant.
- Use appropriate memory/undefined-behavior diagnostics and parser fuzzing when C/C++ implementation warrants them.
- Validate supported macOS, Linux, and Windows builds and interactions before claiming portability. Record unavailable platform checks as unverified.
- Keep heavy work off the UI thread. Measure representative workloads before claiming responsiveness.
- Run checks appropriate to the change. Documentation-only work needs consistency/link checks, not invented runtime tests.

## Scope and licensing

The implemented milestones are a standalone C++ financial engine, CMake build/test setup, a pinned currency catalog, engine split editing, retained historical-currency definitions, an optional synthetic-data SQLCipher storage/recovery prototype, an optional synthetic Qt Quick transaction-sheet experiment, and a partial durable QIF workflow with ordinary committed corrections/removal/Undo, category management, a separately testable rules evaluator/preview adapter, schema-6 rule Save/Apply for rules-enabled import documents, and a rules sheet on that import worker. The schema-6/rules-sheet baseline passed all 72 tests in Release and ASan/UBSan on macOS. The durable UI now includes combined search, stable multiselection, and schema-7 atomic committed bulk category/removal/group Undo. The durable workflow now also has account-scoped persistence, independent account workspaces and guarded UI switching and a bounded same-currency transfer workflow with explicit per-account commit, protection, provisional bank matching and navigation (experimental schema 12 with rules, 13 without). QIF whole-record/split transfer destinations now require explicit resolution through that workflow. Full transfer repair/reversal, reports, and the broader transaction-sheet design remain incomplete. See `doc/status.md` for verified scope. Further implementation must follow the requested scope; this guidance does not authorize publication or selection of a final license.

The owner wants public GitHub availability and permanent free access: selling SpentIt or its derivatives is prohibited, while businesses may use it internally for free. **The earlier Unlicense preference is superseded; exact licensing terms remain unresolved.** Preserve this distinction rather than imposing a blanket ban on commercial use. See `features/007-licensing-and-distribution.md`. Do not add an Unlicense, GPL, or other license without an explicit decision. Do not represent source visibility as open-source licensing or assume third-party libraries inherit our license.
