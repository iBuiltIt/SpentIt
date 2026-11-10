# Accounts

Last updated: 2026-11-09

Account commands are in [feature 002](../features/002-accounts-money-splits.md).
Acceptance and Discard are in
[import-commit-discussion.md](import-commit-discussion.md).

The display name is the creation name or the latest explicit rename. Import
naming does not replace it. The alias list is the creation name, rename
history, and explicitly confirmed named-QIF mappings. Matching is Unicode
case-insensitive and literal, with no fuzzy matching. A name that conflicts
with another account alias or an ordinary category is refused.

Alias reads use bounded caches invalidated by writes. A rollback must not
publish a transactional alias cache. Original import bytes stay unchanged.
