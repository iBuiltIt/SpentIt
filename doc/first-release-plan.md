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
| Core workflow | Finish the existing local accounts, supported imports, manual entries, Review, rules, editing, reports, backup/recovery, and Help. Defer new financial capabilities. |
| Platforms | macOS first. Windows and Linux remain development builds until current tests, recovery, and packages pass. Select supported OS versions from actual testing. No version range is promised. |
| Remaining retention cleanup | Category, undone Review-link, exclusion, and account/opening/currency histories stay deferred unless verification finds a correctness problem or unacceptable growth. Each remediation needs discussion and Go. Ruleset retention and the legacy staged-opening path are already done. |
| Rule inputs | Match current transaction and split values for manual and imported entries. Preserve Uncategorized gating, explicit categories, split-first order, selected-rule ordering, manual text protection, and the prohibition on creating transfers. Defer explicit imported-original conditions. Implement before release with clear previews. Use an experimental-format boundary if needed so existing files are not reinterpreted silently. |
| Account moves and source repair | Defer moving transactions between accounts and repairing source associations. A later design must cover imports, recognition, relationships, and Undo atomically. |
| Transfers and currencies | Defer cross-currency and fee matching, and broader fuzzy automatic matching. Keep existing explicit linking and bounded recognition. Reports stay separated by currency. |
| Essential UI work | The usage-driven UI pass is complete. Complete the busy/readiness audit: feedback lasts until the resulting screen is usable, and conflicting actions are unavailable. Fix clipping, focus, and keyboard defects that obstruct existing workflows. |
| UI extensions | Defer multiline-note editing and additional layout customization. Current workflows still have to be usable. |
| Password changes | Defer for the first release and say so. Later changes need a recoverable rekey workflow. Forgotten-password recovery remains unavailable. |
| File compatibility | The announced first release starts the compatibility commitment. Until then, experimental formats stay disposable and older files stay rejected. After release, require a supported upgrade and recovery policy. Never silently invalidate a released file. |
| Verification | Establish a release-wide regression baseline for every shipped platform, resolve failures, check financial faults and recovery, restore backups, and walk through key native workflows. Fix stale fixtures without weakening assertions. |
| Manual | The 2026-11-09 pass covered MAN-001 through MAN-020. Freeze release behavior, then update the manual for changes after that pass. Verify screenshots, examples, links, and contextual Help against the release candidate. |
| Distribution | Select exact license terms, assemble dependency notices, finish tested packages and signing or notarization, then obtain explicit publication approval. |

Windows has a development build with a failing UI check and excluded storage
tests. WSL has failures. That is not a claim that Windows cannot build, and it
is not release readiness. See [Windows setup](windows-development-setup.md).

## Accepted licensing policy

Free personal and internal business use, no sale of SpentIt or derivatives, and
permanent free access are settled.

- Prohibit charging for access to SpentIt or a hosted derivative.
- Allow separately purchased human support, training, and consulting, provided
  the software remains free to obtain and use.
- Allow commissioned development. Distribution of the result stays under the
  same free-access and no-resale terms.
- Prohibit charging for SpentIt through a bundle. Allow clearly itemized actual
  media and postage costs.
- Require corresponding source for distributed derivatives under the same terms.
  Private internal modifications need not be published.
- Select and review concrete license terms against these decisions before
  publication. Do not improvise or install an assumed license.

These are policy requirements, not an operative license. The intended
classification is source-available, not OSI open source. Third-party terms stay
independent. See [feature 007](../features/007-licensing-and-distribution.md).

## Sequence

1. Implement current-value rule matching and the essential busy/readiness work
   under separate authorization.
2. Complete release verification and fix discovered blockers.
3. Refresh the manual against frozen release behavior.
4. Finalize licensing and packages. Obtain explicit approval before publication.

Remaining inputs are designs and Go for each change, a tested OS range,
reviewed license text, and release approval.
