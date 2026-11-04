# SpentIt — Where did my money go?

Current workflow: Import is above Review in the sidebar and in account context
menus; Review contains Commit and import repairs. Settings is in the lower-left
sidebar only. Interrupted preparation is cleaned up atomically on cancellation or
next open; completed uncommitted imports persist. See the [UI/recovery contract](doc/ui-import-cleanup.md)
and [verification record](doc/status.md).

A local, single-user desktop application in development for understanding spending through reliable imports, reviewable categorization, and explainable reports.

SpentIt is the application being developed and used: it persists documents, imports, drafts, committed changes, and history. It supports manual transaction entry, multiple accounts, QIF and OFX-family statements, transfers, categories, rules, search, bulk editing, and Undo. Current work improves its usability piecemeal. Reports, expanded document lifecycle/recovery, and public distribution are deferred. Local application packaging is described in [packaging instructions](packaging/README.md).

This is an unreleased application. Format compatibility remains governed by the pre-release policy; verification limits and unfinished features are recorded in [project status](doc/status.md). The separate synthetic sheet and storage prototypes are development harnesses, not the application to launch.

## Start here

- [Project status and verified scope](doc/status.md)
- [Engine build and tests](doc/building.md)
- [Transaction-sheet prototype: build, run, and measurements](doc/sheet-prototype.md)
- [Encrypted-storage prototype](doc/storage-prototype.md)
- [Durable QIF workflow](doc/durable-import.md) and [setup/review UI](doc/import-ui.md)
- [Account-scoped workflow and current experimental formats](doc/accounts.md)
- [QIF transfer destinations and explicit resolution](doc/qif-transfers.md)
- [Search and bulk ledger contract](doc/search-ledger.md)
- [Current import/commit handoff](doc/import-commit-discussion.md)
- [Feature requirements and open decisions](features/README.md)
- [Contributor instructions](AGENTS.md) and [dependency boundaries](doc/project-layout.md)

## Run the application

For the existing rules-enabled Release build:

```sh
cmake -S . -B build/import-ui-rules-release -DSPENTIT_APPLICATION_OUTPUT_DIRECTORY="$PWD/build"
cmake --build build/import-ui-rules-release --target spentit_import_experiment -j 2
./build/SpentIt
```

The current build uses experimental schema **50 with rules / 49 without**. Create a new dataset for this version; older experimental datasets are rejected unchanged, without migration. The [unified Review sheet](doc/review-sheet.md) groups outstanding questions by account and is available from Review and Commit. Imports can accumulate before Commit. Commit accepts new transactions across all accounts; Discard uncommitted work removes the selected account's contributions while preserving accepted and independently owned transactions. See [verified scope](doc/status.md#dataset-acceptance-storage-refactor--2026-10-29).

The output location is saved in that build's CMake cache; subsequent rebuilds update `./build/SpentIt`. For dependency setup on a fresh checkout, see [building](doc/building.md) and [the UI setup](doc/import-ui.md). Other build trees default to producing `SpentIt` inside their own build directory.

## Engine quick start

Requires CMake 3.21+, Ninja, and a C++20 compiler. Engine-only builds need neither Qt nor SQLCipher.

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

See the linked prototype instructions for optional targets and additional dependencies. macOS verification is recorded in the project documents. Linux and Windows developer builds are described in [Linux development setup](doc/linux-development-setup.md) and [Windows development setup](doc/windows-development-setup.md). What those hosts actually ran is in [project status](doc/status.md).

## Repository conventions

Track source, tests, documentation, dependency metadata/notices and deliberately pinned reference data. Build output, downloaded dependencies, local editor settings, credentials, runtime databases and bank exports do not belong in Git. `.gitignore` provides common exclusions; it cannot recognize every form of sensitive data. Review staged files before committing. Fixtures must be synthetic or deliberately sanitized.

`.gitattributes` normalizes ordinary text to LF, identifies binary assets, and preserves byte-exact upstream currency XML used by hash checks. `.editorconfig` supplies editing defaults without rewriting existing files. Markdown hard line breaks remain supported.

## Licensing

No project license has been selected. The owner's intent is permanent free access, no sale of SpentIt or its derivatives, and free internal business use; exact terms remain unresolved. These statements are not an operative license. See [licensing and distribution](features/007-licensing-and-distribution.md). Third-party material retains its own terms and notices.
