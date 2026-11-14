---
title: Draft help for status messages
status: review draft, not the bundled manual
---

# Status messages that get an explanation

Whittled on 2026-11-13 from the 951 status sentences that can appear beside **Help with this problem** and are not already in the manual.

A sentence stays here only when it does not already say both what happened and what to do, and the text under it would change what a person does. Sentences that already give the next step were removed. So were sentences whose only honest addition is to reopen the file and look. Those still show on the status line. **Status** shows the full sentence when the line cuts it off. They do not get an explanation dialog.

This is a review draft. It is not the bundled manual, and it is not wired to the button.

130 sentences.

## A transfer Undo changes two account balances at once

Undoing this transfer changes the balance in both accounts. It is not undone as an ordinary edit in one account. Undo it from the transfer, or from Change history, so both sides stay together.

## Acceptance cannot be undone

Accepting transactions is not an Undo step. They stay accepted. You can still change the category, memo, or amount.

## Allocation sum overflow

The split amounts add up to more than SpentIt can store. Use smaller amounts. Nothing was saved.

## Allocations do not equal effective total

The splits do not add up to the transaction amount. Change the split amounts, or change the transaction total. SpentIt will not save a transaction whose parts do not match.

## An account name is a transfer destination. Use the account transfer command rather than saving ordinary category text.

That name is an account, so money to it is a transfer, not a spending category. Choose the other account as the transfer. Do not type the account name into the category.

## Another change ended Undo for this transaction

A later change replaced the Undo step for this transaction. This older step cannot be undone on its own. Look in Change history for the later change.

## At least one allocation is required

A transaction needs at least one split. Add a split, or leave the single amount in place, before you save.

## At most 128 active link exclusions are supported

You already have 128 “not a transfer” exclusions. An exclusion is a note that two transactions are not a transfer. Remove one you no longer need before you add another.

## Backup cancelled. An unfinished .partial file may remain. Use a new filename when you try again.

The copy was cancelled before it finished. A leftover file ending in .partial may be sitting next to the destination. Do not open that leftover as your file. Choose a new name if you copy again.

## Backup publication failed; candidate retained

A replacement copy of the file was not put in place. Keep using the file you already have, with the password it already uses. Do not switch to a leftover copy beside it.

## Backup publication failed; encrypted candidate retained

A replacement copy of the file was not put in place. Keep using the file you already have, with the password it already uses. Do not switch to a leftover copy beside it.

## Backup published; candidate cleanup failed

The new file was saved, but a temporary file beside it could not be removed. Open the saved file and check it before you delete the leftover.

## Cannot protect this file from simultaneous use. Check access to the file and SpentIt's application-data folder.

SpentIt could not lock this file for a single window. Another SpentIt window may already have it open, or the application-data folder cannot be written. Close the other window. Do not open the same file twice.

## Categories allow only one colon separator

A category name can contain one colon, as in Food: Groceries. A second colon is not allowed. Change the name and save again.

## Categories cannot reference parent objects

Type the category as a name, with at most one colon. SpentIt does not store a separate parent category above it.

## Category filters use interpreted transactions, not raw source search

A category search looks at the categories on your transactions. It cannot be combined with a raw QIF: search. Use one or the other.

## Category names cannot contain controls or surrounding whitespace

A category name cannot start or end with a space, and it cannot contain a line break or another hidden character. Retype the name and save.

## Category names must be nonempty UTF-8 segments without surrounding spaces or []|:

A category name cannot be blank, and it cannot contain square brackets or a vertical bar. One colon may separate a group from a name, as in Food: Groceries.

## Change the original transfer before choosing another account.

This side was created from the other transaction. Change that original transaction, or unlink them, before you pick a different account.

## Changing a manual transfer into a category is not this command. Discard the manual origin while it is uncommitted.

This transfer was typed in. To make it an ordinary category, discard the transfer while it is still new, then categorize the transaction. This command will not convert it for you.

## Choose a balancing split

The splits need one row that takes the leftover so they add up to the transaction. Choose that row. SpentIt will not pick it for you.

