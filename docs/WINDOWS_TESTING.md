# Windows x64 alpha: installation and feedback

This is an unsigned development build, not a finished release. Use a disposable
FL Studio project. Full tabs/console, auxiliary routing, MIDI learn, complete
project state, and reliable offline-load readiness are still unfinished.

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

MIDI channels 1–15 address successive manuals; channel 16 is the pedalboard.
Select a division in the left column and enable a stop. Click **Play this division**
to send incoming notes on any MIDI channel to that division. Selecting another
sidebar group only changes the view; click Play this division again to change the
input route. Click **Use MIDI channels** to restore the assignments above. Changing
routes releases held notes, and the route is saved with the project.

The editor shows the division's MIDI note range and last received channel/note.
**Audition note N** plays a half-second note without FL Studio MIDI input; the
adjacent minus/plus buttons select its pitch. It does not automatically enable a
stop. Notes outside the authored range remain silent. Division and control lists
both have paging.
This alpha has one stereo output. Ordinary
percussion follows the definition's one-shot setting; deliberately reiterating
ranks retain the source behavior.

Wait until loading finishes before playback or export. Offline exports during
loading are not yet protected against incomplete output. Recall currently saves
the path, master gain, and all exposed controls by definition keys. Combinations
and crescendo programming are not yet included. Version 0.1 projects reset to
authored registration defaults; recreate/recheck their automation and registration.
Version 0.3 reads 0.2 registrations unchanged and defaults them to native MIDI
channels; 0.3 saves the explicit input route too. Start a fresh test project for 0.3. A missing pack must be loaded again using Load organ.

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

## Version 0.3 retest priorities

- Confirm Great, Solo and auxiliary divisions are available and there are no
  generated repeating 16/8/4/BAS/MEL coupler lists. Barton has 170 authored
  writable controls; legitimate controls with identical labels are kept distinct.
- Enable a stop in each division and use Audition, then click Play this division
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
