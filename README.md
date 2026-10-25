# SpentIt — Where did my money go?

A local, single-user desktop application in development for understanding spending through reliable imports, reviewable categorization, and explainable reports.

Currently implemented: an exact-money C++20 engine, an optional synthetic SQLCipher storage/recovery prototype, and an optional synthetic Qt Quick transaction-sheet prototype. **There is no production financial-document application yet.** Prototype data is synthetic; sheet edits last only for the running session.

## Start here

- [Project status and verified scope](doc/status.md)
- [Engine build and tests](doc/building.md)
- [Transaction-sheet prototype: build, run, and measurements](doc/sheet-prototype.md)
- [Encrypted-storage prototype](doc/storage-prototype.md)
- [Feature requirements and open decisions](features/README.md)
- [Contributor instructions](AGENTS.md) and [dependency boundaries](doc/project-layout.md)

## Engine quick start

Requires CMake 3.21+, Ninja, and a C++20 compiler. Engine-only builds need neither Qt nor SQLCipher.

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

See the linked prototype instructions for optional targets and additional dependencies. macOS verification is recorded in the project documents; Linux and Windows remain unverified.

## Repository conventions

Track source, tests, documentation, dependency metadata/notices and deliberately pinned reference data. Build output, downloaded dependencies, local editor settings, credentials, runtime databases and bank exports do not belong in Git. `.gitignore` provides common exclusions; it cannot recognize every form of sensitive data. Review staged files before committing. Fixtures must be synthetic or deliberately sanitized.

`.gitattributes` normalizes ordinary text to LF, identifies binary assets, and preserves byte-exact upstream currency XML used by hash checks. `.editorconfig` supplies editing defaults without rewriting existing files. Markdown hard line breaks remain supported.

## Licensing

No project license has been selected. The owner's intent is permanent free access, no sale of SpentIt or its derivatives, and free internal business use; exact terms remain unresolved. These statements are not an operative license. See [licensing and distribution](features/007-licensing-and-distribution.md). Third-party material retains its own terms and notices.
