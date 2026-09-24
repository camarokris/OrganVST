# OrganVST

An early GPL VST3 instrument using GrandOrgue's actual organ loader and playback
engine. **This is an engineering prototype, not the completed release described
in [PLAN.md](PLAN.md).** It currently builds and has been tested on Apple Silicon
macOS. The Windows x64 alpha also passes native CI tests and Steinberg validation;
FL Studio host testing is pending.

## Build on the development Mac

Prerequisites: Xcode command-line tools, Git, Python 3, CMake 3.25+, Ninja,
Homebrew packages `wxwidgets wavpack fftw yaml-cpp pkg-config libarchive`.

```sh
python3 scripts/bootstrap.py
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="$(brew --prefix libarchive)"
cmake --build build -j 6
ctest --test-dir build --output-on-failure
```

Exact GrandOrgue and VST3 SDK revisions are in `dependencies.json`; recursive
submodules include VSTGUI. The bootstrap script applies the tracked host patch.
System libraries are not yet pinned or bundled, so this does not yet constitute a
reproducible distributable build. The declared macOS 13 target has not been tested
on macOS 13. Windows has a separate CI build and packaging workflow described below.

The VST3 is `build/VST3/Debug/OrganVST.vst3`. Its build runs Steinberg's validator.
For local development, close the host before replacing an installed copy:

```sh
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
ditto build/VST3/Debug/OrganVST.vst3 "$HOME/Library/Audio/Plug-Ins/VST3/OrganVST.vst3"
codesign --force --deep --sign - "$HOME/Library/Audio/Plug-Ins/VST3/OrganVST.vst3"
```

The current bundle depends on this machine's Homebrew libraries. Do not ship this
bundle to colleagues as a self-contained release. Development signing is not
Developer ID signing or notarization.

## Current operation

Insert OrganVST as an instrument. Choose **Load organ** and select a
GrandOrgue-compatible `.organ`, `.odf`, or single-definition ZIP pack. ZIP packs
are extracted to a private managed folder and removed when the engine is destroyed.
Multi-definition ZIPs currently require manual extraction and definition selection.
Extraction rejects traversal, links, ambiguous names, damaged data, and packs above
64 GiB or 200,000 entries. Use **Cancel load** to cancel an in-flight load. WAV and WavPack samples are loaded
by GrandOrgue. Loading happens in the background. All authored writable controls are available by division, with filters for stops,
couplers, switches and tremulants. Generated internal couplers are excluded.
The first 128 catalog entries are connected to the 128 Boolean DAW parameters;
controls beyond those slots remain fully usable and saved through the editor.
Use Previous/Next within a division; the division list has its own paging.
The editor shows each division’s MIDI channel and provides a short audition note. MIDI channels 1–15 address successive manuals;
channel 16 addresses the pedalboard when present. Output is stereo only so far.

The plugin has separate processor/controller components, sample-offset event
processing, versioned path/gain/keyed-control project state, and rotating background logs.
**Export diagnostics** writes a ZIP with the session logs, control list, and a
reproduction template. Home-directory prefixes are redacted; samples are excluded.
Inspect the archive before sharing it. Normal logs retain full source paths.

On macOS logs are in `~/Library/Application Support/OrganVST/logs`.
On Windows the intended location is `%LOCALAPPDATA%/OrganVST/logs`.

Samples load into memory. Immutable main sample payloads are shared within the
same plugin process; voices, registration, loop/release metadata and auxiliary
buffers remain per instance. Separate sandbox processes cannot share this cache.
The cache compares content, so changed sample data cannot reuse stale bytes.

Version 0.2 restores all exposed controls by stable definition keys. Old 0.1
projects restore their pack and gain with authored registration defaults because
the old control indices included internal couplers. Recheck old automation lanes
and registrations; use a new test project for this alpha.

Current limitations include incomplete combinations/crescendo state restoration, no readiness safeguard
for offline export during asynchronous loading, no assignable mappings or MIDI
learn, no auxiliary routing, no full tabbed interface/console,
no asset collection/relink UI, and an unfinished real-time safety audit. Do not use
this prototype for irreplaceable projects or final exports. The implementation
must pass the remaining gates in PLAN.md before a release is declared complete.

## Headless engine checks

```sh
build/organ_probe /absolute/path/to/instrument.organ
build/organ_probe /absolute/path/to/instrument.organ build/render.wav 2 0 60 48000
```

Render arguments are manual index, stop index, MIDI note, and sample rate. The
probe holds one note for 8 seconds and renders a 4-second release, with irregular
block sizes. It reports signal energy and render wall time, not measured CPU time.
See [docs/VALIDATION.md](docs/VALIDATION.md) for actual results and limitations.

## Source and assets

GPL-2.0-or-later, preserving GrandOrgue's notices and SDK linking exception in
`LICENSE`. Upstream copyright notices remain in fetched source and patches.
Steinberg VST3 SDK and VSTGUI have their own licenses in their pinned source trees.
Windows artifacts include project/upstream source snapshots and license notices.
The combined instrument also includes GPLv3-or-later ZitaConvolver code; retain
its notices and consult the supplied dependency licenses when redistributing.

Barton and other third-party sample packs are external assets with separate
licenses. Keep them in ignored `sample-packs/` or `local-assets/`. Never add them to
Git or bundle them with public releases. Tests generate original synthetic audio;
no Barton audio or artwork is tracked.

## Windows CI alpha builds

[Download the validated Windows alpha](https://github.com/camarokris/OrganVST/actions/runs/35960861796/artifacts/10792935114)
(version 0.2.0, build `a98467d`; GitHub sign-in required, artifact retention 30 days).

The Windows VST3 alpha GitHub Actions workflow uses a native Windows 2022 runner
and the MSYS2 UCRT64 toolchain. It builds pinned engine/SDK sources, runs synthetic
tests and Steinberg validation, collects runtime DLLs and debug symbols, then
validates the packaged instrument with MSYS2 removed from PATH. Successful runs
publish downloadable workflow artifacts, not production releases. See
[Windows installation and testing](docs/WINDOWS_TESTING.md) and
[Windows build architecture](docs/WINDOWS_BUILD.md).

Actions are pinned by commit. MSYS2 packages are rolling versions recorded in the
artifact manifest; the complete toolchain is not yet reproducibly pinned. FL Studio
host validation remains a separate colleague test.
