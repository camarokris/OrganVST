# FL Studio colleague testing checklist

Status: pending. No Windows binary or verified distribution package is available
from this checkpoint. Run this checklist only after a native Windows build passes
Steinberg validation and the package includes dependencies, symbols, and licenses.

Record plugin build/commit, Windows version, FL Studio version, CPU/RAM, audio
interface, driver, sample rate, buffer size, and organ pack/version.

- Scan the plugin and insert it in a blank project; save the scan report.
- Load a pack; confirm progress and useful errors for missing assets.
- Play each manual/pedal assignment; toggle stops and couplers.
- Check sustained notes, release tails, ordinary percussion, and authored repeats.
- Test 44.1/48/96 kHz, small and large buffers, dense registrations and long playback.
- Test mapped automation and auxiliary outputs after these features are implemented.
- Use two instances with different registrations/packs; verify independence.
- Close/reopen the editor during playback; stop/start transport; unload/reload.
- Save and reopen the project with the editor closed; verify every saved setting.
- Export offline and compare note timing and complete tails with realtime playback.
- Move/collect assets and relink after those features are implemented.
- Cancel loading; try malformed definitions, absent samples, and corrupt archives.
- Confirm a failed replacement preserves the previous working organ.
- Export diagnostics, inspect for personal information, add reproduction steps,
  and return the ZIP with the relevant host report. Do not include sample packs.

For a crash, preserve the crash report and exact matching debug symbols. Record
which checklist items were actually run; do not mark unavailable features passed.
