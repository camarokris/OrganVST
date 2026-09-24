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
Enable a stop before playing. This alpha has one stereo output. Ordinary
percussion follows the definition's one-shot setting; deliberately reiterating
ranks retain the source behavior.

Wait until loading finishes before playback or export. Offline exports during
loading are not yet protected against incomplete output. Recall currently saves
only the path, master gain, and initial control parameters; verify registrations
manually. A missing pack must be loaded again using Load organ.

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
