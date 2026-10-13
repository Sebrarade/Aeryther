# Aeryther — 3 Layer Ambient Instrument

**Developer / manufacturer: Volodymyr Gnatenko**  
**Status: initial JUCE source prototype, not yet a tested macOS binary.**

Aeryther is a three-layer ambient instrument designed around the workflow of turning musical phrases into dense, completely wet atmospheric textures. C++17 + JUCE 8, with CMake support for **VST3 / AU (macOS) / Standalone** in two targets: **Aeryther** (MIDI instrument) and **Aeryther FX** (insert processor for a stereo track).

## Current source implementation

- **SAMPLE:** load WAV/AIFF/FLAC from a file picker or file drag-and-drop; editable start/end markers; start BPM; forward, reverse, ping-pong; adjustable crossfade control (the initial loop renderer uses overlapping Hann-window grains); root MIDI note, fine pitch, level.
- **LOOP PLAY:** with Loop Play ON, the host transport starts the continuously looping sample, without MIDI note gating. MIDI still plays WAVE and SUB. With Loop Play OFF, MIDI keys trigger pitched one-shot SAMPLE, WAVE, and SUB.
- **WAVE:** polyphonic sine/triangle/saw/square oscillator, 1–8 unison voices, detune, stereo width, octave and level.
- **SUB:** lowest-held-note monophonic sine/triangle, octave, fine tuning and level.
- **Routing:** separate FILTER, DELAY, CONV, DIRECT switches per layer. DIRECT bypasses all effects; CONV routes each selected layer to **one shared** convolver.
- **Two ADSRs:** AMP ENV runs on the MIDI voices; FILTER ENV is a matrix modulation source. Two LFOs and four modulation matrix slots are implemented (block-rate destinations).
- **Filter:** low-pass, high-pass, band-pass, notch approximation and comb.
- **Delay:** stereo with time, feedback, ping-pong and mix.
- **Convolver:** four original programmatically generated impulse responses, modes **AUUU / CROSS / CATHOLIC / DARK**, Mix default 100% (no direct copy of the source on the Convolver path).
- **Reverb:** custom stereo feedback delay network (4-line FDN), decay control 0.5–60 seconds (theoretically -60 dB at selected time), Mix default 100%, Decay default 60 seconds; subjective tuning and measured validation are still pending.
- **Saving:** APVTS settings and file path are restored with the DAW session; the referenced audio file must still exist at that path.
- **GUI:** functional white JUCE GUI with one pointer line per encoder and clickable parameter controls. The UI is a first draft, not a pixel-perfect reproduction of the concept image.

### Defaults

A *new instance* starts with zero/neutral pitch fine, unison detune, LFO depths, and unity (0 dB) layer level; Convolver Mix and Reverb Mix are **100%**; Reverb Decay is set to its maximum. Other parameters use practical neutral/audio-ready defaults (e.g., unison 1, sustain 100%, root C4, sample BPM 120). A **restored project** preserves all saved parameter settings instead of resetting them.

### Build on macOS

This environment lacks JUCE headers and macOS's compiler/SDK, so an actual AU/VST3 installer has **not** been built here. On a Mac with Xcode command-line tools, CMake, internet access and a compatible JUCE 8 checkout:

```sh
cmake -S . -B build-mac -DAERYTHER_BUILD_PLUGIN=ON -DAERYTHER_FETCH_JUCE=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-mac --config Release -j4
```

Artifacts (after successful compilation) normally appear under `build-mac/Aeryther_artefacts/Release/` or `build-mac/Aeryther_artefacts/` depending on generator.

For the portable tests, which do not require JUCE:

```sh
cmake -S . -B build-core -DAERYTHER_BUILD_PLUGIN=OFF -DAERYTHER_BUILD_TESTS=ON
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

## Known gaps for next iteration

1. **macOS JUCE compile** has not been verified; address build diagnostics on a Mac, including JUCE API differences, before calling this installable.
2. Full tempo/pitch independent stretch is a prototype grain overlap technique, not commercial-grade Elastique; test transients, loop clicks, and time-signature changes.
3. Crossfade adjusts grain overlap-window duration; verify transient quality, reverse/endpoint behavior and parameter smoothing by listening.
4. Convolver IRs are original provisional designs, **not yet the final signature sound**; develop and audition AUUU / CROSS / CATHOLIC / DARK.
5. FDN reverb has an exponential T60 feedback calculation; verify measured tail and tune perceived sound.
6. LFO modulation currently updates blockwise (potential stepping with fast LFO); add sample-level smoothing, expand full matrix coverage and filter envelope depth behavior.
7. Convolver instance/mode changes need crossfades to avoid potential tail cuts.
8. Aeryther FX now has a dedicated CMake target and streamlined plugin UI; both still require a macOS JUCE build and plugin-host testing.
9. Direct Arrangement clip drag-and-drop in Ableton Live is host-dependent and **not guaranteed**. Disk file drops work via JUCE.
10. CPU/performance, sample persistence, plugin validation and Ableton Live tests remain to be completed.

This is a real source milestone, **not a claim of a production-ready plugin**.

## Active macOS CI

The source archive is mirrored in the Sebrarade/Aeryther repository as `ci/source-archive.base64` so the macOS workflow can retrieve and build the latest C++ project. Check `.github/workflows/build-macos.yml`.

The source tests here do not prove that Ableton can instantiate a VST3/AU. A successful CI compile is necessary but not sufficient for a public release; see `docs/RELEASE_CHECKLIST.md`.
