---
id: messages-problems
title: Messages and problems
order: 17
keywords: messages, problems, errors, troubleshooting, failed, unavailable, mismatch
---

# Messages and problems

Search Help for words from the message, or find it below. Choose **Details**
if the bottom status line cuts it short. Quoted text is the message or an
excerpt; a split total varies with your entry.

For a missing transaction or unexpected balance without an error, start with
[Find a particular problem](backup-recovery.md#find-a-particular-problem).

When **Help with this problem** appears beside a message, it opens this
reference without retrying the action. A dialog’s **Help** button opens the
instructions for that task.

## Opening a file {#opening-a-file}

**“Passwords do not match”**

When creating a file, enter the same password in both fields.
[Choose password protection](create-open-file.md#choose-password-protection).

**“Unable to unlock.”**

Check the selected file and retype its password. Damage can produce the same
message; it does not prove the password is wrong. There is no password reset.
A copy opens with the password it had when you made it, which may differ from
the password the working file uses now.
[If the file won’t open](create-open-file.md#if-it-won-t-open).

**“Password changed. This file asks for the new password the next time it is opened.”**

The change is saved. Use the new password the next time you open this file.
[Change or remove the password](backup-recovery.md#change-or-remove-the-password).

**“Password protection removed. Anyone with this file can open it.”**

The file no longer asks for a password. Anyone who has it can open it.

**“That is already the password for this file.”**

The new password matches the one the file already uses. Nothing was changed.

**“The password was changed, but SpentIt could not confirm the file. Close it and open it with the new password before continuing.”**

Close the file and open it with the new password. Check that it opens before
you change anything else.

**“The password was not changed. The file still uses its current password.”**

The file still opens with the password it had. You can try the change again.

**“The password was not changed, but the file needs to be reopened. Close it and open it with the current password.”**

Close the file and open it with the password it already uses. Do not assume
the new password was saved.

**“Current password does not match.”**

Retype the password this file uses now. The file is unchanged.

**“The password cannot be changed while another save is in progress.”**

Wait until that save finishes, then try the password change again.

**“File is already open or unavailable.”**

Close the file in its other SpentIt window. If it is still unavailable,
check the drive and access to the location. Do not delete lock files.
[Opening checks](backup-recovery.md#the-file-will-not-open).

**“This dataset format is not supported by this version of SpentIt; the file has not been changed”**

Keep the original. This version of SpentIt has no upgrade or conversion command.
[Unsupported files](backup-recovery.md#the-file-will-not-open).

## Importing transactions

**“The transaction date or amount is missing.”**

Check the bank export or statement. In **Review**, use **Correct** where
available to enter the missing value.
[Repair an imported record](import-transactions.md#when-something-needs-attention).

**“The split amounts must add up to the transaction amount.”**

The imported parts do not match the total. Check the actual amounts before
correcting the record; do not guess a value just to make it pass.
[Check imported amounts](import-transactions.md#when-something-needs-attention).

**“Currency does not match the target account; no conversion was applied”**

Choose an account with the export’s currency. Changing a currency label does
not convert the money.
[Check the import destination](import-transactions.md#when-something-needs-attention).

**“Incomplete import preparation was removed; you can import the file again.”**

An interrupted preparation was removed. You can import that export again.
[Check the import result](import-transactions.md#check-the-result).

## Editing transactions

**“Splits add up to …”**

The message shows their current sum. Adjust the split amounts to match the
total. If you leave the editor, **Change amount** changes the transaction total
to their sum; **Keep editing** lets you correct the splits instead.
[Balance the splits](add-correct-transactions.md#divide-a-purchase-into-splits).

## Search and rules

**“Unclosed search quote”**

Close the quotation mark, then apply the search again, or clear it. The previous
results stay on screen after a failed search; they are not the new query’s results.
[Fix or clear a search](search-examples.md#clear-a-search-or-fix-it).

**“Use one period filter per query”** / **“Use one category filter per query”**

Use one date range and one category condition in a search.
[Search examples](search-examples.md#narrow-by-category-or-date).

**“Choose a category for every rule”**

Choose an ordinary category in the rule’s editor. Rules cannot create transfers.
[Set up a rule](rules.md#make-a-market-rule).

## Reports

**“Transactions changed. Refresh the report to see current amounts.”**

Choose **Refresh report** before opening another amount.
[Check report results](reports.md#if-the-result-needs-attention).

**“No accounts selected. Edit the report to choose accounts.”**

Return to the report and check at least one account.
[Choose what belongs](reports.md#make-your-first-report).

## Saved changes and uncertain results {#saved-changes-and-uncertain-results}

**“Your change was saved, but the screen could not update.”**

Reopen the file and check the transaction before making more changes.
Do not repeat the edit just to refresh the screen. Rules can show a similar
message naming the saved rules or rule changes.
[Check a saved change](backup-recovery.md#a-change-failed-or-its-result-is-unclear).

**“SpentIt could not confirm whether the import was saved.”** /
**“SpentIt could not confirm whether your change was saved.”**

It may already have completed. Reopen the file and check the affected
transactions before trying again.
[Check an uncertain result](backup-recovery.md#a-change-failed-or-its-result-is-unclear).

**“Couldn’t finish checking the file”**

Choose **Return** to go back to the app. **Quit anyway** closes it;
SpentIt checks again next time you open the file. Neither choice repairs damage.
[Checking on quit](backup-recovery.md#checking-could-not-finish-on-quit).

**“Couldn’t close the file”**

Closing did not finish. **Return** keeps you in the file so you can try
again. **Quit anyway** closes SpentIt and checks the file the next time
you open it. Neither choice repairs damage. Undo remains until a close
succeeds.
[Checking on quit](backup-recovery.md#checking-could-not-finish-on-quit).

**“Undo the current actions, or close and reopen the file first.”**

**Discard new transactions…**, and undoing an import that is not the
latest action, wait until Undo is empty. Undo the actions beside search,
or close the file successfully, then try again.
[When Undo is no longer offered](add-correct-transactions.md#undo-a-saved-change).

**“Statement contains conflicting fields; no import was saved. Obtain a corrected export and try again.”**

**“The statement balance could not be read; no import was saved. Obtain a corrected export and try again.”**

Get a corrected export. These refusals happen before you choose accounts.
A statement that simply has no balance can still be imported.
[When something needs attention](import-transactions.md#when-something-needs-attention).

**“Choose an existing category.”**

The selected rule has no category yet. Choose one in its editor.
[Set up a rule](rules.md#make-a-market-rule).

If your message is not listed, read [When a file or change needs attention](backup-recovery.md#when-a-file-or-change-needs-attention).
