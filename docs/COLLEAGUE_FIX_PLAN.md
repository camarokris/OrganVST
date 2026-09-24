# Colleague alpha feedback repair plan

The September 24 report and three embedded screenshots were reviewed locally.
They are private test evidence and are not published with the source.

## Findings and repairs

1. The old catalog includes generated virtual couplers (repeated 16/8/4/BAS/MEL)
   and the editor only exposes 128 entries. Enumerate authored controls, include
   switches, retain auxiliary manuals, and separate controls by division and kind.
   Keep distinct controls with the same name distinct; do not deduplicate by name.
2. Preserve authored initial engagement, including unison and auxiliary logic.
   Expose all catalog controls independently of the 128 automation slots and
   synchronize actual engine state back to the editor. Prevent stale clicks from
   affecting a replacement organ. Version state and handle legacy index mappings
   explicitly rather than silently reassigning old automation.
3. Make MIDI channel/division relationships visible and supply an audition control
   so stops and couplers can be tested without DAW MIDI routing ambiguity.
4. Samples already load into RAM. Investigate sharing immutable sample payloads
   between instances while keeping registration, voices, couplers, envelopes and
   playback state independent. Validate lifetime and content-change behavior.
5. Add synthetic regression coverage for a catalog over 128 entries, internal
   coupler exclusion, authored defaults, switches, auxiliary divisions, coupler
   playback, state recall and instance isolation. Exercise the local Barton pack
   without putting it or the report/screenshots in Git or build artifacts.
6. Build/test locally, install a new local development copy, run native Windows CI,
   inspect/download the new package and record actual results. FL Studio retesting
   remains the colleague's acceptance check; do not claim it from CI results.

## Progress

- Reviewed all supplied text and screenshots; traced catalog truncation and
  generated-coupler inclusion to OrganInstance and Controller.
- Corrected authored catalog, division/filter UI, all-control command/state path,
  actual-state synchronization, generation checks and version 2 keyed recall.
- Immutable main sample payload cache implemented; mutable playback remains local.
- Synthetic catalog and actual VST3 registration regressions pass locally. Added
  cache concurrency/content-change/lifetime coverage. Native CI and UI checks pending.
- Barton catalog inspection reports 170 writable authored controls: 131 stops,
  28 couplers, 7 switches, 4 tremulants. All ten manual/pedal groups are represented;
  dependent read-only rank controls remain in the model and are operated by switches.
