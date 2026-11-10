# 001 — Documents, storage, and security

Status: **active**
Last updated: 2026-11-09

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

There is no forgotten-password recovery. Changing a file's password is deferred
for the first release. Old backups keep the password they were created with.

## Creation and welcome

Creation defaults to the account name SpentIt, CAD, password protection on, and
**Use default categories** checked. The date-format selector offers the presets
in Settings and a custom pattern of `YYYY`, `MM`, and `DD` once each. The
suggestion follows the chosen currency (`MM/DD/YYYY` for USD, `DD/MM/YYYY` for
GBP, AUD, and NZD, `YYYY-MM-DD` otherwise) and stays if the user overrides it.
Imported dates use the source parser, not this display preference. Stored dates
are canonical Gregorian dates with no timezone conversion.

If the destination exists, Cancel leaves it alone and Overwrite replaces it at
creation time. Overwrite is not a backup. Do not replace a file another process
holds open, and do not follow a symlink. A failure can leave an incomplete
replacement; say so and allow another explicit overwrite.

Startup tries the last-used no-password file once. Success skips the welcome
screen. Otherwise the welcome screen offers Open, New, Browse, and a password
when needed. File → Close returns to that screen and does not reopen the file
just closed.

The welcome screen shows SpentIt, “Where did my money go?”, the file prompt,
and errors. The logo follows system appearance.

## Open, close, backup, and locks

Normal open authenticates, checks the format, and uses the encrypted clean
marker. It does not run a full audit or hash the file. An unclean marker or a
recovery journal requires a full check before writable use. Successful close
expires ordinary Undo and marks the file clean in one transaction. It does not
accept transactions, hash the file, or copy the database. A failed close rolls
back and keeps the session. Check file reports errors and does not repair data.
See [session history](../doc/session-history-baseline.md) and
[open/close checks](../doc/fast-startup.md).

Backup copies through the encrypted backup API, verifies the copy, and publishes
it only after that check. Test a backup by restoring it. Cancel is available
before exclusive publication. A cancelled candidate is not a valid backup.

Private local files and backup candidates lock under the platform
application-data directory. See [document locking](../doc/document-locking.md).

## Not done

- Password change and any in-place switch between password and no-password mode.
- A dedicated startup loading screen was left undecided. Do not invent one.
- Release packaging of SQLCipher and the crypto provider. The application build
  is in [building](../doc/building.md).
