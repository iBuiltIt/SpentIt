---
id: look-after-your-work
title: Look after your work and resolve remaining problems
order: 12
keywords: backup, restore, recovery, settings, password, disk full, interrupted, failed write, older file, troubleshooting
---

# Look after your work and resolve remaining problems

Once you’ve brought your accounts together, keep a backup you can return to.
It holds your saved work at that point, including transactions you haven’t
accepted yet. Finish any edit with **Save** or **Enter** first.

## Save a backup

1. Open **Settings** in the left sidebar. Check the path under **Current file**
   so you know which file you’re copying.
2. Choose **Save backup…**, or **File → Backup…**. They are the same
   command. Choose a folder and a new filename, such as
   `Household-backup-2026-02-28.spentit`. Use a different name for each backup.
3. Wait for the operation to finish. The bottom status line returns to
   **Ready** on success; there is no separate completion dialog.

![Settings for the Household file, with Check file, Save backup and the current file path.](images/recovery-backup.png)
*Save backup makes a separate copy. The Current file path still names the file you are working in.*

Keep a copy on another drive if you want protection against losing the drive
that holds your working file. A backup only contains work saved before it was
made; later changes need another backup.

A password-protected backup needs the same password as its source file.
A backup made without password protection can be read by anyone who has it.

### If the backup does not finish

- **The filename already exists:** choose a new name. Save backup does not
  replace an existing file, even if the file picker offers to replace it.
  The message may only say “Backup publication failed; encrypted candidate retained”.
- **The location is unavailable or full:** check the drive, free space and
  permission to save there, then choose a new filename.
- **You cancelled:** wait for cancellation to finish. An unfinished `.partial`
  file may remain; it is not a completed backup. Use a new name when retrying.

![Settings showing Backup publication failed, with Check file and a Details button.](images/recovery-existing.png)
*Here, an existing destination is refused. Choose another name. Use Details to read a long message.*

## Check a backup or recover your work

Open a backup to check that it contains what you expect. If you need to recover,
make a new working copy from it so you can keep the backup for another attempt.

1. Choose **File → Close file**. Resolve any unfinished edits first.
   Settings has no Close file button. Use **Back**, or an account in the
   sidebar, to leave Settings.
2. On the welcome screen, use **Browse…** to select the backup, enter its
   password if asked, and choose **Open file**.
3. Check the accounts, balances and recent transactions. Open a saved report
   if it helps you check the period you need.
4. To recover, open Settings and use **Save backup…** again, this time with
   a new working name such as `Household-recovered.spentit`.
5. After it finishes, close the backup and open that new working copy.
   Check **Current file** before entering more transactions.

Saving the copy does not switch to it. If you were only checking a backup,
close it and reopen your usual working file instead.

Work added after the backup was made will not be in the recovered copy.
Check what is missing before importing or entering it again.

## Adjust how you read and enter amounts {#adjust-how-you-read-and-enter-amounts}

Settings shows the file’s **Date format**. That format was chosen when the
file was created; there is no control here to change it.

With **Use amount formatting** checked, choose `1,234.56`, `1.234,56`, or
`1 234,56`. The choice is saved for this file and applies to amounts and
calculations you type as well as amounts you read. Thousands separators are
optional, but any you use must be grouped correctly.

