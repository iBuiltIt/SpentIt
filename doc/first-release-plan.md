# First release decisions

Last updated: 2026-11-11
Status: **accepted direction**. Implementation of a change still needs its own Go.
This document does not authorize packaging publication or release.

## Accepted first-release scope

“Finished” means a reliable first public release with an explicit supported
scope, not every possible extension.

| Decision | Accepted answer |
| --- | --- |
| Core workflow | Release 1 is the existing local accounts, supported imports, manual entries, Review, rules, editing, reports, recovery, and Help. A backup is a copy of the closed file. Do not add new financial capabilities. |
| Platforms | macOS first. Windows and Linux remain development builds until current tests, recovery, and packages pass. Select supported OS versions from actual testing. No version range is promised. |
| Remaining retention cleanup | Category, undone Review-link, exclusion, and account/opening/currency histories stay deferred unless verification finds a correctness problem or unacceptable growth. Each remediation needs discussion and Go. Ruleset retention and the legacy staged-opening path are already done. |
| Rule inputs | Match immutable imported fields. Preserve Uncategorized gating, explicit categories, split-first order, selected-rule ordering, manual text protection, and the prohibition on creating transfers. Defer current-value matching and explicit imported-original conditions. |
| Account moves and source repair | Defer moving transactions between accounts and repairing source associations. A later design must cover imports, recognition, relationships, and Undo atomically. |
| Transfers and currencies | Defer automatic cross-currency matching, fee matching, and broader fuzzy automatic matching. An explicit Link of two allocations may join different currencies and keeps each posted amount. No rate is stored. Reports stay separated by currency. |
| Essential UI work | Release 1 uses the interface in the application. Do not open a further UI program. |
| UI extensions | Defer multiline-note editing and additional layout customization. Current workflows still have to be usable. |
| Password changes | Settings can change, add, or remove a password. A copy keeps the password it was made with. There is no forgotten-password recovery. |
| File compatibility | The announced first release starts the compatibility commitment. Until then, experimental formats stay disposable and older files stay rejected. When the owner says to make the release schema, follow [release schema](release-schema.md): the rules database becomes version 1, with no migrator in that change. After release, require a supported upgrade and recovery policy. Never silently invalidate a released file. |
| Verification | Establish a release-wide regression baseline for every shipped platform, resolve failures, check financial faults and recovery, open a copy of a closed file, and walk through key native workflows. Fix stale fixtures without weakening assertions. |
| Manual | The bundled manual is current. A later user-visible change goes in the manual backlog and waits for an owner-authorized pass. |
| Distribution | SpentIt is under the Unlicense. The macOS package command has produced one notarized DMG. Publication still needs explicit approval. Windows and Linux archives are not implemented. |

Windows has a development build with a failing UI check and excluded storage
tests. WSL has failures. That is not a claim that Windows cannot build, and it
is not release readiness. See [Windows setup](windows-development-setup.md).

## License

The owner selected the Unlicense for SpentIt. The text is [LICENSE](../LICENSE).
Third-party components keep their own licenses. Package publication still needs
explicit approval. See [feature 007](../features/007-licensing-and-distribution.md).

## Sequence

1. Build, test, and package on Windows and Linux. Fix blockers those runs find.
2. When the owner says to make the release schema, carry out
   [release schema](release-schema.md) and set the application version.
3. Obtain explicit approval before publication.
