# OrganVST implementation and repository setup

## Repository foundation

Write PLAN.md, AGENTS.md, and .gitignore in the repository root. Preserve this plan
and maintain implementation progress below. AGENTS.md documents build/test commands,
architecture, audio-thread constraints, instance isolation, dependency pinning,
licensing, sample exclusions, and truthful release verification.

Preserve the existing repository on main. Explicitly stage foundation files and
create an initial local commit; do not create a remote or push. Ignore Barton ZIP
and extraction, local sample packs, dependency downloads, build outputs, binaries,
installers, symbols, logs, diagnostics, DAW sessions, editor/OS files, credentials,
and signing material. Preserve trackability of source images and synthetic fixtures.
Verify with git check-ignore and inspect the staged list before committing.

## Instrument architecture

Build a GPL open-source VST3 instrument for Apple Silicon macOS and Windows x64
using pinned GrandOrgue sources, Steinberg VST3 SDK, and VSTGUI.

- Extract the model, loader, combinations, and playback code into a reusable library
  with plugin-owned settings and services.
- Implement host-driven rendering for variable blocks and timestamped events.
  Remove device callback coordination. No allocation, file access, blocking waits,
  or GUI calls in the audio callback.
- Preserve tuning, couplers, tremulants, enclosures, attacks/releases, loop markers,
  and crossfades. Ordinary percussion plays once; explicitly repeating ranks such
  as Barton's Xylophone Reit preserve their definition behavior.
- Load GrandOrgue-compatible .organ/.odf, .wav/.wv, referenced images, supported
  archives, and ZIP packs through managed extraction preserving relative paths.
- Load asynchronously with progress, cancellation, readable errors, and safe
  replacement. Failed replacement retains the existing organ.
- Keep instance settings independent. Never modify source packs or standalone
  GrandOrgue preferences.

## DAW behavior and interface

- Separate VST3 processor/controller; stable identifiers and versioned state.
- 16-channel MIDI with configurable manual/pedal assignments and MIDI learn.
- Stereo main mix plus 16 optional stereo auxiliary buses; rank output groups with
  division presets, rendering shared ranks once. Default main-only; allow exclusive
  auxiliary routing or simultaneous main inclusion.
- Master gain, crescendo, and 128 assignable automation slots with stable IDs and
  saved mappings. Clear incompatible assignments on organ replacement.
- Correct event timing, tails, transport resets, offline rendering, editor-independent
  operation, and latency reporting.
- Save organ identity/location, registration, combinations, crescendo, routing,
  MIDI assignments, and automation mappings.
- Reference external packs. Collect Organ Assets copies the complete pack into a
  user-selected project asset folder. Cross-platform relinking; no embedded samples
  in DAW state. Missing assets remain recoverable. Loading must not silently cause
  incomplete offline exports.
- Resizable synchronized tabs: Stops & Couplers, Crescendo & Expression, Manuals,
  Pedalboard, Console, Settings & Diagnostics. Preserve original console panels,
  artwork, hidden manuals, and auxiliary manual relationships.

## Diagnostics, testing, and delivery

- Asynchronous rotating logs: build, host, instance, audio configuration, loading,
  routing, resource use, and faults.
- Export diagnostic bundles with logs, configuration, asset manifests, and
  reproduction notes. Exclude samples and redact personal path prefixes by default.
- Ship self-contained plugins, matching symbols, installation/troubleshooting
  instructions, corresponding source/licenses, and an FL Studio test checklist.
- Keep Barton separate from distribution and Git history; its sample license is
  separate and noncommercial.

Acceptance gates:
1. Reproducible builds and headless Barton rendering compared against deterministic
   standalone GrandOrgue playback.
2. Steinberg validator and scanning, MIDI, routing, automation, multiple instances,
   lifecycle, recall, and offline export host tests.
3. Full UI, original console, crescendo programming, collection, and relinking.
4. Local Mac REAPER testing, native Windows builds/validation, colleague FL Studio
   testing. Record pending external tests explicitly.
5. 44.1/48/96 kHz, varied/irregular blocks, sample-rate changes, chords, loop boundaries,
   percussion decay, releases, and extended playback. Record CPU, memory, and
   processing headroom with machine and registration.
6. Malformed definitions, corrupt archives, missing assets, cancellation, failed
   replacement, restoration without editor, and diagnostics recovery tests.

## Defaults

macOS 13+ Apple Silicon; Windows 10/11 x64. AU, VST2, Intel Mac, Linux excluded.
Compatibility follows pinned GrandOrgue, not arbitrary .odf files. Preserve authored
loops and diagnose defective sources. Initial development packaging; public signing
and notarization require appropriate credentials.

## Progress

- Foundation committed locally as f7ff07a; repository-only author configured as
  Kris Barrantes <kris.barrantes@outlook.com>. GitHub publication authorized by the user on 2026-09-24.
- Pinned dependencies fetched; host adaptation patch and reusable engine build.
- Early VST3 processor/controller, background loading, basic control grid, stereo
  audio, fixed MIDI assignments, basic state, and diagnostic ZIP export implemented.
- Headless synthetic rendering passes at 44.1/48/96 kHz; Barton single-stop render
  succeeds. Steinberg validator passes 47 tests. REAPER scan, editor, fixture load,
  and visible stop toggle verified.
- Managed single-definition ZIP loading and a cancellation button implemented.
  Archive integrity/path/link checks and two-engine isolation tests pass.
- Every complete-release acceptance gate remains open: standalone parity,
  real-time audit, full state/recall and offline readiness, configurable MIDI and
  automation, aux routing, complete UI/console, crescendo, asset management,
  full release packaging, broader Windows host validation, and colleague FL Studio tests.
- See README.md for current build commands and prototype limitations.
- Execution mode is active; the earlier plan-mode restriction no longer applies.

See docs/VALIDATION.md for actual verification results.

## GitHub and Windows CI follow-up

Published the authorized public repository at https://github.com/camarokris/OrganVST.
Windows x64 CI and packaging are configured, including native tests, SDK validation,
private runtime libraries, symbols, source, licenses, and downloadable artifacts.
Run 35956216929 (commit 74997ad) passed native Windows compilation, 4/4 CTests,
and 47/47 Steinberg validator tests, including the packaged plugin with MSYS2
removed from PATH. The alpha and corresponding project source ZIPs are uploaded.
Initial runtime loading failures were fixed with bundled DLLs and an explicit
Common Controls v6 activation context. FL Studio remains pending; this alpha is
ready for exploratory colleague testing, not the completed release.

## Colleague feedback repair

See docs/COLLEAGUE_FIX_PLAN.md for the September 24 report findings, repair sequence,
and verification. This follow-up targets catalog completeness, control behavior,
MIDI clarity and instance sample memory; full release acceptance remains separate.
