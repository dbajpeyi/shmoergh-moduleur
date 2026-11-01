# Panel art

The plugin's background (`assets/design/stage@3x.png`) and its knob outlines (`src/ui/DesignParts.h`) are generated from the Figma design, exported as PDF into `source/`:

- `moduleur.pdf`: the full composition (backdrop, handle, panel, cables)
- `panel.pdf`: the panel on its own

The background keeps everything static (panel, print, hatching, jacks, screws, handle) and leaves out the cables and moving parts, which the plugin draws itself.

## Regenerating

You need Poppler (`pdftocairo`), Node.js with Playwright, and Python 3 with Pillow and NumPy.

```sh
cd tools/panel-art
mkdir -p build
pdftocairo -svg source/moduleur.pdf build/moduleur.svg
pdftocairo -svg source/panel.pdf build/panel.svg
npm install --no-save playwright && npx playwright install chromium

node make-stage.js build/moduleur.svg build/stage-raw@3x.png 3
python3 host-labels.py build/stage-raw@3x.png ../../assets/design/stage@3x.png
python3 extract-parts.py build/panel.svg ../../src/ui/DesignParts.h
```

`make-stage.js` hides the moving parts and adds the two HOST jacks by cloning an existing jack. `host-labels.py` sets the HOST L / HOST R labels from letters of the panel's own lettering. If the design moves parts around, update the positions in those two scripts and in `src/dsp/Ports.h` and `src/Params.h`.
