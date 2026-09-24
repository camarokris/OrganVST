#!/usr/bin/env python3
"""Exercise the actual GrandOrgue loader and host renderer, without private packs."""
import math
from pathlib import Path
import struct
import subprocess
import sys
import wave


def chunk(tag, data):
    return tag + struct.pack('<I', len(data)) + data + (b'\0' if len(data) % 2 else b'')


def fixtures(dest):
    dest.mkdir(parents=True, exist_ok=True)
    rate = 48000
    # Exactly 200 periods, with a continuous half-second sustain loop.
    samples = [int(12000 * math.sin(2 * math.pi * 200 * i / rate)) for i in range(rate)]
    pcm = struct.pack('<' + 'h' * len(samples), *samples)
    sampler = struct.pack('<9I', 0, 0, round(1e9/rate), 60, 0, 0, 0, 1, 0)
    sampler += struct.pack('<6I', 0, 0, 12000, 35999, 0, 0)
    payload = b'WAVE' + chunk(b'fmt ', struct.pack('<HHIIHH', 1, 1, rate, rate*2, 2, 16))
    payload += chunk(b'smpl', sampler) + chunk(b'data', pcm)
    (dest/'tone.wav').write_bytes(b'RIFF' + struct.pack('<I',len(payload)) + payload)
    odf = '''[Organ]
ChurchName=OrganVST synthetic test
ChurchAddress=Generated fixture
NumberOfManuals=1
HasPedals=N
NumberOfWindchestGroups=1
NumberOfEnclosures=0
NumberOfTremulants=0
NumberOfReversiblePistons=0
NumberOfGenerals=0
NumberOfDivisionalCouplers=0
NumberOfRanks=2
DivisionalsStoreIntermanualCouplers=N
DivisionalsStoreIntramanualCouplers=N
DivisionalsStoreTremulants=N
GeneralsStoreDivisionalCouplers=N

[WindchestGroup001]
NumberOfEnclosures=0
NumberOfTremulants=0

[Manual001]
Name=Test manual
NumberOfLogicalKeys=2
FirstAccessibleKeyLogicalKeyNumber=1
FirstAccessibleKeyMIDINoteNumber=60
NumberOfAccessibleKeys=2
NumberOfStops=2
Stop001=1
Stop002=2
'''
    for n, percussive in [(1,'N'),(2,'Y')]:
        odf += f'''
[Rank{n:03}]
Name=Rank {n}
FirstMidiNoteNumber=60
NumberOfLogicalPipes=2
WindchestGroup=1
Percussive={percussive}
Pipe001=tone.wav
Pipe002=tone.wav

[Stop{n:03}]
Name=Stop {n}
NumberOfRanks=1
Rank001={n}
Rank001FirstPipeNumber=1
FirstAccessiblePipeLogicalKeyNumber=1
NumberOfAccessiblePipes=2
DefaultToEngaged=N
'''
    (dest/'test.organ').write_text(odf)


def run(probe, dest, stop, rate):
    output = dest/f'render-{stop}-{rate}.wav'
    result = subprocess.run([str(probe), str(dest/'test.organ'), str(output), '1',str(stop),'60',str(rate)], capture_output=True, text=True, timeout=60)
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    with wave.open(str(output)) as w:
        assert w.getframerate() == rate and w.getnchannels() == 2
        data = struct.unpack('<' + 'h'*(w.getnframes()*2), w.readframes(w.getnframes()))
    def energy(start, end):
        values=data[int(start*rate)*2:int(end*rate)*2]
        return sum(x*x for x in values)/len(values)
    assert energy(.1,.5) > 1, 'No note attack'
    if stop == 0:
        assert energy(6,7) > 1, 'Sustained note stopped before note-off'
        # 200 Hz at this amplitude cannot naturally jump this far in one frame.
        left=data[2*rate:14*rate:2]
        assert max(abs(a-b) for a,b in zip(left,left[1:])) < 1500, 'Loop discontinuity'
    else:
        assert energy(6,7) == 0, 'Percussive sample incorrectly looped'
    assert energy(11,12) == 0, 'Note/release did not end'
    print(f'PASS stop={stop} rate={rate}: attack, sustain/one-shot, release, irregular blocks')


if __name__ == '__main__':
    probe, dest = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    fixtures(dest)
    for rate in (44100,48000,96000):
        for stop in (0,1):
            run(probe,dest,stop,rate)
    missing = subprocess.run([str(probe),str(dest/'missing.organ')], capture_output=True, timeout=15)
    assert missing.returncode == 1, 'Missing definition did not fail cleanly'
    print('PASS missing definition returns a recoverable error')

    original = (dest/'test.organ').read_text()
    failures = {
        'missing-sample.organ': original.replace('tone.wav', 'absent.wav'),
        'corrupt-sample.organ': original.replace('tone.wav', 'corrupt.wav'),
        'malformed.organ': '[Organ]\nNumberOfManuals=not-a-number\n',
    }
    (dest/'corrupt.wav').write_bytes(b'not a wave file')
    for name, definition in failures.items():
        path = dest/name
        path.write_text(definition)
        result = subprocess.run([str(probe), str(path)], capture_output=True, timeout=15)
        assert result.returncode == 1, f'{name} did not fail cleanly: {result.stderr!r}'
        print(f'PASS {name} returns a recoverable error')
