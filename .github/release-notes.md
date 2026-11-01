The Shmøergh Moduleur as an Audio Unit, VST3 and standalone app for macOS 10.13 or later, running natively on Apple Silicon and Intel Macs.

## Installing

1. Download the zip files below and unzip them.
2. Copy the files:
   - `Moduleur.component` (Audio Unit: Logic Pro, GarageBand, MainStage) → `~/Library/Audio/Plug-Ins/Components/`
   - `Moduleur.vst3` (Ableton Live, Bitwig, Reaper, Cubase…) → `~/Library/Audio/Plug-Ins/VST3/`
   - `Moduleur.app` (standalone) → `/Applications`
3. This release isn't notarized by Apple yet, so macOS blocks it the first time. Open Terminal and run:

   ```sh
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/Moduleur.component \
       ~/Library/Audio/Plug-Ins/VST3/Moduleur.vst3 /Applications/Moduleur.app
   ```

4. Restart your DAW. If Logic Pro doesn't list Moduleur, run `auval -v aumu Mdlr Shmo` once.

To check a download, compare its checksum with `SHA256SUMS.txt`: run `shasum -a 256 Moduleur-*.zip`.

## Getting started

Load **Moduleur** as an instrument and play. It's pre-patched, so it makes sound straight away. In the standalone app, choose your MIDI keyboard under **Options → Audio/MIDI Settings**. No keyboard? Hold the Brain's **1** and **2** buttons for 1.5 seconds, then press **1** to start the built-in sequencer.

Panel design and artwork: CC BY-NC 4.0. Software: MIT.
