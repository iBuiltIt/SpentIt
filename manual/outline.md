# SpentIt help and user manual — proposed outline

Date: 2026-11-05  
Status: proposed content outline for review. Chapters 1–7 are written, with screenshots in chapters 2–7. Chapter 6 is accepted and committed; chapter 7 is owner-accepted for commit. Later chapters are not yet written.

## Purpose and reading order

The manual should help someone answer “Where did my money go?” Start with a
short, entirely nontechnical explanation: bring transactions from your accounts
together, check them, organize them into useful categories, and explore your
spending. Explain that the user remains in control of corrections and choices.
Keep this introduction short enough to read before doing anything in the app.

The main sequence follows a first working session, then grows into everyday use.
It is a guide someone can read in order and a reference they can enter at any
topic. It is not a compulsory tutorial. Each chapter answers a practical question;
screens, menus and buttons are explained where they help answer it.

The chapter titles and divisions below are proposals. This document concerns
manual content only; it does not prescribe or change the help viewer.

## 1. Understand your spending

**Reader's question:** What can SpentIt help me do, and where do I start?

- What SpentIt is for: understanding money coming in, money going out, and where
  spending belongs.
- The broad approach: create a file and accounts → bring in transactions →
  review and organize → accept when ready → explore reports.
- A small everyday example: a purchase, a refund and a credit-card payment.
  Introduce why a payment between your own accounts differs from spending.
- Explain only the essential words: file, account, transaction, category and report.
- Offer two starting paths: import existing transactions, or enter a transaction
  yourself. Both lead into the same guide.

**Illustration:** A simple reading-path illustration, if helpful. No technical
architecture diagram or screenshot is needed for this introduction.

## 2. Create or open your file

**Reader's question:** How do I start, and where is my work kept?

- Welcome screen: create a new file or open an existing one.
- Choose a name and location; explain the consequence before replacing an
  existing file.
- Choose password protection, including what choosing no password means and the
  absence of forgotten-password recovery. Keep the explanation practical.
- Initial account and currency choices; date display and starter categories.
  Explain each choice when setup actually presents it.
- Open and unlock a file later; close it or switch to another file.
- First look at the main window: account list and balances, selected workspace,
  navigation, Settings and the bottom status line.
- Explain early that saved new transactions remain when the file is reopened.
  Distinguish these from an edit still waiting for Save or Enter.

**Screenshots:** Welcome/setup, unlock, and an annotated main-window overview.
**Problems here:** Existing filename, wrong password, unavailable file and file
already open elsewhere. Link to recovery when the next step needs more explanation.

## 3. Set up your accounts

**Reader's question:** How do my bank, cash and credit-card accounts fit here?

- Create and name accounts; explain that importing can also create accounts.
- Choose the appropriate currency and understand which account is selected.
- Explain opening date and opening balance with a concrete example: the balance
  at the starting point and the transactions that follow it.
- Explain zero openings and reported balances without assuming a bank's reported
  balance is the correct opening for the imported date range.
- Read account balances and switch between accounts.
- Correct an opening balance from its ledger row.
- A separate “Later: manage your accounts” topic: reorder, rename, archive,
  reactivate and delete an eligible empty account. Explain retained names when
  they affect import matching or a naming conflict.
- Explain currency reassignment at its point of use, especially that changing
  the currency does not convert the amounts.

**Screenshots:** New account, opening-balance editor and account context menu.
**Problems here:** Conflicting names, invalid openings and unavailable account actions.

## 4. Bring in transactions

**Reader's question:** How do I get my exported transactions into the right accounts?

- Obtain a supported transaction export: QIF, OFX, QFX, QBO or ASO, within the
  application's supported statement types. Explain file selection, not format internals.
- Choose the destination account for a single-account import.
- For a file containing multiple accounts, connect each imported account to an
  existing account or create one. Explain the new-account opening choices here.
- Resolve account names used by transfers.
- Decide what to do with imported categories that do not yet exist:
  Create or Use Uncategorized. Briefly explain categories now and link to chapter 8.
- Read import progress, completion and records needing attention; explain
  cancellation and repair choices where offered.
- Find the imported transactions and understand the next step.
- Import another period or an overlapping export. Explain recognized repeats
  versus possible duplicates that still need a decision.
