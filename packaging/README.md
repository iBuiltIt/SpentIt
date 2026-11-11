# Local application packages

Owner-approved scope, 2026-10-28: use the supplied light/dark artwork, adapt welcome/running icons to system appearance, and prepare local packages. The light artwork is the fixed installed icon. All supplied PNG sizes and both platform containers now preserve transparent outer corners; see [asset maintenance](../assets/images/README.md). Unknown system appearance falls back to light. Desktop environments decide whether they display runtime icon updates; installed Finder/Explorer/launcher icons do not switch. This does not restyle the rest of the existing UI.

These local packages are for development on the build machine. A downloadable macOS app is a separate command, below. SpentIt is under the Unlicense. Dependency notices travel inside the app. See [LICENSE](../LICENSE). Publication still needs an explicit approval.

## Distribution

From the repository root:

```sh
python3 tools/package.py
APPLE_NOTARY_PROFILE=machines-notary python3 tools/package.py --sign --smoke
```

`python3 tools/package.py` builds a rules-enabled Release app in `build-package/` and writes `dist/SpentIt.app` plus a zip. It does not replace `./build/SpentIt`. Diagnostics stay off.

`--sign` signs with the Developer ID Application certificate in the keychain, using the hardened runtime. It writes a DMG with an `/Applications` shortcut. `machines-notary` is the keychain label from `notarytool store-credentials`. It is not the password. The password stays in the login keychain. When that profile, or the other notary variables accepted by `tools/package.py`, is set, the script notarizes the app and the DMG and staples both. It does not upload the DMG anywhere else.

`--smoke` launches that packaged app with Homebrew hidden and creates a file through the same command as Create file. The file is temporary and is removed at the end of the check.

The bundle id is `com.stefanwessels.SpentIt`. Saved settings keep the existing organization and application name. The script copies Qt, its plugins, QML, and every non-system library those binaries link, then refuses to finish if any of them still points outside the app or requires a newer system than macOS 14.4. Qt 6.12's own floor is 14.4, and it is built with the current SDK by `python3 tools/build_macos_14_4.py`. That build does not replace the Homebrew Qt used by `./build/SpentIt`. The 2026-11-11 package is stamped 14.4.

Windows uses the same command and writes a zip. Linux uses the same command and writes a `.tar.gz`. Those steps are added on those machines.

## macOS local packages

Keep using `./build/SpentIt` for the normal development executable. The separate `spentit_macos_bundle` target produces `local-app/SpentIt.app` inside the configured build directory, with the supplied light ICNS and a local bundle identifier. Saved settings retain their existing organization/application identity.

The latest verified local build with transparent corners is `build/packages/macos-transparent/SpentIt.app`; the matching archive is `build/packages/macos-transparent.tar.gz`. Earlier `macos-local` outputs predate that correction.

To create a fresh package from the repository root, with a Release build configured (choose an unused output name):

```sh
python3 tools/package_local.py --build build/import-ui-rules-release \
  --output build/packages/macos-next --qt-deploy /opt/homebrew/bin/macdeployqt
open build/packages/macos-next/SpentIt.app
```

`--qt-deploy` is optional when the matching Qt tool is on PATH; the path shown is only an example. For split Qt installations, add `--libpath /path/to/extra/frameworks` as needed; the deployment tool’s adjacent `lib` directory is searched automatically. Deployment output is saved to `deployment.log`, and reported errors fail packaging even if Qt returns success. The script builds the bundle, copies it to a new directory, runs Qt's deployment tool with the QML source directory, and applies ad-hoc signing. Local packages retain binary symbols (`-no-strip`); the bundle explicitly links QtSvg so split Qt installations expose its framework during deployment. It also creates a `.tar.gz` archive. Ad-hoc signing is not notarization or a distribution signature. A build-tree bundle still uses development dependencies; use the deployed copy for local package testing.

## Linux (not yet executed)

First configure a native Release application build using the [application dependency instructions](../doc/import-ui.md), with storage/import/UI/rules enabled and matching native dependencies. Then:

```sh
python3 tools/package_local.py --build build/linux-release --output build/packages/linux-local
build/packages/linux-local/bin/SpentIt
```

This stages `bin/SpentIt`, a `.desktop` entry and hicolor PNG icons at all supplied sizes, then creates a `.tar.gz`. It requires installed compatible Qt/QML/platform plugins, OpenSSL and PCRE2; it is a host-local install tree, **not a self-contained portable Linux binary**. For desktop integration, install into a prefix whose `bin` is on PATH and whose `share` is in the desktop's data search path, for example:

```sh
cmake --install build/linux-release --component SpentIt --prefix "$HOME/.local"
```

The package script itself does not install into the user's home or change desktop settings.

## Windows (packaging not yet run)

A native rules-enabled Release build is described in [Windows development setup](../doc/windows-development-setup.md). The package command below has not been run, and `windeployqt` has not been executed.

The executable target has a Windows icon resource, using `assets/images/SpentIt.ico` generated losslessly from the supplied light PNG sizes 16–256. Regenerate it with `python3 tools/generate_windows_icon.py`; Python is not required for normal application builds. Both PNG variants remain embedded for runtime theme changes.

When a package is requested:

```powershell
python tools/package_local.py --build build/windows-release --output build/packages/windows-local --exe build/windows-release/Release/SpentIt.exe --qt-deploy C:/Qt/6.8.3/msvc2022_64/bin/windeployqt.exe --dll C:/dependencies/bin/libcrypto-3-x64.dll --dll C:/dependencies/bin/pcre2-8.dll
```

Paths and DLL names are examples; use the matching compiler/Qt/dependency installation and actual executable location. Repeat `--dll` for every required non-Qt runtime DLL. The script builds the application, stages the executable and supplied DLLs, runs `windeployqt` including QML dependencies, and creates a ZIP. Dependency completeness and execution of that ZIP remain unverified. The development executable is covered in the Windows setup document. No installer is produced.

## Failure behavior and verification

Use a new output directory each time. Existing package directories/archives are refused. Failures stop the script and leave an `INCOMPLETE` marker in a partially staged directory. No application datasets or settings are packaged. `--smoke` uses isolated temporary settings and a nonexistent synthetic dataset path; use it for startup verification without opening private data.

Qt references: [system color scheme](https://doc.qt.io/qt-6/qstylehints.html#colorScheme-prop), [macOS deployment](https://doc.qt.io/qt-6/macos-deployment.html), [Windows deployment](https://doc.qt.io/qt-6/windows-deployment.html). Actual verification is recorded in [project status](../doc/status.md).
