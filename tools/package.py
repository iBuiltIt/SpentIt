#!/usr/bin/env python3
"""Build a SpentIt release package.

    python3 tools/package.py
    python3 tools/package.py --sign
    APPLE_NOTARY_PROFILE=machines-notary python3 tools/package.py --sign

macOS writes dist/SpentIt.app. Without --sign it also writes a zip. With
--sign it signs with the Developer ID in the keychain, writes a DMG, and
notarizes when a notary profile or key is already in the environment.

Windows uses this command and writes a zip. Linux uses this command and
writes a .tar.gz. Those archive steps are added on those machines.

The notary password stays in the login keychain. This script does not read
or store it, and it does not upload the archive anywhere but Apple's
notary service.
"""

from __future__ import annotations

import argparse
import os
import platform
import plistlib
import re
import shutil
import stat
import subprocess
import sys
import tempfile
from pathlib import Path


REPO = Path(__file__).resolve().parent.parent
BUNDLE_ID = "com.stefanwessels.SpentIt"
ENTITLEMENTS = REPO / "packaging" / "macos" / "SpentIt.entitlements"
SQLCIPHER_SOURCE = REPO / "build" / "dependencies" / "sqlcipher-build"
QT_PREFIX = Path("/opt/homebrew")
OPENSSL_ROOT = Path("/opt/homebrew/opt/openssl@3")
MACOS_14_4 = REPO / "build-package" / "macos-14.4"
DEPS_14_4 = MACOS_14_4 / "deps"
QT_14_4 = MACOS_14_4 / "qt"
# Qt's own frameworks were built for 14.4. Homebrew's other bottles may be
# newer. The plist records the highest minimum actually present in the app.
WANTED_MIN_OS = (14, 4)


def log(message: str) -> None:
    print(message, flush=True)


def shown_command(cmd: list[str]) -> str:
    redacted = []
    hide_next = False
    for arg in cmd:
        if hide_next:
            redacted.append("***")
            hide_next = False
            continue
        if arg in {"--password", "--key"}:
            redacted.append(arg)
            hide_next = True
            continue
        redacted.append(arg)
    return " ".join(redacted)


def run(cmd: list[str], cwd: Path | None = None, env: dict[str, str] | None = None) -> None:
    log("+ " + shown_command(cmd))
    subprocess.check_call(cmd, cwd=str(cwd) if cwd else None, env=env)


def capture(cmd: list[str]) -> str:
    return subprocess.check_output(cmd, text=True, errors="replace")


def read_version() -> str:
    text = (REPO / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"project\(SpentIt VERSION ([0-9]+(?:\.[0-9]+)*)", text)
    if not match:
        raise SystemExit("could not find the SpentIt version in CMakeLists.txt")
    return match.group(1)


def os_tag() -> str:
    machine = platform.machine().lower()
    arch = {"amd64": "x64", "x86_64": "x64", "arm64": "arm64", "aarch64": "arm64"}.get(machine, machine)
    system = platform.system()
    if system == "Darwin":
        return f"macos-{arch}"
    if system == "Windows":
        return f"windows-{arch}"
    if system == "Linux":
        return f"linux-{arch}"
    raise SystemExit(f"unsupported operating system {system}")


def is_macho(path: Path) -> bool:
    if not path.is_file() or path.is_symlink():
        return False
    try:
        magic = path.read_bytes()[:4]
    except OSError:
        return False
    return magic in {
        b"\xfe\xed\xfa\xce",
        b"\xfe\xed\xfa\xcf",
        b"\xce\xfa\xed\xfe",
        b"\xcf\xfa\xed\xfe",
        b"\xca\xfe\xba\xbe",
        b"\xbe\xba\xfe\xca",
    }


def iter_machos(root: Path) -> list[Path]:
    found = []
    for path in root.rglob("*"):
        if is_macho(path):
            found.append(path)
    return found


def is_system_dep(dep: str) -> bool:
    return dep.startswith(("/usr/lib/", "/System/"))


