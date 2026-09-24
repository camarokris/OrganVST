# Barton silent divisions follow-up

The colleague reports good organization and no crashes, but silence from Pedal,
Great, Solo, second-touch divisions, Toe Pistons and Console Toys. The user confirms
the same local Barton reference pack. The report does not identify incoming MIDI
channels, tested pitches or whether Audition was used; these remain unknown.

## Findings and implementation plan

- Fixed multichannel routing sends channel 1 only to Accomp. Add an explicit
  single-division input route alongside existing multichannel behavior, saved in
  versioned state. Changing routes must release held voices, and editor selection
  must not silently change routing. Show incoming channel/note and valid note range.
- Short auxiliary keyboards need an audition pitch inside the useful lower range.
  Barton Toe Pistons covers MIDI 36–43 but only 36–42 map to toy ranks; Console Toys
  covers 36–37, with distinct bell/triangle mappings and intentional silent pipes.
  Use the first key for short keyboards and provide bounded pitch selection.
- Extend headless tests to every Barton division and actual VST3 channel rerouting,
  route changes while holding notes, saved routing and legacy state compatibility.
  Preserve authored sample behavior; do not turn intentionally silent pipes into sound.
- Build/test locally and on native Windows CI, inspect downloadable packages, and
  provide exact FL Studio retest steps. Host-specific success remains pending until
  actually performed; automated numerical output is not a listening test.

## Progress

Implementation complete. Local CTest 6/6 and SDK validator 47/47 pass. Actual
VST3 tests cover channel-1 rerouting, stale/invalid routes, held-note release,
low-key audition and V2/V3 recall. Representative auditions from all ten Barton
divisions produced finite nonzero audio and ended their tails. This establishes
engine output, not the colleague's exact host failure cause. Mac UI inspection
and native Windows packaging/validation are in progress.