- Correct a mistaken import: distinguish Undo last import, choosing an import
  to undo, and account-wide Discard. Explain affected accounts and restrictions
  before the reader confirms anything.

**Screenshots:** Import entry point, account mapping, missing-category choices,
completion/repair summary and an import-undo preview.
**Problems here:** Unsupported export, invalid date or amount, incorrect account
mapping, import with no new transactions, partial failures and interrupted import.

## 5. Find your way around transactions

**Reader's question:** What am I looking at, and how do I find what needs attention?

- Read the ledger's dates, payees, memos, categories, amounts and balances.
- Interpret positive/negative amounts in the account context, including a
  credit-card example.
- Recognize new and accepted transactions without relying only on row colors.
- Select one or several rows; distinguish selection from opening an editor.
- Search, apply and clear a search; sort and scroll; understand why filtering
  changes what is visible without removing transactions.
- Recognize split rows and transfer navigation arrows.
- Use Back/Forward and return from another account or workspace.
- Open Review and understand its account groups, outstanding items and status.

**Screenshots:** Annotated populated ledger, a search result, and Review overview.
**Problems here:** A transaction appears missing, an unexpected balance, or a
control is unavailable. Begin with account, search and selection checks.

## 6. Review duplicates and link transfers

**Reader's question:** Is this the same transaction twice, or two sides of a transfer?

- Explain the difference with paired examples: two imports of one purchase in
  one account, and a payment from a bank account to a credit-card account.
- **Merge:** keep one transaction for the same event in the same account;
  inspect differences and choose what to keep when asked.
- **Link:** connect postings in different accounts that represent one transfer.
  Both accounts still need their own posting.
- Keep legitimate repeated purchases separate and reject an incorrect suggestion.
- Understand a transfer entry created by SpentIt while the other account's
  imported posting is still missing; resolve it when that posting arrives.
- Review differing dates or amounts; explain the actual allowed choices and
  how to record a fee or difference when offered.
- Navigate to the other side, inspect a linked transfer and undo a mistaken
  decision or unlink when available.
- Merge several eligible duplicate groups; explain the distinction between
  selecting groups and selecting their transaction rows.
- Include manually entered transactions later matched to an import.

**Screenshots:** Duplicate comparison, transfer candidate, linked pair/navigation,
and an amount-difference decision.
**Problems here:** No suitable candidate, multiple plausible candidates, refused
Merge/Link, and a decision that can no longer be undone. Explain the relevant
requirements without describing internal matching machinery.

## 7. Add and correct transactions

**Reader's question:** How do I record something missing or fix a transaction?

- Add a transaction with the ledger's + or Add transaction action.
- Open an existing row for editing; change the date, payee, memo or amount.
- Use the shared calendar: choose a day, browse months/years or select Today.
- Save with the available controls/Enter; cancel creation or resolve an unsaved
  edit when leaving. Explain Save, Discard and Stay in context.
- Enter amounts in the file's chosen style; use simple arithmetic expressions
  with examples where supported.
- Split a purchase across categories; explain how the remaining amount is
  filled, how to take control of it, and why splits must add up to the total.
- Handle refunds and mixed-sign splits with an everyday example.
- Inspect original imported values and restore them where available.
- Rename payees or make supported changes to a selection.
- Delete transactions, understand the confirmation and use the applicable Undo.
- Explain restrictions on linked transfers beside the affected edit or deletion.

**Screenshots:** New entry, inline editor, calendar, split editor and unsaved-changes prompt.
**Problems here:** Invalid date/amount, too many decimal places, splits not adding
up, unavailable original values and unavailable Undo.

## 8. Organize spending with categories

**Reader's question:** How do I group transactions in a way that is useful to me?

- Start with the supplied categories or create your own.
- Explain Uncategorized and why it is useful to find unfinished categorization.
- Create, rename and manage categories using the available category controls.
- Explain names such as Food:Groceries without implying a parent category
  automatically includes other categories.
- Assign a category through the editor's suggestions or right-click Change category.
- Change several selected transactions; choose the relevant split where required.
- Distinguish an ordinary category from an account destination: choosing an
  account makes a transfer, with consequences for its other posting.
- Explain changing a transfer back to an ordinary category at that decision.

**Screenshots:** Category workspace, filtered completion and category/account menu.
**Problems here:** Name conflicts, category in use and ambiguous split selections.