def macho_id(path: Path) -> str | None:
    lines = capture(["otool", "-D", str(path)]).splitlines()
    if len(lines) >= 2 and lines[1].strip():
        return lines[1].strip()
    return None


def otool_deps(path: Path) -> list[str]:
    deps = []
    for line in capture(["otool", "-L", str(path)]).splitlines()[1:]:
        text = line.strip()
        if not text:
            continue
        deps.append(text.split(" (compatibility version", 1)[0].strip())
    return deps


def macho_rpaths(path: Path) -> list[str]:
    lines = capture(["otool", "-l", str(path)]).splitlines()
    found = []
    pending = False
    for line in lines:
        text = line.strip()
        if text.startswith("cmd LC_RPATH"):
            pending = True
            continue
        if pending and text.startswith("path "):
            found.append(text.split(" ", 1)[1].rsplit(" (", 1)[0])
            pending = False
    return found


def parse_version(text: str) -> tuple[int, ...]:
    parts = []
    for piece in text.split("."):
        if not piece.isdigit():
            break
        parts.append(int(piece))
    return tuple(parts) if parts else (0,)


def macho_minos(path: Path) -> tuple[int, ...] | None:
    best: tuple[int, ...] | None = None
    for line in capture(["otool", "-l", str(path)]).splitlines():
        text = line.strip()
        if not text.startswith("minos "):
            continue
        version = parse_version(text.split()[1])
        if best is None or version > best:
            best = version
    return best


def format_version(version: tuple[int, ...]) -> str:
    return ".".join(str(part) for part in version)


def inside(app: Path, path: Path) -> bool:
    try:
        path.resolve().relative_to(app.resolve())
        return True
    except ValueError:
        return False


def framework_root(path: Path) -> Path | None:
    parts = path.resolve().parts
    for index, part in enumerate(parts):
        if part.endswith(".framework"):
            return Path(*parts[: index + 1])
    return None


def make_writable(path: Path) -> None:
    if path.is_symlink():
        return
    path.chmod(path.stat().st_mode | stat.S_IWUSR)


def make_tree_writable(root: Path) -> None:
    if not root.is_symlink():
        make_writable(root)
    for path in root.rglob("*"):
        if not path.is_symlink():
            make_writable(path)


