#!/usr/bin/env python3
"""Fetch pinned upstream trees without adding them to public Git history."""
import argparse
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def run(*args, cwd=None):
    subprocess.run(args, cwd=cwd, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("names", nargs="*", help="Dependencies to fetch (default: all)")
    args = parser.parse_args()
    dependencies = json.loads((ROOT / "dependencies.json").read_text())
    for name in args.names or dependencies:
        if name not in dependencies:
            parser.error(f"Unknown dependency: {name}")
        spec = dependencies[name]
        dest = ROOT / ".deps" / name
        if not dest.exists():
            dest.mkdir(parents=True)
            run("git", "init", str(dest))
            run("git", "remote", "add", "origin", spec["url"], cwd=dest)
        run("git", "config", "core.autocrlf", "false", cwd=dest)
        actual_url = subprocess.check_output(
            ["git", "remote", "get-url", "origin"], cwd=dest, text=True).strip()
        if actual_url != spec["url"]:
            raise RuntimeError(f"Unexpected origin for {name}: {actual_url}")
        dirty = subprocess.check_output(
            ["git", "status", "--porcelain"], cwd=dest, text=True)
        patch = ROOT / "patches" / f"{name}-host.patch"
        expected_diff = patch.read_text() if patch.exists() else ""
        current_diff = subprocess.check_output(["git", "diff"], cwd=dest, text=True)
        head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=dest,
                              capture_output=True, text=True)
        staged_diff = subprocess.check_output(["git", "diff", "--cached"], cwd=dest, text=True)
        already_patched = (dirty and not staged_diff and current_diff == expected_diff and
                           head.stdout.strip() == spec["revision"] and
                           not subprocess.check_output(["git", "ls-files", "--others", "--exclude-standard"], cwd=dest, text=True))
        if dirty and not already_patched:
            raise RuntimeError(f"Refusing to overwrite modified dependency {name}")
        if not already_patched:
            run("git", "fetch", "--depth", "1", "origin", spec["revision"], cwd=dest)
            run("git", "checkout", "--detach", spec["revision"], cwd=dest)
            if patch.exists():
                run("git", "apply", "--check", str(patch), cwd=dest)
                run("git", "apply", str(patch), cwd=dest)
        run("git", "submodule", "update", "--init", "--recursive", "--depth", "1", cwd=dest)


if __name__ == "__main__":
    main()
