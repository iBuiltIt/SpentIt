# Qt prototype dependency metadata

Recorded: 2026-10-25

Qt is dynamically linked by the optional synthetic transaction-sheet prototype only. It is not a dependency of the financial engine or the storage prototype. Final UI technology and SpentIt licensing remain unresolved; no project license or distribution approval is implied.

The tested installation uses Qt 6.12.0, Homebrew package revision 1, arm64 Golden Gate bottles. CMake accepts a separately installed Qt 6.8+ environment and does not fetch mutable dependencies. No Qt source patches or vendored Qt code are present here. Reproducing these exact measurements requires the versions below; a newer compatible installation needs its own verification.

The license expressions below are package metadata reported by `brew info --json=v2`, not a component-by-component distribution review. Upstream sources retain their own notices; SpentIt does not replace them. Consult [Qt licensing](https://doc.qt.io/qt-6/licensing.html) and the corresponding upstream source LICENSES files before packaging/distribution.

## qtbase 6.12.0_1

Package license metadata: `(LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only) AND (GPL-3.0-only WITH Qt-GPL-exception-1.0) AND BSD-3-Clause AND GFDL-1.3-no-invariants-only`.

Bottle SHA-256: `cb6651ad22e6b506eb324d5f6b4d46ef85575ebb16c8ccb8721c99e1c37d6709`.

[Verified package artifact](https://ghcr.io/v2/homebrew/core/qtbase/blobs/sha256:cb6651ad22e6b506eb324d5f6b4d46ef85575ebb16c8ccb8721c99e1c37d6709) · [Upstream source](https://download.qt.io/official_releases/qt/6.12/6.12.0/submodules/qtbase-everywhere-src-6.12.0.tar.xz).

## qtdeclarative 6.12.0_1

Package license metadata: `(LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only) AND (GPL-3.0-only WITH Qt-GPL-exception-1.0) AND BSD-2-Clause AND BSD-3-Clause`.

Bottle SHA-256: `e01498eeb11eeaa6feb6f04df86379cc87bbf7b39b3ab8cc3af30a968fefbff9`.

[Verified package artifact](https://ghcr.io/v2/homebrew/core/qtdeclarative/blobs/sha256:e01498eeb11eeaa6feb6f04df86379cc87bbf7b39b3ab8cc3af30a968fefbff9) · [Upstream source](https://download.qt.io/official_releases/qt/6.12/6.12.0/submodules/qtdeclarative-everywhere-src-6.12.0.tar.xz).

## qtsvg 6.12.0_1

Package license metadata: `(LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only) AND BSD-3-Clause`.

Bottle SHA-256: `d698be4c6f245af24a86e6ac51263591d9482dda1e16c8bf39d537ed55e813c5`.

[Verified package artifact](https://ghcr.io/v2/homebrew/core/qtsvg/blobs/sha256:d698be4c6f245af24a86e6ac51263591d9482dda1e16c8bf39d537ed55e813c5) · [Upstream source](https://download.qt.io/official_releases/qt/6.12/6.12.0/submodules/qtsvg-everywhere-src-6.12.0.tar.xz).

Qt Core/Gui/Concurrent/Test come from qtbase; Qml/Quick/QuickControls2 come from qtdeclarative. qtsvg is an installed dependency of the Qt Quick package. Other package-manager prerequisites are outside the repository. See [build and verification](../../doc/sheet-prototype.md).
