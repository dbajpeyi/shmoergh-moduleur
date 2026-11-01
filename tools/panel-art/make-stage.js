// Builds the plugin's static background from the Figma "moduleur" export:
// hides cables and moving parts, adds the HOST IN jacks + labels, renders at 3x.
// Usage: node make-stage.js <moduleur.svg> <out.png> [scale]
const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');
(async () => {
  const [,, svgPath, out, scaleArg] = process.argv;
  const scale = +(scaleArg || 3);
  const html = path.resolve(svgPath + '.stage.html');
  fs.writeFileSync(html, `<html><body style="margin:0;background:#090909">${fs.readFileSync(svgPath,'utf8').replace(/<\?xml[^>]*>/,'')}</body></html>`);
  const b = await chromium.launch();
  const p = await b.newPage({ viewport: { width: 1144, height: 1527 }, deviceScaleFactor: scale });
  await p.goto('file://' + html);
  const report = await p.evaluate(() => {
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
    setText('HOST L', 380);
    setText('HOST R', 444);
    return { hidden: hidden.length, handle };
  });
  console.log(JSON.stringify(report));
  await p.screenshot({ path: out, clip: { x: 0, y: 0, width: 1144, height: 1527 } });
  await b.close();
})();