## Choose an explicit category; category inheritance is unavailable

Pick the category yourself. This edit will not take a category from a parent name.

## Choose an ordinary category or Uncategorized

Transfers are not chosen from the category list. Pick a spending or income category, or leave the transaction Uncategorized. To move money to another account, use a transfer.

## Choose opposite amounts in the same currency

A link needs a payment in one account and a deposit of the same currency in the other. SpentIt does not convert. Choose those two amounts.

## Choose the complete bulk operation to undo

This edit was saved as one change covering several transactions. Select that whole change in Change history, then undo it. One row of it cannot be undone alone.

## Classification cannot be both mapped and unmapped

An imported category cannot be both “use one I already have” and “skip” at the same time. Pick one.

## Clear categories in the ledger before running rules again

Rules fill Uncategorized transactions only. They do not replace a category you already set, and they do not create transfers. Clear the categories you want rules to fill, then run the rules again.

## Committed records are not changed by rules

Rules do not change transactions that are already accepted. They only fill transactions that are still unaccepted and Uncategorized.

## Confirm how to categorise the difference

The two sides are not the same amount. Linking them leaves a remainder. Choose the category for that remainder. SpentIt will not choose it, and it will not drop the difference.

## Create an assignable category using one optional colon, without a parent object

Type a category you can put on a transaction. One colon may separate a group from a name, as in Food: Groceries. Do not build a chain of parent categories.

## Cross-currency transfers are unsupported

The two accounts use different currencies. SpentIt does not convert and does not guess an exchange rate. The transfer was not created. Use two accounts in the same currency.

## Currency cannot be selected for a new account

The currency is chosen when the account is created. This command cannot change it afterward. Amounts are not converted. Create the account again if you need a different currency.

## Currency code requires three uppercase ASCII letters

A currency is a three-letter code such as USD or EUR. A word or a symbol is not accepted. SpentIt does not convert one currency into another.

## Currency definitions differ

These amounts are not using the same currency record. SpentIt did not treat them as one currency and did not convert. The change was not saved.

## Currency is not in the supported catalog

That currency is not one SpentIt knows. The amount was not saved. SpentIt will not invent a currency or convert into one it does know.

## Currency mismatch

The two amounts are in different currencies. SpentIt did not convert. The change was not saved.

## Currency scale mismatch

The two amounts use a different number of decimal places. SpentIt did not round one to fit the other. The change was not saved.

## Dataset preferences do not match format version

The settings stored in this file do not match this version of SpentIt. The file was not changed. Open it with the version that wrote it.

## Dataset replacement could not be completed

SpentIt could not finish replacing the file. Keep using the file you already have. Do not switch to a half-written copy.

## Decimal exceeds currency scale

The amount has more decimal places than this currency allows, and the extra digits are not zero. SpentIt did not drop them. Type the amount with this currency’s decimal places.

## Definition import must append new rules.

This import would change rules you already have. This command only adds rules. Leave the existing rules out of the import.

## Definition import must retain existing reports unchanged.

This import would change reports you already have. Leave those reports out of the import.

## Definition import must retain existing rules unchanged.

This import would change rules you already have. Leave those rules out of the import.

## Delete requires an empty category

Transactions still use this category, so it cannot be deleted. Archive it if you want it out of the list. The transactions keep the category.

## Designate another remainder before editing this row

This split is the row that makes the parts add up to the transaction. Choose a different split as that leftover row, then edit this one.

## Designate another remainder before removing this row

This split is the row that makes the parts add up to the transaction. Choose a different split as that leftover row, then remove this one.

## Document structure does not match format version

This file was written by a different version of SpentIt. It was not converted. Open it with the version that wrote it.

## Edit the existing transfer rather than its matched source

A transfer already exists for this imported line. Change that transfer. A second link was not created.

## Edit the retained transaction

That row was merged into another transaction. Edit the one you kept.

## Enabled rule targets an unavailable category

This rule points at a category that is archived or deleted. Choose a category that is still in the file. The rule was not applied.

## Encrypted or compressed OFX is not supported; no import was saved

