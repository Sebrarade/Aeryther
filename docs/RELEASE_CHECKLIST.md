# Aeryther 0.1 beta — release gate

Target macOS AU/VST3 (Apple Silicon initially). Developer: Volodymyr Gnatenko.

## Verify before distributing

- [ ] macOS GitHub Actions has built Instrument + FX for AU and VST3.
- [ ] Each plugin passes pluginval or an equivalent host validation.
- [ ] Ableton Live scans and opens both plugins without a crash.
- [ ] Wave plays polyphonic MIDI; sub follows the lowest held note.
- [ ] LOOP PLAY starts when Live plays, stops on transport stop, and ignores incoming MIDI.
- [ ] LOOP PLAY off: SAMPLE follows MIDI pitch; WAVE/SUB still respond.
- [ ] Host timeline moves/looping do not desynchronise sample playback.
- [ ] Forward, reverse, ping-pong, boundaries and crossfades sound free of clicks.
- [ ] SAMPLE drag and drop of filesystem WAV/AIFF/FLAC works.
- [ ] FILTER, DELAY, CONV and DIRECT follow exclusive bypass routing.
- [ ] 100% Convolver Mix contributes no dry sample in CONV routed path.
- [ ] 100% Reverb Mix is fully wet within its own path.
- [ ] All four AUUU/CROSS/CATHOLIC/DARK IR modes are audible and distinct.
- [ ] Project recall and external sample path restoration work.
- [ ] Automation parameters and CPU are stable under high polyphony.
- [ ] Packaging and distribution correctly disclose unsigned status.

This is a QA checklist, not a list of completed tests. Not ready to call a finished release until checked.
