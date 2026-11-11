# Linux development setup

Developer prerequisites and build commands for the tested Ubuntu machine below.
This is not an installation guide for running a packaged SpentIt download.
The downloadable installer is `python3 tools/package.py`, described in
[packaging](../packaging/README.md). The 2026-11-11 package run is recorded
in [status](status.md).
Set `REPO` in the examples to the path of your own checkout.

Status: the rules-enabled Release application launched on two machines.
On 2026-11-02 the owner reported a successful build and launch on Ubuntu 24.04.5
LTS, x86_64, using the `gcc_64` Qt paths below. On 2026-11-03 the owner reported
a successful configure, build, and launch on an aarch64 QEMU virtual machine,
using the `gcc_arm64` Qt paths below. These results are owner-reported.
Automated tests, other distributions, and a clean graphics driver were not checked.
The x86_64 commands were not rerun when this guide was added. Mesa output from
the aarch64 launch was not recorded.

## Host

- Ubuntu 24.04.5 LTS, x86_64
- Already present before the install: CMake 3.28.3, Ninja 1.11.1, g++ 13.3.0, Python 3.12.3, OpenSSL 3.0.13
- Distro pkg-config `libpcre2-8` was 10.42
- Distro Qt packages present were 6.4.2 runtime pieces. apt's `qt6-base-dev` candidate was 6.4.2 and `libpcre2-dev` was 10.42

The application configure requires PCRE2 exactly 10.49 (`libpcre2-8=10.49`) and Qt 6.8 or newer. Those apt packages do not satisfy that, so both were installed under `$HOME/.local/spentit-deps` and the distro copies were left in place.

## Packages

```sh
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake ninja-build pkg-config python3 python3-pip pipx \
  curl ca-certificates bzip2 p7zip-full tcl libssl-dev libgl1-mesa-dev \
  libfontconfig1-dev libfreetype-dev libgtk-3-dev libatspi2.0-dev \
  libx11-dev libx11-xcb-dev libxcb1-dev libxcb-cursor-dev libxcb-glx0-dev \
  libxcb-icccm4-dev libxcb-image0-dev libxcb-keysyms1-dev libxcb-randr0-dev \
  libxcb-render-util0-dev libxcb-shape0-dev libxcb-shm0-dev libxcb-sync-dev \
  libxcb-util-dev libxcb-xfixes0-dev libxcb-xkb-dev \
  libxext-dev libxfixes-dev libxi-dev libxkbcommon-dev libxkbcommon-x11-dev \
  libxrender-dev
```

OpenSSL 3.x was discovered from these Ubuntu headers. No `OPENSSL_ROOT_DIR` was set.

## PCRE2 10.49

```sh
DEPS="$HOME/.local/spentit-deps"
mkdir -p /tmp/spentit-deps
curl --fail --location \
  https://github.com/PCRE2Project/pcre2/releases/download/pcre2-10.49/pcre2-10.49.tar.bz2 \
  -o /tmp/spentit-deps/pcre2-10.49.tar.bz2
echo "53c156e1ba416a20da8e65395daa132da0d80e76910424caca3fcdae7831d384  /tmp/spentit-deps/pcre2-10.49.tar.bz2" \
  | sha256sum -c
rm -rf /tmp/spentit-deps/pcre2-10.49
tar -xjf /tmp/spentit-deps/pcre2-10.49.tar.bz2 -C /tmp/spentit-deps
cd /tmp/spentit-deps/pcre2-10.49
./configure --prefix="$DEPS/pcre2-10.49" \
  --enable-pcre2-8 --enable-unicode --enable-shared --disable-static \
  --disable-pcre2-16 --disable-pcre2-32
make -j"$(nproc)"
make install
```

After install, `PKG_CONFIG_PATH="$DEPS/pcre2-10.49/lib/pkgconfig" pkg-config --modversion libpcre2-8` printed `10.49`.

## SQLCipher 4.19.0

Run from any directory. The helper writes into the checkout.

```sh
REPO="/path/to/SpentIt"
curl --fail --location \
  https://github.com/sqlcipher/sqlcipher/archive/refs/tags/v4.19.0.tar.gz \
  -o /tmp/spentit-sqlcipher-4.19.0.tar.gz
python3 "$REPO/tools/prepare_sqlcipher.py" /tmp/spentit-sqlcipher-4.19.0.tar.gz
```

The helper printed `Pinned SQLCipher generated sources verified: .../build/dependencies/sqlcipher-build`. `sqlite3.c` was present there afterward. Tcl is required because `make sqlite3.c` generates the amalgamation.

## Qt 6.8.3

`qtdeclarative` is a base archive (`icu qtbase qtdeclarative qtsvg qttools qttranslations qtwayland`), not an add-on module. `aqt install-qt -m qtdeclarative` failed with `The packages ['qtdeclarative'] were not found`. Leaving `--archives` unset installs that base set. `qtshadertools` is a real extra module and is required by the Qt Quick CMake package.

The apt packages, the PCRE2 build, and the SQLCipher helper are the same on x86_64 and aarch64. The prebuilt Qt package is the architecture-specific step. Run `uname -m` and follow the matching block. The two Qt trees can sit side by side under `$DEPS/qt/6.8.3/`. Configure and launch must use the tree that matches the machine.

| `uname -m` | aqt host and architecture | Qt directory used below |
| --- | --- | --- |
| `x86_64` | `linux desktop 6.8.3 linux_gcc_64` | `$DEPS/qt/6.8.3/gcc_64` |
| `aarch64` | `linux_arm64 desktop 6.8.3 linux_gcc_arm64` | `$DEPS/qt/6.8.3/gcc_arm64` |

