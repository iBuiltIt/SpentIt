#!/usr/bin/env python3
"""Build the macOS libraries for a SpentIt package that targets macOS 14.4.

Qt 6.12's own floor is 14.4, and it has to be compiled with a newer SDK.
This uses the SDK on this Mac and stamps every library 14.4. It does not
change the Homebrew install used by the daily ./build/SpentIt.

    python3 tools/build_macos_14_4.py

The results land in build-package/deps-14.4 and build-package/qt-14.4.
"""

from __future__ import annotations

import os
import shutil
import stat
import subprocess
import sys
import tarfile
import urllib.request
from pathlib import Path


REPO = Path(__file__).resolve().parent.parent
ROOT = REPO / "build-package" / "macos-14.4"
SRC = ROOT / "src"
WORK = ROOT / "work"
DEPS = ROOT / "deps"
QT = ROOT / "qt"
TARGET = "14.4"
JOBS = "6"


def log(message: str) -> None:
    print(message, flush=True)


def run(cmd: list[str], cwd: Path | None = None, env: dict[str, str] | None = None) -> None:
    log("+ " + " ".join(cmd))
    subprocess.check_call(cmd, cwd=str(cwd) if cwd else None, env=env)


def sdk_path() -> str:
    return subprocess.check_output(["xcrun", "--show-sdk-path"], text=True).strip()


def build_env(extra: dict[str, str] | None = None) -> dict[str, str]:
    env = os.environ.copy()
    flags = f"-mmacosx-version-min={TARGET}"
    env["MACOSX_DEPLOYMENT_TARGET"] = TARGET
    env["SDKROOT"] = sdk_path()
    env["CFLAGS"] = flags
    env["CXXFLAGS"] = flags
    env["LDFLAGS"] = flags
    env["CMAKE_PREFIX_PATH"] = str(DEPS)
    env["PKG_CONFIG_LIBDIR"] = str(DEPS / "lib" / "pkgconfig")
    env["PKG_CONFIG_PATH"] = str(DEPS / "lib" / "pkgconfig")
    # Homebrew CMake would otherwise find the 27.x bottles first.
    env["CMAKE_IGNORE_PREFIX_PATH"] = "/opt/homebrew"
    if extra:
        env.update(extra)
    return env


def download(url: str, name: str) -> Path:
    SRC.mkdir(parents=True, exist_ok=True)
    dest = SRC / name
    if dest.is_file() and dest.stat().st_size > 0:
        return dest
    log(f"downloading {name}")
    request = urllib.request.Request(url, headers={"User-Agent": "SpentIt-packager"})
    with urllib.request.urlopen(request, timeout=120) as response, dest.open("wb") as handle:
        shutil.copyfileobj(response, handle)
    return dest


def extract(archive: Path) -> Path:
    dest = WORK / (archive.name + ".d")
    if dest.is_dir():
        return dest
    dest.mkdir(parents=True)
    with tarfile.open(archive) as tar:
        tar.extractall(dest, filter="data")
    return dest


def source_tree(archive: Path) -> Path:
    root = extract(archive)
    children = [path for path in root.iterdir() if path.name != ".DS_Store"]
    if len(children) == 1 and children[0].is_dir():
        return children[0]
    return root


def stamped(name: str) -> bool:
    return (ROOT / "stamps" / name).is_file()


def stamp(name: str) -> None:
    path = ROOT / "stamps" / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(name + "\n", encoding="utf-8")


def macho_minos(path: Path) -> tuple[int, ...] | None:
    text = subprocess.check_output(["otool", "-l", str(path)], text=True, errors="replace")
    best: tuple[int, ...] | None = None
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped.startswith("minos "):
            continue
        parts = tuple(int(piece) for piece in stripped.split()[1].split(".") if piece.isdigit())
        if best is None or parts > best:
            best = parts
    return best


def is_shared_library(path: Path) -> bool:
    if not path.is_file() or path.is_symlink() or path.suffix != ".dylib":
        if not path.is_file() or path.is_symlink() or ".framework/" not in path.as_posix():
            return False
    try:
        magic = path.read_bytes()[:4]
    except OSError:
        return False
    return magic in {b"\xfe\xed\xfa\xce", b"\xfe\xed\xfa\xcf", b"\xce\xfa\xed\xfe", b"\xcf\xfa\xed\xfe"}


def otool_deps(path: Path) -> list[str]:
    deps = []
    for line in subprocess.check_output(["otool", "-L", str(path)], text=True, errors="replace").splitlines()[1:]:
        text = line.strip()
        if text:
            deps.append(text.split(" (compatibility version", 1)[0].strip())
    return deps


