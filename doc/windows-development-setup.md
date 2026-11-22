# Windows development setup

Use an x64 Visual Studio developer shell, CMake, Ninja and a working Python
installation. The recorded development setup used MSVC toolset 14.44,
Qt 6.8.3, PCRE2 10.49 and OpenSSL 3.5.2. See [platform packaging](../packaging/README.md)
for installer builds. Sanitizer presets require Clang/GCC on a non-Windows host.

## Developer command prompt

```bat
"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44
```

Run the cmd examples in that prompt. For PowerShell use the Visual Studio
Developer PowerShell with the same x64 toolset.

## OpenSSL

Set `OPENSSL_ROOT_DIR` to an OpenSSL 3.x Crypto prefix. The validated setup
used the vcpkg `installed\x64-windows` prefix and its dynamic CRT build. Put
its `bin` directory on `PATH` for app and test runs. The examples use
`C:\path\to\vcpkg` for the local vcpkg installation.

## PCRE2 10.49

Windows configure requires `SPENTIT_PCRE2_ROOT`. It reads `pcre2.h` and rejects
anything other than `PCRE2_MAJOR` 10 and `PCRE2_MINOR` 49, including a
prerelease. It links `pcre2-8.lib` or `pcre2-8-static.lib` and defines
`PCRE2_STATIC`. Unix builds still use pkg-config `libpcre2-8=10.49`.

The archive is the same 10.49 release as the Linux guide. SHA-256
`53c156e1ba416a20da8e65395daa132da0d80e76910424caca3fcdae7831d384`, size
2185271. This host checked that digest, then built the static 8-bit Unicode
library with the 14.44 toolset:

```bat
set DEPS=%USERPROFILE%\.local\spentit-deps
cmake -S "%TEMP%\spentit-deps\pcre2-10.49" -B "%TEMP%\spentit-deps\pcre2-10.49\build" -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_INSTALL_PREFIX="%DEPS%\pcre2-10.49" ^
  -DBUILD_SHARED_LIBS=OFF -DBUILD_STATIC_LIBS=ON ^
  -DPCRE2_BUILD_PCRE2_8=ON -DPCRE2_BUILD_PCRE2_16=OFF -DPCRE2_BUILD_PCRE2_32=OFF ^
  -DPCRE2_SUPPORT_UNICODE=ON -DPCRE2_BUILD_TESTS=OFF -DPCRE2_BUILD_PCRE2GREP=OFF ^
  -DPCRE2_SUPPORT_LIBBZ2=OFF -DPCRE2_SUPPORT_LIBZ=OFF ^
  -DPCRE2_SUPPORT_LIBREADLINE=OFF -DPCRE2_SUPPORT_LIBEDIT=OFF
cmake --build "%TEMP%\spentit-deps\pcre2-10.49\build"
cmake --install "%TEMP%\spentit-deps\pcre2-10.49\build"
```

The installed library is `%DEPS%\pcre2-10.49\lib\pcre2-8-static.lib`, 572048
bytes. The SpentIt cache records that path as `PCRE2_LIBRARY`.

## SQLCipher 4.19.0

`tools/prepare_sqlcipher.py` checks the archive, runs `configure`, and runs
`make sqlite3.c`. It does not run natively in the Visual Studio prompt. On
this host it was run in WSL Ubuntu, and it wrote the amalgamation into the
shared checkout. The helper printed `Pinned SQLCipher generated sources verified`.

Archive SHA-256 `7075f96cbabe45b4ecfc2e6b1745a625f856f695b0827a5506ce9ed85b906aa0`.
Generated `sqlite3.c` SHA-256 `8640c653acadf665cce6331646f60b5b74a4690746f2c4a2d8f688a0570a0c0c`.
Generated `sqlite3.h` SHA-256 `8a9d1bff44d75174ca6dea3ea9bac50a6104d86facb566647b8bb839375b7b3a`.
CMake rejects any other generated pair. Tcl is required on the Unix side
because `make sqlite3.c` generates the amalgamation.

A fresh checkout does not contain `sqlite3.c`. The helper skips extraction when
`build/dependencies/sqlcipher-4.19.0` already exists. A directory left behind
without its files, including `configure`, has to be removed too. The helper
still needs the archive.

