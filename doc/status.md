# Project status

SpentIt 1.0.3 uses rules database version 2. Authenticated version-1 files
upgrade transactionally. The rules-off build still writes version 69. SpentIt
JSON exchange remains version 1. See [release schema](release-schema.md) and
[file exchange](file-exchange.md). For implementation work, use the
[contract index](README.md) and [build and test guide](building.md).

## GitHub release 1.0.3 — 2026-11-21

[SpentIt 1.0.3](https://github.com/iBuiltIt/SpentIt/releases/tag/v1.0.3) was
published from tag `v1.0.3` at commit `8e18159` with these installers.

- macOS Apple Silicon, macOS 14.4 or newer as declared by the bundle.
  [SpentIt-1.0.3-macos-arm64.dmg](https://github.com/iBuiltIt/SpentIt/releases/download/v1.0.3/SpentIt-1.0.3-macos-arm64.dmg)
- Windows x64.
  [SpentIt-1.0.3-windows-x64-setup.exe](https://github.com/iBuiltIt/SpentIt/releases/download/v1.0.3/SpentIt-1.0.3-windows-x64-setup.exe)
- Linux x86-64.
  [SpentIt-1.0.3-linux-x64-setup.run](https://github.com/iBuiltIt/SpentIt/releases/download/v1.0.3/SpentIt-1.0.3-linux-x64-setup.run)

All three GitHub assets matched the local installers' SHA-256 checksums. The
Developer ID–signed macOS app and DMG passed stapled ticket and Gatekeeper
verification. The disk image checksum verified, and the signed app launched
and created a temporary SpentIt file. Launch on macOS 14.4 remains unverified.

## Verification boundaries

- The working development host is macOS arm64 with Qt 6.12.0. Verify the
  affected behavior on the current build. The release installer checks above
  cover the published artifacts.
- Repeat native installation, recovery, and workflow checks when changing a
  platform package. The Windows suite omits fork-based storage tests. The
  [Windows setup](windows-development-setup.md#tests) records the known
  accessibility failure and the limits of WSL verification.
- The reported native Rules accessibility crash (`EXC_GUARD` through
  `QQuickAccessibleAttached::setName`) remains unconfirmed fixed. A bounded
  accessible-name mitigation is in place. Spoken screen-reader output and
  interactive native export-picker overwrite remain unverified.

## Product scope

The release scope is the existing local workflows and documented import
formats. New financial capabilities, parser dialects, multiline note editing,
and further layout customization need an explicit decision. Current-value
rule matching and explicit imported-original conditions remain deferred.
Moving transactions across accounts or rewriting source associations also
remains deferred and would require an atomic design covering recognition,
relationships and Undo. Automatic cross-currency, fee and broader fuzzy
transfer matching remain outside the supported policy.

Category, undone Review-link, exclusion, and account/opening/currency
histories survive close. Revisit their retention only after finding a
correctness issue or unacceptable growth, with separate approval. See
[session history](session-history-baseline.md).

Before publication, verify the release suites, financial fault and recovery
paths, a copy of a closed file, and key native workflows on each shipped
platform. Publication requires explicit owner approval.
