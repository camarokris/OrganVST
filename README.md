# OrganVST

An early GPL VST3 instrument using GrandOrgue's actual organ loader and playback
engine. **This is an engineering prototype, not the completed release described
in [PLAN.md](PLAN.md).** It currently builds and has been tested on Apple Silicon
macOS. Native Windows builds are not yet verified.

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
on macOS 13. Windows dependency configuration and packaging remain outstanding.

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
by GrandOrgue. Loading happens in the background. The first 128 stops/couplers/
tremulants are currently connected directly to the 128 Boolean DAW parameters.
Use Previous/Next for control pages. MIDI channels 1–15 address successive manuals;
channel 16 addresses the pedalboard when present. Output is stereo only so far.

The plugin has separate processor/controller components, sample-offset event
processing, basic path/gain/control project state, and rotating background logs.
**Export diagnostics** writes a ZIP with the session logs, control list, and a
reproduction template. Home-directory prefixes are redacted; samples are excluded.
Inspect the archive before sharing it. Normal logs retain full source paths.

On macOS logs are in `~/Library/Application Support/OrganVST/logs`.
On Windows the intended location is `%LOCALAPPDATA%/OrganVST/logs`.

Current limitations include incomplete state restoration, no readiness safeguard
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
Release source/license assembly is still pending.

Barton and other third-party sample packs are external assets with separate
licenses. Keep them in ignored `sample-packs/` or `local-assets/`. Never add them to
Git or bundle them with public releases. Tests generate original synthetic audio;
no Barton audio or artwork is tracked.
