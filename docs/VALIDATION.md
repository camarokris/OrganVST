# Validation record

Checkpoint: 2026-09-23, Apple M5 Pro, 24 GiB RAM, arm64 macOS 26.6.2 (25G83).
Debug builds using local Xcode/CMake/Ninja/Homebrew. Exact upstream revisions are
in dependencies.json. This checkpoint is not a complete release acceptance pass.

## Commands actually run

- `python3 scripts/bootstrap.py`: succeeded, including a repeat with the host patch
  already applied. Downloaded source remains ignored.
- `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`: succeeded.
- `cmake --build build -j 6`: succeeded. Latest automatic Steinberg validator:
  **47 tests passed, 0 failed**. 64-bit audio is unsupported and reported as such.
  A validator lifecycle failure during development was fixed by stopping/joining
  the loader at terminate before reinitialization.
- `ctest --test-dir build --output-on-failure`: **3/3 tests passed**, 2.45 seconds
  on the latest run. The engine test runs six synthetic renders (two registrations
  at 44.1/48/96 kHz) plus missing definition, missing sample, corrupt sample, and
  malformed definition failure cases.
- Managed ZIP tests pass valid extraction, traversal (slash/backslash), absolute
  paths, drive prefixes, Windows reserved names, case collisions, multiple/missing
  definitions, cancellation, symlinks, truncation, and CRC corruption. Source ZIPs
  remain byte-identical and managed directories are removed after success/failure.
- Two actual OrganInstance engines loaded simultaneously from a loose definition
  and, separately, from a ZIP: activating one leaves the other silent; panic on
  one allows the other to sustain. Both tests pass, including cache cleanup.
  This is an engine test, not a multi-instance DAW certification.
- Explicit rerun of `build/bin/Debug/validator build/VST3/Debug/OrganVST.vst3`
  after the ZIP integration: 47 passed, 0 failed.
- `git check-ignore`: Barton ZIP/extraction, designated sample directories,
  dependency tree, build products, logs, and DAW project paths are ignored.
  Example source PNG and synthetic fixture WAV paths are not ignored.

## Audio checks

Original generated 200 Hz WAV fixture includes a sustain loop. Two two-pipe ranks
reference it: Percussive=N and Percussive=Y. At each sample rate, assertions check
nonzero attack, sustained output versus one-shot silence, release completion, and
bounded adjacent-sample jumps through sustained loop playback. Each render uses
irregular blocks 1, 7, 64, 127, 256, 511, 1024, and 2048 frames. These tests exercise
the actual adapted GrandOrgue engine, not a replacement sampler. They do not prove
all source loops, tremulants, crossfades, timing, or real-time safety are correct.

Barton3-7.Beta3.14 extracted locally under ignored sample-packs/. Headless loader
read its organ and WAVPack samples. A 48 kHz, MIDI note 60 render of manual 2,
stop 0 (Gt Tuba 16), held 8 seconds plus 4 seconds release, completed with total
energy 39.2594 and final-second energy 0. Summed render-call wall time was
0.0335593 seconds for 12 audio seconds. The old console label said “CPU seconds”;
it has been corrected to “wall seconds.” This is not CPU utilization or a worst
callback/headroom measurement. Peak memory, dense registration headroom, long-run
resource use, and a standalone GrandOrgue comparison have not been measured.

No listening assessment, full Barton percussion/reiteration test, or deterministic
standalone comparison is claimed. Source loop defects must remain distinguishable
from engine defects.

## REAPER host smoke check

REAPER 7.80 installed locally. At 48 kHz / 512 frames:
- Scanned and found VST3i OrganVST.
- Inserted the instrument on a track and opened its editor.
- Loaded build/fixtures/test.organ using the native file picker.
- Observed the synthetic organ name and stops/couplers in the editor.
- Clicked Stop 1 and observed its highlighted toggle state.

The installed smoke-test copy predates the legacy-combination error-handler
patch and ZIP/cancellation UI. Subsequent source builds and validator include them. No audible
host MIDI, project recall, multi-instance, automation playback, routing, offline
export, or crash-recovery host pass is claimed. REAPER's tiny displayed idle CPU
reading is not an instrument workload measurement.

## Outstanding acceptance work

All complete-release gates in PLAN.md remain open. Specifically: no allocation/
lock audit of the entire audio call graph, extensive timing/transport tests,
standalone parity, chords and long playback, complete project state and editorless
recall, offline-load readiness, aux routing, user MIDI/automation mappings,
full tabs/console/crescendo, multi-definition ZIP selection/collection/relinking, diagnostic
manifest coverage and export tests, and portable distribution assembly.

