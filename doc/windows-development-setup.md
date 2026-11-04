# Windows development setup

Developer prerequisites and build commands for the tested Windows machine below.
This is not an installation guide for running a packaged SpentIt download.
Set `REPO` in the examples to the path of your own checkout. The commands are
`cmd.exe` lines for the Visual Studio prompt in the next section. PowerShell
does not continue lines with `^`.

Status: on 2026-11-03 the rules-enabled Release application configured, compiled,
and opened its window on the host below. `ctest` registered 40 tests; 39 passed
and `import.ui` failed. The same checkout was also built in this machine's WSL
Ubuntu, in a separate build directory. That `ctest` finished **141 passed,
18 failed, out of 159**, real time 8628.74 sec. The 18 failures are listed
below. macOS was not rebuilt from this machine. Packaging was not run.

## Host

The registry `ProductName` is `Windows 10 Pro`. `DisplayVersion` is `26H2`,
`CurrentBuild` is `26300`, and `UBR` is `9818` (build 26300.9818). Qt Test's
banner on the same host says `windows 11`.

- Visual Studio 2026 Community. The prompt banner was `Visual Studio 2026 Developer Command Prompt v18.10.1`.
- The build used toolset `14.44.35207`. `cl` reported `19.44.35229` for x64. Compiler path: `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe`.
- A second toolset, `14.51.36231`, is installed. It was not used. Pass `-vcvars_ver=14.44`.
- CMake `4.3.1-msvc1`, Ninja `1.13.2`, and `ctest` from that Visual Studio install. They are on `PATH` after the prompt below.
- Python for Qt's installer: `3.12.10` at `C:\Users\swessels\AppData\Local\Programs\Python\Python312\python.exe`. After the Visual Studio prompt, `python` is `3.11.9`. The Qt commands used the `3.12.10` interpreter by path.
- `clang` on `PATH` is `C:\Users\swessels\Apps\llvm-mos\bin\clang.exe`. It was not used.
- The Windows Store `python` and `python3` stubs are not a working interpreter.

First-party targets compile with `/W4 /permissive- /utf-8`. There is no `/WX`.
Requesting the sanitizer preset on Windows stops configuration: `Sanitizer preset requires Clang/GCC on a non-Windows platform`.

## Developer command prompt

```bat
"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44
```

The banner then says the environment was initialized for `x64`. Run the rest of
this guide in that prompt.

## OpenSSL

`find_package(OpenSSL 3.0)` accepts 3.0 and newer. This build linked the existing
vcpkg OpenSSL 3.5.2 at:

`C:\Users\swessels\develop\github\external\vcpkg\installed\x64-windows`

Configure printed `Found OpenSSL` with version `3.5.2`. The header text is
`OpenSSL 3.5.2 5 Aug 2025`. The import library is `lib\libcrypto.lib` and the
runtime DLL is `bin\libcrypto-3-x64.dll`. The MSVC Release build uses the
dynamic CRT (`/MD`), which matches that vcpkg triplet. `VCPKG_ROOT` was set.
No vcpkg toolchain file was passed. Pass `OPENSSL_ROOT_DIR` on the CMake
command line.

OpenSSL 4 was not used. `winget` package `ShiningLight.OpenSSL.Dev` reported
`4.1.0` and was not installed.

A user-local OpenSSL 3.5.9 prefix was not produced. Two attempts are recorded
so they are not mistaken for the library that linked:

- `Win64OpenSSL-3_5_9.msi`, SHA-256 `6e6eb6fb9531c3aebed9ff4655758a67e578d32a72d091ac7f5794caa83bfcc2`, size 297603072. `msiexec /a` without elevation wrote a nested bootstrapper. The nested silent installer exited 2. 7-Zip did not extract the libraries. `C:\Program Files\OpenSSL-Win64` does not exist.
- `openssl-3.5.9.tar.gz`, SHA-256 `603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a`, size 53279637. The digest matched. `Configure VC-WIN64A` with `no-asm`, using `C:\Program Files\Git\usr\bin\perl.exe`, stopped before compilation: `Can't locate Locale/Maketext/Simple.pm`. NASM was not on `PATH`. Nothing was installed under `%USERPROFILE%\.local\spentit-deps\openssl-3.5.9`.

Another machine needs an OpenSSL 3.x Crypto library that CMake finds through
`OPENSSL_ROOT_DIR`, and that build's `libcrypto` DLL on `PATH` when the
application or tests launch. This guide has no completed 3.5.9 install recipe.

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

