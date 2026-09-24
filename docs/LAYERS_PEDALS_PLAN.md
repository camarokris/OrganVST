# Version 0.4: layers, control tabs and pedals

The colleague reports 0.3 works as expected in FL Studio, with remaining requests
for simultaneous divisions, correct control categories, tremulants as a tab and
missing crescendo pedals. This is exploratory feedback, not a completed host test
matrix. No additional private report is published.

## Implementation

- Retain native 16-channel independent playback. Add a saved 16-bit destination
  mask for layering chosen divisions (and an explicit all-divisions action).
  Changing a mask releases held notes. Track input keys per source channel so an
  overlapping note-off does not cut another held layered key. Preserve V1–V3 state.
- Replace All with distinct Stops, Couplers, Tremulants, Switches and Pedals tabs.
  Tremulants are global controls, not keyboard divisions. Keep control IDs/catalog
  ordering unchanged so existing automation still addresses the same controls.
- Expose definition-authored enclosures with automation and saved keyed values.
  Add a plugin-owned 32-step registration crescendo with explicit capture/clear,
  saved steps and stable automation. Unprogrammed steps use the closest stored step below, or leave registration
  unchanged if no lower step is stored.
  This does not claim import of standalone GrandOrgue crescendo banks. No samples
  or source pack settings are modified.
- Preallocate crescendo storage and route-note bookkeeping off the audio thread;
  use atomic commands/state. No audio-thread allocation or serialization.
- Test layers/overlapping note releases, category membership, crescendo capture,
  transitions/recall, expression attenuation, older-state migration, actual VST3,
  local host UI and Windows packaged validation. Publish matching builds and guides.

## Status

Implementation, local automated checks and REAPER host UI/recall checks complete;
native Windows packaging and validation complete (run 36028054385).
Windows CTest 8/8 and packaged validator 47/47 passed; colleague FL Studio 0.4
retest remains pending. Local CTest 7/7 passes, including actual VST3 performance
recall, and validator 47/47 passes. The attached private 0.3 log confirms Barton
loaded 170 controls, all 312 logged note observations were channel 1, and routing
was repeatedly changed to one division. No error/failure entries were found by
the diagnostic search; this does not establish an exhaustive absence of faults. Full original release gates remain open.