Native Windows compilation, SDK validation and alpha packaging now pass as
recorded below. Initial wxWidgets loading failures were resolved with a Common
Controls v6 activation context. No colleague FL Studio test has run. macOS 13 compatibility and self-contained dynamic-library packaging have not
been verified. Development build outputs are not distribution artifacts.

## ZIP implementation follow-up

Added Homebrew libarchive and used it for extraction after tests demonstrated that
wxWidgets normalizes both names and file-type metadata. Failed intermediate tests
were corrected and rerun; the final suite above passes. Libarchive is an additional
system dependency not yet pinned/bundled for distribution. The current CMake cache
sets CMAKE_PREFIX_PATH=/opt/homebrew/opt/libarchive; README documents the portable
Homebrew prefix command. ZIP loading has not yet been exercised through REAPER's
picker or with the full Barton ZIP.

## GitHub publication and Windows CI

Public repository: https://github.com/camarokris/OrganVST. CI uses Windows Server
2022 x64, MSYS2 UCRT64 and GCC 16.2.0 in the initial runs. Tracked pinned SDK
patches and MinGW UI adapters resolve compilation differences. Runtime dependency
staging and a C host-facing loader isolate the engine's library search directory.

Local follow-up: `actionlint .github/workflows/windows.yml` passed;
`ctest --test-dir build --output-on-failure` passed 3/3 (2.12 seconds). These are
Mac checks, not Windows host results. Public tracked files exclude sample packs,
downloaded dependencies, binaries, logs and local DAW projects.

### Successful Windows alpha run

- Run: https://github.com/camarokris/OrganVST/actions/runs/35956216929
- Source commit: `74997ad6dfc80fc91cfa81836b67e755b746319f`.
- Native CTest: 4/4 passed, 1.75 seconds: engine_render, asset_packs,
  independent_instances, windows_ui_tasks. Synthetic engine tests cover
  44.1/48/96 kHz and irregular blocks; no Barton pack is present in CI.
- Steinberg validator: 47 passed, 0 failed. The packaged plugin passes again
  with PATH limited to Windows system directories. Unsupported extreme sample
  rates are rejected during the validator's sample-rate capability test.
- Packaged runtime DLLs, matching DWARF symbols, license notices, source snapshot,
  install/checklist documents and SHA-256 manifest are available in the run's
  `OrganVST-Windows-x64-74997ad...` artifact (30-day retention).

This verifies Windows Server 2022 CI, not interactive Windows 10/11 or FL Studio.
No Windows GUI interaction, colleague host test, listening pass, performance
benchmark, production signing, or completed release acceptance is claimed.

Downloaded artifact inspection: all 161 manifest file hashes matched. The plugin
ZIP contains 29 DLLs (including the engine), sounds and debug symbols; the source
ZIP contains 12,401 entries including the patched pinned dependency trees. No
Barton/sample-pack or .git directory was present. The host-facing loader imports
only KERNEL32 and Windows CRT API-set DLLs, as checked with llvm-objdump.

## Version 0.2 colleague feedback repair

Reviewed the September 24 colleague report and all three embedded screenshots.
The report establishes a 0.1 FL Studio catalog/sounding failure; it does not
establish a complete host acceptance pass. Source report/images remain local.

Local Apple Silicon Debug build: Steinberg validator 47/47 passed. Expanded
CTest suite 6/6 passed (3.42 seconds): engine_render, asset_packs,
independent_instances, authored_catalog, plugin_registration, sample_cache.
The actual VST3 is dynamically loaded for plugin_registration: authored defaults,
control index 131 playback, stale message rejection and editorless keyed state
recall are exercised. The synthetic catalog tests 134 controls, authored couplers,
switch-controlled stops and auxiliary audition. Cache tests cover concurrent loads,
changed payloads, lifetime after the original instance is destroyed and reclamation.
An initial one-pipe fixture exercised GrandOrgue's effects semantics; it was
corrected to two pipes for the ordinary manual/coupler regression.

Local Barton catalog inspection: 170 writable authored controls, comprising 131
stops, 28 couplers, 7 switches and 4 tremulants. Great has 46 entries, Solo 44,
Accomp 22, Pedal 18; second-touch, trap, toe-piston and console-toy groups remain.
Read-only dependent controls stay in the engine; their controlling switches are
exposed. This catalog inspection is not a listening pass of every Barton rank.

