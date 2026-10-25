# ISO 4217 currency reference data

`list-one.xml` is the unmodified SIX List One snapshot, published **2026-09-17**, retrieved **2026-10-25**.

- [SIX maintenance agency and current lists](https://www.six-group.com/en/products-services/financial-information/market-reference-data/data-standards.html)
- [Original XML download](https://www.six-group.com/dam/download/financial-information/data-center/iso-currrency/lists/list-one.xml)
- SHA-256: `33139b438657d1cee116ba737807ea71d19d6de4b90f799a09c56f0cc6a1b0ff`

This is reference data, not a linked software dependency. Attribution and provenance are retained here; no SpentIt license is assigned to upstream material. Distribution review must include upstream data terms as well as software dependencies.

The owner selected all current ISO 4217 currencies with defined minor-unit scales. We operationalize that as every distinct code in List One with a numeric minor-unit field, including the fund/accounting codes in that list. Duplicate country entries collapse to one code only when their scales agree. The snapshot yields 165 codes, with scales 0, 2, 3, and 4. Entries with `N.A.` scales (including precious metals, testing, and no-currency codes) are excluded. Historical codes are not added from List Three. This is a pinned current-currency policy, not historical-statement coverage or cash-rounding policy.

The offline generator extracts only code/scale facts to `src/engine/currency_catalog.inc`:

```sh
python3 tools/generate_currency_catalog.py
python3 tools/generate_currency_catalog.py --check
```

Python 3 is only needed for catalog maintenance/checking, not normal builds. No build or application operation downloads data.

To update: obtain a new authoritative snapshot, review effective dates and code/scale changes, update this provenance and hash, regenerate, review the diff, update tests, and run all engine checks. Changes must be deliberate; never silently reinterpret an existing amount. Use the retained-definition policy below when refreshing. An upstream catalog refresh alone is not a migration policy.

## Retained amount definitions

`amount-definitions.json` is an append-only reviewed registry. `current-definitions.json` maps current account choices to retained identities. Neither a withdrawal nor catalog refresh may remove or reinterpret a retained identity. Reuse an identity for an unchanged definition; add a distinct identity for a changed/reused definition. Historical lookup requires an explicit identity; this milestone does not infer dates or accept caller-supplied scales. This avoids treating current minor units as universally valid across a code's entire history.

The original snapshot is separately preserved as `list-one-2026-09-17.xml`. The 165 snapshot definitions and two historical definitions are supported. SIX List Three (published 2026-01-01) supplies withdrawal evidence but **no scales**. The BGN and HRK scale-2 definitions were checked against the corresponding SIX amendments, retained here. No other historical scale is inferred.

Retrieved 2026-10-25:

- [list-three.xml](https://www.six-group.com/dam/download/financial-information/data-center/iso-currrency/lists/list-three.xml): SHA-256 `98fde2423cdb916dd59dcf5fe96222edad8fa198d865c1c83dbc464b9cc52387`.
- [amendment-174.pdf](https://www.six-group.com/dam/download/financial-information/data-center/iso-currrency/amendments/dl-currency-iso-amendment-174.pdf): SHA-256 `bf19b1aa9908a1368fdef34151454ecfe664ce83b50a4c6e75bbdf8dd7f64e80`.
- [amendment-180.pdf](https://www.six-group.com/dam/download/financial-information/data-center/iso-currrency/amendments/dl-currency-iso-amendment-180.pdf): SHA-256 `8b2a8a146b2dbaa4d95734b3615ef71db7c4642e75f13e38197101f082f5a8a3`.

Run the generator check and `python3 tests/tools/currency_refresh.py` after catalog changes. The latter builds an isolated simulated withdrawal and verifies retained amount readability. Source references and identifiers are data provenance, not a selection of project licensing terms.
