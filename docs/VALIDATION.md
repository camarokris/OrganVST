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
- `ctest --test-dir build --output-on-failure`: **1/1 test passed**, 1.23 seconds
  on the latest run. This test runs six synthetic engine renders (two registrations
  at 44.1/48/96 kHz) plus missing definition, missing sample, corrupt sample, and
  malformed definition failure cases.
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

The installed smoke-test copy predates the final legacy-combination error-handler
patch; the subsequent source build and validator include that patch. No audible
host MIDI, project recall, multi-instance, automation playback, routing, offline
export, or crash-recovery host pass is claimed. REAPER's tiny displayed idle CPU
reading is not an instrument workload measurement.

## Outstanding acceptance work

All complete-release gates in PLAN.md remain open. Specifically: no allocation/
lock audit of the entire audio call graph, extensive timing/transport tests,
standalone parity, chords and long playback, complete project state and editorless
recall, offline-load readiness, aux routing, user MIDI/automation mappings,
full tabs/console/crescendo, ZIP/managed assets/collection/relinking, diagnostic
manifest coverage and export tests, and portable distribution assembly.

No native Windows build or validation has run. No colleague FL Studio test has
run. macOS 13 compatibility and self-contained dynamic-library packaging have not
been verified. Development build outputs are not distribution artifacts.