```sh
REPO="/mnt/c/path/to/SpentIt"
curl --fail --location \
  https://github.com/sqlcipher/sqlcipher/archive/refs/tags/v4.19.0.tar.gz \
  -o /tmp/spentit-sqlcipher-4.19.0.tar.gz
python3 "$REPO/tools/prepare_sqlcipher.py" /tmp/spentit-sqlcipher-4.19.0.tar.gz
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
set PY=C:\Users\swessels\AppData\Local\Programs\Python\Python312\python.exe
"%PY%" -m pip install --user aqtinstall
"%PY%" -m aqt install-qt --outputdir "%USERPROFILE%\.local\spentit-deps\qt" windows desktop 6.8.3 win64_msvc2022_64 -m qtshadertools
```

The CMake package is `%USERPROFILE%\.local\spentit-deps\qt\6.8.3\msvc2022_64`.
`qmake.exe` is in that prefix's `bin`. This is the Qt 6.8 open-source Windows
desktop build for MSVC 2022, 64-bit. The macOS verification used Qt 6.12.0.
This Windows build stays on 6.8.3.

Qt 6.8 does not ship a fonts directory. Tests print `QFontDatabase: Cannot find font directory C:/Users/swessels/.local/spentit-deps/qt/6.8.3/msvc2022_64/lib/fonts`. The window still opened.

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
  -DOPENSSL_ROOT_DIR="C:\Users\swessels\develop\github\external\vcpkg\installed\x64-windows" ^
  -DSPENTIT_PCRE2_ROOT="%DEPS%\pcre2-10.49" ^
  -DSPENTIT_STORAGE_PROTOTYPE=ON ^
  -DSPENTIT_IMPORT_WORKFLOW=ON ^
  -DSPENTIT_IMPORT_UI=ON ^
  -DSPENTIT_RULES=ON ^
  -DSPENTIT_SQLCIPHER_SOURCE="%REPO%\build\dependencies\sqlcipher-build" ^
  -DSPENTIT_APPLICATION_OUTPUT_DIRECTORY="%REPO%\build"