def unsign(path: Path) -> None:
    subprocess.run(["codesign", "--remove-signature", str(path)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)


def rewrite_id(path: Path, install_name: str) -> None:
    make_writable(path)
    unsign(path)
    run(["install_name_tool", "-id", install_name, str(path)])


def rewrite_dep(path: Path, old: str, new: str) -> None:
    if old == new:
        return
    make_writable(path)
    unsign(path)
    run(["install_name_tool", "-change", old, new, str(path)])


def main_executable_dir(app: Path) -> Path:
    return app / "Contents" / "MacOS"


def resolve_dep(app: Path, binary: Path, dep: str) -> Path | None:
    main_dir = main_executable_dir(app)
    if dep.startswith("@executable_path/"):
        candidate = main_dir / dep.split("/", 1)[1]
        return candidate.resolve() if candidate.exists() else None
    if dep.startswith("@loader_path/"):
        candidate = binary.parent / dep.split("/", 1)[1]
        return candidate.resolve() if candidate.exists() else None
    if dep.startswith("@rpath/"):
        name = dep.split("/", 1)[1]
        # dyld searches rpaths on the loading image and on the images that
        # loaded it, including the main executable.
        owners = [binary]
        main = main_dir / "SpentIt"
        if main.is_file() and main.resolve() != binary.resolve():
            owners.append(main)
        for owner in owners:
            for rpath in macho_rpaths(owner):
                root = rpath.replace("@executable_path", str(main_dir)).replace("@loader_path", str(owner.resolve().parent))
                candidate = Path(root) / name
                if candidate.exists():
                    return candidate.resolve()
        return None
    candidate = Path(dep)
    if candidate.is_file():
        return candidate.resolve()
    return None


def find_named(build: Path, name: str) -> Path | None:
    """Return a Mach-O for this load name. Prefer the 14.4 prefix."""
    for directory in (
        DEPS_14_4 / "lib",
        DEPS_14_4 / "openssl-3" / "lib",
        DEPS_14_4 / "openssl-4" / "lib",
        QT_14_4 / "lib",
    ):
        candidate = directory / name
        if candidate.exists() and is_macho(candidate.resolve()):
            return candidate
    for path in build.rglob(name):
        if "CMakeFiles" in path.parts:
            continue
        resolved = path.resolve()
        if is_macho(resolved):
            return path if path.is_symlink() else resolved
    brew = Path("/opt/homebrew/lib") / name
    if brew.exists() and is_macho(brew.resolve()):
        return brew
    return None


def preferred_id(app: Path, binary: Path) -> str:
    frameworks = (app / "Contents" / "Frameworks").resolve()
    resolved = binary.resolve()
    try:
        relative = resolved.relative_to(frameworks)
    except ValueError:
        current = macho_id(binary)
        return current or f"@rpath/{binary.name}"
    return f"@rpath/{relative.as_posix()}"


def bundle_outside(app: Path, src: Path, copied_from: dict[Path, Path]) -> Path:
    frameworks = app / "Contents" / "Frameworks"
    frameworks.mkdir(parents=True, exist_ok=True)
    root = framework_root(src)
    if root is not None and not inside(app, root):
        dest_root = frameworks / root.name
        previous = copied_from.get(dest_root)
        if previous is not None and previous != root.resolve():
            raise SystemExit(f"two frameworks would both be named {root.name}")
        if not dest_root.exists():
            run(["ditto", str(root), str(dest_root)])
            copied_from[dest_root] = root.resolve()
            log(f"  bundled {root.name}")
        relative = src.resolve().relative_to(root.resolve())
        dest = dest_root / relative
        if not dest.exists():
            raise SystemExit(f"copied {root.name} but {relative} is missing")
        return dest
    dest = frameworks / src.name
    previous = copied_from.get(dest)
    if previous is not None and previous != src.resolve():
        raise SystemExit(f"two libraries would both be named {src.name}")
    if not dest.exists():
        shutil.copy2(src, dest)
        copied_from[dest] = src.resolve()
        log(f"  bundled {src.name}")
    make_writable(dest)
    return dest


def ensure_dep(app: Path, build: Path, binary: Path, dep: str, copied_from: dict[Path, Path]) -> Path:
    resolved = resolve_dep(app, binary, dep)
    if resolved is not None and inside(app, resolved):
        return resolved
    if resolved is None:
        resolved = find_named(build, Path(dep).name)
    if resolved is None:
        raise SystemExit(f"{binary} depends on {dep}, and that file was not found")
    if inside(app, resolved):
        return resolved
    return bundle_outside(app, resolved, copied_from)


def frameworks_rpath(app: Path, binary: Path) -> str:
    frameworks = app / "Contents" / "Frameworks"
    relative = os.path.relpath(frameworks, binary.resolve().parent)
    if relative == ".":
        return "@loader_path"
    return "@loader_path/" + Path(relative).as_posix()


def install_name(cmd: list[str]) -> None:
    log("+ " + shown_command(cmd))
    result = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode == 0:
        return
    message = (result.stdout or "").strip()
    if "do not fit" in message:
        log("left an existing install name in place because the binary has no room for a longer one")
        return
    raise SystemExit(f"command failed ({result.returncode}): {shown_command(cmd)}\n{message}")


def clean_rpaths(app: Path, binary: Path) -> None:
    make_writable(binary)
    unsign(binary)
    for rpath in list(macho_rpaths(binary)):
        if rpath.startswith("@executable_path") or rpath.startswith("@loader_path"):
            continue
        install_name(["install_name_tool", "-delete_rpath", rpath, str(binary)])
    needed = frameworks_rpath(app, binary)
    if needed not in macho_rpaths(binary):
        install_name(["install_name_tool", "-add_rpath", needed, str(binary)])


def close_bundle(app: Path, build: Path) -> None:
    pending = iter_machos(app)
    seen: set[Path] = set()
    copied_from: dict[Path, Path] = {}
    queued_frameworks: set[Path] = set()
    steps = 0
    while pending:
        binary = pending.pop(0)
        real = binary.resolve()
        steps += 1
        if steps > 5000:
            raise SystemExit("dependency walk did not finish")
        if real in seen or not is_macho(real):
            continue
        seen.add(real)
        make_writable(real)
        unsign(real)
        own = macho_id(real)
        for dep in otool_deps(real):
            if not dep or dep == own or is_system_dep(dep):
                continue
            target = ensure_dep(app, build, real, dep, copied_from).resolve()
            new_id = preferred_id(app, target)
            if macho_id(target) != new_id:
                rewrite_id(target, new_id)
            if dep != new_id:
                rewrite_dep(real, dep, new_id)
            root = framework_root(target)
            if root is not None and inside(app, root) and root not in queued_frameworks:
                queued_frameworks.add(root)
                for extra in iter_machos(root):
                    if extra.resolve() not in seen:
                        pending.append(extra)
            elif target not in seen:
                pending.append(target)
        clean_rpaths(app, real)
    assert_closed(app)


def assert_closed(app: Path) -> None:
    for binary in iter_machos(app):
        own = macho_id(binary)
        for dep in otool_deps(binary):
            if not dep or dep == own or is_system_dep(dep):
                continue
            if dep.startswith(("/opt/homebrew", "/usr/local")):
                raise SystemExit(f"{binary} still links an outside library: {dep}")
            resolved = resolve_dep(app, binary, dep)
            if resolved is None or not inside(app, resolved):
                raise SystemExit(f"{binary} still links an outside library: {dep}")
        for rpath in macho_rpaths(binary):
            if rpath.startswith("@executable_path") or rpath.startswith("@loader_path"):
                continue
            raise SystemExit(f"{binary} still has an outside rpath: {rpath}")


def highest_minos(app: Path) -> tuple[int, ...]:
    best: tuple[int, ...] = (0,)
    for binary in iter_machos(app):
        version = macho_minos(binary)
        if version is not None and version > best:
            best = version
    if best == (0,):
        raise SystemExit("could not read a macOS minimum from the app")
    return best


def patch_info_plist(app: Path, version: str, minimum: tuple[int, ...]) -> None:
    path = app / "Contents" / "Info.plist"
    with path.open("rb") as handle:
        info = plistlib.load(handle)
    info["CFBundleIdentifier"] = BUNDLE_ID
    info["CFBundleDisplayName"] = "SpentIt"
    info["CFBundleVersion"] = version
    info["CFBundleShortVersionString"] = version
    info["LSMinimumSystemVersion"] = format_version(minimum)
    info["NSHighResolutionCapable"] = True
    info["LSApplicationCategoryType"] = "public.app-category.finance"
    with path.open("wb") as handle:
        plistlib.dump(info, handle)
    log(f"minimum macOS {format_version(minimum)}")
    if minimum > WANTED_MIN_OS:
        raise SystemExit(
            f"the app requires macOS {format_version(minimum)}. "
            "Rebuild the 14.4 libraries with python3 tools/build_macos_14_4.py."
        )


def install_notices(app: Path) -> None:
    dest = app / "Contents" / "Resources" / "Licenses"
    dest.mkdir(parents=True, exist_ok=True)
    shutil.copy2(REPO / "LICENSE", dest / "LICENSE.txt")
    shutil.copytree(REPO / "third_party" / "notices", dest / "notices", dirs_exist_ok=True)
    for relative in (
        "third_party/qt/README.md",
        "third_party/sqlcipher/LICENSE.md",
        "third_party/sqlcipher/README.md",
        "third_party/pcre2/LICENSE.md",
        "third_party/pcre2/README.md",
    ):
        target = dest / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(REPO / relative, target)


def developer_id(explicit: str | None) -> str:
    if explicit:
        return explicit
    text = capture(["security", "find-identity", "-v", "-p", "codesigning"])
    found = []
    for line in text.splitlines():
        if "Developer ID Application:" not in line:
            continue
        start = line.find('"')
        end = line.rfind('"')
        if start != -1 and end > start:
            found.append(line[start + 1:end])
    if len(found) == 1:
        return found[0]
    if not found:
        raise SystemExit("no Developer ID Application certificate in the keychain. Pass --identity or install the certificate.")
    raise SystemExit("more than one Developer ID Application certificate:\n" + "\n".join(found) + "\nPass --identity.")


def sign_path(path: Path, identity: str | None, entitlements: Path | None = None) -> None:
    if identity is None:
        run(["codesign", "--force", "--sign", "-", str(path)])
        return
    cmd = ["codesign", "--force", "--options", "runtime", "--timestamp", "--sign", identity]
    if entitlements is not None:
        cmd.extend(["--entitlements", str(entitlements)])
    cmd.append(str(path))
    run(cmd)


def sign_app(app: Path, identity: str | None) -> None:
    run(["xattr", "-cr", str(app)])
    main = app / "Contents" / "MacOS" / "SpentIt"
    loose = []
    frameworks = []
    for path in app.rglob("*"):
        if not path.is_dir() or path.is_symlink() or not path.name.endswith(".framework"):
            continue
        if any(part.endswith(".framework") for part in path.relative_to(app).parts[:-1]):
            continue
        frameworks.append(path)
    for binary in iter_machos(app):
        if binary.resolve() == main.resolve():
            continue
        if any(part.endswith(".framework") for part in binary.parts):
            continue
        loose.append(binary)
    loose.sort(key=lambda path: len(path.parts), reverse=True)
    frameworks.sort(key=lambda path: len(path.parts), reverse=True)
    for binary in loose:
        sign_path(binary, identity)
    for framework in frameworks:
        sign_path(framework, identity)
    entitlements = ENTITLEMENTS if identity is not None else None
    sign_path(app, identity, entitlements)
    run(["codesign", "--verify", "--strict", "--verbose=2", str(app)])


def assert_arm64(app: Path) -> None:
    text = capture(["lipo", "-archs", str(app / "Contents" / "MacOS" / "SpentIt")])
    arches = text.split()
    if arches != ["arm64"]:
        raise SystemExit(f"SpentIt architectures are {arches}. This package is Apple Silicon.")


def reset_package_build(build: Path) -> None:
    """Drop a CMake cache that still points at Homebrew. Keep the 14.4 libraries."""
    if not build.exists():
        return
    cache = build / "CMakeCache.txt"
    if not cache.is_file():
        return
    text = cache.read_text(encoding="utf-8", errors="replace")
    qt_line = next((line for line in text.splitlines() if line.startswith("Qt6_DIR:")), "")
    if str(QT_14_4) in qt_line:
        return
    for child in build.iterdir():
        if child.name == "macos-14.4":
            continue
        if child.is_dir():
            shutil.rmtree(child)
        else:
            child.unlink()


def configure_and_build(build: Path, extra: list[str]) -> None:
    if not (SQLCIPHER_SOURCE / "sqlite3.c").is_file():
        raise SystemExit(
            "SQLCipher source is missing. From the repository root, run the prepare step in doc/building.md."
        )
    qt_prefix = QT_14_4 if (QT_14_4 / "lib" / "cmake" / "Qt6" / "Qt6Config.cmake").is_file() else None
    openssl_root = DEPS_14_4 / "openssl-3" if (DEPS_14_4 / "openssl-3" / "lib").is_dir() else None
    if qt_prefix is None or openssl_root is None:
        raise SystemExit("The 14.4 Qt build is missing. Run python3 tools/build_macos_14_4.py")
    if shutil.which("ninja") is None:
        raise SystemExit("ninja is required to package on macOS.")
    reset_package_build(build)
    cmake = [
        "cmake", "-S", str(REPO), "-B", str(build), "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=14.4",
        f"-DCMAKE_PREFIX_PATH={qt_prefix};{DEPS_14_4};{openssl_root}",
        "-DCMAKE_IGNORE_PREFIX_PATH=/opt/homebrew",
        f"-DQt6_DIR={qt_prefix}/lib/cmake/Qt6",
        f"-DOPENSSL_ROOT_DIR={openssl_root}",
        "-DSPENTIT_STORAGE_PROTOTYPE=ON",
        "-DSPENTIT_IMPORT_WORKFLOW=ON",
        "-DSPENTIT_IMPORT_UI=ON",
        "-DSPENTIT_RULES=ON",
        "-DSPENTIT_DIAGNOSTICS=OFF",
        f"-DSPENTIT_SQLCIPHER_SOURCE={SQLCIPHER_SOURCE}",
        *extra,
    ]
    pkg_config = os.pathsep.join([
        str(DEPS_14_4 / "lib" / "pkgconfig"),
        str(openssl_root / "lib" / "pkgconfig"),
    ])
    run(cmake, env={**os.environ, "PKG_CONFIG_LIBDIR": pkg_config, "PKG_CONFIG_PATH": pkg_config})
    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
    if "SPENTIT_DIAGNOSTICS:BOOL=ON" in cache:
        raise SystemExit("refusing to package a diagnostics build")
    if "SPENTIT_RULES:BOOL=ON" not in cache:
        raise SystemExit("refusing to package without rules")
    qt_line = next((line for line in cache.splitlines() if line.startswith("Qt6_DIR:")), "")
    if str(qt_prefix) not in qt_line:
        raise SystemExit("the package build did not select the 14.4 Qt")
    run(["cmake", "--build", str(build), "--target", "spentit_macos_bundle", "--parallel", "2"])


def built_app(build: Path) -> Path:
    for candidate in (build / "local-app" / "SpentIt.app", build / "local-app" / "Release" / "SpentIt.app"):
        if (candidate / "Contents" / "MacOS" / "SpentIt").is_file():
            return candidate
    raise SystemExit("the package build did not produce local-app/SpentIt.app")


def deploy_qt(app: Path, build: Path) -> None:
    deploy = str(QT_14_4 / "bin" / "macdeployqt")
    if not Path(deploy).is_file():
        raise SystemExit("macdeployqt was not found in the 14.4 Qt build")
    libpaths = [
        QT_14_4 / "lib",
        DEPS_14_4 / "lib",
        DEPS_14_4 / "openssl-3" / "lib",
        DEPS_14_4 / "openssl-4" / "lib",
        build / "src" / "ui",
        build / "src" / "ui" / "controls",
    ]
    command = [
        deploy, str(app),
        f"-qmldir={REPO / 'src' / 'ui'}",
        f"-qmldir={REPO / 'src' / 'ui' / 'controls'}",
        "-always-overwrite", "-no-strip",
    ]
    command.extend(f"-libpath={path}" for path in libpaths if path.is_dir())
    log("+ " + shown_command(command))
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    log_path = app.parent / "deployment.log"
    log_path.write_text(result.stdout or "", encoding="utf-8")
    if result.returncode != 0 or "ERROR:" in (result.stdout or ""):
        tail = "\n".join((result.stdout or "").splitlines()[-40:])
        raise SystemExit(f"Qt deployment failed. See {log_path}\n{tail}")
    cocoa = app / "Contents" / "PlugIns" / "platforms" / "libqcocoa.dylib"
    if not cocoa.is_file():
        raise SystemExit("the Cocoa platform plugin was not copied into the app")


def write_qt_conf_if_missing(app: Path) -> None:
    path = app / "Contents" / "Resources" / "qt.conf"
    if path.is_file():
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "[Paths]\nPlugins = PlugIns\nImports = Resources/qml\nQmlImports = Resources/qml\n",
        encoding="utf-8",
    )