`gcc_64` is the x86_64 Qt. On an aarch64 machine its QML scanner cannot run. Configure then stops with `Syntax error: word unexpected (expecting ")")` and `Failed to scan target spentit_import_experiment for QML imports`, from `gcc_64/lib/cmake/Qt6Qml/Qt6QmlMacros.cmake`. Use `gcc_arm64` on that machine.

After installation, `file` names the architecture of the tool that will run:

```sh
file "$HOME/.local/spentit-deps/qt/6.8.3/gcc_64/bin/qmake"
file "$HOME/.local/spentit-deps/qt/6.8.3/gcc_arm64/bin/qmake"
```

The x86_64 tool is reported as x86-64. The aarch64 tool is reported as ARM aarch64.

### x86_64 Qt

```sh
DEPS="$HOME/.local/spentit-deps"
pipx install aqtinstall
"$HOME/.local/bin/aqt" install-qt --outputdir "$DEPS/qt" \
  linux desktop 6.8.3 linux_gcc_64 -m qtshadertools
```

On the x86_64 machine, aqtinstall 3.3.0 reported `Finished installation`. The CMake package was at `$DEPS/qt/6.8.3/gcc_64`. This is the Qt 6.8 open-source Linux desktop build for x86_64. The macOS verification used Qt 6.12.0.

### aarch64 Qt

```sh
DEPS="$HOME/.local/spentit-deps"
pipx install aqtinstall
"$HOME/.local/bin/aqt" install-qt --outputdir "$DEPS/qt" \
  linux_arm64 desktop 6.8.3 linux_gcc_arm64 -m qtshadertools
```

This installs the CMake package at `$DEPS/qt/6.8.3/gcc_arm64`. The owner used that directory for the aarch64 launch on 2026-11-03.

## Configure, build, and launch

Paste one block into a new terminal. Leave out `set -euo pipefail`. Pasting that option into the interactive terminal closes the window when `cmake` fails, and the error leaves with the window.

`LD_LIBRARY_PATH` below replaces any Qt directory already on that path. A shell which previously exported `gcc_64` will keep loading the x86_64 libraries if that directory stays on the path.

If `build/import-ui-rules-release` was already configured against the other Qt tree, remove that directory from the checkout before configuring again. This leaves `build/dependencies`, including the SQLCipher sources, in place:

```sh
rm -rf build/import-ui-rules-release
```

### x86_64 configure, build, and launch

```sh
REPO="/path/to/SpentIt"
DEPS="$HOME/.local/spentit-deps"
cd "$REPO"

export PKG_CONFIG_PATH="$DEPS/pcre2-10.49/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export LD_LIBRARY_PATH="$DEPS/pcre2-10.49/lib:$DEPS/qt/6.8.3/gcc_64/lib"

cmake -S . -B build/import-ui-rules-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$DEPS/qt/6.8.3/gcc_64" \
  -DCMAKE_BUILD_RPATH="$DEPS/pcre2-10.49/lib;$DEPS/qt/6.8.3/gcc_64/lib" \
  -DSPENTIT_STORAGE_PROTOTYPE=ON \
  -DSPENTIT_IMPORT_WORKFLOW=ON \
  -DSPENTIT_IMPORT_UI=ON \
  -DSPENTIT_RULES=ON \
  -DSPENTIT_SQLCIPHER_SOURCE="$REPO/build/dependencies/sqlcipher-build" \
  -DSPENTIT_APPLICATION_OUTPUT_DIRECTORY="$REPO/build"

cmake --build build/import-ui-rules-release --target spentit_import_experiment -j"$(nproc)"
./build/SpentIt
```

On the x86_64 machine the build ended with `[43/43] Linking CXX executable .../build/SpentIt`. The owner launched that executable and reported the window was up.

On that x86_64 launch, Mesa printed:

```text
libEGL warning: failed to get driver name for fd -1
libEGL warning: MESA-LOADER: failed to retrieve device information
MESA: error: ZINK: failed to choose pdev
libEGL warning: egl: failed to create dri2 screen
```

The window still appeared. These lines were not investigated. No test suite was run on that machine.

### aarch64 configure, build, and launch

```sh
REPO="/path/to/SpentIt"
DEPS="$HOME/.local/spentit-deps"
cd "$REPO"

export PKG_CONFIG_PATH="$DEPS/pcre2-10.49/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export LD_LIBRARY_PATH="$DEPS/pcre2-10.49/lib:$DEPS/qt/6.8.3/gcc_arm64/lib"

cmake -S . -B build/import-ui-rules-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$DEPS/qt/6.8.3/gcc_arm64" \
  -DCMAKE_BUILD_RPATH="$DEPS/pcre2-10.49/lib;$DEPS/qt/6.8.3/gcc_arm64/lib" \
  -DSPENTIT_STORAGE_PROTOTYPE=ON \
  -DSPENTIT_IMPORT_WORKFLOW=ON \
  -DSPENTIT_IMPORT_UI=ON \
  -DSPENTIT_RULES=ON \
  -DSPENTIT_SQLCIPHER_SOURCE="$REPO/build/dependencies/sqlcipher-build" \
  -DSPENTIT_APPLICATION_OUTPUT_DIRECTORY="$REPO/build"

cmake --build build/import-ui-rules-release --target spentit_import_experiment -j"$(nproc)"
./build/SpentIt
```

On 2026-11-03 the owner ran this `gcc_arm64` configure, build, and launch on the aarch64 QEMU virtual machine and reported that it worked. No test suite was run there. Mesa output from that launch was not recorded.
