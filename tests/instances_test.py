#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys
import zipfile
from render_test import fixtures
probe, root = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
fixtures(root)
for source in (root/'test.organ', root/'test.zip'):
    if source.suffix == '.zip':
        with zipfile.ZipFile(source, 'w', compression=zipfile.ZIP_DEFLATED) as z:
            for name in ('test.organ','tone.wav'):
                z.write(root/name, 'pack/'+name)
    result=subprocess.run([str(probe),str(source),str(root/'data')],capture_output=True,text=True,timeout=30)
    assert result.returncode==0, result.stdout+result.stderr
    print(source.name, result.stdout.strip())
cache=root/'data'/'pack-cache'
assert not cache.exists() or not list(cache.iterdir()), 'Extracted pack survived engine destruction'
