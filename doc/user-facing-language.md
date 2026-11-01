# User-facing language

Accepted owner direction, 2026-11-01. Applies to labels, buttons, dialogs,
notifications, progress, help, accessibility text and errors shown by SpentIt.

Before keeping a message, ask:

- What does the user do with this message?
- Do they need it to make a decision?
- Is this the simplest accurate way to say it?

If a sentence answers none of these questions, remove it. Explain the user's
action and its relevant consequences. Do not narrate storage, tracking or
implementation details. Engineering documentation and internal identifiers may
retain precise technical terminology.

## Writing rules

Use familiar financial words and direct verbs. An opening balance editor needs
“Update the opening date and/or balance.” It does not need “source evidence stays
unchanged” or “the previous opening remains in history.” Those statements give
the user nothing to do or decide.

Name the action on its button: Save, Import, Link, Accept transactions, Delete,
Cancel. Avoid Yes/No when the action can be named. A label must describe what the
button actually does. Do not ask users to confirm a value merely because they
just entered it. Editing a value later must remain possible where supported.

Use the same words throughout a workflow:

| Internal concept | User-facing wording |
| --- | --- |
| Evidence / immutable source | Original imported transaction or imported file |
| Commit / committed | Accept transactions / accepted |
| Uncommitted draft | New transaction or transaction not yet accepted |
| Allocation | Split |
| Catalog revision saved | Category updated |
| Ruleset revision saved | Rules saved |
| Dataset | File, where referring to the document |
| Window geometry | Window size and position |

Do not show operation IDs, revision counters, source batch IDs or lists of
internal limits as routine instructions. Show names users recognize. Explain a
limit when it is reached, including what the user can change. Keep useful
matching criteria and amount/date requirements where they guide a choice.

## Consequences and errors

Keep information that changes a decision: which transactions and accounts will
be deleted, whether changing currency converts amounts, whether a file can be
read without a password, and whether a transfer has a matching transaction.
State these consequences concretely, close to the action. Avoid unrelated
history, backup and preservation assurances.

Errors should say what happened and what to do next. Do not conceal a real
problem with a generic success message. Distinguish a change known to be saved
from one whose outcome is unknown:

- “Your change was saved, but the screen could not update. Reopen the file and
  check the transaction before making more changes.”
- “SpentIt could not confirm whether the import was saved. Reopen the file and
  check the imported transactions before trying again.”

Do not recommend repeating an operation when it might already have succeeded.
Do not simplify away financial constraints or claim that data is safe when that
has not been established. Technical details belong in diagnostic contexts unless
the user needs them to act, such as identifying an unreadable file or invalid
input.

## Review practice

Read text in context with the available controls, including empty, busy, error
and recovery states. Remove redundant paragraphs before rewriting them. Check
that the next step exists, the label matches the action, and success wording
matches what actually completed. Preserve internal keys and financial behavior
when changing presentation. Record actual verification and remaining limits in
[status](status.md).
