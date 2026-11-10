# Document session locks

Owner Go: 2026-11-04. Implemented scope and executed checks are recorded in
[status](status.md).

## Accepted behavior

Ordinary private local datasets and backup candidates do not create adjacent
`.lock` files. Empty session-lock files persist in application data instead:

| Platform | Location |
| --- | --- |
| macOS | `~/Library/Application Support/SpentIt/Locks/` |
| Windows | Local AppData known folder, then `SpentIt/Locks/` |
| Linux | `$XDG_DATA_HOME/SpentIt/Locks/`, or `~/.local/share/SpentIt/Locks/` |

Storage resolves these locations with native OS account/known-folder APIs and
the Linux XDG convention; it does not depend on Qt or the desktop's historical
“Durable import experiment” settings identity. Lock filenames contain SHA-256
identifiers, not account names, passwords or transaction data. Hashing is not a
confidentiality guarantee for guessable paths. Files contain no document data.

The kernel owns the lock until the session closes or its process exits. Locks
are never stolen, expired, or deleted to obtain access. This is still separate
from SQLCipher's transaction locks. Journal, encryption and durability settings
are unchanged. There is no document-format change.

## Coordination and failure behavior

Every storage entry point uses the same default namespace, including create,
open, cached open, backup creation and backup verification. The lock directory
must be a real, owner-private directory on a recognized local filesystem.
Failure to establish that directory rejects the operation; it does not silently
fall back to an unprotected session. Applications running as the same OS user
must use the same application-data location; changing XDG configuration between
concurrent instances is not supported.

A path reservation uses the parent directory's filesystem identity and the
requested filename. An additional file-identity lock uses the volume/device and
file identifier, preventing an existing file from bypassing the session lock
through a rename, alternate parent path or case/Unicode alias. Creation obtains
the reservation before exclusive creation and binds the new identity before
opening SQLCipher. Overwrite locks the existing identity before truncating it.
Identity is checked again before and after opening the database. Dataset
symlinks and multiply linked files remain rejected.

Lock files are opened without following symlinks, checked for ownership and
regular-file identity, and held until after the database closes. POSIX lock
descriptors are close-on-exec. Acquisition is nonblocking. A failed acquisition
releases all locks already taken by that attempt. The test-only constructor
directory argument allows isolated lock-mechanism tests; ImportStore always
uses the platform default.

## Shared and unsupported locations

The current conservative local-filesystem recognition is APFS/HFS on macOS;
ext-family, XFS, Btrfs and tmpfs on Linux; and local fixed/removable/RAM drives
with persistent ACL support on Windows. This is a safety classification, not a
claim that every listed filesystem or platform has been tested.

An unrecognized or remote dataset filesystem retains the adjacent lock as well
as the private locks. The adjacent lock is also retained when an existing
dataset is not owned exclusively by the current user: POSIX owner-only mode
bits are required, macOS extended ACLs are conservatively treated as shared,
and Windows requires an owner-only DACL. Permission changes cannot bypass the
same user's private identity lock. External permission/ownership changes while
a dataset is open are not a supported sharing mechanism.

Shared sidecars retain the earlier ownership checks and filesystem-dependent
locking limits; this change does not promise multi-user or network-database
support. A nonlocal or unsupported application-data filesystem is rejected.
Unknown filesystems do not silently receive weaker private-only coordination.

## Transition and cleanup

Existing adjacent `.lock` files are acquired and retained, including for private
datasets. This honors a live older holder and avoids unsafe automatic unlinking.
The application does not remove existing sidecars. New local backup candidates
do not leave `.partial.lock` files unless the fallback/legacy policy applies.

All copies of SpentIt that access a dataset must be updated before retiring its
old lock file. After all older and current sessions are fully closed, an owner
may remove an empty legacy `.lock` or `.partial.lock` file once. A subsequent
private local open will use only application-data locks. Older binaries must
not subsequently access that dataset: they can create an independent adjacent
lock and do not understand the new namespace. Automatic compatibility with
those experimental binaries is not claimed.

Never remove the active application-data lock directory. Persistent empty files
there are intentional; automatic pruning would reintroduce the unlink race.
SQLite recovery journals and incomplete backup datasets are not lock files and
must not be removed as part of this housekeeping.

## Verification requirements

Exercise competing processes before creation and during a session; renamed
files and parent aliases; overwrite rejection without changing existing bytes;
forced process termination and retry; shared/legacy fallback; malformed lock
paths, permissions and symlinks; failed authentication; backup publication,
cancellation and restore; and storage I/O/crash recovery. Native Windows/Linux,
network shares, cross-user access and filesystem variants require their own
execution evidence before portability claims. See status for actual results.
