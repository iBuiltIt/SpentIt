# Review: OFX statement header fields

Status: implemented after owner Go, 2026-11-07. Conflicting statement fields and unreadable unsuppressed balances refuse the import before mapping. See [verification and limits](../doc/status.md#ofx-statement-validation-and-incomplete-storage-follow-up--2026-11-07). The original findings and proposed checks below are retained as review context.

Date: 2026-11-07.

## Question

Can a bank or credit-card file produce a zero opening, or the wrong account identity, while the mapping screen shows a normal statement?

Transaction rows are in better shape. A bad `TRNAMT`, date, transfer, or repeated transaction field blocks that record and keeps the original bytes. `CURDEF` disagreement refuses the file. QIF refuses a later `B` balance that disagrees with the first. This review is only the statement-level OFX fields that do not get that treatment.

The accepted profile is `features/003-ofx-profile.md`. Reported balance is `LEDGERBAL` / `BALAMT`, and only when that text parses exactly for the statement currency (profile, “Dates, amounts, and currency”). A statement `CODE` other than `0` refuses the file. Account identity is the institution id plus `ACCTID`.

## 1. An unparseable ledger balance looks like no balance

`parse_ofx` (`src/engine/statements.cpp`, around lines 542–545) does this:

```cpp
if(!account.suppress_balance && !account.ledger.empty()) {
    try { (void)Money::parse(account.ledger, currency); view.reported_balance = account.ledger; }
    catch(const FinancialError&) {}
}
```

The empty catch drops `BALAMT` text that `Money::parse` rejects. No account diagnostic is set. The file is still admissible.

The mapping screen (`src/ui/import_model.cpp`, around lines 1838–1840) uses one label for a missing optional and for this drop:

```cpp
QString reported=source.reported_balance?text(*source.reported_balance):QString("Not supplied");
bool offer_reported=source.reported_balance.has_value();
```

A new-account mapping then defaults to a zero opening. The user can confirm zero while the file contained a balance. A transaction `TRNAMT` that fails the same parser is repairable and surfaced in `make_record`. QIF keeps raw `B` text so the user can choose Ignore. The OFX profile chose exact parse before staging; it does not say to describe a rejected balance as “Not supplied”.

There is a second parser on the same value. When `reported_balance` is set, the mapping screen parses it again with `qif::amount`. If that throws, the catch is empty, `offer_reported` stays true, and the label stays the raw text. `Money::parse` and `qif::amount` need to agree on every staged balance.

`tests/engine/statement_tests.cpp` locks a parseable `10.00` and locks `BALLIST` suppression of a parseable balance. It does not lock a `BALAMT` that `Money::parse` rejects.

Prove, with synthetic SGML and XML fixtures:

- A statement whose only ledger balance is exact, such as `10.00`, still offers that amount.
- A balance `Money::parse` rejects, including a grouped value such as `1,234.56` and a fraction finer than the currency, does not become a confirmed zero opening under the words “Not supplied”. Either the mapping screen says the balance could not be read, or the file is refused before a destination can be confirmed. Pick one and lock it. Leave the profile’s “stage only an exact parse” rule in place unless the owner chooses to offer the raw text the way QIF does.
- A staged balance that `Money::parse` accepted also passes `qif::amount` for the same currency. One failure path covers both.
- `BALLIST` still suppresses a staged balance, and a missing `LEDGERBAL` still says the balance was not supplied.

## 2. A later statement field replaces the earlier one

`Builder::leaf` (`src/engine/statements.cpp`, around lines 114–136) assigns the current statement’s status, `BANKID`, `ACCTID`, and `BALAMT` on every occurrence. It does not compare the new value with the one already stored. Each `STMTTRNRS` / `CCSTMTTRNRS` is its own account (`open`, around lines 74–77), so this is repeated fields inside one statement, not a second account in the file.

A second different `CURDEF` already refuses the file (around lines 129–131). Repeated transaction fields set `conflict` in `field` and block the row. The profile says conflicting repeated values of the same transaction field block that record. Statement-level fields have no equivalent.

What the last value decides:

- The last `BALAMT` is the only reported opening. Nothing says an earlier balance was different.
- The last `ACCTID` or `BANKID` becomes the statement identity used for mapping and for later reimport matching.
- The last statement `CODE` decides success. A later `0` hides an earlier non-zero code, and the statement is imported. A later non-zero code refuses the file, which is the safe direction.

No test read in this pass uses two different `BALAMT`, `ACCTID`, `BANKID`, or `CODE` values in one statement.

Prove, with one synthetic statement per case:

- Two different `BALAMT` values are refused, or both values are shown before an opening can be confirmed. A single `BALAMT` is unchanged.
- Two different `ACCTID` or `BANKID` values are refused. A single pair still maps to the same account as today.
- Two different statement `CODE` values are refused. A file whose only code is `0` still imports. A file whose only code is non-zero still refuses with “Statement status is not success”.
- A second identical value is accepted. Repeating the same `ACCTID` must not refuse a normal bank file.

## Out of scope

- Transaction `TRNAMT`, `DTPOSTED`, `FITID`, and transfer blockers.
- The same-calendar-day duplicate rule and the seven-day transfer-link window.
- QIF date order, which already refuses conflicting orders and asks on an ambiguous file.
- Changing opening defaults for a balance that parsed exactly.