```sh
REPO="/mnt/c/path/to/SpentIt"
curl --fail --location \
  https://github.com/sqlcipher/sqlcipher/archive/refs/tags/v4.19.0.tar.gz \
  -o /tmp/spentit-sqlcipher-4.19.0.tar.gz
rm -rf "$REPO/build/dependencies/sqlcipher-4.19.0" "$REPO/build/dependencies/sqlcipher-build"
python3 "$REPO/tools/prepare_sqlcipher.py" /tmp/spentit-sqlcipher-4.19.0.tar.gz
```

PowerShell can download the archive and delete the old directory, but it cannot
run the helper to completion. `prepare_sqlcipher.py` executes the Unix
`configure` script and `make sqlite3.c`. From the Developer PowerShell, start
that same Ubuntu step with `wsl.exe`. Change the `/mnt/c/path/to/SpentIt` path
to the checkout.

```powershell
wsl.exe -d Ubuntu -- bash -lc 'cd /mnt/c/path/to/SpentIt && curl --fail --location https://github.com/sqlcipher/sqlcipher/archive/refs/tags/v4.19.0.tar.gz -o /tmp/spentit-sqlcipher-4.19.0.tar.gz && rm -rf build/dependencies/sqlcipher-4.19.0 build/dependencies/sqlcipher-build && python3 tools/prepare_sqlcipher.py /tmp/spentit-sqlcipher-4.19.0.tar.gz'
```

MSVC then compiles those sources. The `spentit_sqlcipher` target uses `/W0`.
`sqlite3.c` is not edited. The `fullfsync` pragma remains in the SQLCipher
setup and is still read back as `1`.

## Qt 6.8.3

`qtdeclarative` is a base archive, not an extra module. `qtshadertools` is a
real extra module and is required by the Qt Quick CMake package. aqtinstall
`3.3.0` reported `Finished installation`. The installed pieces included
`qtbase`, `qtdeclarative`, `qtsvg`, `qttools`, `qttranslations`, and
`qtshadertools`.

```bat
set PY=C:\path\to\python.exe
"%PY%" -m pip install --user aqtinstall
"%PY%" -m aqt install-qt --outputdir "%USERPROFILE%\.local\spentit-deps\qt" windows desktop 6.8.3 win64_msvc2022_64 -m qtshadertools
```

The CMake package is `%USERPROFILE%\.local\spentit-deps\qt\6.8.3\msvc2022_64`.
`qmake.exe` is in that prefix's `bin`. This is the Qt 6.8 open-source Windows
desktop build for MSVC 2022, 64-bit. The macOS verification used Qt 6.12.0.
This Windows build stays on 6.8.3.

Qt 6.8 can log a missing fonts-directory warning at startup.

## Configure, build, and launch

Use a build directory that a WSL configure has not already written. This host
used `build\import-ui-rules-release`. `SPENTIT_APPLICATION_OUTPUT_DIRECTORY`
places the executable at `build\SpentIt.exe`. A WSL configure of the same
checkout used `build/wsl-import-ui-rules-release` and writes `build/SpentIt`.
Those two outputs can sit side by side. One `-B` directory must not be
configured by both generators.

```bat
set REPO=C:\path\to\SpentIt
set DEPS=%USERPROFILE%\.local\spentit-deps
cd /d "%REPO%"

cmake -S "%REPO%" -B "%REPO%\build\import-ui-rules-release" -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH="%DEPS%\qt\6.8.3\msvc2022_64" ^
  -DOPENSSL_ROOT_DIR="C:\path\to\vcpkg\installed\x64-windows" ^
  -DSPENTIT_PCRE2_ROOT="%DEPS%\pcre2-10.49" ^
  -DSPENTIT_STORAGE_PROTOTYPE=ON ^
  -DSPENTIT_IMPORT_WORKFLOW=ON ^
  -DSPENTIT_IMPORT_UI=ON ^
  -DSPENTIT_RULES=ON ^
  -DSPENTIT_SQLCIPHER_SOURCE="%REPO%\build\dependencies\sqlcipher-build" ^
  -DSPENTIT_APPLICATION_OUTPUT_DIRECTORY="%REPO%\build"

cmake --build "%REPO%\build\import-ui-rules-release" --target spentit_import_experiment
```

The same configure and build from the Developer PowerShell, which creates
`build\import-ui-rules-release` when that directory is absent:

