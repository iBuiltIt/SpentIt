---
id: review-duplicates-transfers
title: Review duplicates and link transfers
order: 6
keywords: duplicate, merge, link, transfer, matching, payment, fee, survivor, undo, unlink
---

# Review duplicates and link transfers

Two entries can mean two very different things. Two imports of your
Market purchase may describe one purchase. A payment from Chequing
to Credit card belongs in both accounts.

**Merge** keeps one transaction for the same event in one account.
**Link** connects the two sides of a transfer between accounts.

Open **Review** in the sidebar. Read the question above each group
before answering **Yes** or **No**. Use **Inspect** to read a transaction’s
details; opening it does not answer the question.

## Is it the same purchase? {#is-it-the-same-purchase}

Under **Are these the same?**, compare the date, amount and payee
with your statement. Expand the category’s split count to see its
parts and memos. Two purchases with the same date and amount can
still be separate purchases.

- **Yes** merges the displayed transactions. Keep this for entries
  that really describe the same event.
- **No** keeps them separate and remembers that answer.
- For more than two entries, select the transaction rows you want
  to merge and choose **Selected yes**. The other entries remain.

If asked **Choose the transaction to keep**, choose the entry whose
current details you want to retain. Its payee, memo, categories and
splits stay; Merge does not combine everyone’s edited fields.

![Choose the transaction to keep dialog offering the Market groceries and Market entries for the same 42.18 purchase.](images/review-keep.png)
*Here both copies are new, so you choose which one to keep. Cancel leaves them separate.*

With one accepted entry, SpentIt normally keeps that entry without
asking. A manually entered transaction takes priority when matching
it to an import: the manual entry stays and gains its imported original.
Manual entries are not automatically consumed just because a matching
import arrives.

For these examples, we return to the original Market purchase and
refund, without the extra Corner shop purchase from the previous
chapter. Keeping one 42.18 purchase and the 6.00 refund leaves the
card balance at **-36.18**. If you kept Corner shop too, the balance is
**-56.18** before the payment and **-20.00** afterward. Keep that purchase;
these pictures show only the Market example.

### Merge several matches

Use **Include match** beside each duplicate question you have checked.
For a group larger than two, select at least two transaction rows
first. Choose the entry to keep if asked.

The group’s label changes to **Selected match** with a number.
Choose **Merge N selected matches** to merge those groups together.
One **Undo** reverses that whole batch.

Selecting transaction rows chooses the entries within a group;
**Include match** chooses the group for the batch. Leaving Review or
changing the file’s transactions clears the pending group choices.
Inspection alone keeps them. If any selected match is no longer
valid, the batch is refused rather than partly merged.

## Is it a payment between accounts? {#is-it-a-payment-between-accounts}

Our card payment is **-36.18 in Chequing** and **+36.18 in Credit card**.
The bank might date it January 28 and the card provider January 29.
These are two postings of one transfer, not a duplicate purchase.

In a transfer question, check both account names, the signs and the
dates. Select the matching candidate’s round button if there is more
than one. A preselected candidate is only a suggestion.

![Review transfer question connecting a 36.18 Credit card deposit with the 36.18 Chequing withdrawal dated one day earlier.](images/review-link.png)
*Both accounts need their posting. Yes links the selected pair; it does not merge them into one account.*

Choose **Yes** to link the selected pair, or **No** to reject all the displayed
comparisons, keeping the transactions. If either transaction has several
splits, use **Split to link** to choose the relevant part on each side.
Check any preselected split before proceeding.

Suggestions use the same currency, opposite signs, and dates no more
than seven calendar days apart. Ordinary spending categories can
prevent a transfer suggestion. Don’t remove a valid category merely
to make a suggestion appear.

After linking, the category names the other account. Click its arrow
to visit the related posting, then use **Back** to return.

![Credit card ledger after linking the payment, showing Chequing as the payment category with a navigation arrow and a zero balance.](images/review-linked.png)
*The payment clears the 36.18 card debt. Chequing still contains its withdrawal.*

### If SpentIt created the other side {#if-spentit-created-the-other-side}

An imported transfer can name another account before that account’s
bank posting arrives. SpentIt creates the corresponding entry there.
Review may ask you to find its bank match.

When the real posting arrives, linking it replaces the extra generated
entry where eligible. Read the question’s explanation before confirming;
this is why an account balance may change when a generated duplicate
is removed.

If you have checked the transfer and have no bank posting to match,
use **Accept without bank match** where offered. This acknowledges the
missing match; it is separate from **Accept transactions**. You can
match an imported posting later. Don’t add another transfer simply
to clear the question.

## When the amounts differ {#when-the-amounts-differ}

Suppose Chequing shows **37.18 paid** but Credit card received **36.18**.
If the extra **1.00** is a real fee, it belongs in an ordinary category,
not in the amount transferred.

For eligible suggestions, **Yes** opens **Link the matching amount**.
Choose the category for the difference, then **Link**. **Cancel**
leaves the question unanswered.

![Link the matching amount dialog offering to link 36.18 and categorise the 1.00 difference, with Link and Cancel buttons.](images/review-difference.png)
*Choose a suitable fee category if the statement confirms a fee. Uncategorized leaves it to categorize later.*

SpentIt splits the larger amount: 36.18 becomes the transfer and 1.00
gets your chosen category. The two imported transaction totals stay unchanged.
Check the statement before treating a difference as a fee.

These suggestions require an explicit outgoing transfer destination,
an outgoing amount larger than the incoming amount, and a difference
no greater than 4% of the outgoing amount. They do not convert currencies.
If no suitable suggestion appears, check the accounts, dates, amounts
and categories rather than changing accurate values to force a match.

## If there is no suitable question

For a duplicate you have identified yourself, select the entries in
one account’s ledger, right-click and choose **Merge**. Check which
entry will be kept, just as in Review.

For a transfer whose two entries each have just one split, select
the first entry, right-click and choose **Link**. Switch accounts,
select the other entry, then right-click and choose **Link** again.
Review any confirmation before applying it. The command still checks
whether the two postings can be linked.

For recurring unwanted suggestions, **Exclude…** on a transfer row
opens **Don’t suggest links**. By default, payee or memo text must
match the whole field, ignoring capitalization but keeping spaces and
punctuation. You can limit the
exclusion to that account. Choose **Preview** before **Save**;
it affects future suggestions, not just the current question. Manage
these exclusions through **Transfer suggestions…** in Review.

## Undo a mistaken answer

Use **Undo** at the bottom of Review while it is available. It reverses
the latest review decision, including a Merge or Link. You can step back
through a sequence of review decisions; a batch Merge is one step.

Other changes to the file end that Undo sequence. If Undo is unavailable,
don’t delete a posting just to break a link: that also changes the account
balance. Inspect both sides before making a correction. The current UI
has no separate **Unlink** button.

A refused Merge or Link leaves a message explaining what needs attention.
Different current totals or existing transfer relationships can prevent
a merge. Refresh your comparison and correct only values you know are
wrong. If SpentIt cannot confirm whether an operation was saved, reopen
the file and inspect the result before trying again.
