# Release notes

## v0.3.0 — 1 November 2026

A light appearance, modelled on the white Moduleur.

- **Light appearance.** Right-click the panel and choose **Appearance → Light**. It's a flat, vector take on the white hardware: solid red, green and navy blocks in place of the hatching, knobs in pink, navy, green, red and white, and light labels and jacks on the navy strip. Dark stays the default, and your choice is saved with your project.
- The lettering at the bottom right of the panel now reads **MODULEUR / Analog synthesizer** in both appearances.

## v0.2.0 — 1 November 2026

The first official release from Shmøergh, with the final panel design.

- **New panel design.** The panel, knobs, selector, sliders, buttons and patch cables are drawn to Shmøergh's final design.
- **Brain controls match the panel.** The knobs are now called A, B and C, and the buttons 1 and 2, as printed on the panel (they were X, Y, Z and A, B). In your DAW the automation parameters are now called Brain A, Brain B and Brain C.
- **Jack names match the panel print**, for example Filter LPF In and Brain CV Out 1.
- **Sturdier projects.** Damaged or hand-edited project data can no longer produce invalid audio or stack several cables on one input. If the audio ever becomes invalid, the output mutes and the synth resets itself.
- **Checksums.** Each release now comes with a `SHA256SUMS.txt` to check your downloads against.
- **Licence.** The panel design and artwork are licensed under CC BY-NC 4.0, like the hardware, and the software under MIT.
- The white and black circuit-board panels from v0.1.0 are retired.

**Upgrading from v0.1.0.** The plugin is now published as Shmøergh's own (manufacturer code `Shmo`), so your DAW lists it as a new instrument. Projects made with v0.1.0 need Moduleur inserted again.

## v0.1.0 — 1 November 2026

The first public build, released as an unofficial software clone.

- All eight Moduleur modules, modelled from the open-source schematics: two oscillators, the mixer with its sidechain, the diode ladder filter, two envelopes and VCAs, the crush/LFO/output module, and the Brain running Le Controlleur (MIDI to CV and a step sequencer).
- Patch cables, with the default patch board's connections built in.
- A front panel drawn from the Moduleur's circuit-board panel, in white or black.
- Audio Unit, VST3 and standalone app, running natively on Apple Silicon and Intel Macs.