cmake --build "%REPO%\build\import-ui-rules-release" --target spentit_import_experiment
```

Configure identified MSVC `19.44.35229.0` and OpenSSL `3.5.2`. The link after
the directory-flush correction ended with `Linking CXX executable ...\build\SpentIt.exe`.
First-party `/W4` output in that rebuild had no warnings. SQLCipher stayed `/W0`.

Launch from the same prompt. Qt and OpenSSL both have to be on `PATH`. Qt loads
plugins relative to `Qt6Core.dll` in the Qt `bin` directory. `windeployqt` was
not run.

```bat
set PATH=%USERPROFILE%\.local\spentit-deps\qt\6.8.3\msvc2022_64\bin;C:\Users\swessels\develop\github\external\vcpkg\installed\x64-windows\bin;%PATH%
build\SpentIt.exe
```

On 2026-11-03 the process stayed up with window title `SpentIt`. That launch
was before directory publication used write access. Creating a document at
that point failed in the parent flush, with `Encrypted storage operation failed`.
The executable was relinked after the correction in the next section. The
passing UI tests then created, reopened, saved, and committed documents.

## Tests

Use the same `PATH` as launch. `ctest` is the one beside the Visual Studio CMake.

```bat
ctest --test-dir "%REPO%\build\import-ui-rules-release" --output-on-failure --parallel 4
```

On 2026-11-03 this printed `98% tests passed, 1 tests failed out of 40` and
`Total Test time (real) = 208.00 sec`. The failure was `32 - import.ui (Failed)`
at 176.71 sec.

`tests/storage/CMakeLists.txt` returns immediately on `WIN32`. Those translation
units include `unistd.h` and call `fork` / `waitpid` in the same functions as
the other storage cases. They were not ported, and no Windows crash harness
was added. The whole storage directory is absent, so Windows registers 40
tests. A Linux `ctest` of this tree registers 159, including the storage cases.

The `import.ui` log that `ctest` prints is SQLCipher `hmac check failed for pgno=1`
and `sqlite3Codec` decrypt errors. Those lines go to stderr from deliberate
wrong-password and invalid-file attempts. They are not the assertion.

These test executables are Windows-subsystem programs, so Qt Test writes
`PASS` and `FAIL` with `OutputDebugString`. `ctest` does not capture that.
A `DBWIN_BUFFER` listener showed:

```text
FAIL!  : ImportUiTests::accessibilityAndKeyboard() 'guardAccessible=QAccessible::queryAccessibleInterface(guardSave)' returned FALSE.
```

Under `QT_QPA_PLATFORM=offscreen`, the unsaved-changes Save button
(`objectName` `guardSave`) has no accessible interface. Waiting on the same
query for five seconds still fails. The Windows accessibility bridge also
prints `Error: 31` (`ERROR_GEN_FAILURE`) during these offscreen runs. The
ledger list and the payee field in the same test do get interfaces. The
assertion was left in place.

One run of that binary with the native Windows platform, which is not the
`ctest` configuration, created the Save button's interface and then failed a
later search check: the ledger row count stayed 3 where the test expects 0.
That run was not repeated with the window focused, and it is not the `ctest`
result.

`import.navigation` passed in 27.12 sec. Qt 6.8 warns that `Menu.popup()` is
not an invokable; the available method is `popup(QQmlV4FunctionPtr)`. The test
tries the invokable first, then evaluates the QML expression `popup()`. Qt
6.12 still has the nullary invokable, so that call remains first.

`import.benchmark_smoke` passed in 12.95 sec. The full-mode benchmark submits
each staged import-category decision before commit. The product still refuses
acceptance until those decisions are chosen.

Other times from that same run: `import.rules_ui` 33.56 sec,
`import.category_ui` 21.85 sec, `import.committed_ui` 26.74 sec,
`import.group_benchmark_smoke` 3.42 sec, `import.recovery_ui` 2.67 sec,
`import.qml` 0.85 sec.

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
- SQLite filenames are UTF-8 (`u8string()`). The verification-cache receipt hashes the UTF-8 form of the lexically normal absolute path.
- A private file is created with `FILE_SHARE_DELETE`, so a later rename can proceed while that handle is open.

`aclapi.h` pulls in `rpcndr.h`, which defines `small` and `hyper` as macros.
The header undefines both after the include. `NOMINMAX` and
`WIN32_LEAN_AND_MEAN` are set before `windows.h`.

The passing tests exercised create, reopen, save, and commit. They did not
separately prove a refused hard link, a second process failing to take the
lock, reparse-point rejection, the owner-only DACL, the receipt path hash, or
behavior across power loss.

## Same checkout from WSL

This machine's WSL Ubuntu already matched the
[Linux development setup](linux-development-setup.md): Qt 6.8.3 `gcc_64`,
PCRE2 10.49, and Ubuntu OpenSSL 3.0.13 (`/usr/lib/x86_64-linux-gnu/libcrypto.so`).
Configure used `build/wsl-import-ui-rules-release`. The application link was
warning-clean. `build/SpentIt` opened a WSLg window on Wayland (`xdg-shell`,
Wayland-EGL). Mesa printed ZINK/EGL warnings of the same class as the Linux
guide, and the window still updated.

The checkout itself is on DrvFS (`/mnt/c`). Storage-test temporary files were
under `/tmp`, on the WSL disk. DrvFS locking and durability remain the weaker
filesystem for the source tree and for any document opened from `/mnt/c`.

`ctest --test-dir build/wsl-import-ui-rules-release --output-on-failure --parallel 2`
finished with the totals at the top of this guide. These storage cases passed:
`storage.crashes` 12.97 sec, `storage.io` 22.41 sec, `storage.backup_crashes`
66.91 sec, and `storage.backup_failures` 97.68 sec.

`accounts.io` passed in 126.98 sec, `accounts.crashes` in 127.76 sec, `accounts.mapping_openings` in 3.11 sec, and `accounts.mapping_opening_faults` in 657.45 sec. Failures observed before those account cases continued:

- `import.review_sheet_faults` reached its 1200 sec timeout.
- `transferstore.io` (225.61 sec) and `transferstore.crashes` (229.37 sec). Both printed operations 0 through 4 (81, 81, 77, 81, and 157 write/sync positions) and then `Transfer store assertion at line 368` in `tests/storage/transfer_store_tests.cpp`. That line is the propose-match fault loop (operation 5). After an injected write or sync failure, the transfer sequence was neither the value from before the attempt nor exactly one greater after the retry. An isolated rerun had not been done.
- `accounts.basics` (0.57 sec) stopped with `Choose a category for every rule`. The test saves a rule whose only action is a payee template.
- `accounts.validation` (11.54 sec) and `accounts.groups` (0.62 sec) stopped with `Choose the imported categories before accepting transactions`. `accounts.group_io` (305.68 sec) and `accounts.group_crashes` (304.19 sec) printed the same message on mapped fault sweep kind 4, after group operations 0 through 3 (129, 165, 21, and 81 write/sync positions).
- `accounts.combined` (0.56 sec) and `accounts.combined_io` (0.84 sec) stopped with `Clear categories in the ledger before running rules again`, then `Isolated combined scenario failed, status 256`.

Those three messages are the category-gated rule and import-category checks from commit `e350ebd`. The account tests still exercise the earlier save and accept calls. The Unix source diff in this Windows session is warning line-breaks, identifier renames, and the Windows `#ifdef` branches. Those were not edited again because of these failures.

