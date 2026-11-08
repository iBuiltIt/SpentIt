# 007 — Licensing and distribution

Status: **active**  
Last updated: 2026-11-08  
Implementation: About, version copying and offline terms/third-party notices are implemented under the 2026-11-08 Go; see current status for verification. No final SpentIt license is selected or added. Local icon/packaging support exists with platform verification and distribution-compliance gaps. No public distribution exists.

## Copyright attribution — owner decision 2026-11-08

Use exactly **© 2026 Stefan Wessels** in the planned About dialog and application
copyright metadata. The owner selected personal attribution rather than Wessels
Consulting Ltd. This records the requested attribution; it is not an assignment
of rights, a liability determination or selection of the final application license.
Third-party copyrights remain intact and separately attributed.

The owner subsequently approved the About design and gave implementation Go.
The plain-language risk notice below is approved for display. Final legal license
and limitation-of-liability terms remain unselected; no acceptance checkbox or
contract-assent flow is authorized. The owner's question
about use at one's own risk does not authorize weakening correctness, security,
recovery or verification requirements.

## About and third-party notices — accepted 2026-11-08

The owner accepted About as part of the next usage-driven UI phase, ahead of the
release plan. The subsequent Go authorizes this About implementation. Further UI items
remain to be supplied. See [the accepted release plan](../doc/first-release-plan.md).

- Main About panel: application icon, SpentIt name, “Where did my money go?”,
  version/build and copyright. The owner explicitly removed the extra description
  “A local application for understanding your spending.” Do not reintroduce it.
- **Third-party software…** opens an offline-readable list of the actual shipped
  components and versions, with copyright notices and full applicable licenses.
  Include transitive dependencies and required notices, not only Qt, SQLCipher
  and PCRE2. Derive the contents from the shipped build and its exact versions.
- **SpentIt license…** provides the final application terms once selected. Do not
  present the accepted policy as an operative license before that selection.
- Bundle notices/license texts with the application and expose them through About;
  access must not depend on a website or internet connection. Full license texts
  need not occupy the main About panel. Make Qt use prominently acknowledged.
- About is the user-visible route to notices, not the whole distribution compliance
  process. Applicable source availability, replacement/relinking and other
  dependency obligations remain packaging requirements.

Verification when implemented must check identity/version accuracy, offline
access, readable/selectable notices, keyboard/focus behavior and agreement between
bundled notices and the actual shipped dependency inventory. Record any manual
changes in the deferred backlog rather than updating the manual during feature work.

## About design and implementation Go — 2026-11-08

- A compact read-only window shows the application icon, SpentIt, the tagline,
  version/build, **Copy version information**, **© 2026 Stefan Wessels**, the
  warranty summary and a prominent Qt/LGPL acknowledgement.
- **License and terms…**, **Third-party software…**, and **Close** are the actions.
  The first two open readable/selectable, scrollable pages within the same window,
  with Back and Close. Escape returns from a reading page, then closes About.
- Use the native macOS application-menu About entry; elsewhere use Help → About
  SpentIt. It works before a file is open. Preserve unsaved edits and financial
  decision modality; reading/copying invokes no financial command or worker action.
- Copy version/build, Qt version, OS and architecture only, with no file paths,
  financial data or personal machine identifiers.
- Short notice: “Provided as is, without warranty. See License and terms.”
- Approved plain-language terms text:

> SpentIt is provided free of charge, “as is”, without warranty to the extent
> permitted by law. Accuracy, availability, security and preservation of data are
> not guaranteed. Use it at your own risk. Keep independent backups and check
> important results against your original records. Do not rely on SpentIt as your
> only financial record.

Until exact licensing terms are selected, the terms page explicitly says that the
final distribution license is not selected and this notice does not grant
redistribution rights or replace final terms. Third-party terms remain separate.
No claim is made that this notice excludes every liability or waives statutory
rights. The later release process must settle final legal terms and their
pre-download/use presentation; explicit acceptance remains a separate decision.

The implementation embeds the [reviewed notice set](../third_party/notices/README.md).
It includes Qt license texts/runtime-source attributions and known macOS transitive
dependencies, with explicit notice-coverage messages for mismatched installations.
This does not complete deployed-package source/relinking or license-compliance review.

## Licensing policy refinements — accepted 2026-11-08

The owner accepted these answers to the earlier drafting questions:

- No charging for access to SpentIt or a hosted derivative.
- Separately purchased human support, training and consulting are allowed when
  software remains freely available and payment is not required to obtain/use it.
- Commissioned development services are allowed. Distributed resulting applications
  remain subject to the same free-access/no-resale terms.
- Charging for SpentIt through a bundle is prohibited. Clearly itemized actual
  media/postage costs are allowed.
- Distributed derivatives must include corresponding source under the same terms;
  private internal modifications need not be published.

The exact license still needs selection and review against this policy and shipped
dependencies. No license is installed by these decisions. Use “source-available”
rather than describing the restrictive intended terms as OSI open source.

