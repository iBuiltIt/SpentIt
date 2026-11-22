# Linux development setup

Select the Qt installer and build paths for your architecture. The documented development
setup uses Ubuntu 24.04 LTS, Qt 6.8.3 and PCRE2 10.49. For installers, see
[platform packaging](../packaging/README.md).

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

### aarch64 Qt

```sh
DEPS="$HOME/.local/spentit-deps"
pipx install aqtinstall
"$HOME/.local/bin/aqt" install-qt --outputdir "$DEPS/qt" \
  linux_arm64 desktop 6.8.3 linux_gcc_arm64 -m qtshadertools
```

## Configure, build, and launch

Run from the checkout root. Use a fresh CMake build directory if the existing
one was configured with a different architecture or toolchain. Keep the
SQLCipher amalgamation in `build/dependencies`.

```sh
DEPS="$HOME/.local/spentit-deps"
QT_ARCH=gcc_64  # Use gcc_arm64 on aarch64.
QT_ROOT="$DEPS/qt/6.8.3/$QT_ARCH"

export PKG_CONFIG_PATH="$DEPS/pcre2-10.49/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export LD_LIBRARY_PATH="$DEPS/pcre2-10.49/lib:$QT_ROOT/lib"

cmake -S . -B build/import-ui-rules-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$QT_ROOT" \
  -DCMAKE_BUILD_RPATH="$DEPS/pcre2-10.49/lib;$QT_ROOT/lib" \
  -DSPENTIT_STORAGE_PROTOTYPE=ON \
  -DSPENTIT_IMPORT_WORKFLOW=ON \
  -DSPENTIT_IMPORT_UI=ON \
  -DSPENTIT_RULES=ON \
  -DSPENTIT_SQLCIPHER_SOURCE="$PWD/build/dependencies/sqlcipher-build" \
  -DSPENTIT_APPLICATION_OUTPUT_DIRECTORY="$PWD/build"

cmake --build build/import-ui-rules-release --target spentit_import_experiment -j"$(nproc)"
./build/SpentIt
```

Use the current test registrations and [build guide](building.md) to verify
the affected paths. A desktop launch alone does not verify durability.
