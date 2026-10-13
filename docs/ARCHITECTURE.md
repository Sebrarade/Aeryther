# Audio architecture

```text
                         HOST MIDI
                      /     |     \
SAMPLE: LOOP PLAY ON: host transport + BPM + grain overlap
SAMPLE: LOOP PLAY OFF: MIDI-triggered pitched one-shot
                           |
WAVE: MIDI polyphonic 1..8 detuned unison oscillators
SUB:  lowest held MIDI note, independent monophonic oscillator
                           |
               three separate stereo layers
                           |
                  per layer routing
                    /          \
              DIRECT ON     DIRECT OFF
                |            |     |
             MASTER       FILTER  (optional)
                             |
                           DELAY  (optional)
                             |
                       CONV enabled?
                       /           \
                  SHARED CONV     BYPASS CONV
                       \           /
                        REVERB (post-conv)
                             |
                           MASTER
```

DIRECT is *exclusive* for a layer, not a second dry send. If SAMPLE and WAVE are sent to the 100%-wet Convolver, neither reaches the output dry. If SUB DIRECT is enabled, its unsubmerged direct bass is intentionally audible at the master output. Delay/Filter apply on a per-layer signal path before the shared Convolver. The shared Reverb is last on the processed bus; Direct layers bypass it.

A fresh plugin must be audio-ready with all fine-tuning and modulation neutral, 0 dB layer gain (rather than muted), full convolution/reverb wet mix by default. The host restoring a session takes precedence over init defaults.