Native Windows 0.2 CI and colleague FL Studio retesting remain pending until
recorded below. Shared payload counts measure immutable main sample bytes, not
complete process RSS; loop/release metadata and voices remain per instance.

REAPER 7.80 inspection opened the 0.2 editor, loaded Barton through its picker,
showed Great and Pedal 2T and confirmed persistent highlights after toggling
Gt Tuba 16 and Bass Drum (beyond the former control limit). Closing the editor
then exposed a heap-corruption crash: Editor::close called forget after
CFrame::close, which already releases its ownership. Removed that second release;
the successful repeat host lifecycle verification is recorded below.

`catalog_probe ... audition` rendered representative Barton divisions at 48 kHz:
Pedal Contra Bourdon 32 energy 0.425144, Accomp Contra Viole 16 0.290959,
Great Gt Tuba 16 1.6959, Solo Tuba 16 1.6959; each short note's tail ended.
These are numerical render checks, not a listening comparison. On this Mac's
Debug build the load-plus-test process took 53.25 seconds wall / 51.63 seconds user,
with 1,385,121,496 bytes peak memory footprint (macOS time -l). Shared main payloads
were 1,267,168,620 bytes across 2,504 unique blocks, with 1,952 reuses within the pack.
These numbers do not measure sustained-chord CPU/headroom or a second Barton copy.

### Editor lifetime and host recall follow-up

After the frame-release correction, REAPER 7.80 survived two custom/generic editor
cycles, a loaded-editor close, subsequent reopen/close and file dialogs without a
new crash. A new disposable project using the 134-control synthetic organ enabled
Late voice (catalog index 131), duplicated the track, saved, reopened with both
editors closed, and saved again. Both instances' serialized V2 engine states
contained all 134 controls and Manual2/Stop0 remained enabled. Runtime diagnostics
showed 104,972 shared payload bytes / 3 blocks for both instances, with reuse counts
increasing. This is a focused host recall/duplication check, not full automation,
transport or offline-export acceptance.

Local machine: Apple M5 Pro, 24 GiB RAM, macOS 26.6.2, arm64. New Debug bundle and
matching dSYM are in dist/local-0.2.0; installed copy is in the user's VST3 folder.
The local bundle still depends on Homebrew and is for this development Mac.

### Windows 0.2 delivery verification

Native Windows CI run 35960861796 for code commit a98467d succeeded. Build-time
Steinberg validator: 47 passed, 0 failed. CTest: 7/7 passed in 2.11 seconds.
The packaged plugin also passed 47/47 validator tests with MSYS2 removed from PATH.
Downloaded both ZIPs and verified all 161 manifest file hashes and the full commit
identity. The corresponding-source ZIP contains 12,410 entries including the new
control/state/cache sources and GrandOrgue patch. Neither ZIP includes the private
report or sample packs. Artifact:
https://github.com/camarokris/OrganVST/actions/runs/35960861796/artifacts/10792935114

Local post-fix CTest: 6/6 passed in 2.39 seconds; validator: 47/47. The installed
Mac bundle and dist/local-0.2.0 copy match and pass ad-hoc code-sign verification.
Colleague FL Studio 0.2 testing remains pending. These checks do not complete the
remaining full-release gates in PLAN.md.

## Version 0.3 silent-divisions follow-up

The colleague reports good organization and no crashes in Barton but silent Pedal,
Great, Solo, second-touch, Toe Pistons and Console Toys. Incoming channel/pitch and
Audition usage were not supplied, so the exact FL Studio cause is not established.

Implemented explicit all-input-to-one-division routing with native-channel fallback,
V3 saved routing (V2 registrations retained), note-range/last-input display, and
selectable audition pitch. Short keyboards start on their first key rather than
clamping middle C to an unmapped upper key. Diagnostic input/route observations
are logged by the worker, never from the audio thread; they are periodic snapshots,
not a complete event trace. The private checklist is ignored by Git.

On the same Apple M5 Pro development Mac: `cmake --build build -j 6` completed,
Steinberg validator passed 47/47, and `ctest --test-dir build --output-on-failure`
passed 6/6 in 2.44 seconds. The actual VST3 test covers native versus explicit
channel-1 routing, stale/invalid commands, route-change note release, short-key
pitch 36 audition, editorless route recall and reading V2 registrations.