```powershell
$repo = 'C:\path\to\SpentIt'
$deps = Join-Path $env:USERPROFILE '.local\spentit-deps'

cmake -S $repo -B "$repo\build\import-ui-rules-release" -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="$deps\qt\6.8.3\msvc2022_64" `
  -DOPENSSL_ROOT_DIR='C:\path\to\vcpkg\installed\x64-windows' `
  -DSPENTIT_PCRE2_ROOT="$deps\pcre2-10.49" `
  -DSPENTIT_STORAGE_PROTOTYPE=ON `
  -DSPENTIT_IMPORT_WORKFLOW=ON `
  -DSPENTIT_IMPORT_UI=ON `
  -DSPENTIT_RULES=ON `
  -DSPENTIT_SQLCIPHER_SOURCE="$repo\build\dependencies\sqlcipher-build" `
  -DSPENTIT_APPLICATION_OUTPUT_DIRECTORY="$repo\build"

cmake --build "$repo\build\import-ui-rules-release" --target spentit_import_experiment
```

Launch from the same prompt. Qt and OpenSSL both have to be on `PATH`. Qt loads
plugins relative to `Qt6Core.dll` in the Qt `bin` directory.

```bat
set PATH=%USERPROFILE%\.local\spentit-deps\qt\6.8.3\msvc2022_64\bin;C:\path\to\vcpkg\installed\x64-windows\bin;%PATH%
build\SpentIt.exe
```

From the Developer PowerShell, in the same window that just built:

```powershell
$env:PATH = "$deps\qt\6.8.3\msvc2022_64\bin;C:\path\to\vcpkg\installed\x64-windows\bin;" + $env:PATH
& "$repo\build\SpentIt.exe"
```

`$repo` and `$deps` are the variables from the PowerShell configure above. The
`PATH` change lasts for that window. Running `build\SpentIt.exe` without those
two `bin` directories on `PATH` fails to load the Qt DLLs. `windeployqt` was
not run, so the DLLs are not copied next to the executable.

## Tests

Use the same `PATH` as launch. `ctest` is the one beside the Visual Studio CMake.

```bat
ctest --test-dir "%REPO%\build\import-ui-rules-release" --output-on-failure --parallel 4
```

The Windows CMake configuration excludes the POSIX storage suite, which
uses fork/waitpid. In the recorded Windows development run, offscreen
`import.ui` failed at `ImportUiTests::accessibilityAndKeyboard` because
Qt returned no accessible interface for the unsaved-changes Save button.
Windows QtTest failure details may appear in `OutputDebugString` rather than
CTest output. SQLCipher diagnostics from deliberate wrong-password and
corruption tests do not identify the failed assertion.

## File publication on Windows

`src/storage/windows_files.hpp` is the Windows side of the storage file
operations. The Unix calls remain in the storage sources under `#else` and
were not edited.

- Exclusive create uses `CreateFileW` with `CREATE_NEW`.
- The document lock uses `LockFileEx` with an exclusive, fail-immediately lock. The lock file is not deleted, and a live lock is not taken over.
- Identity is the volume serial, the file index, and a link count of 1. Reparse points and directories are rejected.
- Publication uses `CreateHardLinkW`. That call fails when the destination name already exists.
- A new file receives an owner-only DACL. An Administrator can still bypass that ACL by privilege. That bypass was not given its own test.
- Directory publication opens the directory with `GENERIC_WRITE` and `FILE_FLAG_BACKUP_SEMANTICS`, then calls `FlushFileBuffers`. Attribute-only access opens the directory, and `FlushFileBuffers` then returns `ERROR_ACCESS_DENIED` (5). The call is the NTFS analogue of a parent `fsync`. It is not a physical power-loss proof.
- SQLite filenames are UTF-8 (`u8string()`).
- A private file is created with `FILE_SHARE_DELETE`, so a later rename can proceed while that handle is open.

`aclapi.h` pulls in `rpcndr.h`, which defines `small` and `hyper` as macros.
The header undefines both after the include. `NOMINMAX` and
`WIN32_LEAN_AND_MEAN` are set before `windows.h`.

The passing tests exercised create, reopen, save, and commit. They did not
separately prove a refused hard link, a second process failing to take the
lock, reparse-point rejection, the owner-only DACL, the receipt path hash, or
behavior across power loss.

## Same checkout from WSL

Follow the [Linux setup](linux-development-setup.md) and use a separate
configuration directory, such as `build/wsl-import-ui-rules-release`.
Use the WSL Linux filesystem for test files. A build from a DrvFS checkout does
not establish DrvFS document locking or durability. The recorded WSL run
launched the app but had test failures. Verify affected tests on the current
checkout before relying on WSL results.