Uncheck it for ungrouped numbers with a decimal point, such as `1234.56`.
Changing presentation does not change the amounts or convert currencies.
See [Enter an amount or a calculation](add-correct-transactions.md#enter-an-amount-or-a-calculation)
for examples.

**Remember window size and position** controls how the application window
opens next time. **Check file** checks the open file. It does not clear
Undo. After SpentIt stops unexpectedly, it checks the file when you open
it again.

Settings has no control to change a password or turn password
protection on or off for an existing file. There is no forgotten-password
recovery for the file or its backups.

## Copy transactions or saved definitions {#copy-transactions-or-saved-definitions}

A backup is an encrypted copy of the whole file. The File menu Export
commands write ordinary text. They are not backups. If you have an
unfinished edit, SpentIt asks you to save or discard it first before
using these commands.

**QIF Export…** copies transactions. The accounts start checked, including
archived ones. Set **From** and **To** for each account you want. Both
dates are included. Leave **Include categories** checked to keep ordinary
categories. Leave **Include opening balance** checked to include an opening.
A range that covers the whole account uses the stored opening. A range that
starts later adds the earlier transactions into the opening, so the balance
still matches. Uncheck it for no opening line. Transfers are included either
way. Choose **Export…**, then a new filename. Line breaks in the export
become spaces. Your transactions in SpentIt stay as they are. The file is
plain text. Anyone who has it can read it. When you import it again, each
account still needs its currency. SpentIt does not convert amounts.

**SpentIt Export…** copies saved **Rules**, **Reports** and **Categories**.
They start checked. It does not copy transactions, or edits you have not
saved. Choose **Export…** and a new filename. The file is unencrypted text.
If the export fails or you cancel it, an existing file is left as it was.

**SpentIt Import…** reads that file into the open SpentIt file. Each section
starts with its items selected. **All** selects or clears that section.
Rules and reports are added as new items, even when a name matches one you
already have. The imported rules are not applied. Categories merge by name.
If the Income setting differs, choose **Keep existing** or **Use imported**.
A missing category must be selected so SpentIt can create it, or mapped to
a category you already have. A missing account must be mapped to an account
with the same currency. Choose **Import** when the choices are complete.
**Cancel** stops the step while that is still possible.

## When a file or change needs attention

### The file will not open

First check the [opening checks](create-open-file.md#if-it-won-t-open): the
selected file, password, drive and whether another SpentIt window has it open.
Do not delete lock files to get around an “already open” message.

Older experimental files may be refused as an unsupported format. This version
has no upgrade or conversion command. Keep the old file; do not overwrite it
with a new empty one. A backup in the same old format will have the same limit.

If the message says the file failed verification or is damaged, do not keep
trying to change it. Try opening a separate backup and checking it as described
above. SpentIt has no general repair button.

### A change failed, or its result is unclear

Read the message before trying again. Choose **Details** if the bottom line
cuts it short.

- **The change was saved, but the screen could not update:** reopen the file
  and inspect the affected transactions, account or rule. Do not repeat the
  change just to refresh the screen.
- **SpentIt could not confirm whether the change was saved:** reopen and check
  the result before retrying. It may already have completed.
- **A write failed:** check available space, drive connection and permission
  to write. Reopen and inspect the affected work before deciding what to repeat.

After an interruption, reopen the file and check Review. If SpentIt says an
incomplete import was removed, you can import that export again. Completed
imports and saved new transactions do not need accepting merely to survive
closing the app. See [Check the result](import-transactions.md#check-the-result)
for records that still need attention.

### Checking could not finish on quit {#checking-could-not-finish-on-quit}

While SpentIt is closing, wait for **Closing SpentIt…** to finish.

If **Couldn’t finish checking the file** appears, **Return** brings you back
to the app. **Quit anyway** closes it; SpentIt will check the file again the
next time you open it. Neither choice repairs a damaged file.

If closing itself cannot finish, the title is **Couldn’t close the file**.
**Return** brings you back so you can try again. **Quit anyway** closes
SpentIt and checks the file the next time you open it. Neither repairs
damage. Undo remains until a close succeeds. See
[When Undo is no longer offered](add-correct-transactions.md#undo-a-saved-change).

### If you still need help

Note the exact message, the action you took, and your operating system.
On a Mac, choose **About SpentIt** in the application menu. On Windows and
Linux, choose **Help → About SpentIt**. **Copy version information** copies
the version and build. **License and terms…** and **Third-party software…**
open in that window, without a network connection. **Back** returns from
those pages. **Close** leaves About. **Provided as is, without warranty**
is not a promise that a file cannot be damaged. Keep a backup.

A small made-up example can help explain
the problem. Remove names, account details and amounts from screenshots you
share; do not send passwords, your financial file or bank exports.

## Find a particular problem

For an error or status message, see [Messages and problems](messages-problems.md).

- [Missing transaction or unexpected balance](find-transactions.md#if-something-looks-missing-or-wrong)
- [Unreadable imported dates or amounts](import-transactions.md#when-something-needs-attention)
- [Undo a mistaken import](import-transactions.md#undo-a-mistaken-import)
- [Duplicate purchase or transfer question](review-duplicates-transfers.md#is-it-the-same-purchase)
- [Splits do not add up](add-correct-transactions.md#divide-a-purchase-into-splits)
- [Undo an edit or deletion](add-correct-transactions.md#undo-a-saved-change)
- [Category name refused](categories.md#if-a-name-is-refused)
- [Rule matches are not what you expected](rules.md#if-the-matches-aren-t-what-you-expected)
- [Transactions cannot all be accepted](accept-transactions.md#when-review-still-has-questions)
- [Empty, unexpected or out-of-date report](reports.md#if-the-result-needs-attention)
