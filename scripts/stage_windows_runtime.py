#!/usr/bin/env python3
"""Stage only resolved runtime DLLs before the SDK loads a plugin for validation."""
from pathlib import Path
import sys
from package_windows import copy_dependencies
for binary in sys.argv[2:]:
    path=Path(binary)
    copy_dependencies(path,path.parent,Path(sys.argv[1]))
# Diagnose loader failures before moduleinfotool can hide the dependency name.
import ctypes
ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x8000)
for binary in sys.argv[2:]:
    path=Path(binary)
    if path.suffix.lower() != '.dll':
        continue
    try:
        ctypes.WinDLL(str(path.resolve()), winmode=0x1100)
    except OSError:
        for dependency in sorted(path.parent.glob('*.dll')):
            try:
                ctypes.WinDLL(str(dependency.resolve()), winmode=0x1100)
            except OSError as error:
                print(f'Cannot load {dependency.name}: {error}', flush=True)
        raise
