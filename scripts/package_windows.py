#!/usr/bin/env python3
"""Assemble an alpha bundle, recursively resolve DLLs, and preserve build evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]

def output(*args, cwd=ROOT):
    return subprocess.check_output(args, cwd=cwd, text=True).strip()

def copy_dependencies(binary, destination, runtime):
    pending = [binary]
    seen = set()
    runtime_files = {p.name.lower(): p for p in runtime.glob('*.dll')}
    system = Path(os.environ['WINDIR'])/'System32'
    system_files = {p.name.lower() for p in system.glob('*.dll')}
    while pending:
        current = pending.pop()
        for name in re.findall(r'DLL Name:\s*(\S+)', output('objdump', '-p', str(current))):
            key = name.lower()
            if key in seen:
                continue
            seen.add(key)
            if key in runtime_files:
                source = runtime_files[key]
                target = destination/source.name
                shutil.copy2(source, target)
                pending.append(target)
            elif key not in system_files and not key.startswith(('api-ms-win-', 'ext-ms-win-')):
                raise RuntimeError(f'Unresolved dependency of {current.name}: {name}')

def zip_tree(source, destination):
    with zipfile.ZipFile(destination, 'w', zipfile.ZIP_DEFLATED) as z:
        for path in sorted(source.rglob('*')):
            if path.is_file():
                z.write(path, source.name/path.relative_to(source))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True)
    args = parser.parse_args()
    bundles = [p for p in args.build.rglob('OrganVST.vst3') if p.is_dir()]
    validators = list(args.build.rglob('validator.exe'))
    if len(bundles) != 1 or len(validators) != 1:
        raise RuntimeError(f'Expected one bundle and validator, got {bundles}, {validators}')
    args.output.mkdir(parents=True, exist_ok=True)
    package = args.output/'OrganVST-Windows-x64'
    package.mkdir()  # Refuse to mingle artifacts from different builds.
    bundle = package/'OrganVST.vst3'
    shutil.copytree(bundles[0], bundle)
    binary = bundle/'Contents/x86_64-win/OrganVST.vst3'
    if not binary.is_file():
        raise RuntimeError(f'Missing Windows x64 binary: {binary}')
    copy_dependencies(binary, binary.parent, args.runtime)
    symbols = package/'debug-symbols'
    symbols.mkdir()
    subprocess.run(['objcopy', '--only-keep-debug', str(binary), str(symbols/'OrganVST.debug')], check=True)
    # Keep the plugin unstripped so native crash inspection also works directly.
    validation = args.output/'validation'
    validation.mkdir()
    validator = validation/'validator.exe'
    shutil.copy2(validators[0], validator)
    copy_dependencies(validator, validation, args.runtime)
    for source, name in [('docs/WINDOWS_TESTING.md','INSTALL-AND-TEST.md'),
                         ('docs/FL_STUDIO_TESTING.md','FL-STUDIO-CHECKLIST.md'),
                         ('LICENSE','LICENSE'),('dependencies.json','dependencies.json')]:
        shutil.copy2(ROOT/source, package/name)
    licenses = package/'licenses'
    licenses.mkdir()
    # Preserve upstream licensing, including notices for dynamically linked DLLs.
    for base, prefix in [(ROOT/'.deps/grandorgue','GrandOrgue'),
                         (ROOT/'.deps/vst3sdk','VST3SDK'),
                         (args.runtime.parent/'share/licenses','MSYS2')]:
        for path in base.rglob('*'):
            if path.is_file() and '.git' not in path.parts and (
                prefix == 'MSYS2' or path.name.lower().startswith(('license','copying','copyright','authors'))):
                target = licenses/prefix/path.relative_to(base)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, target)
    commit = output('git','rev-parse','HEAD')
    manifest = {'commit':commit, 'compiler':output('g++','--version'),
                'packages':output('pacman','-Q'), 'files':{}}
    for path in package.rglob('*'):
        if path.is_file():
            manifest['files'][path.relative_to(package).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    (package/'build-manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    zip_tree(package, args.output/'OrganVST-Windows-x64.zip')
    # Exact source snapshot, including modified pinned dependencies and submodules.
    # Only tracked files are eligible, so private packs and build trees stay out.
    with zipfile.ZipFile(args.output/'OrganVST-corresponding-project-source.zip','w',zipfile.ZIP_DEFLATED) as z:
        for directory, prefix in [(ROOT,'OrganVST'),(ROOT/'.deps/grandorgue','OrganVST/.deps/grandorgue'),
                                  (ROOT/'.deps/vst3sdk','OrganVST/.deps/vst3sdk')]:
            names = output('git','ls-files','-z','--recurse-submodules',cwd=directory).split('\0')
            for name in names:
                path=directory/name
                if path.is_file():
                    z.write(path, prefix+'/'+name)
    print(f'Packaged {commit}; FL Studio host testing remains pending.')

if __name__ == '__main__':
    main()