This statement is encrypted or compressed. SpentIt did not import it and did not change your file. Export a plain statement from the bank and import that.

## Every source account requires an explicit mapping

Each account in the statement needs a destination account. SpentIt does not guess. Choose one for each, then continue.

## Explicit mixed-sign choice required

These splits contain both money in and money out. Confirm that you mean to keep both signs, then save. SpentIt will not assume that is what you want.

## Explicitly allow mixed signs to save this allocation

These splits contain both money in and money out. Confirm that you mean to keep both signs, then save. SpentIt will not assume that is what you want.

## File label does not match the statement grammar; no import was saved

The file does not read as a QIF, OFX, QFX, QBO, or ASO bank or credit-card statement. SpentIt did not import it and did not change your file. Export the statement again from the bank.

## Import exceeds resource limits

The statement is larger than SpentIt will read in one import. Nothing from this attempt was saved. Split the export into smaller files and import those.

## Import resource limit exceeded

The statement is larger than SpentIt will read in one import. Nothing from this attempt was saved. Split the export into smaller files and import those.

## Invalid decimal syntax

SpentIt could not read that amount as a number. Check the decimal mark and thousands separator in Settings, or type the amount again. The amount was not saved.

## Invalid QIF date/profile

A date in this QIF file does not match the month-first or day-first order you chose. SpentIt did not guess. Choose the order the file uses, or correct the dates, and import again.

## Invalid QIF digit grouping

An amount in this QIF file groups its digits in a way SpentIt cannot read. Correct the amount in the export and import again. Nothing was saved from that amount.

## Invalid UTF-8 or unsupported control character

The file contains text SpentIt will not read, or a hidden control character. Nothing was imported. Export the statement again as plain text.

## Malformed OFX; no import was saved

The statement is not a complete OFX, QFX, QBO, or ASO document. SpentIt did not import it and did not change your file. Export it again from the bank.

## Memory-only temporary storage is required

This copy of SpentIt was built without the temporary-storage setting it requires, so it will not open the file. Use the released SpentIt app.

## Merge transactions within the same account

These transactions are not all in one account. A merge stays inside a single account. Select transactions from one account.

## Money overflow

The amount is larger than SpentIt can store for this currency. Use a smaller amount. Nothing was saved.

## No bank or credit-card statement was found

The file has no bank or credit-card statement. Investment and other statement types are left out. Export a bank or credit-card statement. Nothing was imported.

## No reported balance is present

This statement did not include a balance. You can still import the transactions. For a new account, choose zero or the balance you mean. SpentIt will not invent one.

## OFX header is missing or does not match the body; no import was saved

The start of the statement file does not match the rest of it. SpentIt did not import it and did not change your file. Export the statement again.

## Opening balance is frozen or changed

The opening balance was already set, or it has changed since. Use Correct opening on the account. This command did not change it.

## Opening choices apply only to new accounts

An opening choice from an import applies only while the account is being created. For an account you already have, use Correct opening.

## Password changes are not supported by this gateway

This command cannot change the password on this file. The password is unchanged.

## Password length is invalid

Use a password of 1 to 1,000 characters. An empty password is not accepted on a protected file.

## Password must contain 1–1000 UTF-8 bytes without NUL

Use a password of 1 to 1,000 characters. An empty password is not accepted on a protected file, and a hidden null character is not allowed.

## Pattern compilation failed at byte <n> (code <n>)

The rule pattern is not one SpentIt can use. The number is where reading stopped. You can ignore the code. Correct the pattern and preview again.

## Pattern has too many captures

The rule pattern is too complicated for SpentIt to run. Simplify the pattern and preview again. Nothing was categorized by this attempt.

## Pattern memory limit reached

The rule pattern is too large for SpentIt to run. Shorten it and preview again. Nothing was categorized by this attempt.

## Pattern must be bounded valid UTF-8 without NUL

The rule pattern contains text SpentIt will not use, or a hidden null character. Retype the pattern as ordinary text and preview again.

## Preserve imported transfer allocations; resolve their destinations in Transfers

