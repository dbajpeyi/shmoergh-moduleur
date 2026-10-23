# How the Moduleur is modelled

The rack runs at twice the host sample rate (JUCE half-band IIR oversampling) and works in volts. Each
module is a behavioural model of its KiCad schematic in
[shmoergh/moduleur](https://github.com/shmoergh/moduleur); component references below point there.
The values are chosen to match the hardware's ranges and feel, not to be a SPICE-accurate simulation.

## Patching

`src/dsp/Ports.h` lists every jack with its panel position (hole centres from
`enclosure/fr4/01-front/graphics/front-edges.svg`) and the normalisations:

- **Default patch PCB** (`modules/08-default-patch`), traced from the pad nets of `default-patch.kicad_pcb`
  against each UI board's patch header: JP3/JP5/JP4/JP6 (oscillators → mixer), JP7 (mixer → filter LP),
  JP8 (LFO TRI → CV/1), JP9 (ENV 2 → CV/2), JP10 (filter → VCA 1), JP11 (VCA 1 → output), and the
  cross-wired JP1/JP2 (pitch) and JP12/JP13 (gate) pairs.
- **Switched jacks:** a cable in an input replaces its normal. A cable in an *output* also breaks that output's
  default connection, because the hardware routes the signal through the jack's TN contact.
- **ENV → VCA jumper** (`JP_DEFAULT_ENV_TO_VCA1`) is wired to the jack's normal, independent of the patch board.

The routing is resolved once per block from lock-free atomics written by the UI.

## Oscillator (`02-vco`)

- Pitch sum (U1A): keyboard CV + TUNE (0..12 V through 130k/100k → 0..5.2 V) + FINE (±12 V through
  510k/100k/510k → ±1.07 V), each through 100k: 1 V/oct. 0 V = C1 (32.70 Hz), matching the calibration
  procedure and Le Controlleur's `kZeroCVMidiNote = 24`.
- FM enters the reference-current node through 1M against 12 V/1M, so it is linear FM: f · (1 + V/12).
- Hard sync fires when SYNC rises through about −1.09 V (U2B with R14/R15).
- Saw ±5 V; triangle = rectified saw; square = comparator of saw against
  −(WIDTH + PWM)·100k/220k, which gives 50 % at 0 V and 5–95 % at the extremes; sub = Q0 of the CD4040,
  toggled on every reset (including sync resets); noise is filtered white noise, AC coupled.
- All discontinuities (resets, sync, square edges, sub toggles) get PolyBLEP corrections on both sides,
  using a one-sample look-back.

## Mixer & sidechain (`03-sidechain-mixer`)

IN/1–3 through A-taper sliders → VCA. IN/4 → gain stage 1 + RV/10k (×1–11) → half-wave peak detector
(≈4 ms attack, ≈180 ms release trimmer settings) → THRESHOLD (0–6 V) minus peak, negative part only (D1)
→ gain reduction of the IN/1–3 VCA. IN/4 is also mixed in with its own level. OUT is an A-taper main
volume. Op-amp stages saturate softly at about ±10.5 V.

## Filter (`04-vcf`)

The diode ladder is three capacitors (C10/C12/C14) coupled by diode pairs and driven by a diff pair. As
node equations, with `x` the diff-pair drive and `u` the HP input (which drives the capacitor reference):

```
c1' = ωc ( g0·x − g1·(v1 − v2) )
c2' = ωc ( g1·(v1 − v2) − g2·(v2 − v3) )
c3' = ωc ( g2·(v2 − v3) − g3·v3 )        with  c = v − u,   x = lp − k·v3,   out = v3
```

`gi = tanh(z)/z` of the previous sample's voltages model the diode and diff-pair non-linearity. Each
sample is a trapezoidal (TPT) implicit step: a 3×3 linear solve. The linear system's characteristic
polynomial is s³ + 5s² + 6s + 1 + k, so it self-oscillates at √6·ωc when k = 29. The resonance pot reaches
k ≈ 31.5, just past that, like RV_MAX_RESO trimmed hot. The cutoff CV (0–12 V knob plus two attenuverted
CVs) maps exponentially to about 16 Hz–20 kHz. The output stage is AC coupled, and resonance costs
passband level, partly made up.

## Envelope & VCA (`05-adsr-vca`)

The RC envelope charges towards the op-amp rail (an overshoot target) and switches to decay at the peak,
as U1B's Schmitt trigger does. It then decays to the sustain level and releases to 0 on gate off. Time
constants are 10 µF × (R_min + A-taper pot), so about 0.2 ms to 10 s. Gate threshold 0.5 V with
hysteresis. Output scaled to 8 V (the recommended RV_OUT_GAIN setting). The VCA is linear in CV
(0 V silent, 8 V unity) with slight diff-pair compression.

## Crush, LFO, output (`06-utils-output`)

- Crush: S&H of the ±12 V-clamped input, clocked by the relaxation oscillator (160 Hz–19 kHz, A1M pot) or
  by rising edges above ~3 V on TRIGGER.
- LFO: triangle ±4.6 V / square ±4.9 V, 0.1–200 Hz, both scaled by LFO AMNT (dual A20k).
- Output: ×0.28 line trim (10 Vpp → 2.8 Vpp), 1.6 Hz AC coupling, DRIVE gain 1–5.5×, anti-parallel red
  LEDs (~1.75 V knee), A10k volume. 1.75 V at the LEDs is about −1 dBFS.

## Brain (`07-brain` + Le Controlleur)

`src/dsp/Brain.cpp` follows `app-controller.cpp`, `midi-to-cv-engine.cpp`, the SDK's `midi-to-cv.cpp`
and `sequencer-engine.cpp`. It keeps the same button timings (120 ms dual-press window, 400 ms short
press, 1.5 s mode switch, 350 ms double tap), last-note priority with a 25-note stack, ±2 semitone bend,
0–5 V CC outputs, and the sequencer's xorshift mutation, range/quantiser/transpose and swing maths.
Plugin differences: MIDI comes from the host, omni until a channel is chosen, and in external-sync mode
the DAW transport stands in for MIDI clock when nothing is patched into PULSE IN.
