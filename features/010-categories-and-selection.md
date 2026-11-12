# 010 — Categories and selection

Status: **active**
Last updated: 2026-11-12

A category is an independently assignable name. It may contain one colon, shown
as `Stem: Name`. A second colon is rejected. `Auto: Gas` does not create `Auto`.
Renaming, archiving, or deleting one name does not change another that shares
its stem. Identity is case-insensitive. Display keeps the created capitalization
until an explicit rename.

Accounts appear in the category editor as read-only rows. Account names and
confirmed aliases share this namespace and identify transfers, including
unbracketed QIF split names. Use account commands to change an account.

## Income

Categories has an Income checkbox. A category the person adds, and a
category created from an import, starts unchecked. A new file that uses the
starter categories checks Income for Income: Business / Self-Employment,
Income: Investment & Interest, Income: Other, Income: Pension & Retirement,
and Income: Salary & Wages. Taxes: Income is an expense. Every other starter
category starts unchecked. An existing file keeps the checks it already has.
The flag does not follow the name and does not change transaction signs or
report arithmetic. Rename and archive keep it.

Default order is Expenses A–Z, Income A–Z, then Accounts A–Z. Uncategorized
stays first where it is offered. Workspace Z–A and Order added apply inside
each ordinary group for that visit. Accounts stay A–Z. A new row starts at the
bottom for the visit. Changing its Income flag puts it in the matching group
immediately.

Reports keeps explicit saved category selections. Its checklist has tri-state
Expenses and Income groups. A group action includes categories hidden by
search and does not change Uncategorized or the other group. Changing a flag
or adding a category does not change a saved report's selection.

## Editor and pickers

The editor is a compact table. New category adds a blank row. Enter or leaving
the field saves. Escape cancels. Invalid or duplicate names stay in the field
and use the bottom status line. Rename, Archive, Unarchive, and Delete unused
category are row actions and save immediately. Deleted definitions stay
available to history. Show archived lists archived rows only.

Sort choices last for the visit, not as a saved preference. Selectors always
use the grouped alphabetical order above, sized to the current labels.

The ledger **Change category** submenu applies immediately to the selection
through the atomic assignment command, with one Undo. Ordinary category changes
preserve an unresolved duplicate decision. A split transaction needs a split
choice. An ambiguous multi-split selection is refused. Keep the viewport.

Choosing an account assigns a transfer through the existing transfer commands.
Choosing an ordinary category again ends the link, removes only a disposable
generated draft, and restores both sides together on Undo. Current account
destinations are disabled in the menu. Rules still cannot create transfers.

## Starter categories

**Use default categories** starts checked and creates this list. Unchecked
creates an empty catalog. Names stay editable. Each line is one category. The
five Income names in the Income section above start checked. Taxes: Income and
the other names start unchecked. A category added later starts unchecked.

```text
Housing: Rent & Mortgage
Housing: Home Maintenance
Housing: Home Improvement
Housing: Furniture & Appliances
Utilities: Electricity, Gas & Heating
Utilities: Water, Sewer & Garbage
Utilities: Internet
Utilities: Phones
Utilities: TV & Streaming
Auto: Gas & Fuel
Auto: Maintenance & Repair
Auto: Insurance
Auto: Parking & Tolls
Auto: Payment & Other
Transportation: Public Transit
Transportation: Rideshare & Taxi
Food: Groceries
Food: Restaurants & Dining
Food: Coffee & Fast Food
Amazon
Shopping: Clothing
Shopping: Electronics & General Shopping
Medical: Doctor & Hospital
Medical: Pharmacy
Health & Wellness: Gym & Fitness
Health & Wellness: Personal Care
Entertainment & Hobbies: Entertainment
Entertainment & Hobbies: Hobbies
Travel: Flights
Travel: Hotels & Lodging
Travel: Other
Family & Education: Child Expenses & Childcare
Family & Education: Education
Other & Miscellaneous: Gifts & Donations
Other & Miscellaneous: Pets
Other & Miscellaneous: Insurance
Other & Miscellaneous: Fees, Interest & Bank Charges
Other & Miscellaneous: Other
Income: Salaries & Wages
Income: Pension
Income: Investment Income
```

Successful close keeps category history. Do not delete that history as a
cleanup.
