# 007 — Licensing and distribution

Status: **active**  
Last updated: 2026-10-25  
Implementation: no license selected or added; no distribution exists.

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

A blanket noncommercial restriction would contradict the permitted internal business use. Exact terms still need to distinguish selling copies from paid support, hosting, bundled distribution, and paid modifications. The owner's broader statement that nobody should make money from these efforts expresses the anti-monetization intent; do not silently turn it into a prohibition on the expressly permitted business use. Trademark rules address branding/confusion but cannot prevent someone using a different brand as requested here.

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

The sale-versus-business-use policy is resolved. The exact license remains to be selected or reviewed. Determine how proposed terms handle paid hosting/access, support, consulting, paid modifications, bundles, and cost-only distribution without undermining the accepted policy. Source-sharing requirements for derivatives also remain unspecified. Bring concrete wording and its consequences to the owner rather than reopening the settled internal-business-use question.

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