def strip_main(app: Path, dsym: Path) -> None:
    main = app / "Contents" / "MacOS" / "SpentIt"
    if dsym.exists():
        shutil.rmtree(dsym)
    run(["dsymutil", str(main), "-o", str(dsym)])
    run(["strip", "-x", str(main)])


def clean_env(home: Path) -> dict[str, str]:
    user = os.environ.get("USER") or os.environ.get("LOGNAME") or "spentit"
    return {
        "HOME": str(home),
        "TMPDIR": str(home / "tmp"),
        "PATH": "/usr/bin:/bin:/usr/sbin:/sbin",
        "USER": user,
        "LOGNAME": user,
        "SHELL": "/bin/zsh",
        "LANG": os.environ.get("LANG", "en_US.UTF-8"),
        "LC_ALL": os.environ.get("LC_ALL", "en_US.UTF-8"),
        "QT_QPA_PLATFORM": "cocoa",
    }


def smoke_create(app: Path) -> None:
    home = Path(tempfile.mkdtemp(prefix="spentit-smoke-"))
    try:
        (home / "tmp").mkdir()
        (home / "Documents").mkdir()
        log("launching the packaged app with Homebrew hidden")
        try:
            result = subprocess.run(
                [str(app / "Contents" / "MacOS" / "SpentIt"), "--smoke-create"],
                env=clean_env(home),
                cwd=str(home),
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                timeout=90,
                check=False,
            )
        except subprocess.TimeoutExpired as exc:
            output = exc.stdout or ""
            if isinstance(output, bytes):
                output = output.decode("utf-8", "replace")
            raise SystemExit("clean launch timed out\n" + "\n".join(output.splitlines()[-40:])) from exc
        text = result.stdout or ""
        (app.parent / "smoke-create.log").write_text(text, encoding="utf-8")
        created = [line[8:] for line in text.splitlines() if line.startswith("Created ")]
        if result.returncode != 0 or not created:
            tail = "\n".join(text.splitlines()[-40:])
            raise SystemExit(f"clean launch failed ({result.returncode})\n{tail}")
        path = Path(created[-1])
        if not path.is_file() or path.stat().st_size <= 0:
            raise SystemExit(f"create reported success but {path} is missing")
        log(f"clean launch created a file ({path.stat().st_size} bytes)")
    finally:
        shutil.rmtree(home, ignore_errors=True)


