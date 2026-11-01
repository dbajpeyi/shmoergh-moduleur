Unofficial software clone of the [Shmøergh Moduleur](https://github.com/shmoergh/moduleur) for macOS 10.13+ (Apple Silicon and Intel).

## Install

1. Download the zips below and unzip them.
2. Copy the files:
   - `Moduleur.component` (Audio Unit, for Logic, GarageBand…) → `~/Library/Audio/Plug-Ins/Components/`
   - `Moduleur.vst3` (Ableton, Bitwig, Reaper…) → `~/Library/Audio/Plug-Ins/VST3/`
   - `Moduleur.app` (standalone) → `/Applications`
3. These builds are not notarised by Apple, so macOS blocks them at first. Run this once in Terminal:

   ```sh
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/Moduleur.component \
       ~/Library/Audio/Plug-Ins/VST3/Moduleur.vst3 /Applications/Moduleur.app
   ```

4. Restart your DAW. If Logic doesn't list it, run `auval -v aumu Mdlr Shmc` once.

In a DAW, load **Moduleur** as an instrument and play MIDI. In the standalone app, pick your MIDI keyboard under *Options → Audio/MIDI Settings*, or hold the Brain's **A+B** buttons for 1.5 s and press **A** to start the built-in sequencer.

The panel artwork and hardware design are © Shmøergh, CC BY-NC 4.0: non-commercial use only.
