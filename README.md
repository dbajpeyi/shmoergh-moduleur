# Moduleur (plugin)

A software version of the [Shmøergh Moduleur](https://github.com/shmoergh/moduleur), the open-source
analog modular synthesizer, as an **Audio Unit, VST3 and standalone app** for macOS (VST3/standalone also
build on Linux).

![Moduleur plugin](docs/screenshot.png)

The front panel is drawn from the Moduleur's own FR4 enclosure artwork
(`enclosure/fr4/01-front/graphics`), so every knob, slider, jack and LED sits where it does on the hardware.
The sound comes from behavioural models of each module's schematic:

| Module | What's modelled |
| --- | --- |
| **Oscillator ×2** | Saw core with 1 V/oct expo pitch (OCTAVE 0–5.2 V, FINE ±1 oct), diode triangle shaper, comparator square with WIDTH + PWM, CD4040 ÷2 **SUB** (OSC 1) and transistor **NOISE** (OSC 2), hard sync, linear FM, TRI/SAW/SQR rotary switch. Band-limited (PolyBLEP). |
| **Mixer & Sidechain** | 3 DC-coupled channels on sliders into a VCA, channel 4 with GAIN (×1–11), peak detector, THRESHOLD subtractor, so IN/4 can "pump" the mix. Main OUT level. |
| **Filter** | 3-pole diode ladder (the D1–D10 / 3×1 nF network) with separate **IN/LP** and **IN/HP** inputs, resonance up to self-oscillation, two CV inputs with log attenuverters. |
| **Envelope & VCA ×2** | RC ADSR (10 µF cap, log timing pots up to ~10 s), 0–8 V out, LED; diff-pair VCA. ENV OUT is jumpered to the VCA CONTROL like the hardware jumper. |
| **Crush / LFO / Output** | JFET sample & hold "bitcrusher" with its 160 Hz–19 kHz relaxation clock (or external TRIGGER), triangle/square LFO 0.1–200 Hz with LFO AMNT, output stage with DRIVE into red-LED clipping, VOLUME. |
| **Brain** | A port of the [Le Controlleur](https://github.com/shmoergh/le-controlleur) firmware (MIDI to CV and the generative Sequencer), using the same X/Y/Z pots, A/B buttons and LEDs. MIDI comes from your DAW. |

Everything runs at 2× oversampling, in volts like the real thing.

## Playing it

Load it as an instrument and play MIDI. It ships with the **default patch board** fitted (the Moduleur's
optional passive PCB), so it plays without cables:

```
OSC 1 OUT, OSC 2 OUT, OSC 1 SUB, OSC 2 NOISE -> Mixer IN/1..4 -> Filter IN/LP -> VCA 1 -> Output
LFO TRI -> Filter CV/1        ENV 2 -> Filter CV/2     (turn the LEVEL attenuverters to hear them)
PITCH and GATE jacks are mirrored between OSC 1/2 and ENV 1/2
```

Two cables are patched on a fresh instance: **Brain OUT/1 → OSC 1 PITCH** and **Brain PULSE → ENV 1 GATE**.
The patch board mirrors them to OSC 2 and ENV 2. Brain OUT/2 carries velocity (0–5 V) by default.

**Patching:** drag from any jack to another. Drag a cable out of an input to move it, or drop it on the panel
to unplug it. Right-click a jack to remove its cables. As on the hardware, plugging into a jack breaks its
default-patch connection (e.g. patching OSC 2 OUT elsewhere removes it from the mixer).

**Brain (Le Controlleur):**
- *MIDI to CV* (default): hold **A** and turn **X** to pick the MIDI channel (omni until you set one). Hold **B**
  and turn **Y** to choose OUT/1 or OUT/2 for pitch, and **Z** for velocity / mod wheel / unison / duophonic.
  Tap **A+B** for panic.
- Hold **A+B for 1.5 s** to switch to the *Sequencer*: **A** play/pause, **X** tempo (fully left = external sync
  to PULSE IN or the DAW transport), **Y** octave range, **Z** randomness. Hold **B** for swing / scale /
  length; double-tap and hold **B** to transpose.

Double-click a control to reset it. Right-click the panel for the menu: white or black FR4 panel, size, cable
opacity, Brain firmware, removing the default patch board, init patch and panic. Every control is a host
parameter, so you can automate it.

**Plugin-only additions:** two **HOST IN** jacks in the bottom strip carry the plugin's (sidechain) audio input,
so you can patch a drum loop into Mixer IN/4 to pump the mix, or run audio through the filter. The **ON/OFF**
switch mutes the output.

## Installing on macOS

### Pre-built (from CI)

Each push builds universal (Apple Silicon + Intel) binaries in GitHub Actions: open the latest run under
**Actions → Build → Moduleur-macOS** and download the zips. Tagged releases (`v*`) attach them to a GitHub
release.

- `Moduleur.component` → `~/Library/Audio/Plug-Ins/Components/`
- `Moduleur.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
- `Moduleur.app` → `/Applications` (the standalone; pick your MIDI keyboard and audio device under *Options*)

The builds are ad-hoc signed, not notarised. After downloading, clear the quarantine flag:

```sh
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/Moduleur.component \
    ~/Library/Audio/Plug-Ins/VST3/Moduleur.vst3 /Applications/Moduleur.app
```

If Logic doesn't list it, run `auval -v aumu Mdlr Shmc` once, or restart Logic.

### Build it yourself

Requires Xcode (or the Command Line Tools) and CMake ≥ 3.22. JUCE 8 is fetched automatically.

```sh
cmake -S . -B build -G Xcode          # or: -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release     # DSP + processor tests
```

On macOS the AU and VST3 are copied into `~/Library/Audio/Plug-Ins/` after building (turn this off with
`-DMODULEUR_COPY_AFTER_BUILD=OFF`). The standalone app is in
`build/ShmoerghModuleur_artefacts/Release/Standalone/`.

## Project layout

```
src/dsp/      Module models (Modules.h), Brain firmware port, rack/patching engine, port + normal tables
src/ui/       Panel artwork, knobs, faders, jacks, LEDs, patch cables
src/          Plugin processor/editor, parameter table (Params.h)
assets/panel  Front-panel copper and silkscreen artwork from the Moduleur repository
tests/        Headless DSP and processor tests
docs/DSP.md   Notes on how each circuit is modelled
```

## Credits and licence

This is an unofficial fan project, not affiliated with or endorsed by Shmøergh.

- The Moduleur hardware design and the panel artwork in `assets/panel` are © Shmøergh, licensed
  [CC BY-NC 4.0](https://creativecommons.org/licenses/by-nc/4.0/). That makes this plugin **non-commercial**.
- The Brain behaviour is ported from Shmøergh's Le Controlleur / Brain SDK (MIT).
- The plugin code is MIT licensed (see `LICENSE`). It uses [JUCE](https://juce.com) under the AGPLv3 /
  JUCE licence.
