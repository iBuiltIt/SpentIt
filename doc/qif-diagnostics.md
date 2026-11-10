# Local QIF diagnostics

Last updated: 2026-11-09

A read-only diagnostic shares the engine parser. It does not create a file and
does not relax the grammar. It suppresses values and paths.

From the repository root after a Release build:

```sh
./build/import-ui-rules-release/src/engine/spentit_qif_diagnose CAD dmy
```

Choose the currency code and `dmy` or `mdy`. ISO dates do not use that order.
For a mixed-currency file, money diagnostics are provisional until checked with
each account's currency. Structural split diagnostics do not depend on currency.

Do not record private paths, filenames, or field values.
