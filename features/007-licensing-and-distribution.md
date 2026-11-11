# 007 — Licensing and distribution

Status: **active**
Last updated: 2026-11-11

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
LICENSE file. About does not display that text. Notices must work without a
network.

## Packaging

Local development packages remain. The distribution command is
`python3 tools/package.py`. On macOS, `--sign` writes a Developer ID DMG and
notarizes it when `APPLE_NOTARY_PROFILE` is set. Icons follow system appearance,
with light as the fallback. Installed icons use the light variant. See
[packaging/README.md](../packaging/README.md).

On Windows the same command writes a per-user installer,
`dist/SpentIt-<version>-windows-x64-setup.exe`. It installs under the user's
local Programs folder, adds Start menu and desktop icons, and registers
`.spentit`. Opening that file starts SpentIt and opens it. `--sign` applies
Authenticode when a certificate is available in the environment. The installer
refuses Windows older than Windows 10 version 1809, Qt 6.8's published floor.
That floor has not been launch-tested. On Linux the same command writes a
per-user installer, `dist/SpentIt-<version>-linux-<arch>-setup.run`. It
installs under `~/.local/opt/SpentIt`, adds an application menu entry, and
registers `.spentit`. Opening that file starts SpentIt and opens it. The
installer does not sign the program. Linux and Windows library lists are not
in LICENSE yet. The macOS package minimum is 14.4. It has not been launched
on a 14.4 Mac. Publication still needs explicit approval.