def macho_id(path: Path) -> str | None:
    lines = subprocess.check_output(["otool", "-D", str(path)], text=True, errors="replace").splitlines()
    if len(lines) >= 2 and lines[1].strip():
        return lines[1].strip()
    return None


def fix_dependency_ids(prefix: Path) -> None:
    """Give every library an @rpath id so Qt's tools can load them while building."""
    libs = [path for path in prefix.rglob("*") if is_shared_library(path)]
    for lib in libs:
        lib.chmod(lib.stat().st_mode | stat.S_IWUSR)
        ident = macho_id(lib)
        if not ident or ident.startswith(("@rpath/", "/usr/lib/", "/System/")):
            new_id = ident
        else:
            new_id = f"@rpath/{Path(ident).name}"
            subprocess.check_call(["install_name_tool", "-id", new_id, str(lib)])
        for dep in otool_deps(lib):
            if dep == ident or dep.startswith(("@rpath/", "@loader_path/", "@executable_path/", "/usr/lib/", "/System/")):
                continue
            if "/" in dep and not str(prefix) in dep:
                continue
            subprocess.check_call(["install_name_tool", "-change", dep, f"@rpath/{Path(dep).name}", str(lib)])
        if lib.suffix != ".dylib":
            continue
        rpaths = subprocess.check_output(["otool", "-l", str(lib)], text=True, errors="replace")
        if "@loader_path" not in rpaths:
            subprocess.call(["install_name_tool", "-add_rpath", "@loader_path", str(lib)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def require_floor(prefix: Path) -> None:
    bad = []
    for path in prefix.rglob("*"):
        if not is_shared_library(path):
            continue
        version = macho_minos(path)
        if version is not None and version > (14, 4):
            bad.append(f"{version} {path}")
    if bad:
        raise SystemExit("a library was built for a newer macOS than 14.4:\n" + "\n".join(bad[:20]))


def cmake_install(name: str, source: Path, args: list[str], source_subdir: str = "") -> None:
    if stamped(name):
        log(f"skip {name}")
        return
    src = source / source_subdir if source_subdir else source
    build = WORK / f"build-{name}"
    if build.exists():
        shutil.rmtree(build)
    cmd = [
        "cmake", "-S", str(src), "-B", str(build), "-G", "Ninja",
        f"-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_INSTALL_PREFIX={DEPS}",
        f"-DCMAKE_PREFIX_PATH={DEPS}",
        f"-DCMAKE_OSX_DEPLOYMENT_TARGET={TARGET}",
        "-DCMAKE_OSX_ARCHITECTURES=arm64",
        f"-DCMAKE_IGNORE_PREFIX_PATH=/opt/homebrew",
        "-DBUILD_SHARED_LIBS=ON",
        *args,
    ]
    env = build_env()
    run(cmd, env=env)
    run(["cmake", "--build", str(build), "--parallel", JOBS], env=env)
    run(["cmake", "--install", str(build)], env=env)
    require_floor(DEPS)
    stamp(name)


def configure_install(name: str, source: Path, args: list[str], source_subdir: str = "") -> None:
    if stamped(name):
        log(f"skip {name}")
        return
    src = source / source_subdir if source_subdir else source
    build = WORK / f"build-{name}"
    if build.exists():
        shutil.rmtree(build)
    build.mkdir(parents=True)
    env = build_env()
    run([str(src / "configure"), f"--prefix={DEPS}", *args], cwd=build, env=env)
    run(["make", "-j", JOBS], cwd=build, env=env)
    run(["make", "install"], cwd=build, env=env)
    require_floor(DEPS)
    stamp(name)


def build_openssl(name: str, source: Path, prefix: Path) -> None:
    if stamped(name):
        log(f"skip {name}")
        return
    build = WORK / f"build-{name}"
    if build.exists():
        shutil.rmtree(build)
    shutil.copytree(source, build, symlinks=True)
    for path in build.rglob("*"):
        if path.is_file() and not path.is_symlink():
            path.chmod(path.stat().st_mode | stat.S_IWUSR)
    env = build_env()
    run([
        "perl", str(build / "Configure"), "darwin64-arm64-cc", "shared", "no-tests",
        f"--prefix={prefix}", "--libdir=lib", f"-mmacosx-version-min={TARGET}",
    ], cwd=build, env=env)
    run(["make", "-j", JOBS], cwd=build, env=env)
    run(["make", "install_sw"], cwd=build, env=env)
    require_floor(prefix)
    stamp(name)


def build_icu(source: Path) -> None:
    if stamped("icu"):
        log("skip icu")
        return
    src = source / "icu" / "source" if (source / "icu" / "source").is_dir() else source / "source"
    if not src.is_dir():
        src = source
    build = WORK / "build-icu"
    if build.exists():
        shutil.rmtree(build)
    build.mkdir(parents=True)
    env = build_env()
    run([
        str(src / "configure"), f"--prefix={DEPS}",
        "--enable-shared", "--disable-static", "--disable-samples", "--disable-tests",
    ], cwd=build, env=env)
    run(["make", "-j", JOBS], cwd=build, env=env)
    run(["make", "install"], cwd=build, env=env)
    fix_dependency_ids(DEPS)
    require_floor(DEPS)
    stamp("icu")


def link_deps_into_qt() -> None:
    """Qt tools look for @rpath libraries next to the frameworks."""
    dest = QT / "lib"
    dest.mkdir(parents=True, exist_ok=True)
    for directory in (DEPS / "lib", DEPS / "openssl-3" / "lib", DEPS / "openssl-4" / "lib"):
        if not directory.is_dir():
            continue
        for path in directory.iterdir():
            if path.name.endswith(".framework") or ".dylib" in path.name:
                target = dest / path.name
                if target.exists():
                    continue
                target.symlink_to(path.resolve() if path.is_symlink() or path.is_file() else path)


def build_qt(name: str, source: Path, extra: list[str]) -> None:
    if stamped(name):
        log(f"skip {name}")
        return
    build = WORK / f"build-{name}"
    fix_dependency_ids(DEPS)
    link_deps_into_qt()
    if (build / "lib").is_dir():
        fix_dependency_ids(build / "lib")
    cmd = [
        "cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_INSTALL_PREFIX={QT}",
        f"-DCMAKE_PREFIX_PATH={QT};{DEPS};{DEPS / 'openssl-4'}",
        f"-DCMAKE_OSX_DEPLOYMENT_TARGET={TARGET}",
        "-DCMAKE_OSX_ARCHITECTURES=arm64",
        "-DCMAKE_IGNORE_PREFIX_PATH=/opt/homebrew",
        "-DQT_BUILD_EXAMPLES=OFF",
        "-DQT_BUILD_TESTS=OFF",
        "-DQT_NO_APPLE_SDK_AND_XCODE_CHECK=ON",
        *extra,
    ]
    env = build_env({"PKG_CONFIG_LIBDIR": os.pathsep.join([
        str(DEPS / "lib" / "pkgconfig"),
        str(DEPS / "openssl-4" / "lib" / "pkgconfig"),
        str(DEPS / "openssl-3" / "lib" / "pkgconfig"),
    ])})
    run(cmd, env=env)
    run(["cmake", "--build", str(build), "--parallel", JOBS], env=env)
    run(["cmake", "--install", str(build)], env=env)
    require_floor(QT)
    stamp(name)


def main() -> int:
    for path in (SRC, WORK, DEPS, QT):
        path.mkdir(parents=True, exist_ok=True)
    env = build_env()
    os.environ.update(env)

    cmake_install("brotli", source_tree(download(
        "https://github.com/google/brotli/archive/refs/tags/v1.2.1.tar.gz", "brotli-1.2.1.tar.gz")), [])
    cmake_install("libpng", source_tree(download(
        "https://github.com/pnggroup/libpng/archive/refs/tags/v1.6.59.tar.gz", "libpng-1.6.59.tar.gz")),
        ["-DPNG_SHARED=ON", "-DPNG_STATIC=OFF", "-DPNG_TESTS=OFF"])
    cmake_install("jpeg", source_tree(download(
        "https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.2.1/libjpeg-turbo-3.2.1.tar.gz",
        "libjpeg-turbo-3.2.1.tar.gz")),
        ["-DENABLE_SHARED=ON", "-DENABLE_STATIC=OFF", "-DWITH_JPEG8=ON"])
    cmake_install("zstd", source_tree(download(
        "https://github.com/facebook/zstd/archive/refs/tags/v1.5.7.tar.gz", "zstd-1.5.7.tar.gz")),
        ["-DZSTD_BUILD_SHARED=ON", "-DZSTD_BUILD_STATIC=OFF", "-DZSTD_BUILD_PROGRAMS=OFF"],
        source_subdir="build/cmake")
    cmake_install("double-conversion", source_tree(download(
        "https://github.com/google/double-conversion/archive/refs/tags/v3.4.0.tar.gz",
        "double-conversion-3.4.0.tar.gz")), [])
    configure_install("libb2", source_tree(download(
        "https://github.com/BLAKE2/libb2/releases/download/v0.98.1/libb2-0.98.1.tar.gz",
        "libb2-0.98.1.tar.gz")), ["--enable-shared", "--disable-static"])
    cmake_install("graphite2", source_tree(download(
        "https://github.com/silnrsi/graphite/releases/download/1.3.15/graphite2-1.3.15.tgz",
        "graphite2-1.3.15.tgz")), [])
    cmake_install("pcre2", source_tree(download(
        "https://github.com/PCRE2Project/pcre2/releases/download/pcre2-10.49/pcre2-10.49.tar.bz2",
        "pcre2-10.49.tar.bz2")), [
            "-DPCRE2_BUILD_PCRE2_8=ON", "-DPCRE2_BUILD_PCRE2_16=ON", "-DPCRE2_BUILD_PCRE2_32=OFF",
            "-DPCRE2_BUILD_TESTS=OFF", "-DPCRE2_SUPPORT_JIT=ON",
        ])
    build_openssl("openssl-3", source_tree(download(
        "https://github.com/openssl/openssl/releases/download/openssl-3.5.9/openssl-3.5.9.tar.gz",
        "openssl-3.5.9.tar.gz")), DEPS / "openssl-3")
    build_openssl("openssl-4", source_tree(download(
        "https://github.com/openssl/openssl/releases/download/openssl-4.1.0/openssl-4.1.0.tar.gz",
        "openssl-4.1.0.tar.gz")), DEPS / "openssl-4")
    build_icu(source_tree(download(
        "https://github.com/unicode-org/icu/releases/download/release-79.1/icu4c-79.1-sources.tgz",
        "icu4c-79.1-sources.tgz")))
    cmake_install("freetype", source_tree(download(
        "https://github.com/freetype/freetype/archive/refs/tags/VER-2-15-0.tar.gz",
        "freetype-2.15.0.tar.gz")), [
            "-DFT_DISABLE_HARFBUZZ=ON", "-DFT_REQUIRE_PNG=ON", "-DFT_REQUIRE_BROTLI=ON",
        ])
    cmake_install("harfbuzz", source_tree(download(
        "https://github.com/harfbuzz/harfbuzz/releases/download/15.1.0/harfbuzz-15.1.0.tar.xz",
        "harfbuzz-15.1.0.tar.xz")), [
            "-DHB_BUILD_SUBSET=OFF", "-DHB_HAVE_FREETYPE=ON", "-DHB_HAVE_GRAPHITE2=ON",
            "-DHB_HAVE_GLIB=OFF",
        ])

    qt_args = [
        "-DFEATURE_sql_mysql=OFF", "-DFEATURE_sql_odbc=OFF", "-DFEATURE_sql_psql=OFF",
        "-DFEATURE_openssl_linked=ON", "-DFEATURE_glib=OFF", "-DFEATURE_dbus=OFF",
        "-DFEATURE_system_doubleconversion=ON", "-DFEATURE_system_freetype=ON",
        "-DFEATURE_system_harfbuzz=ON", "-DFEATURE_system_jpeg=ON",
        "-DFEATURE_system_libb2=ON", "-DFEATURE_system_pcre2=ON",
        "-DFEATURE_system_png=ON", "-DFEATURE_system_zlib=ON",
        "-DFEATURE_system_md4c=OFF", "-DFEATURE_system_textmarkdownreader=OFF",
        f"-DOPENSSL_ROOT_DIR={DEPS / 'openssl-4'}",
    ]
    build_qt("qtbase", source_tree(download(
        "https://download.qt.io/official_releases/qt/6.12/6.12.0/submodules/qtbase-everywhere-src-6.12.0.tar.xz",
        "qtbase-everywhere-src-6.12.0.tar.xz")), qt_args)
    build_qt("qtshadertools", source_tree(download(
        "https://download.qt.io/official_releases/qt/6.12/6.12.0/submodules/qtshadertools-everywhere-src-6.12.0.tar.xz",
        "qtshadertools-everywhere-src-6.12.0.tar.xz")), [])
    build_qt("qtdeclarative", source_tree(download(
        "https://download.qt.io/official_releases/qt/6.12/6.12.0/submodules/qtdeclarative-everywhere-src-6.12.0.tar.xz",
        "qtdeclarative-everywhere-src-6.12.0.tar.xz")), [])
    build_qt("qtsvg", source_tree(download(
        "https://download.qt.io/official_releases/qt/6.12/6.12.0/submodules/qtsvg-everywhere-src-6.12.0.tar.xz",
        "qtsvg-everywhere-src-6.12.0.tar.xz")), [])
    log("DEPS READY")
    log(f"Qt: {QT}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as exc:
        command = exc.cmd if isinstance(exc.cmd, str) else " ".join(exc.cmd)
        log(f"command failed ({exc.returncode}): {command}")
        sys.exit(exc.returncode or 1)