def notary_args() -> list[str] | None:
    profile = os.environ.get("APPLE_NOTARY_PROFILE")
    if profile:
        return ["--keychain-profile", profile]
    key = os.environ.get("APPLE_NOTARY_KEY")
    key_id = os.environ.get("APPLE_NOTARY_KEY_ID")
    issuer = os.environ.get("APPLE_NOTARY_ISSUER")
    if key and key_id and issuer:
        return ["--key", key, "--key-id", key_id, "--issuer", issuer]
    apple_id = os.environ.get("APPLE_ID")
    team = os.environ.get("APPLE_TEAM_ID")
    password = os.environ.get("APPLE_APP_PASSWORD")
    if apple_id and team and password:
        return ["--apple-id", apple_id, "--team-id", team, "--password", password]
    return None


def notarize(archive: Path) -> bool:
    creds = notary_args()
    if creds is None:
        log("signed. Notarization was skipped.")
        log("Set APPLE_NOTARY_PROFILE to the machines-notary keychain label, then run again with --sign.")
        return False
    run(["xcrun", "notarytool", "submit", str(archive), *creds, "--wait"])
    return True


def staple(path: Path) -> None:
    run(["xcrun", "stapler", "staple", str(path)])
    run(["xcrun", "stapler", "validate", str(path)])