These imported lines are transfers. Do not turn them into categories here. Choose the other account in Transfers.

## Projection is incomplete; review the pending import

The balance on screen leaves out an import that is not finished. Finish or review that import before you rely on the balance.

## Record repair is not supported by this dataset

This file cannot correct that imported row in place. Fix the export and import it again, or leave the row unresolved.

## Rescale would drop a non-zero fractional digit

Changing the number of decimal places would throw away a digit that is not zero. SpentIt did not round. The amount was left as it was.

## Restored text must match the imported value

Restore puts back the payee or memo from the import. The text you entered is not that original, so it was not saved. Clear your edit, or type the original text.

## Rule application cannot change the account balance

A rule cannot change an amount. It can set a category on an Uncategorized transaction. The amount was left as it was, and this rule run was not saved.

## Rule application cannot replace manual fields

A rule does not replace a category, payee, or memo you already set. Clear that field first if you want the rule to fill it. This rule run was not saved.

## Rule changed an assigned category

A rule tried to replace a category that was already set. Rules only fill Uncategorized transactions. The category was left as you set it, and this rule run was not saved.

## Rule matching failed (code <n>)

The pattern did not run. You can ignore the code. Correct the pattern and preview again. Nothing was categorized by this attempt.

## Rule reapply changed a transfer category

A rule cannot change the category of a transfer. Transfers stay transfers. This rule run was not saved.

## Rules are not enabled for this dataset

This file has no rules. Rule commands are not available. Categories you set yourself are unchanged.

## Rules are unavailable in this file.

This file has no rules. Rule commands are not available. Categories you set yourself are unchanged.

## Rules require an imported split

This rule applies to one split that came from the import. Choose that split. A rule cannot create a transfer.

## Rules require PCRE2 10.49 with Unicode support

This copy of SpentIt cannot run rule patterns. Use the released SpentIt app. Your categories are unchanged.

## Scale differs from supported currency definition

The number of decimal places does not match this currency. SpentIt did not round the amount. Type it with the currency’s decimal places.

## Select imported transactions. Generated transfer entries cannot use this action.

This action applies to a transaction that came from a statement. A transfer line that SpentIt created is not one of those. Select the imported transaction.

## Signon failed and no statement was included; no import was saved

The bank file says the sign-on failed, and it contains no statement. SpentIt did not import it and did not change your file. Download the statement again.

## Split rules cannot change the transaction payee

A rule on a split can set that split’s category or memo. It cannot rename the payee. Rename the payee yourself if that is what you want.

## Statement account id is missing; no import was saved

The statement does not name the account. SpentIt did not import it and did not change your file. Export the statement again.

## Statement currency <code> does not match the target account; no conversion was applied.

The statement’s currency is not the currency of the account you chose. SpentIt did not convert and did not import. Choose an account in that currency, or export a statement in the account’s currency.

## Statement currency is missing or unsupported; no conversion was applied

The statement has no currency SpentIt recognizes. SpentIt did not invent one and did not import. Export the statement again with its currency shown.

## Statement exceeds 64 MiB limit

The statement file is larger than 64 MB. SpentIt did not import it. Split the export into smaller files.

## Statement exceeds a resource limit. The original file is unchanged.

The statement is too large or too deeply nested to read. SpentIt did not import it and did not change your file. Export a smaller file.

## Statement read failed

SpentIt could not read the statement. Nothing was imported. Check that the file is still there, or export it again from the bank.

## Statement read failed or exceeded limit

SpentIt could not finish reading the statement, or the statement is over a size limit. Nothing was imported. Export a smaller file and try again.

## Statement status is not success; no import was saved

The bank file says the statement was not produced successfully. SpentIt did not import it and did not change your file. Download it again.

## Template must be bounded valid UTF-8 without NUL

The rule pattern contains text SpentIt will not use, or a hidden null character. Retype the pattern as ordinary text and preview again.

## That prior occurrence is already matched in this import

This statement line was already matched to a transaction in this import. It was not matched again.

## The origin controls this generated draft. Change or discard the origin instead of deleting the destination.