`build/catalog_probe sample-packs/Barton3-7.Beta3.14/Barton3-7.organ
build/barton-test-data audition` passed all ten divisions at 48 kHz using 127-frame
blocks. Representative summed energies: Pedal 0.425144, Accomp 0.290959,
Great/Solo/Great 2T 1.6959 each, Pedal 2T 1.61881, Accomp 2T 2.81752,
Acc Traps 1.9906, Toe Pistons 54.6234, Console Toys 4.03928. Each produced finite
nonzero audio, followed by an ended tail. These are numerical sample-engine tests,
not listening, exhaustive rank testing, FL Studio or standalone-parity acceptance.

Final local code df6bbc4: SDK validator 47/47; CTest 6/6 in 2.63 seconds. Matching
Mac bundle/dSYM are in dist/local-0.3.0 and the bundle is installed in the user's
VST3 directory with verified ad-hoc signing. It still uses local Homebrew libraries.

REAPER 7.80 at 48 kHz / 512 samples: inspected the 0.3 interface, selected Second
and Play this division, then browsed Auxiliary without changing routing. Auxiliary
correctly displayed notes 36–37 and its audition pitch could be stepped from 36 to
37. Saved with editors closed, quit/restarted REAPER, and verified the restored
route. Both saved V3 instances retained 134 controls; the first routed to Second,
the other retained native channels. Later, an original channel-1 MIDI clip at
pitch 60 showed received ch 1 / note 60 and visible meter output from Second with
its Late voice enabled and First's default voice disabled. Returning to native
channels made that same clip silent as expected. This is a visible host output
check, not an audio listening assessment. The initial virtual-keyboard attempt
produced no incoming event; the explicit MIDI clip supplied the verified input.

Exported diagnostics through the editor and inspected the ZIP: it includes input
channel/pitch observations, routing changes and auxiliary key ranges, excludes
samples, and redacts the home directory prefix. Repeated editor closures remained
stable. No FL Studio 0.3 results have yet been received.

Windows run 36008635926 at df6bbc461171945f732405942d7b7a4eb2f7c1c4 succeeded:
47/47 build-time validator tests, 7/7 CTests in 2.16 seconds, and 47/47 packaged
validator tests with MSYS2 removed from PATH. Downloaded both artifacts and checked
all 161 package manifest hashes. The ZIP has 29 DLLs and updated testing instructions;
its corresponding-source ZIP has 12,411 entries. No private checklist, organ packs,
local projects or colleague document is included. Download:
https://github.com/camarokris/OrganVST/actions/runs/36008635926/artifacts/10811284176

This completes the planned feedback repair and build delivery; it does not close
the original full-release gates or establish that the colleague's FL Studio setup
now passes. That retest remains required.

## Version 0.4 layers, tabs and pedals

Colleague feedback reports 0.3 worked as expected, then asks for simultaneous
divisions, distinct tabs and pedals. This is exploratory FL Studio feedback, not
completion of the full test matrix. The additional private log reports 170 Barton
controls, 1,267,168,620 shared sample bytes, 312 channel-1 note observations and
repeated single-division route changes. The diagnostic search found no error/failure
entries. The raw log remains ignored and is not distributed.

Local Apple Silicon Debug: validator 47/47; CTest 7/7 in 2.77 seconds. Added routing
coverage for overlapping source-channel notes and independent channels. Expanded
the original synthetic fixture with a zero-minimum enclosure. Tests exercise
expression attenuation, captured quiet/loud crescendo, skipping to the nearest
stored lower step, capture clearing, actual VST3 messages and V2/V3/V4 state recall.
The first test attempted to recall an unchanged pedal position; corrected it to
move away and back, matching deliberate preservation of manual stop edits while
the pedal is stationary.

Private Barton numerical tests at 48 kHz with 127-frame buffers passed all ten
representative auditions plus layered registration, captured crescendo on a held
note and final note release. Both Main and Solo enclosures were found. With both
closed, summed energy was 0.359474 versus 8.74885 open. Barton specifies a 20%
minimum level, so closed enclosures intentionally remain audible. These are
numerical engine tests, not a listening or standalone GrandOrgue parity pass.

New crescendo storage and route bookkeeping are preallocated. Capture/playback
uses atomic storage; serialization and diagnostic file I/O stay off the audio
thread. The full pre-existing upstream real-time audit remains open. The new
32-step crescendo is plugin-owned and does not import standalone crescendo banks.
Local interactive host checks and native Windows delivery follow below.
