# Windows x64 alpha: installation and feedback

This is an unsigned development build, not a finished release. Use a disposable
FL Studio project. The full console artwork, auxiliary routing, MIDI learn, standalone combination
import and reliable offline-load readiness are still unfinished.

## Download and install

1. On GitHub, open Actions → Windows VST3 alpha → a successful run.
2. Download its OrganVST-Windows-x64 artifact and extract the outer artifact ZIP.
3. Extract OrganVST-Windows-x64.zip. Keep the supplied source ZIP and licenses.
4. Close FL Studio. Copy the **whole OrganVST.vst3 folder**, including Contents
   and adjacent DLLs inside it, to `C:\Program Files\Common Files\VST3\`.
   Windows may request administrator permission for that folder.
5. Open FL Studio's Plugin Manager and scan for plugins. Add OrganVST as an
   instrument. You do not need MSYS2 or a compiler installed.
6. Choose Load organ and select your own extracted .organ/.odf or a ZIP containing
   one compatible definition. No third-party organ samples are supplied.

## Divisions and layers

**Use MIDI channels** plays independent parts: channels 1–15 address successive
manuals and channel 16 addresses the pedalboard. This permits simultaneous parts
in one instance when your DAW sends their corresponding channels.

For one keyboard playing several divisions, select each division and click **Add
to layer**. A plus sign marks included divisions. **Remove from layer** removes
that division; removing the final one returns to native MIDI channels. **Play only
this** selects one destination, while **Layer all divisions** includes all exposed
manuals, pedal and auxiliary groups. Each division still needs its stops enabled.
Browsing tabs/divisions never changes routing. Changes release held notes and
routing is saved with the project. Layering auxiliary toy divisions can trigger
their effects when their mapped notes are played; choose only desired divisions.

Stops and Couplers are distinct tabs. Tremulants has its own global tab and no
longer appears as a keyboard division. Switches includes authored rank switches.
The catalog order/first 128 automation IDs are unchanged.

The editor shows the selected division's numeric MIDI range and last received
channel/note. **Audition note N** tests the selected division independently of the
layer mask. Use minus/plus to select its pitch, and enable a stop first. Outside
its authored note range a division remains silent.

## Crescendo and expression

The **Pedals** tab exposes definition-authored expression enclosures; Barton has
Main and Solo. Drag or click their bars to open/close the swell boxes. Barton's
minimum enclosure level is 20%, so fully closed is quieter rather than silent.
DAW parameters Crescendo and Expression 1, Expression 2, etc. can be automated or
linked to hardware through the DAW. Direct MIDI learn remains unfinished.

The plugin-owned crescendo stores 32 registration steps in the project:

1. Move Crescendo to step 1. Set a quiet registration (or turn all stops off),
   return to Pedals and click **Capture registration at this step**.
2. Move to a higher step, choose a louder registration in the control tabs, then
   capture that step. Repeat as desired; **Clear this step** removes a capture.
3. Move or automate Crescendo. It recalls the closest stored step at or below the
   pedal position, so jumping over a programmed step still works. Before the first
   stored step, registration remains unchanged.

Capture stores the exposed stops, couplers, switches and tremulants. Expression
positions are independent. Empty programs do not invent a registration or boost
master volume. Standalone GrandOrgue crescendo banks are not imported by this
plugin-owned program. Captures, pedal positions and layers are saved per instance;
source organ files and standalone settings are unchanged.

Wait until loading finishes before playback or export. Offline exports during
loading are not yet protected against incomplete output. Recall currently saves
the path, master gain, and all exposed controls by definition keys. Standalone combinations are not yet included. Version 0.1 projects reset to
authored registration defaults; recreate/recheck their automation and registration.
Version 0.4 retains 0.2 registrations and migrates 0.3 single-division routes to
one-division layers. Use a copy of your project for testing; older plugins cannot
read the new V4 state. A missing pack must be loaded again using Load organ.

## Diagnostics

Use Export diagnostics in the editor. Logs are under
`%LOCALAPPDATA%\OrganVST\logs`. The exported ZIP redacts the user-directory prefix
and excludes samples. Inspect it before sharing; add the steps that reproduce the
problem and your Windows/FL Studio/audio-device versions. Include the commit from
build-manifest.json. Preserve any crash report and the matching debug-symbols
folder. MinGW builds use DWARF .debug symbols, not MSVC PDB files.

A passing CI build proves compilation, synthetic engine tests, and Steinberg
validation, including validation with MSYS2 removed from PATH. It does not prove
FL Studio compatibility. Follow FL-STUDIO-CHECKLIST.md and report actual results.

## Build/source information

The source ZIP contains this project's tracked files, the pinned GrandOrgue and
VST3 SDK trees, their submodules, and applied integration changes. The manifest
records MSYS2 dependency package versions. External MSYS2 dependency sources and
build recipes are maintained at https://github.com/msys2/MINGW-packages ; package
information is at https://packages.msys2.org/. System packages are currently
rolling dependencies, not a fully pinned reproducible toolchain.

Source files retain their individual license notices. The combined instrument
includes GPLv3-or-later ZitaConvolver code alongside GPL-2.0-or-later GrandOrgue.
Preserve all supplied licenses and source materials when sharing this alpha.
Barton and any other independently obtained packs have separate licenses and must
not be added to the plugin package or public repository.

## Version 0.4 retest priorities

- Confirm Great, Solo and auxiliary divisions are available and there are no
  generated repeating 16/8/4/BAS/MEL coupler lists. Barton has 170 authored
  writable controls; legitimate controls with identical labels are kept distinct.
- Enable a stop in each division and use Audition, then click Play only this
  and play FL Studio notes in the displayed numeric MIDI range. Record whether
  Audition and host notes each work, plus the Received channel/note indicator.
- In Use MIDI channels mode, Barton uses Accomp=1, Great=2, Solo=3, Pedal 2T=4,
  Accomp 2T=5, Great 2T=6, Acc Traps=7, Toe Pistons=8, Console Toys=9, Pedal=16.
  Test authored couplers with a destination division stop enabled.
- Barton Toe Pistons has toy sounds on MIDI notes 36–42; note 43 is unmapped.
  Enable both Looped Toys and Percussive Toys to hear all mapped toy notes.
  Console Toys uses MIDI 36 for the bell and 37 for the triangle. Its two controls
  are both named Bell in the definition: enable both for this test. The other
  pipes are intentionally silent. Use numeric MIDI pitches because DAWs differ
  in octave naming. The plugin does not transpose ordinary middle-C notes into
  these toy keys.
- Try xylophone/trap switches and authored reiteration; ordinary percussion should
  decay. Auxiliary/second-touch divisions retain their original relationships.
- Save/reopen and duplicate an instance after loading completes. Controls beyond
  the first 128 must retain their state. Distinct registrations must stay independent.
- Duplicate instances share immutable sample payloads only when the DAW loads them
  in the same process. Each instance retains its own voices, controls and metadata;
  memory will therefore grow somewhat. Initial loading still decodes samples.
- Export diagnostics after reproducing a problem. Logs include catalog entries and
  shared-sample byte/block counts. Do not send the organ pack with the report.

- Layer Great and Solo from one incoming channel; remove each in turn. Also play
  independent parts using native channels. Change layers during a held chord.
- Confirm Couplers are absent from Stops and tremulants are accessible in their
  tab without a Tremulants division in the sidebar.
- Verify Main/Solo expression and a captured quiet/loud crescendo sequence;
  automate pedals, save/reopen with the editor closed, and duplicate the instance.