def make_dmg(app: Path, archive: Path, identity: str, volume_name: str) -> None:
    staging = app.parent / f".{app.stem}-dmg"
    if staging.exists():
        shutil.rmtree(staging)
    staging.mkdir()
    run(["ditto", str(app), str(staging / app.name)])
    (staging / "Applications").symlink_to("/Applications")
    if archive.exists():
        archive.unlink()
    run([
        "diskutil", "image", "create", "from",
        "--format", "UDZO",
        "--volumeName", volume_name,
        str(staging),
        str(archive),
    ])
    shutil.rmtree(staging)
    run(["codesign", "--force", "--timestamp", "--sign", identity, str(archive)])
    if notarize(archive):
        staple(archive)
        log(f"notarized {archive.name}")


def zip_app(app: Path, archive: Path) -> None:
    if archive.exists():
        archive.unlink()
    run(["ditto", "-c", "-k", "--keepParent", str(app), str(archive)])
    log(f"wrote {archive.name}")


def notarize_app_then_dmg(app: Path, dmg: Path, identity: str, version: str) -> None:
    if notary_args() is None:
        make_dmg(app, dmg, identity, f"SpentIt {version}")
        return
    holding = app.parent / f".{app.stem}-notarize.zip"
    zip_app(app, holding)
    try:
        if not notarize(holding):
            return
        staple(app)
        log("stapled SpentIt.app")
    finally:
        holding.unlink(missing_ok=True)
    make_dmg(app, dmg, identity, f"SpentIt {version}")
    run(["spctl", "--assess", "--type", "execute", "--verbose=4", str(app)])


