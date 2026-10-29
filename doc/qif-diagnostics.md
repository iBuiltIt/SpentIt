# Local QIF diagnostics

Last updated: 2026-10-28

Accepted scope: locate blocked split records without sharing financial values, then measure parsing separately from durable import processing. This does not relax the QIF grammar or infer repairs. The initial diagnostic addition did not change the format. The subsequent accepted split-order fix uses bank-v6 and dataset schemas 30/29; the standalone tool still reads QIF without creating a dataset.

## Running privately

From the repository root after a Release build:

```sh
./build/import-ui-rules-release/src/engine/spentit_qif_diagnose CAD dmy
```

Choose the currency code and `dmy` or `mdy` profile appropriate to the source. ISO dates are independent of that order. The supplied currency applies to every source account; for mixed-currency exports, money diagnostics are provisional until checked with each account's actual profile. Structural split diagnostics do not depend on currency.

Enter the file's path at the program's local prompt, without quotes. Do not put it in the command line, paste it into a chat, or redirect the original contents to a diagnostic log. The program opens the original read-only, reads at most 64 MiB into memory, and uses the application QIF parser. It does not create a dataset, copy the input, write a report file, use the network, or print the path, account/category names, financial values or exception contents. On macOS/Linux it disables process core dumps before reading. This is not a guarantee of secure memory erasure or control over operating-system swap/crash reporting or terminal recording. Existing application imports retain evidence in their dataset; running this standalone tool does not alter those datasets.

The output contains account indices, account-local transaction ordinals, original source line ranges, the offending line, fixed diagnostic reasons, whitelisted one-character field markers, counts and timings. Unknown markers become `?`; values are omitted. An embedded CR stays inside its field and does not increase the LF-based source line number. At most 100 diagnostics and 100 markers per displayed record are printed. Counts include undisplayed errors. Exit codes: 0 no parser diagnostics, 1 parser diagnostics found, 2 invocation/read/resource/execution failure. No partial result is permission to commit.

Only the resulting value-free report is intended for sharing. This permits authoring a synthetic regression case before considering any additional grammar support. The owner's actual file and its location have not been accessed or recorded.

## Application diagnostics

Blocked batch messages now identify original-file line ranges and the exact error line, including across account sections. Split reasons distinguish missing `$`, repeated `S`, repeated `E`, parent/unsupported fields after splits, and conflicting parent `L`. The first-error-per-record policy remains; subsequent errors may also exist. Selected-row diagnostics use the same specific reason. These additions do not repair, skip, publish or discard records.

## Synthetic verification and measurement

The engine regression tests exercise all current split reasons, CRLF and embedded CR, whole-file offsets after account headers, and value-free output. `python3 tests/app/qif_diagnostic_tests.py <diagnostic executable>` checks the actual local prompt/read path, unchanged source bytes, absence of output files, and sanitized open/profile errors using exclusively synthetic inputs.

`spentit_qif_diagnose CAD dmy --synthetic` generates 116,621 lines / 19,436 transactions in memory with four deliberately invalid split structures. It tests the same line count, not the unknown structure of the owner's file. `spentit_account_tests group_benchmark` measures capture, materialization with returned group status, repeated group-status retrieval, and Abort on that synthetic workload in a temporary encrypted dataset. It does not commit financial records. Parsing alone is not an end-to-end responsiveness measurement. See [status](status.md) for actual results and remaining limitations.
