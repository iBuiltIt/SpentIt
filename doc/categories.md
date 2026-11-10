# Categories

Last updated: 2026-11-09

Current behavior is [feature 010](../features/010-categories-and-selection.md).

Stable allocation identities and original source-allocation associations stay
with the allocation. Reordering splits does not invent a new source association
from the display index. A new manual split has no source association. Explicit
manual edits, including blanks, stay manual. A caller cannot clear that
protection by relabeling ownership. A rule mark is stored only with a
rule-owned field.

Category history that is no longer referenced is part of the deferred retention
cleanup. Do not remove it without a separate Go.
