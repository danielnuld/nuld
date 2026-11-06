// NULD loading screen for the web: splash.c (the launcher's and link.elf's) redrawn on a <canvas data-nuld-splash>.
// Same 1280x720 coordinates, floor, wire sphere, the four logo variants 3 s each with a glitch cut, status line,
// HUD corners and scanlines. Reduced motion: one still frame of variant A.
(function () {
  var BG = '#06070A', FG = '#E6E8EC', MUTED = '#8A919C', ACID = '#C8FF2E', PI = Math.PI;
  var MSGS = ['Starting USB', 'Reading memory cards', 'Loading covers'];
  var BARS = [3, 1, 2, 1, 4, 1, 1, 3, 2, 1, 1, 2, 4, 1, 3, 1, 2, 2, 1, 4, 1, 1, 3, 1, 2, 1, 4, 2, 1, 3];
  var TOP = [230, 232, 252, 190], BOT = [370, 372, 348, 420];

  function rgba(hex, a) {
    var n = parseInt(hex.slice(1), 16);
    return 'rgba(' + (n >> 16) + ',' + (n >> 8 & 255) + ',' + (n & 255) + ',' + a.toFixed(3) + ')';
  }
  function blink(s) { return Math.floor(s * 1.9) & 1; }

  function mark(g, cx, cy, r, accent) { // the ∅: ring + acid slash
    var t = r * 0.18;
    g.lineWidth = t; g.strokeStyle = FG;
    g.beginPath(); g.arc(cx, cy, r - t / 2, 0, 2 * PI); g.stroke();
    var d = r * 0.985;
    g.strokeStyle = accent; g.lineCap = 'butt';
    g.beginPath(); g.moveTo(cx - d, cy + d); g.lineTo(cx + d, cy - d); g.stroke();
  }
  function font(g, f) { g.font = f; }

  function variant(g, k, dx, s, accent, ver) {
    g.textBaseline = 'alphabetic';
    if ('fontStretch' in g) g.fontStretch = 'normal';
    if (k === 0) { // A: ring + NULD in Michroma
      font(g, '134px Michroma'); g.letterSpacing = '8px';
      var ww = g.measureText('NULD').width, x = 640 - (132 + 40 + ww) / 2 + dx;
      mark(g, x + 66, 300, 66, accent);
      g.fillStyle = FG; g.fillText('NULD', x + 172, 348);
      g.letterSpacing = '0px';
    } else if (k === 1) { // B: $ nulld in Plex Mono 500 with a block cursor
      font(g, '500 132px "IBM Plex Mono"');
      var w1 = g.measureText('$ ').width, w2 = g.measureText('nulld').width, bx = 640 - (w1 + w2 + 70) / 2 + dx;
      g.fillStyle = accent; g.fillText('$', bx, 346);
      g.fillStyle = FG; g.fillText('nulld', bx + w1, 346);
      if (!blink(s)) { g.fillStyle = accent; g.fillRect(bx + w1 + w2 + 12, 254, 58, 104); }
    } else if (k === 2) { // C: NULD// in Archivo 800 at 125 %
      if ('fontStretch' in g) g.fontStretch = 'expanded';
      font(g, '800 132px Archivo');
      var wn = g.measureText('NULD').width, wt = wn + g.measureText('//').width, cx = 640 - wt / 2 + dx;
      g.fillStyle = FG; g.fillText('NULD', cx, 345);
      g.fillStyle = accent; g.fillText('//', cx + wn, 345);
    } else { // D: the hardware label
      var lw = 680, lx = 300 + dx, ly = 196, top = 150, bot = 64;
      g.fillStyle = FG;
      g.fillRect(lx, ly, lw, 2); g.fillRect(lx, ly + top + bot, lw, 2);
      g.fillRect(lx, ly, 2, top + bot + 2); g.fillRect(lx + lw - 2, ly, 2, top + bot + 2); g.fillRect(lx, ly + top, lw, 2);
      mark(g, lx + 86, ly + top / 2 + 2, 52, accent);
      if ('fontStretch' in g) g.fontStretch = 'expanded';
      font(g, '800 88px Archivo'); g.fillStyle = FG; g.fillText('NULD', lx + 170, ly + top / 2 + 34);
      if ('fontStretch' in g) g.fontStretch = 'normal';
      var b = lx + 18;
      for (var i = 0; i < BARS.length; i++) { g.fillRect(b, ly + top + 14, BARS[i], bot - 26); b += BARS[i] + 2; }
      g.fillRect(b + 14, ly + top, 2, bot);
      font(g, '14px "IBM Plex Mono"');
      g.fillStyle = '#C9CED6'; g.fillText('SCPH-75001 · EE 294.912 MHz', b + 32, ly + top + 26);
      g.fillStyle = accent; g.fillText('v' + ver + ' · 2026 · nuld.local', b + 32, ly + top + 48);
      var sx = lx + (s * 175) % lw;
      g.fillStyle = rgba(accent, 0.75); g.fillRect(sx, ly + 2, 2, top + bot - 2);
    }
  }

  function floor(g, s, accent) {
    var cx = 640, yh = 432, k = 288, ph = (s * 0.9) % 1, amax = 0.5;
    for (var i = 0; i < 16; i++) {
      var z = i + 1 - ph;
      if (z < 0.5) continue;
      var y = yh + k / z, a = amax * (1 - (z - 1) / 9);
      if (a > 0.015 && y < 720) { g.fillStyle = rgba(accent, a); g.fillRect(0, Math.floor(y), 1280, 1); }
    }
    g.lineWidth = 1;
    for (var j = -14; j <= 14; j++) {
      var x0 = cx + 300 * j / 0.6, y0 = yh + k / 0.6, x1 = cx + 300 * j / 10, y1 = yh + k / 10;
      var gr = g.createLinearGradient(x0, y0, x1, y1);
      gr.addColorStop(0, rgba(accent, amax)); gr.addColorStop(1, rgba(accent, 0));
      g.strokeStyle = gr; g.beginPath(); g.moveTo(x0, y0); g.lineTo(x1, y1); g.stroke();
    }
  }

  function sphere(g, s, accent) {
    var cx = 640, cy = 300, R = 196, ct = Math.cos(-0.28), st = Math.sin(-0.28), rot = s * 2 * PI / 28;
    g.lineWidth = 1;
    for (var m = 0; m < 14; m++) {
      var n = m < 9 ? 24 : 36, px = 0, py = 0;
      for (var i = 0; i <= n; i++) {
        var lat, lon;
        if (m < 9) { lat = -PI / 2 + PI * i / n; lon = m * PI / 9 + rot; } else { lat = (m - 11) * PI / 6; lon = 2 * PI * i / n + rot; }
        var x = Math.cos(lat) * Math.cos(lon), y = Math.sin(lat), z = Math.cos(lat) * Math.sin(lon);
        var y2 = y * ct - z * st, z2 = y * st + z * ct;
        var sx = cx + R * x, sy = cy - R * y2;
        if (i) {
          g.strokeStyle = rgba(accent, (0x12 + (z2 + 1) * 0x1C) / 128);
          g.beginPath(); g.moveTo(px, py); g.lineTo(sx, sy); g.stroke();
        }
        px = sx; py = sy;
      }
    }
  }

  function hud(g, x, y, w, h, len, accent) {
    g.fillStyle = accent;
    g.fillRect(x, y, len, 2); g.fillRect(x, y, 2, len);
    g.fillRect(x + w - len, y, len, 2); g.fillRect(x + w - 2, y, 2, len);
    g.fillRect(x, y + h - 2, len, 2); g.fillRect(x, y + h - len, 2, len);
    g.fillRect(x + w - len, y + h - 2, len, 2); g.fillRect(x + w - 2, y + h - len, 2, len);
  }

  function frame(g, s, accent, ver) {
    var k = Math.floor(s / 3) & 3, inn = s % 3;
    g.fillStyle = BG; g.fillRect(0, 0, 1280, 720);
    floor(g, s, accent);
    sphere(g, s, accent);
    if (inn >= 0.18 && inn <= 2.9) variant(g, k, 0, s, accent, ver);
    else { // the glitch cut: the logo redrawn in five bands, shifted
      var off = [-10, 7, -4, 9, -6], f = Math.floor(s * 60), top = TOP[k] - 10, bh = (BOT[k] - TOP[k] + 20) / 5;
      var band = function (y0, y1, dx) { g.save(); g.beginPath(); g.rect(0, y0, 1280, y1 - y0); g.clip(); variant(g, k, dx, s, accent, ver); g.restore(); };
      band(0, top, 0);
      for (var b = 0; b < 5; b++) band(top + b * bh, top + (b + 1) * bh, off[(b + f) % 5]);
      band(top + 5 * bh, 720, 0);
      if (inn < 0.05) { g.fillStyle = rgba(FG, 0.09); g.fillRect(0, 0, 1280, 720); }
    }
    // status line: the boot as the console shows it, then again
    var t = s % 12, st = t < 1.5 ? MSGS[0] : t < 3 ? MSGS[1] : MSGS[2], prog = Math.min(1, t / 10);
    if (t >= 3) { var c = Math.min(20, Math.floor((t - 3) / 7 * 20)); st += '  ' + (c < 10 ? '0' : '') + c + ' / 20'; }
    font(g, '15px "IBM Plex Mono"');
    var tw = g.measureText(st).width, tx = 640 - tw / 2;
    g.fillStyle = BG; g.fillRect(tx - 12, 482, tw + 38, 26);
    g.fillStyle = FG; g.fillText(st, tx, 500);
    if (!blink(s)) { g.fillStyle = accent; g.fillRect(tx + tw + 4, 486, 8, 16); }
    g.fillStyle = '#2A303A'; g.fillRect(490, 524, 300, 2);
    g.fillStyle = accent; g.fillRect(490, 524, 300 * prog, 2);
    hud(g, 24, 24, 1232, 672, 28, accent);
    font(g, '14px "IBM Plex Mono"'); g.fillStyle = MUTED;
    g.fillText('NULD ' + ver, 64, 46);
    var r = 'EE R5900 · 294.912 MHz';
    g.fillText(r, 1216 - g.measureText(r).width, 46);
    g.fillText('SCPH-75001', 1216 - g.measureText('SCPH-75001').width, 690);
    g.fillStyle = rgba(FG, 0.07); // scanlines
    for (var y = 0; y < 720; y += 3) g.fillRect(0, y, 1280, 1);
  }

  function start(cv) {
    var g = cv.getContext('2d'), accent = cv.getAttribute('data-accent') || ACID, ver = cv.getAttribute('data-version') || '1.2';
    var still = window.matchMedia && matchMedia('(prefers-reduced-motion: reduce)').matches, t0 = performance.now();
    function size() {
      var d = window.devicePixelRatio || 1, w = cv.clientWidth || 1280;
      cv.width = Math.round(w * d); cv.height = Math.round(w * 9 / 16 * d);
    }
    function tick(now) {
      if (!cv.isConnected) return;
      if (cv.width !== Math.round((cv.clientWidth || 1280) * (window.devicePixelRatio || 1))) size();
      accent = cv.getAttribute('data-accent') || accent;
      g.setTransform(cv.width / 1280, 0, 0, cv.height / 720, 0, 0);
      frame(g, still ? 1.5 : (now - t0) / 1000, accent, ver);
      if (!still) requestAnimationFrame(tick);
    }
    size();
    (document.fonts ? document.fonts.ready : Promise.resolve()).then(function () { requestAnimationFrame(tick); });
  }

  function scan() {
    var list = document.querySelectorAll('canvas[data-nuld-splash]');
    for (var i = 0; i < list.length; i++) if (!list[i].__nuld) { list[i].__nuld = 1; start(list[i]); }
  }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', scan); else scan();
  new MutationObserver(scan).observe(document.documentElement, { childList: true, subtree: true });
})();