def package_macos(args: argparse.Namespace) -> None:
    version = read_version()
    tag = os_tag()
    build = args.build_dir if args.build_dir.is_absolute() else (Path.cwd() / args.build_dir).resolve()
    dist = args.dist if args.dist.is_absolute() else (Path.cwd() / args.dist).resolve()
    dist.mkdir(parents=True, exist_ok=True)
    marker = dist / "INCOMPLETE"
    marker.write_text("Packaging has not completed. Do not distribute this directory.\n", encoding="utf-8")
    identity = developer_id(args.identity) if args.sign else None
    if identity:
        log(f"signing with {identity}")
    if not args.skip_build:
        configure_and_build(build, args.cmake_args)
    source = built_app(build)
    app = dist / "SpentIt.app"
    if app.exists():
        shutil.rmtree(app)
    run(["ditto", str(source), str(app)])
    deploy_qt(app, build)
    for path in iter_machos(app):
        make_writable(path)
        unsign(path)
    close_bundle(app, build)
    make_tree_writable(app)
    minimum = highest_minos(app)
    patch_info_plist(app, version, minimum)
    install_notices(app)
    write_qt_conf_if_missing(app)
    assert_arm64(app)
    strip_main(app, dist / f"SpentIt-{version}-{tag}.dSYM")
    sign_app(app, identity)
    if args.smoke:
        smoke_create(app)
    stem = f"SpentIt-{version}-{tag}"
    if args.sign:
        assert identity is not None
        notarize_app_then_dmg(app, dist / f"{stem}.dmg", identity, version)
    else:
        zip_app(app, dist / f"{stem}.zip")
    marker.unlink()
    log(f"package ready in {dist}")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Package SpentIt for distribution.")
    parser.add_argument("--build-dir", type=Path, default=REPO / "build-package")
    parser.add_argument("--dist", type=Path, default=REPO / "dist")
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--sign", action="store_true", help="macOS: Developer ID, DMG, and notarize when credentials are set")
    parser.add_argument("--identity", help="macOS codesign identity")
    parser.add_argument("--smoke", action="store_true", help="launch the packaged app with Homebrew hidden and create a file")
    parser.add_argument("--cmake-args", action="append", default=[], help="extra cmake configure argument. Repeat for each one")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    system = platform.system()
    if system == "Linux":
        raise SystemExit(
            "Linux packaging uses this command and writes a .tar.gz. "
            "The Linux archive step is added on the Linux machine."
        )
    if system == "Windows":
        raise SystemExit(
            "Windows packaging uses this command and writes a .zip. "
            "The Windows archive step is added on the Windows machine."
        )
    if system != "Darwin":
        raise SystemExit(f"unsupported operating system {system}")
    if "--sign" in argv and system != "Darwin":
        raise SystemExit("--sign is the macOS Developer ID / DMG / notarize path")
    package_macos(parse_args(argv))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except subprocess.CalledProcessError as exc:
        command = exc.cmd if isinstance(exc.cmd, str) else shown_command(list(exc.cmd))
        log(f"command failed ({exc.returncode}): {command}")
        sys.exit(exc.returncode or 1)
