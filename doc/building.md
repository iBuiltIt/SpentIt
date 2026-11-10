# Building

Last updated: 2026-11-09

Working experimental formats are **70 with rules / 69 without**. Older
experimental files are rejected unchanged. There is no migration.

## Prerequisites

- CMake 3.21 or newer, Ninja, and a C++20 compiler.
- The application also needs Qt 6, OpenSSL, the pinned SQLCipher amalgamation,
  and PCRE2 10.49. Engine-only builds need none of those.
- Sanitizers need Clang or GCC on a non-Windows platform.

Platform setup is in [linux-development-setup.md](linux-development-setup.md)
and [windows-development-setup.md](windows-development-setup.md). Qt and
SQLCipher provenance are in `third_party/qt/README.md` and
`third_party/sqlcipher/README.md`.

## Engine

From the repository root:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

The same pattern works for the `release` and `sanitized` presets. Each preset
uses an ignored `build/<preset>/` tree. In-source configuration is rejected.
`BUILD_TESTING=OFF` disables tests. Release tests still run; they do not depend
on `NDEBUG`.

Optional rules-only engine tests, without the application:

```sh
cmake -S . -B build/rules-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSPENTIT_RULES=ON
cmake --build build/rules-debug
ctest --test-dir build/rules-debug --output-on-failure
```

## SQLCipher amalgamation

Ordinary engine builds do not download it. Prepare the pinned source before an
application build:

```sh
curl --fail --location https://github.com/sqlcipher/sqlcipher/archive/refs/tags/v4.19.0.tar.gz -o /tmp/spentit-sqlcipher-4.19.0.tar.gz
python3 tools/prepare_sqlcipher.py /tmp/spentit-sqlcipher-4.19.0.tar.gz
```

Point `OPENSSL_ROOT_DIR` at the installed OpenSSL if CMake cannot find it. On
this macOS setup that path is `/opt/homebrew/opt/openssl@3`.

## Application

```sh
cmake -S . -B build/import-ui-rules-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/opt/homebrew \
  -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3 \
  -DSPENTIT_STORAGE_PROTOTYPE=ON -DSPENTIT_IMPORT_WORKFLOW=ON \
  -DSPENTIT_IMPORT_UI=ON -DSPENTIT_RULES=ON \
  -DSPENTIT_SQLCIPHER_SOURCE="$PWD/build/dependencies/sqlcipher-build" \
  -DSPENTIT_APPLICATION_OUTPUT_DIRECTORY="$PWD/build"
cmake --build build/import-ui-rules-release --target spentit_import_experiment -j 2
./build/SpentIt
```

`CMAKE_PREFIX_PATH` and `OPENSSL_ROOT_DIR` are local examples, not hardcoded
requirements. A rules-off tree uses the same flags with `SPENTIT_RULES=OFF`.
Import-workflow builds need PCRE2 even when rules are off.

The flags `SPENTIT_STORAGE_PROTOTYPE` and `SPENTIT_IMPORT_WORKFLOW` enable the
application storage and import targets. They are not a separate prototype to
run instead of `./build/SpentIt`.

Run the tests that cover the change. A configure command is not a passing run.
Record the host, the build, and the result in the work notes you give the
owner. Do not treat an old status page as a current green run.
