# 001 — Documents, storage, and security

Status: **active**
Last updated: 2026-11-21

Encrypted local files are the application. One running process has one active
file. There is no application-wide instance limit. Settings remembers the file
path. Close the file before opening another. Accounts in one file are not
separate users. All accounts share that file so cross-account operations can
be atomic.

## Protection

Password protection defaults on at creation. The user may turn it off and
knowingly waive confidentiality. No-password mode still uses the encrypted
format with a fixed public format key, documented independently of one
installation. That key is not a user password and must not unlock a
password-protected file. A wrong password and damaged ciphertext can look the
same: say the file could not be unlocked. Do not claim the password is
definitely wrong.

There is no forgotten-password recovery. Settings can change a file's password,
add one, or remove protection after confirmation. A copy made beforehand keeps
the password it was made with. A wrong current password leaves the file unchanged.

A password is 1 to 64 characters. An empty password says “Enter a password.”
A longer password says “Use a password of 1 to 64 characters.” and leaves the
password box in place. Help for that sentence says the password is stored as
UTF-8 text. This check does not change the file format.

## Creation and welcome

Creation defaults to the account name SpentIt, CAD, password protection on, and
**Use default categories** checked. The date-format selector offers six presets
and a custom pattern of `YYYY`, `MM`, and `DD` once each. The
suggestion follows the chosen currency (`MM/DD/YYYY` for USD, `DD/MM/YYYY` for
GBP, AUD, and NZD, `YYYY-MM-DD` otherwise) and stays if the user overrides it.
Imported dates use the source parser, not this display preference. Stored dates
are canonical Gregorian dates with no timezone conversion.

In Settings → Current file settings, the same selector and example let the
user save a new format for the open file. The choice takes effect immediately
for date display and entry, including ledger searches and reports. Existing
calendar dates and original imported evidence are preserved. Saving the
preference preserves ordinary Undo and clears pending Redo, like other file
settings. See [release schema](../doc/release-schema.md).

If the destination exists, Cancel leaves it alone and Overwrite replaces it at
creation time. Overwrite is not a backup. Do not replace a file another process
holds open, and do not follow a symlink. A failure can leave an incomplete
replacement; say so and allow another explicit overwrite.

Startup tries the last-used no-password file once. Success skips the welcome
screen. Otherwise the welcome screen offers Open, New, Browse, and a password
when needed. Confirming a file in Browse starts opening it. If a password is
needed, the password field receives focus. Press Return or choose Open file to
submit the password. Cancelling Browse leaves the welcome screen as it was.
Choosing Open file with an empty password leaves the password box in place and
says “Enter a password.” File → Close returns to the welcome screen with the
file closed.

The welcome screen shows SpentIt, “Where did my money go?”, the file prompt,
and errors. The logo follows system appearance.

## Open, close, backup, and locks

Normal open authenticates, checks the format, and uses the encrypted clean
marker. It does not run a full audit or hash the file. An unclean marker or a
recovery journal requires a full check before writable use. Successful close
expires ordinary Undo and marks the file clean in one transaction. It does not
accept transactions, hash the file, or copy the database. A failed close rolls
back and keeps the session. The full check reports errors and does not repair data.
See [session history](../doc/session-history-baseline.md) and
[open/close checks](../doc/fast-startup.md).

The application does not write a backup. After a successful close, and while
SpentIt is not running, a copy of the file is another file of the same format.
It opens with Browse under its own name and the same password. A copy taken
while the file is open, or after a crash before a successful close, is not a
consistent copy. Do not add an in-application backup command.

Private local files lock under the platform
application-data directory. See [document locking](../doc/document-locking.md).
