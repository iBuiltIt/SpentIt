---
id: search-examples
title: Search examples
order: 15
keywords: search, filter, examples, phrase, period, category-exact, QIF, regex, regular expression, pattern, quick reference
---

# Search examples

Select an account and type into the search box above its transactions.
Choose **Apply** or press **Enter**. These examples search the ledger;
**Search Help** searches this guide instead.

## Start with words

- `Market` finds the purchase and refund in our example. Capitalization
  does not matter, and the text can appear inside a longer word.
- `Market groceries` requires both words. One can match the payee and
  the other a memo or category.
- `"Corner shop"` finds that phrase together in one field.

Ordinary search uses the transaction’s current text, including split
categories and memos. A space means “and”; `OR` and `NOT` are not operators.

## Narrow by category or date

- `category:Groceries` finds Groceries and names beginning with
  **Groceries:**.
- `category-exact:Groceries` finds only the exact category Groceries.
- `category:"Food: Groceries"` finds that category if you use it.
- `period:2026` finds entries dated anywhere in 2026.
- `period:2026-01-15` finds one day.
- `period:2026-01-01..2026-01-31 Market` finds Market entries in January,
  including both boundary dates.
- `period:jan` finds January in the current year.
  `period:may-june` covers May through June in the current year.

The full-date examples above use year-month-day. Use your file’s saved date
format: with day/month/year, January 15 is `period:15/01/2026`.
For a range crossing New Year, use full dates rather than `dec-jan`.

You can combine words, one category filter and one period filter:
`category:Groceries period:2026 Market`.

A matching split brings in the **whole transaction**. For Corner shop,
searching for Household includes its full -20.00 purchase, not just the
-8.00 Household part. Filtered withdrawal/deposit totals include that full
amount; the account balance does not change. Use
[report details](reports.md#open-the-transactions-behind-an-amount) to see
which parts contribute to a spending total.

## Advanced: search with a pattern

Use `regex:"pattern"` when words alone are not enough. For example:

- `regex:"^Market$"` matches a whole field containing Market, so it
  excludes the payee Market refund.
- `regex:"^Market"` matches a field starting with Market.
- `period:2026 regex:"^Market"` adds a date filter.

Patterns ignore capitalization by default. `regex:"(?-i)^Market$"`
requires that capitalization. Search tests each field separately; a match
need not come from the payee. Outside `regex:"…"`, punctuation such as `*`
is ordinary text, not a wildcard.

### Common pattern pieces

The table below applies inside `regex:"…"` in ledger search and in
**Pattern (regular expression)** in Rules. In Rules, enter just the pattern,
without the `regex:` wrapper. Search finds entries; it does not replace text.

| Piece | Meaning | Try it with bank text |
| --- | --- | --- |
| `^` | Start of the field. | `^PAYPAL` finds descriptions starting with PAYPAL. |
| `$` | End of the field. | `MARKET$` finds descriptions ending with MARKET. |
| `.` | Any one character, except a line break. | `SHOP.123` matches SHOP-123 or SHOP 123. |
| `*` | Repeat the preceding piece zero or more times. | `AB*` matches A, AB or ABBB. |
| `+` | Repeat the preceding piece one or more times. | `AB+` matches AB or ABBB, but not A. |
| `?` | Make the preceding piece optional: zero or one. | `SHOP-?123` matches SHOP123 or SHOP-123. |
| `\s` | One whitespace character: a space, tab or line break. | `PAYPAL\s+EBAY` allows one or more spaces between the words. |
| `\d` | One digit, including digits from other writing systems. | `REF\s*\d+` matches REF123 or REF 123. |
| `[ABC]` | One character from the brackets. | `SHOP[ -]123` allows a space or hyphen. |
| `[0-9]` | One character from 0 through 9. | `[0-9]+` matches one or more digits from 0 through 9. |
| `[^0-9]` | One character other than 0 through 9. | `^[^0-9]+` matches **Market** and its following space in **Market 1234**, stopping before the first digit. |
| `{4}` | Exactly four of the preceding piece. | `CARD[0-9]{4}$` matches an ending such as **CARD1234**, with exactly four digits after CARD. |
| `(…)` | Group and capture the matched piece; Rules can reuse it in a template. | `SHOP\s+(.+)` captures the text after SHOP and its spaces. |
| `\` before punctuation | Treat a special character literally. | `\*` matches an actual asterisk; `\.` matches a dot. |
| `\Q…\E` | Treat everything between these markers literally. | `\QMarket (Downtown)\E` includes the parentheses as ordinary text. |

The distinction between `*` and `+` is whether an empty piece is allowed.
`\s*` allows no spaces; `\s+` requires at least one. Neither means “any text”
on its own: `.*` means any number of characters, while `.+` requires at least
one. Both stay on the same line by default and take as much as they can while
still allowing the rest of the pattern to match.

Brackets choose **one character**, not a word. `[EBAY]` means E, B, A or Y;
write `EBAY` to match the word. A hyphen between characters makes a range;
put it last, as in `[ -]`, to mean a literal hyphen. A `^` immediately after
`[` means “except”; outside brackets it marks the start of the field.

For a literal double quote inside a quoted search, use `\"`.
To match a literal backslash, use `\\\\` inside the search quotes
(`\\` in a Rules pattern). To find text that looks like an operator,
quote the whole term:
`"regex:word"` searches for those literal characters.

Rules can also use captured text to change a payee or memo; see
[Keep the useful part of a PayPal description](rules.md#keep-the-useful-part-of-a-paypal-description).

## Advanced: search the imported original

Start with a separate `QIF:` token to search original imported records,
including their field codes and line breaks:

- `QIF: Market` finds Market in the original, even after you rename the payee.
- `QIF: period:2026 Market` also limits entries by their current ledger date.

Despite its name, this mode also searches retained records from OFX-family
imports. Entries without an imported original, including ordinary manual
entries and Opening Balance, do not match. You cannot combine this mode
with `category:` or `category-exact:`; those use current category assignments.

## Clear a search or fix it

Choose **× (Clear search)**, or press **Escape** in the search box, to show
all entries again. Clearing restores your earlier selection unless you
selected a search result; then that selection stays.

If a search fails, the previous results stay on screen. They are not results
for the failed query. Close any unfinished quote, check dates and use only
one period and one category filter. For a pattern error or a search that
reaches its limit, simplify the pattern or narrow the dates, then try again.

[More about finding transactions](find-transactions.md#find-a-purchase).