The statement suites through `statement.faults` then passed, including `statement.generated_faults` (423.24 sec), `statement.import_undo_faults` (536.65 sec), `statement.reimport_faults` (262.30 sec), `statement.rollback_faults` (149.38 sec), and `statement.acceptance_faults` (128.11 sec). `import.replacement` failed in 0.89 sec at `tests/storage/import_tests.cpp:518`. That check follows a rejected create through a symlink and requires the original file bytes to stay unchanged. `import.basics` through `import.backup` passed, then `import.io` passed in 753.41 sec and `import.crashes` in 759.12 sec. `import.backup_failures` and `import.backup_crashes` each hit the 2400 sec timeout. `import.backup_publication`, `import.backup_cancellation`, `import.validation`, `import.opening`, and `import.cancellation` then passed. The committed suites through `committed.crashes` passed, including `committed.io` (305.52 sec) and `committed.crashes` (314.51 sec).

`category.setup` failed in 0.43 sec at `tests/storage/category_tests.cpp:65`. That line creates a document with starter categories and requires a catalog of 41. The starter list in `src/storage/category_store.inc` currently has 46 names; the five `Utils:` entries are the difference. `category.catalog` through `category.validation` passed, then `category.io` passed in 330.65 sec and `category.crashes` in 337.06 sec. The rule-store cases through `rulestore.io` passed, including `rulestore.category_faults` (158.01 sec) and `rulestore.io` (184.22 sec).

`rulestore.bulk` failed in 1.82 sec. `tests/storage/bulk_cases.inc:22` asks a bulk preview of a pending row to fail with text containing `committed`. The operation failed with `Select an accepted transaction that has not been deleted.` `rulestore.crashes` then passed in 188.32 sec, `rulestore.bulk_io` in 312.59 sec, `rulestore.bulk_crashes` in 318.52 sec, and `rulestore.account_faults` in 226.34 sec. `import.manual_transactions` passed in 72.42 sec. `import.navigation` failed in 23.45 sec: 18 cases passed, 2 failed, 1 skipped. Both failures are `QMetaObject::invokeMethod(..., "popup")` at the lines compiled into that binary (`import_navigation_tests.cpp` 467 and 506). Qt 6.8 only exposes `popup(QQmlV4FunctionPtr)`. The current source calls `showPopup`, which tries the invokable and then evaluates the QML expression `popup()`. That WSL binary was compiled before that change. The rebuilt Windows `import.navigation` passed. `import.recovery_ui` passed in 2.79 sec, `import.committed_ui` in 24.32 sec, `import.category_ui` in 18.98 sec, `import.rules_ui` in 31.05 sec, `import.qml` in 0.70 sec, and `import.group_benchmark_smoke` in 3.40 sec.

`import.ui` failed in 150.57 sec: 80 cases passed and `ImportUiTests::accessibilityAndKeyboard` failed because `guardAccessible` was null at `tests/ui/import_model_tests.cpp:1583` in that binary. That is the same offscreen unsaved-changes Save button failure as the Windows run. `import.benchmark_smoke` failed in 10.65 sec with `commit: Choose the imported categories before accepting transactions`. The current benchmark source submits those staged decisions before commit, and the rebuilt Windows `import.benchmark_smoke` passed. This WSL binary was compiled before that edit.

`ctest` then printed `89% tests passed, 18 tests failed out of 159` and `Total Test time (real) = 8628.74 sec`. The 18 are `import.review_sheet_faults` (timeout), `transferstore.io`, `transferstore.crashes`, `accounts.basics`, `accounts.validation`, `accounts.combined`, `accounts.combined_io`, `accounts.groups`, `accounts.group_io`, `accounts.group_crashes`, `import.replacement`, `import.backup_failures` (timeout), `import.backup_crashes` (timeout), `category.setup`, `rulestore.bulk`, `import.ui`, `import.navigation`, and `import.benchmark_smoke`.

The WSL test compile warned on Qt `QTRY_VERIFY` / `QTRY_COMPARE`
(`-Wconversion` inside the Qt macros). Objects compiled before a later line
split also warned on sized deallocation in the diagnostics collector test and
on two storage one-line `if` statements. The Unix warning flags were left
unchanged. The application link had no warnings.

## Limits

- Packaging and `windeployqt` were not run. The package command in [local packages](../packaging/README.md) is still an example.
- macOS was not rebuilt here. Its recorded verification used Qt 6.12.0.
- Sanitizer builds are refused on Windows.
- Physical power loss was not tested.
- The `import.ui` offscreen failure is the Windows `ctest` result and the same case in the WSL run.
- Fork-based storage tests were not ported and did not run on Windows.
