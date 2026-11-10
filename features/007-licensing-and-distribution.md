# 007 — Licensing and distribution

Status: **active**
Last updated: 2026-11-09

No license file is installed. These requirements are not an operative license
and not a guarantee of enforceability.

## Settled policy

- Public source availability and permanent free access.
- Selling SpentIt or a derivative, including a rebranded build, is prohibited.
- Internal business use is free. A user's own commercial activity is not
  prohibited use of SpentIt.
- Charging for access to SpentIt or a hosted derivative is prohibited.
- Paid human support, training, consulting, and commissioned development are
  allowed while the software remains free to obtain and use.
- A bundle may not charge for SpentIt. Itemized actual media and postage costs
  are allowed.
- Distributed derivatives need corresponding source under the same terms.
  Private internal modifications need not be published.
- The classification is source-available, not OSI open source. Do not call
  restrictive terms open source.

Unlicense and MIT permit commercial redistribution. GPL permits selling copies.
Neither satisfies the no-resale decision. Do not install either, or a custom
clause presented as a finished license. Creative Commons is not for this
software. Select concrete terms with the owner before publication.

## Dependencies

Qt, SQLCipher, and PCRE2 keep their own notices. Package the texts for the
versions actually shipped. A SpentIt license cannot erase them. Prefer
LGPL-available Qt modules and dynamic linking, and meet the notice and
relinking obligations when distributing. Avoid GPL-only Qt modules.

## About

About shows the application identity and version, **© 2026 Stefan Wessels**,
and offline third-party notices. It will also show the SpentIt license once
one is selected. The extra descriptive sentence was removed. Notices must work
without a network.

## Packaging

Local macOS bundles, Linux install trees, and Windows packaging preparation
exist. Installers and public distribution do not. Icons follow system
appearance, with light as the fallback. Installed icons use the light variant.
See [packaging/README.md](../packaging/README.md). Windows packaging is not
release-ready.

## Not done

The exact license text, a dependency inventory of the shipped build, signing
or notarization, and publication approval.