Sources checked 2026-11-08: [OSI definition](https://opensource.org/osd),
[Qt LGPL obligations](https://www.qt.io/development/open-source-lgpl-obligations),
and [SQLCipher license](https://github.com/sqlcipher/sqlcipher/blob/master/LICENSE.txt).
Qt guidance calls for prominent notification of LGPL library use and a license
copy. SQLCipher's binary redistribution conditions require its notices, conditions
and disclaimer in accompanying materials. Package the texts applicable to the
actual versions shipped; these web references do not replace that inventory.

## Local packaging — accepted 2026-10-28

The owner approved the supplied PNG/ICNS artwork for the welcome screen and application icons. Welcome/runtime icons follow system appearance with light as the unknown/default fallback. Installed icons use the light variant. Local macOS bundles, Linux install trees/archives, and Windows resource/packaging preparation are in scope; installers and public distribution are not. This dated packaging checkpoint predates the subsequent Windows build/launch; current Windows tests and packaging remain incomplete, as recorded in [Windows verification](../doc/windows-development-setup.md). See [local package commands and limits](../packaging/README.md) and [verification](../doc/status.md).

## Accepted intent

- Personal-use application with intended public GitHub availability.
- Selling SpentIt or its derivatives, including rebranded versions, must be prohibited.
- Businesses may use it internally for free. Commercial activity by a user is not itself prohibited use of SpentIt.
- The owner's intent is permanent free access: "Forever free." The restriction must carry through derivatives rather than disappear when someone modifies or rebrands the application.
- The earlier preference for Unlicense is superseded by this clarification. Do not install an assumed replacement license.

## Findings and choices

Unlicense and permissive licenses such as MIT permit commercial redistribution. GPL provides copyleft protections for distributed covered derivatives, but it also permits selling copies and does not prohibit rebranding. Therefore GPL is not a solution to a literal no-resale requirement.

A license that prohibits sale/commercial redistribution is not open source under the Open Source Definition. Source can still be publicly visible on GitHub under appropriate source-available terms; visibility and permission to redistribute are different questions.

The owner has selected the no-resale direction, with free internal business use explicitly allowed. Investigate suitable source-available software terms and check dependency compatibility. GPL is not an alternative that satisfies this decision. Do not improvise a custom legal clause as if it were a vetted license.

A blanket noncommercial restriction would contradict the permitted internal business use. The accepted 2026-11-08 refinements above distinguish selling copies from paid support, hosting, bundled distribution, and paid modifications; exact legal wording remains to be selected. The owner's broader statement that nobody should make money from these efforts expresses the anti-monetization intent; do not silently turn it into a prohibition on the expressly permitted business use. Trademark rules address branding/confusion but cannot prevent someone using a different brand as requested here.

Creative Commons advises against using its licenses for software. Do not choose CC BY-NC merely because its name appears to fit the intent.

## Dependency plan (proposed)

- Qt: choose LGPL-available modules, preferably dynamic linking; fulfill applicable notices, library-source, replacement/relinking, and installation obligations when distributing. Avoid accidentally introducing GPL-only modules under a restrictive application license.
- SQLCipher Community: retain its BSD-style license and required notices.
- PCRE2: retain its BSD-style license and applicable exception/notices.
- Review the exact shipped dependency versions, crypto provider, transitive dependencies, and packaging. Our eventual license cannot erase their terms.

## Acceptance criteria before distribution

- Owner selects licensing terms with an accurate explanation of what they allow and prohibit.
- Terms preserve free internal business use and prohibit sale of SpentIt and derivatives; derivative distribution cannot discard that restriction.
- Repository license statements match that choice and do not incorrectly label restrictive terms as open source.
- Dependency/module inventory and notices match the actual build.
- Binary packaging meets relevant dependency obligations on each shipped platform.
- Public examples, fixtures, and repository history contain no personal financial data or secrets.

## Remaining drafting questions

The policy questions, including paid services, hosting, bundles, cost-only distribution and derivative source sharing, are resolved above. The exact license remains to be selected or reviewed. Bring concrete wording and its consequences to the owner, checking fit and dependency compatibility rather than reopening settled policy.

These documents record requirements, not an operative license or a guarantee of legal enforceability. No license file has been added. Documentation and local development can proceed while exact terms are resolved.

## Sources checked 2026-10-25

- [Open Source Definition: redistribution and fields of endeavor](https://opensource.org/osd)
- [GNU: selling free software](https://www.gnu.org/philosophy/selling.en.html)
- [GNU license FAQ](https://www.gnu.org/licenses/gpl-faq.en.html)
- [Unlicense text](https://unlicense.org/)
- [Creative Commons FAQ: software licensing](https://creativecommons.org/faq/)
- [Qt LGPL obligations](https://www.qt.io/development/open-source-lgpl-obligations)
- [SQLCipher Community licensing](https://www.zetetic.net/sqlcipher/community/)
- [PCRE2 license](https://github.com/PCRE2Project/pcre2/blob/main/LICENCE.md)
