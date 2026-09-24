#!/usr/bin/env python3
from pathlib import Path
import subprocess, sys
from render_test import fixtures
probe, root = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
fixtures(root)
base=(root/'test.organ').read_text()
# Reuse an original synthetic loop, never an external sample pack.
base=base[:base.index('[Manual001]')].replace('NumberOfManuals=1','NumberOfManuals=3')
base=base.replace('NumberOfRanks=2','NumberOfRanks=1\nNumberOfSwitches=1')
base+='''[Rank001]
Name=Shared rank
FirstMidiNoteNumber=60
NumberOfLogicalPipes=2
WindchestGroup=1
Percussive=N
Pipe001=tone.wav
Pipe002=tone.wav

[Manual001]
Name=First
NumberOfLogicalKeys=2
FirstAccessibleKeyLogicalKeyNumber=1
FirstAccessibleKeyMIDINoteNumber=60
NumberOfAccessibleKeys=2
NumberOfStops=130
NumberOfCouplers=1
Coupler001=1
'''
for n in range(1,131):base+=f'Stop{n:03}={n}\n'
for n in range(1,134):
 name='Default voice' if n==1 else ('Late voice' if n==131 else ('Logic rank' if n==132 else ('Aux voice' if n==133 else f'Voice {n}')))
 base+=f'\n[Stop{n:03}]\nName={name}\nNumberOfRanks=1\nRank001=1\nRank001FirstPipeNumber=1\nFirstAccessiblePipeLogicalKeyNumber=1\nNumberOfAccessiblePipes=2\nDefaultToEngaged={"Y" if n in (1,133) else "N"}\n'
 if n==132:base+='Function=And\nSwitchCount=1\nSwitch001=1\n'
base+='''
[Manual002]
Name=Second
NumberOfLogicalKeys=2
FirstAccessibleKeyLogicalKeyNumber=1
FirstAccessibleKeyMIDINoteNumber=60
NumberOfAccessibleKeys=2
NumberOfStops=2
Stop001=131
Stop002=132
NumberOfSwitches=1
Switch001=1

[Manual003]
Name=Auxiliary
Displayed=N
NumberOfLogicalKeys=2
FirstAccessibleKeyLogicalKeyNumber=1
FirstAccessibleKeyMIDINoteNumber=36
NumberOfAccessibleKeys=2
NumberOfStops=1
Stop001=133

[Coupler001]
Name=Second to first
DestinationManual=2
DestinationKeyshift=0
CoupleToSubsequentUnisonIntermanualCouplers=N
CoupleToSubsequentUpwardIntermanualCouplers=N
CoupleToSubsequentDownwardIntermanualCouplers=N
CoupleToSubsequentUpwardIntramanualCouplers=N
CoupleToSubsequentDownwardIntramanualCouplers=N
UnisonOff=N
DefaultToEngaged=N

[Switch001]
Name=Switch voice
DefaultToEngaged=N
'''
(root/'catalog.organ').write_text(base)
p=subprocess.run([str(probe),str(root/'catalog.organ'),str(root/'data')],capture_output=True,text=True,timeout=90)
assert p.returncode==0,p.stdout+p.stderr
print(p.stdout.splitlines()[-1])
