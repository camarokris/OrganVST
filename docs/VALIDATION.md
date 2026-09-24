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

No native Windows build or validation has run. No colleague FL Studio test has
run. macOS 13 compatibility and self-contained dynamic-library packaging have not
been verified. Development build outputs are not distribution artifacts.

## ZIP implementation follow-up

Added Homebrew libarchive and used it for extraction after tests demonstrated that
wxWidgets normalizes both names and file-type metadata. Failed intermediate tests
were corrected and rerun; the final suite above passes. Libarchive is an additional
system dependency not yet pinned/bundled for distribution. The current CMake cache
sets CMAKE_PREFIX_PATH=/opt/homebrew/opt/libarchive; README documents the portable
Homebrew prefix command. ZIP loading has not yet been exercised through REAPER's
picker or with the full Barton ZIP.
