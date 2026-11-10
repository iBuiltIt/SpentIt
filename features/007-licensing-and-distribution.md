# 007 — Licensing and distribution

Status: **active**
Last updated: 2026-11-09

The owner selected the Unlicense for SpentIt. The text is [LICENSE](../LICENSE).
It permits any use, including selling copies. That choice replaces the earlier
no-resale drafting. Third-party components are excluded from the dedication and
keep their own licenses.

## Dependencies

Qt, SQLCipher, and PCRE2 keep their own notices. Package the texts for the
versions actually shipped. A SpentIt license cannot erase them. Prefer
LGPL-available Qt modules and dynamic linking, and meet the notice and
relinking obligations when distributing. Avoid GPL-only Qt modules.

## About

About shows the application identity and version, **© 2026 Stefan Wessels**,
and offline third-party notices. The Unlicense text lives in the repository
LICENSE file. About does not yet display that text. Notices must work
without a network.

## Packaging

Local macOS bundles, Linux install trees, and Windows packaging preparation
exist. Installers and public distribution do not. Icons follow system
appearance, with light as the fallback. Installed icons use the light variant.
See [packaging/README.md](../packaging/README.md). Windows packaging is not
release-ready.

## Not done

A dependency inventory of each shipped build, signing or notarization, and
publication approval. Linux and Windows library lists are not in LICENSE yet.
