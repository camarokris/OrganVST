# OrganVST contributor instructions

Read PLAN.md before changing architecture. Update its progress and docs/VALIDATION.md
with actual commands and outcomes. Never describe an unperformed DAW, listening,
Windows, or colleague test as passed.

## Architecture

Use pinned GrandOrgue model/loader/playback code with the Steinberg VST3 SDK and
VSTGUI. Keep plugin services, engine adaptation, and UI separate. No standalone
audio or MIDI device ownership inside the plugin. Preserve definition semantics,
including deliberate reiterating percussion. All mutable organ state is per instance.
Editor closure must not stop playback, loading, or state restoration.

No allocation, blocking synchronization, filesystem access, logging I/O, or GUI
calls on the audio thread. Prepare resources off-thread, use bounded communication,
and reclaim replaced engines off-thread. Respect event sample offsets and host
lifecycle; do not assume fixed block sizes or an open editor.

## Dependencies and builds

Dependency revisions live in dependencies.json. Downloaded source belongs in .deps/;
track adaptation patches in patches/, never edit dependencies without recording a
reproducible patch. Use CMake/Ninja with build/ as the output directory.
Run `python3 scripts/bootstrap.py` to fetch exact dependency revisions.
Build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
-DCMAKE_PREFIX_PATH="$(brew --prefix libarchive)"` on this Mac, then
`cmake --build build -j 6`. Test: `ctest --test-dir build --output-on-failure`.
The plugin build runs Steinberg validator automatically. See README.md for
prerequisites and local installation. Do not invent successful host tests.

## Public repository hygiene

Keep Barton and all externally supplied organ packs out of Git. Use sample-packs/
or local-assets/ for additional packs and local-projects/ for DAW sessions. Small
original synthetic fixtures may be tracked under tests/fixtures/ with provenance.
Do not commit secrets, signing material, diagnostics, binaries, or downloaded SDKs.
Inspect `git diff --cached --stat` and `git diff --cached --name-only` before commits.
Do not create remotes or push without explicit authorization.

Preserve GPL-2.0-or-later notices and upstream exceptions. Include corresponding
source and dependency notices in distribution. Sample-pack licenses are separate;
do not bundle Barton in public artifacts.

## Verification and releases

Run focused tests after relevant changes and Steinberg validator for plugin builds.
Validate deterministic rendering against standalone GrandOrgue, state recall,
multiple instances, MIDI/automation, auxiliary routing, loop/release behavior,
percussion decay, variable buffers, and offline export. Record machine, host,
configuration, and measured resource use. Native Windows and FL Studio results
must remain pending until actually run. Include matching symbols, install steps,
diagnostic export instructions, source/licenses, and an FL Studio checklist.
