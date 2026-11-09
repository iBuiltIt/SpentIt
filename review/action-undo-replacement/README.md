# Replacement Undo measurements

Synthetic measurements taken on macOS, 2026-11-09. This is the replacement
implementation, not the archived generic row-capture experiment.

`measurements.json` records driver output, elapsed wall time, sampled temporary
journal size and (where available) simultaneous main-file/journal size. CSV files
contain SQLCipher DBSTAT totals by table/index: pages, allocated bytes, payload and
unused bytes. They include indexes rather than counting only application rows.

The identical driver is `tests/storage/session_storage_measure.cpp`. Build target:
`spentit_session_storage_measure`, with rules/storage/import workflow enabled.
Invocation: `spentit_session_storage_measure OUTPUT_DIRECTORY MODE ROW_COUNT`.
`measure.py BINARY OUTPUT_DIRECTORY MODE ROW_COUNT` adds sampled peaks and elapsed
time. Use a fresh output directory per invocation. Modes are `import`, `edits`, `undo`,
`linked`, and `accumulated`; accumulated uses 19,000 initial rows and four later
25-row sessions. The 50,000-row probe imports only, then closes and reopens.

Comparators are unmodified production sources at Git revisions `cf8f1a5` and
`b4b65df`, with only this same measurement driver copied into each isolated build.
The replacement uses experimental formats 70/69. All files are encrypted synthetic
fixtures. No private data, VACUUM, database padding, or weakened durability is used.

The sampler checks database, rollback journal, WAL and SHM file lengths every 2 ms.
These are sampled lower bounds on high-water usage, not exact VFS event traces.
Latencies include full verification and concurrent test/build load; they are not
isolated responsiveness claims. DBSTAT is collected with a separate read-only
SQLCipher diagnostic compiled with `SQLITE_ENABLE_DBSTAT_VTAB`; production storage
settings are unchanged.

Small workloads expose the fixed schema/coordinator cost compared with b4b65df:
replacement live pages are 40,960 bytes higher for repeated edits and ordinary
source removal, and 28,672 bytes higher for linked source removal. Retained payload
is lower for both removal cases and 897 bytes higher for repeated edits. All three
remain below cf8f1a5 in physical and retained payload size. The 19,000-row and
50,000-row workloads are below both comparators in retained physical/payload size.
The accumulated temporary peak exceeds cf8f1a5 but is below b4b65df; close-time
pruning already accounts for that difference. No additional growth allowance is
assumed.