## 9. Use rules for repeated categorization

**Reader's question:** How can SpentIt recognize transactions I categorize the same way?

- Start from one familiar example, such as categorizing a recurring grocery payee.
- Find, create, select and edit a rule; explain matching fields and actions in
  ordinary language. Put advanced pattern examples in an optional reference.
- Choose an explicit category. Explain that only Uncategorized items are eligible
  and that rules do not create transfers.
- Explain transaction rules and split rules using the same example transaction.
- Explain rule ordering and when rules are applied during import.
- Choose All transactions in this account or New transactions only.
- Preview matching transactions; distinguish saving a rule from applying it.
- Explain Save and apply, selected-rule actions, available all-enabled actions
  and Undo without implying unrelated drafts are saved together.
- Enable, disable or delete a rule; explain that this alone does not reverse
  categories already assigned.
- Leave with unfinished edits: Save, Discard or Stay.

**Screenshots:** Rules list/editor, preview and leave-screen decision.
**Problems here:** No matches, unexpected matches, already categorized items,
invalid patterns and a rule that cannot be saved or applied. Verify current
matching behavior before explaining original versus edited text.

## 10. Accept transactions — now or later

**Reader's question:** What does accepting mean, and do I have to do it now?

- Recap the difference between saving an edit and accepting transactions.
- Use the visible label **Accept transactions**; include “commit” as a lookup
  term for readers who know that word.
- Explain the scope across accounts, including related transfer postings.
- Review outstanding issues and distinguish a blocking problem from a choice
  the application allows the user to proceed with.
- Explain what remains unresolved when valid transactions are accepted while
  failed imported records still need attention.
- Leave transactions new and return later: saved new transactions remain in the
  file and contribute to balances and reports.
- Explain that accepted transactions can still be corrected through supported actions.
- Explain the quit-time acceptance question and the consequence of each choice.
- Compare saving, accepting, undoing an import, deleting selected transactions and
  account-wide Discard in a small action/consequence table.

**Screenshots:** Acceptance entry point, review/blocking state and quit prompt.
**Problems here:** Nothing to accept, unresolved issues and acceptance that cannot complete.

## 11. Answer questions with reports

**Reader's question:** Where did my money go, and how does it compare with another period?

- Build a first report from the same accounts and categories used throughout the guide.
- Select accounts and categories; explain inclusion choices and separate currencies.
- Choose a period: moving presets versus fixed custom dates; use the calendar.
- Add comparison periods and explain their order and possible overlap.
- Save, show, rename, duplicate and delete reports; resolve unsaved report edits.
- Read totals and category rows. Explain net spending, refunds and income with
  small examples, including why a report's signs may differ from ledger signs.
- Explain included new and accepted transactions, excluded opening balances and
  transfer principal, and included ordinary fees.
- Choose no chart, separate-period bars, grouped-period bars or pies; explain
  when a pie cannot represent the selected values.
- Open the contributing transactions and identify which splits count in a result.
- Correct an entry from the report ledger, return to results and understand
  refresh, stale results and “No longer included”.
- Explain why a spending total is not an account balance.

**Screenshots:** Report editor, comparison table with grouped bars, pie example,
and contributing transactions with a split included only in part.
**Problems here:** Empty report, unexpected total, unavailable selected account/category,
negative pie values and results that could not refresh. Do not promise printing
or export before those actions are available.

## 12. Look after your work and resolve remaining problems

**Reader's question:** How do I protect my work and recover when something goes wrong?

- Save a backup, choose its location and recognize completion.
- Open/check a backup and follow the recovery actions the application actually offers.
- Find the right file and distinguish it from a backup before continuing work.
- Explain relevant Settings, including date/amount presentation and protection
  controls that are actually available; link back to their first-use topics.
- Troubleshoot opening failures, unsupported older files, unavailable locations,
  failed writes and interruptions that affect more than one workflow.
- Distinguish “the change was saved but the screen could not update” from
  “SpentIt could not confirm whether the change was saved”. Explain checking
  the file before retrying an operation that might already have completed.
- Explain the limits of password recovery and supported recovery actions plainly.
- If a problem remains, describe what information is useful when seeking help
  without sharing passwords or private financial records. Name a support route
  only when one actually exists.

