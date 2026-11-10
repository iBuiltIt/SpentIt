# SpentIt — Where did my money go?

A local, single-user desktop application for understanding spending through reliable imports, reviewable categorization, and explainable reports.

SpentIt persists encrypted files, imports, new and accepted transactions, and session Undo. It supports manual entry, several accounts, QIF and OFX-family statements, transfers, categories, rules, search, reports, and file exchange. Public distribution is gated. Local packaging is in [packaging/README.md](packaging/README.md).

This is an unreleased application. Older experimental files are rejected with no migration. Open work is in [project status](doc/status.md).

## Start here

- [Contributor instructions](AGENTS.md)
- [Current status](doc/status.md)
- [Build](doc/building.md)
- [Feature requirements](features/README.md)
- [Dependency boundaries](doc/project-layout.md)

## Run the application

For the existing rules-enabled Release build:

```sh
cmake -S . -B build/import-ui-rules-release -DSPENTIT_APPLICATION_OUTPUT_DIRECTORY="$PWD/build"
cmake --build build/import-ui-rules-release --target spentit_import_experiment -j 2
./build/SpentIt
```

The current build uses experimental schema **70 with rules / 69 without**. Create a new file for this version. Older experimental files are rejected unchanged, without migration. Review groups outstanding questions by account. Imports can accumulate before Accept transactions. Accept applies across the file. Discard removes the selected account's exclusively owned uncommitted work and preserves accepted transactions and other sources.

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

SpentIt is released under the Unlicense. See [LICENSE](LICENSE). Third-party
components keep their own licenses, which are named in that file and shown in
About.
