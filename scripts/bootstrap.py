#!/usr/bin/env python3
"""Fetch pinned upstream trees and apply explicit root/submodule patches."""
import argparse
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def run(*args, cwd=None):
    subprocess.run(args, cwd=cwd, check=True)

def output(*args, cwd):
    return subprocess.check_output(args, cwd=cwd, text=True)

def check_patch(dest, patch):
    expected = patch.read_text() if patch and patch.exists() else ''
    diff = output('git','diff','--ignore-submodules=all',cwd=dest)
    staged = output('git','diff','--cached','--ignore-submodules=all',cwd=dest)
    untracked = output('git','ls-files','--others','--exclude-standard',cwd=dest)
    if staged or untracked or (diff and diff != expected):
        raise RuntimeError(f'Refusing to overwrite modified dependency {dest}')
    return bool(diff)

def apply_patch(dest, patch):
    if check_patch(dest, patch) or not patch or not patch.exists():
        return
    run('git','apply','--check',str(patch),cwd=dest)
    run('git','apply',str(patch),cwd=dest)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('names',nargs='*',help='Dependencies to fetch (default: all)')
    args=parser.parse_args()
    dependencies=json.loads((ROOT/'dependencies.json').read_text())
    for name in args.names or dependencies:
        if name not in dependencies:
            parser.error(f'Unknown dependency: {name}')
        spec=dependencies[name]
        dest=ROOT/'.deps'/name
        if not dest.exists():
            dest.mkdir(parents=True)
            run('git','init',str(dest))
            run('git','remote','add','origin',spec['url'],cwd=dest)
        run('git','config','core.autocrlf','false',cwd=dest)
        if output('git','remote','get-url','origin',cwd=dest).strip()!=spec['url']:
            raise RuntimeError(f'Unexpected origin for {name}')
        patch=ROOT/'patches'/f'{name}-host.patch'
        patched=check_patch(dest,patch)
        head=subprocess.run(['git','rev-parse','HEAD'],cwd=dest,capture_output=True,text=True)
        if patched and head.stdout.strip()!=spec['revision']:
            raise RuntimeError(f'Patched dependency {name} has an unexpected revision')
        nested=spec.get('submodule_patches',{})
        # Audit already populated submodules before any checkout operation.
        submodules=output('git','submodule','status','--recursive',cwd=dest) if head.returncode==0 else ''
        for line in submodules.splitlines():
            if line.startswith('-'):
                continue
            relative=line.split()[1]
            check_patch(dest/relative,ROOT/nested[relative] if relative in nested else None)
        if not patched:
            run('git','fetch','--depth','1','origin',spec['revision'],cwd=dest)
            run('git','checkout','--detach',spec['revision'],cwd=dest)
            apply_patch(dest,patch)
        run('git','submodule','update','--init','--recursive','--depth','1',cwd=dest)
        for relative,filename in nested.items():
            apply_patch(dest/relative,ROOT/filename)

if __name__=='__main__':
    main()
