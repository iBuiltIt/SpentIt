#!/usr/bin/env python3
"""Stage a fresh, host-native local package. Never overwrite an existing package."""
import argparse
from pathlib import Path
import platform
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def run(*args):
    subprocess.run([str(a) for a in args], check=True)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True, help='Configured native Release build tree')
    parser.add_argument('--output', type=Path, required=True, help='New package directory (must not exist)')
    parser.add_argument('--qt-deploy', help='Path to macdeployqt or windeployqt; otherwise use PATH')
    parser.add_argument('--libpath', type=Path, action='append', default=[], help='Additional macOS framework/library search directory')
    parser.add_argument('--exe', type=Path, help='Windows executable, after the storage port is complete')
    parser.add_argument('--dll', type=Path, action='append', default=[], help='Windows non-Qt runtime DLL; repeat for each dependency')
    args = parser.parse_args()
    build, output = args.build.resolve(), args.output.resolve()
    system = platform.system()
    archive_path = Path(str(output) + ('.zip' if system == 'Windows' else '.tar.gz'))
    if archive_path.exists():
        parser.error('Archive already exists; choose a fresh output name.')
    if output.exists():
        parser.error('Output already exists; choose a fresh directory.')
    if not (build / 'CMakeCache.txt').is_file():
        parser.error('Build must contain a configured CMake tree.')
    cache = (build / 'CMakeCache.txt').read_text()
    if 'CMAKE_CONFIGURATION_TYPES:' not in cache and 'CMAKE_BUILD_TYPE:STRING=Release' not in cache:
        parser.error('Use a Release build tree for local packages.')
    deploy = None
    if system in ('Darwin', 'Windows'):
        deploy = shutil.which(args.qt_deploy or ('macdeployqt' if system == 'Darwin' else 'windeployqt'))
        if not deploy:
            parser.error('Qt deployment tool not found; supply --qt-deploy.')
    if system == 'Windows':
        if not args.exe or not args.exe.is_file():
            parser.error('Windows storage is not implemented. Supply --exe only after a native application build is available.')
        for dll in args.dll:
            if not dll.is_file() or dll.suffix.lower() != '.dll':
                parser.error(f'Not a runtime DLL: {dll}')
        names = [dll.name.lower() for dll in args.dll]
        if len(names) != len(set(names)):
            parser.error('Duplicate DLL basenames.')
    elif system not in ('Darwin', 'Linux'):
        parser.error(f'Unsupported platform: {system}')
    target = 'spentit_macos_bundle' if system == 'Darwin' else 'spentit_import_experiment'
    run('cmake', '--build', build, '--config', 'Release', '--target', target, '--parallel', '2')
    output.mkdir(parents=True, exist_ok=False)
    # A failed deployment deliberately leaves an INCOMPLETE marker for diagnosis.
    marker = output / 'INCOMPLETE'
    marker.write_text('Packaging has not completed. Do not use this directory.\n')
    if system == 'Darwin':
        source = build / 'local-app/SpentIt.app'
        if not source.exists():
            source = build / 'local-app/Release/SpentIt.app'
        app = output / 'SpentIt.app'
        shutil.copytree(source, app, symlinks=True)
        library_paths = [Path(deploy).parent.parent / 'lib', *args.libpath]
        command = [str(deploy), str(app), f'-qmldir={ROOT / "src/ui"}',
                   '-always-overwrite', '-no-strip', '-codesign=-',
                   *(f'-libpath={path.resolve()}' for path in library_paths)]
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (output / 'deployment.log').write_text(result.stdout)
        if result.returncode or 'ERROR:' in result.stdout:
            raise SystemExit(f'Qt deployment failed; inspect {output / "deployment.log"}.')
        run('codesign', '--verify', '--deep', '--strict', app)
    elif system == 'Linux':
        run('cmake', '--install', build, '--config', 'Release', '--component', 'SpentIt', '--prefix', output)
        # Archive is a host-local install tree, not a self-contained AppImage.
        (output / 'RUNTIME-REQUIREMENTS.txt').write_text(
            'Requires matching system Qt 6.8+ (Quick, Controls, QML, platform plugins), OpenSSL 3 and PCRE2 10.49.\n'
            'Run bin/SpentIt. Install this tree into a prefix on PATH for desktop menu integration.\n'
            'No cross-distribution portability is claimed.\n')
    else:
        exe = output / 'SpentIt.exe'
        shutil.copy2(args.exe, exe)
        for dll in args.dll:
            shutil.copy2(dll, output / dll.name)
        run(deploy, '--release', '--qmldir', ROOT / 'src/ui', exe)
        (output / 'RUNTIME-REQUIREMENTS.txt').write_text(
            'Prepared after a separate Windows storage port. Verify OpenSSL/PCRE2 and all other non-Qt DLLs,\n'
            'compiler runtime, plugins and startup on the target machine. windeployqt handles Qt, not every dependency.\n')
    shutil.copy2(ROOT / 'packaging/README.md', output / 'LOCAL-PACKAGE.md')
    shutil.copytree(ROOT / 'third_party', output / 'dependency-metadata',
                    ignore=shutil.ignore_patterns('*.xml', '*.json', '*.pdf'))
    marker.unlink()
    try:
        archive = shutil.make_archive(str(output), 'zip' if system == 'Windows' else 'gztar',
                                      root_dir=output.parent, base_dir=output.name)
    except Exception:
        marker.write_text('Archive creation failed. The archive may be incomplete.\n')
        raise
    print(f'Local package: {output}\nArchive: {archive}')

if __name__ == '__main__':
    main()