**Screenshots:** Backup controls, completion and selected recovery/error dialogs
whose choices need explanation.

Finish with a symptom/message index linking to the task-specific explanations.
Do not repeat those instructions here or collect every common error at the end.

## Quick reference after the main guide

These are alternate entry points into the same manual, not another walkthrough.

- **Where do I find…?** Screens, menus and important buttons, linked to their tasks.
- **Keyboard and mouse reference:** Verified shortcuts, selection, editing,
  navigation and calendar controls, with platform differences where needed.
- **Search examples:** Common searches first; advanced syntax separately.
- **Words used in SpentIt:** Short definitions, including Merge versus Link,
  new versus accepted, opening balance, split, refund and transfer.
- **Messages and problems:** Searchable exact message wording, plain-language
  meaning and links to supported next steps.

## Writing and screenshot plan

Each task should have a short goal, where to begin, the necessary steps and
choices, and how to recognize the result. Add related problems and a natural
next step. Explain disabled controls, empty screens, progress and cancellation
when they change what the reader can do. Avoid a button inventory detached from tasks.

Owner refinement, 2026-11-05: respect the reader’s time. Use no more words than
the task needs, while keeping plain English and an approachable voice.

Write in short pieces, with room between them. Use plain words, and start from
why a person wants to know. The reader should be able to take one piece at a
time. Do not stack definitions or steps into a single block.

Introduce a concept at first encounter, even if its full chapter comes later.
In particular, imports need brief explanations of categories, transfers and
saved new transactions immediately. Put destructive consequences before the
confirmation step. Use the actual on-screen labels, with everyday wording in
explanations. No storage internals, engineering terminology or implementation
history belongs in the user chapters.

Use one invented household example throughout. The accounts are Chequing, a
credit card, and Cash. Chapter 1 uses a Market grocery purchase of 42.18 on
2026-01-15, a 6.00 refund on 2026-01-20, and a 36.18 credit-card payment from
Chequing on 2026-01-28. Later chapters add income, a mixed-category purchase,
an overlapping import, and a manual entry later found in an import. Keep the
example in one currency until a chapter explains separate report results.
Dates are year-month-day. Amounts use a comma for thousands and a dot for
cents, as in 2026-01-15 and 1,234.56. The guide says the reader's own file
follows the date and amount settings saved for that file. Do not use private
accounts, statements, filenames or screenshots.

Capture the actual application when each chapter is written. Use an overview
when orientation matters and focused crops when a small control matters. Every
image needs a useful caption and alternative text; the prose must still explain
the task without the picture. Show a decision dialog when its choices have
consequences. Use consistent sample data and presentation, with additional
platform/theme captures only when they change the instructions. Planned images
belong in `manual/images/`; no placeholder images are required for this outline.

## Review and completion criteria

Write and review chapters incrementally against the current application. This
outline is not a claim that every listed action is available or verified on every
platform. Check exact labels, prerequisites, outcomes, restrictions and error
wording before turning each topic into instructions. Historical design documents
contain superseded behavior; do not copy them directly into the manual.

For each chapter, confirm that a new reader can perform the task, recognize its
result and recover from its common problems. Check the relevant menus, buttons,
empty/selected/editing states and screenshots for omissions. Keep unresolved
behavior questions as author notes until settled, rather than inventing an answer.

Finally, follow the complete example from an empty file through reports, checking
continuity, cross-references, terminology, screenshot consistency and missing
choices. Also test lookup reading: someone arriving at a single topic from an
error or search should have enough context to act.

Chapter 1 is written in [index.md](index.md) and is the topic Help opens.
Chapter 2, [Create or open your file](create-open-file.md), is also bundled,
with three screenshots. Chapter 3, [Set up your accounts](accounts.md), adds
account creation, opening, and menu screenshots. Chapter 4,
[Bring in transactions](import-transactions.md), adds import destination, mapping,
category-choice, ledger-result, and Undo screenshots. Chapter 5,
[Find your way around transactions](find-transactions.md), adds ledger, search
and Review screenshots. Chapter 6,
[Review duplicates and link transfers](review-duplicates-transfers.md), adds
keep-choice, transfer-question, linked-ledger and amount-difference screenshots.
Later chapters and context links
are separate work. The sample
pages used to check the viewer live under `tests/fixtures/help/` and are not
shown in Help.
