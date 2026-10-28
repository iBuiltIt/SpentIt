# OFX, QFX, QBO, and ASO statement profile

Last updated: 2026-10-27

Implementation contract for the bank and credit-card statement slice of [feature 003](003-qif-import-review.md). Feature 003 stays **active**. This profile does not finish QIF product scope or transfer repair. New documents use experimental schema 22 with rules and schema 21 without rules. Schema 20 and schemas 18 and 19 are rejected with `Unsupported experimental format; no migration was attempted`. No migration was added. `aso` remains a legal file label in those format checks. Account commands are recorded in [feature 002](002-accounts-money-splits.md#applied-account-management-checkpoint--2026-10-27).

## Specification used

Statement behavior was checked against the body of the **OFX Banking Specification, Version 2.3, October 2020** (title page: OFX Banking Specification, adopted by the Financial Data Exchange OFX Work Group). The work-group index is [financialdataexchange.org/about-fdx/ofx-work-group](https://financialdataexchange.org/about-fdx/ofx-work-group/). It lists Banking 2.3 and the Banking, Credit Card, Investment, Securities, Bill Pay, Profile, and Signon message sets.

Sections whose statement bodies were read: 11.4.2 Bank Statement Download, 11.4.3 Credit Card Statement Download, and 11.4.4 `STMTTRN` / `STMTTRNP`, including the `TRNTYPE` list. The table of contents was used for 2.2, 2.2.1, 2.8.1, 2.8.2, 3.2.3 `FITID`, 3.2.5 `TRNAMT`, 3.2.6 `MEMO`, 3.2.7 `DTSTART`/`DTEND`, 3.2.8 common data types, 5.2 currency, 11.3.1 bank accounts, and 11.3.2 credit-card accounts. The common-data-type and file-header subsection bodies were not re-read. Header admission below is the checkpoint implementation of the 1.x SGML and 2.x XML split, recorded as such. Section numbers from other OFX write-ups are not used. The consulted PDF is outside the repository and is not a project file.

Public descriptions treat OFX as the statement grammar, QFX as Quicken Web Connect labeling of that grammar, and QBO as QuickBooks Web Connect labeling. Version 2.3 does not define separate QFX or QBO grammars. SpentIt stores the user file label (`ofx`, `qfx`, `qbo`, or `aso`) separately from the grammar (`ofx-sgml` or `ofx-xml`). Content does not distinguish the `ofx`, `qfx`, and `qbo` labels. An empty label that sniffs as OFX is stored as `ofx`. A `.aso` name or an explicit `aso` label stores format `aso` and then uses the SGML or XML parser the body requires.

## ASO

ASO for this checkpoint was identified by in-place structural inspection of the owner's private export on 2026-10-27. That export is OFX 1.0.2 SGML credit-card statement syntax with a `.aso` label. No path, filename, account identifier, amount, name, FITID, or other content was recorded. The handling rules in [sample handling](009-durable-import-milestone.md#private-sample-availability) still apply.

A well-formed file of this shape imports, reviews, and commits through the existing `ofx-sgml-1` parser. The stored format is `aso` and the stored grammar is `ofx-sgml`. An ASO file whose body is OFX XML uses `ofx-xml-2` and grammar `ofx-xml`. A body that is neither OFX SGML nor OFX XML is refused with `File label does not match the statement grammar; no import was saved`. The original file stays unchanged and no partial import is saved. Sniffing an unlabeled body still follows the existing OFX/QIF rules and does not invent an `aso` label.

The inspected shape is CRLF, unindented, and ASCII. It has a nine-field `KEY:VALUE` header, a blank line, then SGML. Accepted header values for this shape are `OFXHEADER:100`, `DATA:OFXSGML`, `VERSION:102`, `SECURITY:TYPE1`, `ENCODING:USASCII`, `CHARSET:1252`, `COMPRESSION:NONE`, `OLDFILEUID:NONE`, and `NEWFILEUID:NONE`. `SECURITY` may be empty, `NONE`, or `TYPE1` on both SGML and XML headers. `TYPE1` is a header declaration on a readable body; SpentIt does not decrypt it. Any other `SECURITY` value, and `COMPRESSION` other than empty or `NONE`, still refuse with `Encrypted or compressed OFX is not supported; no import was saved`. Declared charset 1252 is accepted when the body is valid UTF-8, including an ASCII body. A body that is not valid UTF-8 is refused before capture. Windows-1252 is not guessed. An Intuit `INTU.BID` header is not required.

Leaf tags in this shape are unclosed. Aggregates are closed. The message path is `OFX` / `CREDITCARDMSGSRSV1` / `CCSTMTTRNRS` / `CCSTMTRS`, with one `CCACCTFROM`, then `BANKTRANLIST`, then `LEDGERBAL` and `AVAILBAL`. Signon is `SIGNONMSGSRSV1` / `SONRS` with `STATUS` (`CODE`, `SEVERITY`, `MESSAGE`), `DTSERVER`, and `LANGUAGE`. `USERKEY`, when a file contains it, stays only inside the immutable original bytes. It is not copied into header notes, occurrence fields, uninterpreted text, diagnostics, logs, or a separate column. The synthetic fixture omits `USERKEY`.

`DTSTART` may be `YYYYMMDD`. `DTEND`, `DTPOSTED`, `DTASOF`, and `DTSERVER` may be 14 digits plus an OFX timezone bracket `[offset:ZONE]`. The calendar date remains the first eight digits. The time and zone stay in `time_evidence`. The calendar date is not shifted. A `STMTTRN` in this shape may contain only `TRNTYPE`, `DTPOSTED`, `TRNAMT`, `FITID`, and `NAME`, in that order. `MEMO` and check number stay optional. `FITID` is an opaque non-empty string. `DEBIT` and `CREDIT` are admitted, and the rest of the existing OFX transaction-type enum stays admitted. Amounts have an optional leading minus, a decimal point, and two fractional digits, with no grouping separators. `CURDEF` must still match the target account. There is no conversion.

The synthetic fixture is `tests/fixtures/statements/card-sgml.aso`. It uses the header above, a fake account id, no `USERKEY`, one debit and one credit, timezone-bracket dates, opaque FITIDs, and ledger and available balances. `.gitattributes` marks that file `-text` so the CRLF bytes stay exact.

## Admitted messages

Parsers: `ofx-sgml-1` and `ofx-xml-2`. The QIF parser remains `bank-v3`.

Admitted top-level message sets are `SIGNONMSGSRSV1`, `BANKMSGSRSV1`, and `CREDITCARDMSGSRSV1`. Any other top-level set, including investment, securities, bill pay, and profile, refuses the file before capture with `Unsupported OFX message set; no transactions were imported`. A bank statement that also contains another message set refuses the whole file. No partial financial import is saved.

Bank download is `STMTTRNRQ`/`STMTTRNRS` with `STMTRS`: `CURDEF`, `BANKACCTFROM`, `BANKTRANLIST` of `STMTTRN`, optional `LEDGERBAL`/`AVAILBAL`/`MKTGINFO`. Credit-card download is `CCSTMTTRNRQ`/`CCSTMTTRNRS` with `CCSTMTRS`. Version 2.3 places `STMTTRN` inside the credit-card `BANKTRANLIST`. SpentIt also accepts a `CCSTMTTRN` aggregate with the same transaction fields. That tag is not an official 2.3 name; it is admitted so a file that uses it remains reviewable instead of being dropped.

`BANKTRANLIST` children are `DTSTART`, `DTEND`, `STMTTRN`, and `CCSTMTTRN`. `DTSTART` and `DTEND` are ignored. Pending `BANKTRANLISTP` / `STMTTRNP` are not admitted. Their presence refuses the file before capture as `Malformed OFX; no import was saved`. Pending lines are not silently omitted. Version 2.3 says pending transactions have no `FITID` and must not be stored for reconciliation; SpentIt does not invent a pending-posting policy.

These response children also refuse the whole file before capture, because interpreting or dropping them would choose unstated financial meaning: `STMTRS` cash-advance balance and interest rate; `CCSTMTRS` cash-advance balance, available cash advance, cash-advance credit limit, purchase/cash/transfer interest, and reward information from section 11.4.3. Any statement child outside the admitted set is malformed. `BALLIST` and any other aggregate whose name contains `BAL`, other than `LEDGERBAL`, `AVAILBAL`, and `BALAMT`, suppresses a staged reported balance and does not by itself refuse the file. `AVAILBAL` is not the ledger balance.

A statement `STATUS` `CODE` other than `0` refuses the file with `Statement status is not success; no import was saved`. Missing statement status is malformed. A non-zero signon code with at least one statement is recorded as `SIGNON status N` and parsing continues. Signon failure with no statement refuses with `Signon failed and no statement was included; no import was saved`. No statement and no signon failure refuses with `No bank or credit-card statement was found`. A successful empty `BANKTRANLIST` is one account with zero records. An empty result is not success when the file is malformed.

## Header, encoding, and grammar

OFX 1.x SGML requires `OFXHEADER:100`, `DATA:OFXSGML`, and a non-empty `VERSION`. OFX 2.x XML requires `<?OFX OFXHEADER="200" VERSION="..." ...?>`, optionally after `<?xml ...?>`. The header and the body grammar must agree. A bare `<OFX>` without that header is malformed. An unrecognized `VERSION` stays in the stored header and does not drop transactions when the admitted structure parses. `SECURITY` may be absent, `NONE`, or `TYPE1`. `COMPRESSION` must be absent or `NONE`. Any other value refuses with `Encrypted or compressed OFX is not supported; no import was saved`. `TYPE1` does not select a decryption step. The header is capped at 64 KiB. The exact header bytes, including Intuit fields such as an institution id, are source evidence. No live bank-id license check is performed. An unrecognized institution id is not a reason to drop transactions.

The body must be valid UTF-8. An ASCII-only body is accepted even when a header names another charset. A body that is not valid UTF-8 refuses the file before capture. Windows-1252 is not guessed. SGML values are literal. XML decodes the five predefined entities and numeric character references. `<!` declarations, including `DOCTYPE` and `CDATA`, and processing instructions in the body are malformed. Comments are ignored. Mixed content is refused. Self-closing empty elements are allowed.

SGML uses a bounded aggregate list: known statement aggregates, plus names ending in `MSGSRSV1` or `MSGSRQV1`, parse until their closing tag. Any other tag is a leaf whose value runs to the end of the line or the next `<`, so compact SGML and line-oriented SGML both parse. An empty leaf is an empty value. Depth is capped at 32. Each finished `STMTTRN` or `CCSTMTTRN` becomes one record; the parser does not keep a full document tree. A leaf close tag such as `</BAL>` inside SGML is malformed. `BALLIST` in SGML therefore uses leaf syntax inside the aggregate.

A requested label of `qif` on OFX content, or `ofx`/`qfx`/`qbo`/`aso` on content that is neither OFX SGML nor OFX XML, refuses with `File label does not match the statement grammar; no import was saved`.

## Dates, amounts, and currency

The calendar date is the first eight `YYYYMMDD` digits of `DTPOSTED` when they are a real Gregorian date. A general OFX timezone bracket after the time stays in the remainder. There is no timezone shift and no DMY/MDY guess. Any remaining time text, including the zone, is `time_evidence` only. `DTSTART` may be eight digits. `DTEND`, `DTASOF`, and `DTSERVER` may carry the same time and zone form and do not change the posted calendar date. A missing or impossible date is `Issue::date`. It is repairable only when the rest of the transaction structure is intact. The stored group profile for an OFX-family file is DMY so a later MDY retry cannot alias the same bytes; the profile does not reinterpret the year-first date.

`TRNAMT` is strict decimal text in the statement currency scale, through the existing money type. The sign is the customer's perspective, as in version 2.3. `TRNTYPE` does not change the sign. A missing or invalid amount stays an unavailable original. Zero is not fabricated. A structurally intact amount or date problem is repairable. A normal statement transaction with no split breakdown is one allocation. Splits are not invented. `LOANPMTINFO` is not interpreted; its presence is uninterpreted evidence and a non-repairable financial blocker.

`CURDEF` must be a supported catalog currency. A missing or unsupported code refuses the file with `Statement currency is missing or unsupported; no conversion was applied`. Two different `CURDEF` values in one statement refuse the file as malformed before capture. At prepare time, `CURDEF` must match the mapped or created target account. A mismatch refuses the whole prepare with `Statement currency CODE does not match the target account; no conversion was applied` and saves nothing. There is no conversion policy.

A transaction `CURRENCY` that differs from `CURDEF` blocks that record with `currency_mismatch`. `TRNAMT` is kept as uninterpreted text and the original amount stays unavailable. `ORIGCURRENCY` blocks the record. `TRNAMT` is not treated as an already converted account amount.

## Fields that block a record

These stay in evidence and block the record, so the account cannot be committed until the row is removed from the included set by an explicit review decision. They are not dropped as harmless and they are not repairable:

- `CORRECTFITID` or `CORRECTACTION` (corrections are not applied)
- `INV401KSOURCE`, `IMAGEDATA`
- unknown `TRNTYPE`, missing `TRNTYPE`, and `HOLD`
- `TRNTYPE` `XFER`, `BANKACCTTO`, or `CCACCTTO` (`Issue::transfer`)
- conflicting repeated values of the same field
- `NAME` and `PAYEE` both present and different
- `CHECKNUM` and `REFNUM` both present and different
- any other unknown `STMTTRN` child, including a loan-payment breakdown

`NAME` wins over `PAYEE` when only one is used. `CHECKNUM` wins over `REFNUM` when only one is used. `SIC`, `PAYEEID`, `SRVRTID`, `DTUSER`, `DTAVAIL`, and `EXTDNAME` are uninterpreted and do not block. A missing `FITID` records `FITID absent` and does not by itself block a structurally intact transaction. Version 2.3 says each posted `STMTTRN` contains a `FITID`; this checkpoint still accepts a missing `FITID` as reviewable evidence.

Statement transfers cannot use the QIF bracket-destination workflow. They remain visible transfer blockers with the original bytes retained. No destination account is created or resolved by name. Allocation-level Merge, unlink, replacement, and the locked committed link edits are implemented for the transfer workflow in [feature 008](008-transfer-matching-and-lifecycle.md#accepted-committed-transfer-editing--2026-10-27). Moving a committed row and rewriting source-occurrence links remain open.

Reported balance is `LEDGERBAL`/`BALAMT` only, and only when that text parses exactly for the statement currency. `DTASOF` is not an opening date. Extra balance aggregates suppress the staged reported balance. Opening publication stays on the existing created-account review.

## Accounts in one file

Every statement account is mapped explicitly through the existing whole-source account workflow. An OFX-family file opens that mapping even when it contains one account. The evidence label is the institution id, a space, and the account number, or the account number alone when the institution id is empty. A created account name is that label. Currency comes from `CURDEF` and must match the target. The account number is required; a missing `ACCTID` refuses the file. No section is omitted. Account identity and currency are not inferred from a name. Account rename, archive, deletion, opening-balance correction after commit, and currency reassignment are the implemented point-4 commands in [feature 002](002-accounts-money-splits.md#applied-account-management-checkpoint--2026-10-27).

## Duplicate recognition

Dispositions remain unresolved, include, and duplicate. Commit rejects unresolved rows. A duplicate disposition posts no money and adds an association to the chosen committed occurrence. The same candidate occurrence is not reused inside one batch. Matching uses source facts for the target account: original date, original amount, currency, and `FITID` when present. Edited payee, date, and effective amount are not the match key.

When both sides have the same non-empty `FITID` (including a `FITID` already associated with a QIF-originated ledger row), the same target account, compatible currency, and equal available original amounts, the basis is `same_fitid` even if the dates differ. The row still needs an explicit duplicate disposition. It is not a silent drop and not a second posting. The same `FITID` with a different amount, a different currency, or an unavailable amount is `uncertain_fitid`. It is still an explicit-disposition candidate. It is not auto-excluded.

Absent `FITID` uses the QIF candidate comparison of source account, currency, original amount, and original date. The source account is the user-mapped SpentIt account. Institution id strings are not match keys. Two different non-empty `FITID`s are legitimate repeats, including the same amount and date, and are not candidates. Differing dates on otherwise compatible evidence are `uncertain_date` when at least one side is `ofx`, `qfx`, `qbo`, or `aso`. QIF-compared-with-QIF rows whose dates differ stay non-candidates. An uncertain match is never a silent drop. Counts and totals alone do not prove identity.

Byte-identical reimport of the same account, profile, bytes, and source-account index still follows the existing QIF rule: the new rows take a duplicate disposition aimed at the prior committed occurrences. That disposition remains reviewable and overrideable. Evidence for both imports is retained.

A repeated `FITID` inside one not-yet-committed file makes the later row unresolved. Its candidate list does not include the uncommitted sibling, so it cannot be marked duplicate of that sibling. The user can include it, which posts it. Commit rejects the row while it stays unresolved. No within-batch duplicate target was invented.

After a user confirms an OFX-family row as a duplicate of a QIF row, the association stores that `FITID` on the ledger transaction. A later export with a different `FITID`, the same amount, and the same date is then a legitimate repeat rather than another candidate. Before any `FITID` is associated, a QIF row with an empty `FITID` still matches the same date and amount.

## Source representation

Schemas 18 and 19 store, for every import including QIF:

- `sources.bytes`: the original file, byte for byte
- `source_headers`: file label, grammar, and exact header text (empty for QIF; OFX header bytes plus signon and root-attribute notes). Legal labels are `qif`, `ofx`, `qfx`, `qbo`, and `aso`. Grammar stays `qif`, `ofx-sgml`, or `ofx-xml`. `USERKEY` is not written into this column.
- `source_occurrences`: one immutable fact row per parsed record, including file offsets, `FITID`, source-account key, time evidence, `TRNTYPE`, currency, original units, original date, payee, memo, reference, uninterpreted text, and blocked state
- `source_associations`: links from a ledger transaction to one or more source occurrences

`records.raw` remains the exact occurrence slice. Rules and manual edits do not change those bytes or the occurrence facts. Include inserts a self-association inside the commit transaction. Duplicate inserts an association to the chosen ledger row and does not post again. Cleared rows get no association. Identical operation retries return the existing receipt before another insert. Update and delete of these evidence tables abort.

Occurrence verification re-parses the slice. An OFX slice reparse must reproduce the date, amount, payee, reference, and `FITID`. The stored file label is kept. A stored `FITID` that disagrees with the reparsed slice is corrupt.

## Limits

File size 64 MiB, 200,000 records, 64 KiB per field, 64 KiB header, depth 32, and 1,000 statement wrappers. Crossing a limit is an inadmissible resource rejection, not a truncated success. Invalid input and execution failure stay distinct from a valid empty statement. Diagnostic and uninterpreted text stop at the field cap by rejecting the file rather than silently shortening financial evidence.

## Review surface

The durable UI file dialog accepts `qif`, `ofx`, `qfx`, `qbo`, and `aso`, including an `ASO files (*.aso)` filter. The action label is `Import statement…`. The dialog and QML do not run SQL or money arithmetic. Date order applies to QIF only; the control stays disabled until a date order is chosen, and OFX-family prepare, including `aso`, stores DMY and ignores MDY. The source panel shows format, grammar, `FITID` when present, and the calendar date. A duplicate candidate label shows the prior format and the match basis. The ledger search token remains `QIF:` and matches retained occurrence bytes, so an OFX slice can match. The token was not renamed.

## Decision gates left visible

These choices would change posted money, duplicate exclusion, or evidence retention, and this checkpoint does not invent a policy for them:

- posting or ignoring pending `STMTTRNP` lines
- treating cash-advance, interest, and reward aggregates as harmless
- splitting a loan payment into allocations
- applying `CORRECTACTION` or treating `ORIGCURRENCY` as a conversion
- auto-resolving an OFX transfer destination
- a duplicate target for a repeated `FITID` that exists only inside the current uncommitted batch

Surrounding behavior is a file refusal or a non-repairable record blocker, with the original file unchanged.
