// Builds the plugin's static background from the Figma "moduleur" export:
// hides cables and moving parts, adds the HOST jacks, renders at 3x.
// Usage: node make-stage.js <moduleur.svg> <out.png> [scale] [dark|light]
// "light" recolours the dark design into the white hardware look: solid red,
// green and navy blocks instead of hatching, navy hardware, dark print.
const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');
(async () => {
  const [,, svgPath, out, scaleArg, modeArg] = process.argv;
  const scale = +(scaleArg || 3);
  const mode = modeArg || 'dark';
  const html = path.resolve(svgPath + '.stage.html');
  fs.writeFileSync(html, `<html><body style="margin:0;background:#090909">${fs.readFileSync(svgPath,'utf8').replace(/<\?xml[^>]*>/,'')}</body></html>`);
  const b = await chromium.launch();
  const p = await b.newPage({ viewport: { width: 1144, height: 1527 }, deviceScaleFactor: scale });
  await p.goto('file://' + html);
  const report = await p.evaluate((mode) => {
    const svg = document.querySelector('svg');
    const leaves = [...svg.querySelectorAll('path,use,rect,circle,ellipse')].filter(e => !e.closest('defs,clipPath,mask,symbol'));
    const rgb = e => (e.getAttribute('fill') || '').replace(/\s/g, '');
    const hidden = [];
    const hide = e => { e.style.display = 'none'; hidden.push(e); };
    const WHITE = 'rgb(100%,100%,100%)', ORANGE = 'rgb(100%,52.941895%,0%)', POINTER = 'rgb(6.666565%,6.666565%,6.666565%)';
    const LED_ON = 'rgb(100%,0%,0%)', LED_OFF = 'rgb(21.176147%,9.411621%,9.803772%)';
    const GREYS = ['rgb(74.902344%,74.902344%,74.902344%)', 'rgb(71.765137%,71.765137%,71.765137%)', 'rgb(85.098267%,85.098267%,85.098267%)'];
    // The first cable plug is the first element drawn in a cable colour after the handle.
    const handle = leaves.findIndex(e => rgb(e) === 'rgb(97.647095%,97.647095%,97.647095%)');
    leaves.forEach((e, i) => {
      const r = e.getBoundingClientRect();
      const f = rgb(e);
      if (i > handle + 18) { hide(e); return; }                       // cables + plugs (drawn last)
      if (f === WHITE && r.y > 190) { hide(e); return; }               // knob caps, selector, cutoff, slider caps, pointers on orange
      if (f === ORANGE && r.width < 40) { hide(e); return; }           // orange knob caps
      if (f === POINTER && r.width < 5 && r.height < 10) { hide(e); return; }
      if (f === LED_ON || f === LED_OFF) { hide(e); return; }          // LED cores (drawn live)
      if (GREYS.includes(f)) { hide(e); return; }                      // buttons 1/2, ON/OFF cap
    });

    // HOST IN jacks: clone the SYNC jack of OSC 1 (centre 124,635 in this export).
    const near = (cx, cy, w) => leaves.filter(e => { const r = e.getBoundingClientRect(); return Math.abs(r.x + r.width / 2 - cx) < 0.6 && Math.abs(r.y + r.height / 2 - cy) < 0.6 && Math.abs(r.width - w) < 0.6; });
    const parts = [near(124, 635, 40)[0], near(124, 635, 42)[0], near(124, 635, 32)[0], near(124, 635, 14)[0]];
    if (parts.some(x => !x)) return { error: 'jack parts not found' };
    const nodes = parts.map(e => e.tagName === 'use' && e.parentElement.getAttribute('mask') ? e.parentElement : e);
    const anchor = nodes[3];
    const NS = 'http://www.w3.org/2000/svg';
    for (const [dx, dy] of [[256, 768], [320, 768]]) {
      const g = document.createElementNS(NS, 'g');
      g.setAttribute('transform', `translate(${dx} ${dy})`);
      nodes.forEach(n => g.appendChild(n.cloneNode(true)));
      anchor.parentNode.insertBefore(g, anchor.nextSibling);
    }

    // Labels "HOST L" / "HOST R" set from the panel's own glyphs, on the 12V DC baseline.
    const first = [...svg.querySelectorAll('use')].find(u => u.getAttribute('xlink:href') === '#glyph-1-25' && Math.abs(+u.getAttribute('x') - 168.416) < 0.01);
    if (!first) return { error: 'label glyph not found' };
    const glyph = { H: [12, 7.872], O: [13, 8.076], S: [3, 7.2], T: [0, 7.0], ' ': [8, 3.06], L: [20, 6.6], R: [1, 7.6] };
    const setText = (text, centreX) => {
      const width = [...text].reduce((w, c) => w + glyph[c][1], 0);
      let x = centreX - width / 2;
      for (const c of text) {
        const u = first.cloneNode(true);
        u.setAttributeNS('http://www.w3.org/1999/xlink', 'xlink:href', '#glyph-1-' + glyph[c][0]);
        u.setAttribute('x', x.toFixed(3));
        first.parentNode.appendChild(u);
        x += glyph[c][1];
      }
    };
    // "HOG MODULEUR" -> "MODULEUR". The lettering is drawn as masked shapes, so
    // the first word (H O G + space, x 745-786, baseline 1400) is covered with
    // plain panel. The line is right-aligned, so MODULEUR stays where it is.
    const hog = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
    hog.setAttribute('x', 744); hog.setAttribute('y', 1385); hog.setAttribute('width', 42); hog.setAttribute('height', 20);
    hog.setAttribute('fill', mode === 'light' ? '#EEEEEC' : 'rgb(14.509583%, 14.509583%, 15.293884%)');
    hog.setAttribute('data-moduleur', 'hog');
    svg.appendChild(hog);

    if (mode === 'light') {
      const norm = f => (f || '').replace(/\s/g, '').toLowerCase();
      const C = {
        backdrop: '#F8F8F7', panel: '#EEEEEC', print: '#13192B', printOnNavy: '#F4F4F2', line: '#C9CCCF',
        navy: '#0B1630', face: '#1F2D48', screw: '#18253F', grip: '#050D1F', handle: '#14223A', foot: '#061933',
        red: '#E0412F', green: '#3F9A6B', block: '#0B1A36', ringOnNavy: '#3B4A66', faceOnNavy: '#16233D',
      };
      const P = {
        backdrop: 'rgb(3.529358%,3.529358%,3.529358%)', panel: 'rgb(14.509583%,14.509583%,15.293884%)',
        print: 'rgb(60.392761%,60.392761%,61.177063%)', line: 'rgb(29.411316%,29.411316%,31.37207%)',
        black: 'rgb(0%,0%,0%)', face: 'rgb(11.764526%,11.764526%,11.764526%)', screw: 'rgb(10.980225%,10.980225%,10.980225%)',
        grip: 'rgb(6.666565%,6.666565%,6.666565%)', handle: 'rgb(97.647095%,97.647095%,97.647095%)', white: 'rgb(100%,100%,100%)',
      };
      // Hatched areas (oscillators, mixer circle, lower ENV 2 strip) become solid
      // blocks: keep each area's mask, replace the hatch lines with a fill.
      const NS2 = 'http://www.w3.org/2000/svg';
      const hatch = leaves.filter(e => e.tagName === 'use' && e.parentElement.getAttribute('mask') && e.getBoundingClientRect().width > 700);
      const blockColour = r => (r.y > 1000 ? C.block : (r.y > 500 ? C.green : C.red));
      hatch.forEach(u => {
        const r = u.getBoundingClientRect();
        const rect = document.createElementNS(NS2, 'rect');
        rect.setAttribute('x', -2000); rect.setAttribute('y', -2000); rect.setAttribute('width', 6000); rect.setAttribute('height', 6000);
        rect.setAttribute('fill', blockColour(r));
        u.replaceWith(rect);
      });
      const inNavy = r => { const cx = r.x + r.width / 2, cy = r.y + r.height / 2; return cx > 316 && cx < 572 && cy > 1083; };
      // Recolour everything except mask contents (masks work on luminance).
      [...svg.querySelectorAll('[fill]')].forEach(e => {
        if (e.closest('mask') || e.getAttribute('data-moduleur')) return;
        const f = norm(e.getAttribute('fill'));
        const r = e.getBoundingClientRect();
        const isLeaf = leaves.includes(e);
        if (f === P.backdrop) e.setAttribute('fill', C.backdrop);
        else if (f === P.panel) {
          if (isLeaf && r.width > 1000) e.setAttribute('fill', C.panel);
          else if (isLeaf) e.setAttribute('display', 'none');      // label knockouts: blocks show through
          else e.setAttribute('fill', C.panel);
        }
        else if (f === P.print) e.setAttribute('fill', isLeaf && inNavy(r) ? C.printOnNavy : C.print);
        else if (f === P.line) e.setAttribute('fill', C.line);
        else if (f === P.black) e.setAttribute('fill', inNavy (r) && r.width > 30 ? C.ringOnNavy : C.navy);   // incl. the cloned HOST jacks
        else if (f === P.face) e.setAttribute('fill', inNavy (r) ? C.faceOnNavy : C.face);
        else if (f === P.screw) e.setAttribute('fill', C.screw);
        else if (f === P.grip) e.setAttribute('fill', C.grip);
        else if (f === P.handle) e.setAttribute('fill', C.handle);
        else if (f === P.white && r.y < 190) e.setAttribute('fill', C.foot);
      });
    }
    return { hidden: hidden.length, handle, mode };
  }, mode);
  console.log(JSON.stringify(report));
  await p.screenshot({ path: out, clip: { x: 0, y: 0, width: 1144, height: 1527 } });
  await b.close();
})();
