# First release decisions and next UI work

Last updated: 2026-11-08
Status: **accepted direction; implementation requires a separate Go**.

The owner accepted the decisions below, then placed usage-driven UI improvements
before the release sequence. This document records those decisions; it does not
authorize implementation, license installation, packaging publication or release.
Older handoffs remain historical where they conflict with this accepted scope.
See [current status](status.md) for implemented behavior and actual verification.

## Next phase: usage-driven UI improvements

Change History discussion led to a unified Undo proposal, whose generic per-row
import capture architecture the owner rejected. The [replacement design and
handoff](action-undo-design.md) now describes user-operation restoration plus one
atomic source-owned import rollback. It retains file-wide ordering and reversible
acceptance, permits retained import-created accounts/categories, and explicitly
narrows the definition/removal scope. The owner initially authorized design only,
then authorized implementation in a new session. The replacement is implemented;
see its [verification record](action-undo-implementation.md) for measured storage,
passing checks and remaining limits. The final Change History layout remains
separate. Earlier design approval is not authority to resume the rejected architecture.

Address the owner's items discovered through use first. This is a separate phase
from the essential busy/readiness audit below, and is not limited to About.
The rest of the owner's list has not yet been supplied; do not invent it or treat
About as the complete phase. Discuss each requested change before implementation Go.

The accepted [About requirements](../features/007-licensing-and-distribution.md#about-and-third-party-notices--accepted-2026-11-08)
include application identity/version, the owner-selected **© 2026 Stefan Wessels**,
offline third-party notices and licenses,
and access to the final SpentIt license once selected. The owner subsequently gave
About implementation Go; see [status](status.md) for its verification. The extra
descriptive sentence was explicitly removed. Other UI items still need their own
discussion and Go. MAN-013 records deferred About manual work in the
[manual backlog](manual-update-backlog.md).

The owner subsequently authorized Review cleanup, Rules workspace usability and
ledger context-menu rule creation, followed by Reports workspace usability. These are implemented with verification limits
recorded in [status](status.md); they do not complete the open usage-driven UI phase
or authorize current-value matching. Manual follow-ups are tracked as MAN-014,
MAN-015 and MAN-016 in the [backlog](manual-update-backlog.md).

## Accepted first-release scope

“Finished” means a reliable first public release with explicit supported scope,
not completion of every possible extension.

| Decision | Accepted answer |
| --- | --- |
| Core workflow | Finish existing local accounts, supported imports, manual entries, Review, rules, editing, reports, backup/recovery and Help. Defer new financial capabilities. |
| Platforms | macOS first. Windows and Linux remain development builds until current tests, recovery behavior and packages pass. Select supported OS versions from actual testing; no version range is promised yet. |
| Remaining retention cleanup | Category, undone Review-link, exclusion and account/opening/currency histories remain deferred unless verification finds a correctness problem or unacceptable growth. Record measured limits; each remediation needs discussion and Go. Ruleset retention and legacy staged-opening retirement are already complete. No numerical growth threshold is invented here. |
| Rule inputs | Match current transaction and split values for manual and imported entries. Preserve Uncategorized gating, explicit categories, split-first and selected-rule ordering, manual text protection and the prohibition on creating transfers. Defer explicit imported-original conditions. Implement before release with clear previews; use an experimental-format boundary if needed to avoid silent reinterpretation. This is accepted behavior, not implemented behavior. |
| Account moves and source repair | Defer moving transactions between accounts and repairing source associations. A later design must cover imports, recognition, relationships and Undo atomically. No unsafe delete/recreate shortcut is promised. |
| Transfers and currencies | Defer cross-currency/fee matching and broader fuzzy automatic matching. Retain existing explicit linking and bounded recognition. Reports stay separated by currency; no inferred exchange rates. |
| Essential UI work | Complete the broader busy/readiness audit: feedback lasts until the resulting screen is usable and conflicting actions are unavailable. Fix clipping, focus and keyboard defects obstructing existing workflows. This follows the separate usage-driven UI phase. |
| UI extensions | Defer multiline-note editing and additional layout customization/persistence. These do not remove the obligation to make current workflows usable. |
| Password changes | Defer for the first release and document the limitation. Later changes need a recoverable replacement/rekey workflow. Forgotten-password recovery remains unavailable. |
| File compatibility | The explicitly announced first release starts the compatibility commitment. Until then retain disposable experimental formats and unchanged rejection of older files. After release require a supported upgrade/recovery policy; never silently invalidate released files. |
| Verification | Establish a current release-wide regression baseline for every shipped platform, resolve failures, check financial faults/recovery, restore backups, and walk through key native workflows. Fix stale fixtures without weakening assertions; distinguish deliberate exclusions from passing coverage. |
| Manual | Freeze release behavior, then complete the 12 pending MAN-001 through MAN-012 items together, plus any added by the preceding UI work. Verify screenshots, examples, links and contextual Help against the release candidate. |
| Distribution | Select exact licensing terms, assemble dependency compliance materials, finish tested packages and signing/notarization/installation checks, then obtain explicit publication approval. |

Current evidence includes a Windows build/launch with a failing UI check and
excluded storage tests, and WSL failures. This is not a claim that Windows cannot
build, nor that either platform is release-ready. See [Windows verification](windows-development-setup.md).

## Accepted licensing policy

Free personal and internal business use, no sale of SpentIt or derivatives, and
free access remain settled. The owner also accepted:

- Prohibit charging for access to SpentIt or a hosted derivative.
- Allow separately purchased human support, training and consulting, provided
  the software remains freely available and payment is not required to obtain/use it.
- Allow commissioned development services; distribution of the resulting
  application remains subject to the same free-access/no-resale terms.
- Prohibit charging for SpentIt through a bundle. Allow clearly itemized actual
  media/postage costs.
- Require corresponding source for distributed derivatives under the same terms.
  Private internal modifications need not be published.
- Select and review concrete license terms against these decisions before
  publication. Do not improvise or install an assumed license.

These are accepted policy requirements, not an operative license. The intended
classification is source-available, not OSI open source. Third-party terms remain
independent. See [licensing requirements and sources](../features/007-licensing-and-distribution.md).

## Sequence and remaining gates

1. Discuss and implement the owner's usage-driven UI items under their own Go,
   including About; the list is still open.
2. Implement the bounded current-value rule change and essential UI/readiness work
   under separate implementation authorization.
3. Complete release verification and fix discovered blockers.
4. Refresh the manual in one owner-authorized pass against frozen behavior.
5. Finalize licensing and packages; licensing investigation may run alongside
   engineering. Obtain explicit approval before publication.

Remaining concrete inputs are the rest of the owner's UI list, detailed designs
and Go for changes, tested OS support range, reviewed license text, and release
approval. Preserve already accepted decisions rather than asking them again.
Reconcile stale feature/index/handoff summaries before release, without rewriting
historical test results or presenting deferred features as complete.
