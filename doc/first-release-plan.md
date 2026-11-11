# First release decisions

Last updated: 2026-11-09
Status: **accepted direction**. Implementation of a change still needs its own Go.
This document does not authorize license installation, packaging publication,
or release.

The owner's usage-driven UI pass is complete. Those changes were made piecemeal
and are in the application. Discuss any further requested change before
implementation Go.

Current-value rule matching, below, is accepted behavior and is not implemented.

## Accepted first-release scope

“Finished” means a reliable first public release with an explicit supported
scope, not every possible extension.

| Decision | Accepted answer |
| --- | --- |
| Core workflow | Finish the existing local accounts, supported imports, manual entries, Review, rules, editing, reports, recovery, and Help. A backup is a copy of the closed file. Defer new financial capabilities. |
| Platforms | macOS first. Windows and Linux remain development builds until current tests, recovery, and packages pass. Select supported OS versions from actual testing. No version range is promised. |
| Remaining retention cleanup | Category, undone Review-link, exclusion, and account/opening/currency histories stay deferred unless verification finds a correctness problem or unacceptable growth. Each remediation needs discussion and Go. Ruleset retention and the legacy staged-opening path are already done. |
| Rule inputs | Match current transaction and split values for manual and imported entries. Preserve Uncategorized gating, explicit categories, split-first order, selected-rule ordering, manual text protection, and the prohibition on creating transfers. Defer explicit imported-original conditions. Implement before release with clear previews. Use an experimental-format boundary if needed so existing files are not reinterpreted silently. |
| Account moves and source repair | Defer moving transactions between accounts and repairing source associations. A later design must cover imports, recognition, relationships, and Undo atomically. |
| Transfers and currencies | Defer automatic cross-currency matching, fee matching, and broader fuzzy automatic matching. An explicit Link of two allocations may join different currencies and keeps each posted amount. No rate is stored. Reports stay separated by currency. |
| Essential UI work | The usage-driven UI pass is complete. Complete the busy/readiness audit: feedback lasts until the resulting screen is usable, and conflicting actions are unavailable. Fix clipping, focus, and keyboard defects that obstruct existing workflows. |
| UI extensions | Defer multiline-note editing and additional layout customization. Current workflows still have to be usable. |
| Password changes | Defer for the first release and say so. Later changes need a recoverable rekey workflow. Forgotten-password recovery remains unavailable. |
| File compatibility | The announced first release starts the compatibility commitment. Until then, experimental formats stay disposable and older files stay rejected. When the owner says to make the release schema, follow [release schema](release-schema.md): the rules database becomes version 1, with no migrator in that change. After release, require a supported upgrade and recovery policy. Never silently invalidate a released file. |
| Verification | Establish a release-wide regression baseline for every shipped platform, resolve failures, check financial faults and recovery, open a copy of a closed file, and walk through key native workflows. Fix stale fixtures without weakening assertions. |
| Manual | The 2026-11-09 pass covered MAN-001 through MAN-020. Freeze release behavior, then update the manual for changes after that pass. Verify screenshots, examples, links, and contextual Help against the release candidate. |
| Distribution | SpentIt is under the Unlicense. Assemble dependency notices for each shipped build, finish tested packages and signing or notarization, then obtain explicit publication approval. |

Windows has a development build with a failing UI check and excluded storage
tests. WSL has failures. That is not a claim that Windows cannot build, and it
is not release readiness. See [Windows setup](windows-development-setup.md).

## License

The owner selected the Unlicense for SpentIt. The text is [LICENSE](../LICENSE).
Third-party components keep their own licenses. Package publication still needs
explicit approval. See [feature 007](../features/007-licensing-and-distribution.md).

## Sequence

1. Implement current-value rule matching and the essential busy/readiness work
   under separate authorization.
2. Complete release verification and fix discovered blockers.
3. Refresh the manual against frozen release behavior.
4. When the owner says to make the release schema, carry out
   [release schema](release-schema.md).
5. Finish packages and obtain explicit approval before publication.

Remaining inputs are designs and Go for each change, a tested OS range, and
release approval.
