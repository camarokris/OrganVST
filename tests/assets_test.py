#!/usr/bin/env python3
"""ZIP integrity, traversal, portability, cancellation and cleanup checks."""
from pathlib import Path
import stat
import subprocess
import sys
import zipfile

probe, root = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
root.mkdir(parents=True, exist_ok=True)
cache = root/'cache'

def check(name, entries, succeeds=False, cancel=False):
    source = root/(name+'.zip')
    with zipfile.ZipFile(source, 'w', compression=zipfile.ZIP_DEFLATED) as z:
        for path, content in entries:
            z.writestr(path, content)
    before = source.read_bytes()
    result = subprocess.run([str(probe), str(source), str(cache)] + (['cancel'] if cancel else []), capture_output=True, text=True, timeout=20)
    assert result.returncode == (0 if succeeds else 1), (name, result.stdout, result.stderr)
    assert source.read_bytes() == before, 'Source ZIP modified'
    assert not list(cache.iterdir()), 'Managed extraction leaked'
    print('PASS', name)

check('valid', [('console/test.organ', '[Organ]'), ('console/images/art.png', 'fixture')], True)
check('traversal', [('../outside.organ', 'bad')])
check('windows-traversal', [('folder\\..\\..\\outside.organ', 'bad')])
check('absolute', [('/outside.organ', 'bad')])
check('drive', [('C:/outside.organ', 'bad')])
check('reserved', [('con.organ', 'bad')])
check('collision', [('a.organ','one'),('A.organ','two')])
check('multiple', [('a.organ','one'),('b.organ','two')])
check('missing', [('sample.wav', 'fixture')])
check('cancel', [('test.organ', 'x'*1000000)], cancel=True)
link=zipfile.ZipInfo('linked.organ');link.create_system=3
link.external_attr=(stat.S_IFLNK|0o777)<<16
check('symlink', [(link, '/outside')])
corrupt=root/'corrupt.zip';corrupt.write_bytes(b'PK\x03\x04truncated')
r=subprocess.run([str(probe),str(corrupt),str(cache)],capture_output=True,timeout=20)
assert r.returncode==1 and not list(cache.iterdir())
# CRC corruption in a stored entry is distinguishable from decompression failure.
corrupt=root/'crc.zip'
with zipfile.ZipFile(corrupt,'w',compression=zipfile.ZIP_STORED) as z:
    z.writestr('test.organ', 'unique-fixture-payload')
corrupt.write_bytes(corrupt.read_bytes().replace(b'unique-fixture-payload', b'broken-fixture-payload'))
r=subprocess.run([str(probe),str(corrupt),str(cache)],capture_output=True,timeout=20)
assert r.returncode==1 and not list(cache.iterdir()), (r.stdout,r.stderr)
print('PASS corrupt ZIP and CRC failures')