SpentIt created this side of the transfer from the other transaction. Change or delete that original side. This generated side follows it.

## These allocations do not meet this suggestion's matching policy; use explicit manual Link instead

These two amounts are too far apart in date, size, or account for the suggested match. The link was not made. Select both transactions and choose Link if they are the transfer you mean.

## These transactions are linked to other accounts. They stay in this account.

Delete does not remove a transaction that is linked to another account. The link stays as it is. Select the transactions that are not linked, then choose Delete again.

## This account uses another currency. Select both transactions and choose Link.

The other account uses a different currency. SpentIt will not convert the amount. If you still want them connected, select both transactions and choose Link. Otherwise leave them unlinked.

## This action cannot yet be undone.

This kind of change is not on the Undo list, so it was not undone. You can still correct the transaction by editing it.

## This change belongs to a selection edit and cannot be undone individually.

This transaction was changed as part of a selection. Undo that whole change from Change history. One row of it cannot be undone alone.

## This change belongs to an earlier file session and can no longer be undone.

This change is from before the file was last closed successfully. Ordinary Undo no longer reaches it. You can still edit the transaction.

## This correction would change a linked transfer balance

Changing this amount would change the other side of a transfer. The edit was not saved. Unlink the transfer, or edit it so one side stays a payment and the other a deposit of the same size.

## This link preserves amounts in two currencies. It is not a match and this command does not repair it.

This link keeps two amounts in different currencies. SpentIt does not treat it as a matched transfer and will not convert or repair it here.

## This QIF file contains conflicting date orders. Correct the source dates and import again; no import was saved.

Some dates in the file only make sense month-first, and others only day-first. SpentIt did not guess and did not import. Correct the dates and import the file again.

## This record cannot be repaired

This imported row has a problem SpentIt will not correct for you, such as a broken split. Fix the export and import it again, or skip the row.

## This transfer command would change committed money

This would change an amount on a transaction you already accepted. SpentIt did not save it. Correct the amount in the ledger if it is wrong.

## Too many splits

The transaction has more splits than SpentIt will store. Combine some splits, then save.

## Transfer amount does not reconcile

The two sides are not a payment and a deposit of the same size. SpentIt did not pick a side and did not drop the difference. The transfer was not saved.

## Transfer amounts disagree; explicit resolution is required

The two sides are not the same amount. SpentIt did not pick a side and did not drop the difference. Edit one side, or choose a category for the difference.

## Transfer-linked entries require transfer-aware editing; this command is unavailable

This transaction is one side of a transfer. Edit it from the transfer so the other account stays in step. This ordinary edit was not saved.

## Unable to authenticate encrypted dataset

SpentIt could not unlock the file. Retype the password. A damaged file can show the same message, so this does not prove the password is wrong. There is no password reset.

## Unclosed search quote

A quotation mark in the search was never closed. Close it, or delete the quoted word, then search again. The results still on screen are the previous search.

## Unexpected SQLCipher version

This copy of SpentIt cannot read the file’s encryption. Open it with the SpentIt version that created it. The file was not changed.

## Unknown or unresolved currency definition

That currency is not one SpentIt can use. The amount was not saved, and nothing was converted.

## Unknown period month

The month name in the search is not one SpentIt recognizes. Use a full name such as january, or type the dates.

## Unsupported account section structure; no import was saved. The original file is unchanged.

The QIF file has an account section SpentIt cannot read. Nothing was imported, and your file was not changed. Export the accounts again.

## Unsupported account section structure; source file remains unchanged

The QIF account section cannot be read. The statement file and your SpentIt file were left unchanged. Export the accounts again.

## Unsupported document schema; no migration was attempted

This file was written by a different version of SpentIt. It was not converted. Open it with the version that wrote it.

## Unsupported OFX message set; no transactions were imported

The statement is not a bank or credit-card statement SpentIt imports. Investment statements are left out. No transactions were imported. Export a bank or credit-card statement.

## Use whole-import Abort for this grouped source

This statement was imported as one group across accounts. Cancel the whole import. One account from the group cannot be cancelled alone.
