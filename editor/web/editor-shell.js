// Page-side setup for the browser build of PyrosBuilder.
//
// Loaded BEFORE PyrosBuilder.js: emscripten reads Module.canvas and
// Module.setStatus while it starts, so they have to exist by then.
(function () {
  const canvas = document.getElementById('canvas');
  const overlay = document.getElementById('overlay');
  const msg = document.getElementById('msg');
  const bar = document.querySelector('#bar > i');

  // The backing store must be sized in DEVICE pixels, while CSS sizes the
  // element in layout pixels. Conflating the two gives a canvas that is either
  // blurry on a retina display or renders at a quarter size on it - and every
  // mouse coordinate the editor receives is then wrong by the same factor,
  // which shows up as clicks landing next to the thing you clicked.
  function resize() {
    const dpr = window.devicePixelRatio || 1;
    const w = Math.max(320, Math.floor(canvas.clientWidth * dpr));
    const h = Math.max(240, Math.floor(canvas.clientHeight * dpr));
    if (canvas.width === w && canvas.height === h) return;
    canvas.width = w;
    canvas.height = h;
    // SDL2 learns about it through a resize event; the editor already handles
    // WindowType_Resize, so nothing C++-side needs to change for this.
    window.dispatchEvent(new Event('resize'));
  }

  // Debounced: a drag of the window edge fires this continuously, and
  // reallocating the GL backbuffer on every pixel makes the drag crawl.
  let pending = 0;
  function scheduleResize() {
    clearTimeout(pending);
    pending = setTimeout(resize, 80);
  }
  window.addEventListener('resize', scheduleResize);
  // devicePixelRatio changes when the window moves between monitors; there is
  // no event for it, so watch the media query that tracks it.
  (function watchDpr() {
    const mq = matchMedia(`(resolution: ${window.devicePixelRatio}dppx)`);
    mq.addEventListener('change', () => { scheduleResize(); watchDpr(); }, { once: true });
  })();

  const fsBtn = document.getElementById('fs');
  function toggleFullscreen() {
    if (document.fullscreenElement) document.exitFullscreen();
    // The BODY, not the canvas: fullscreening the canvas alone drops the
    // button out of the document flow and there is then no way back except
    // Escape - which people do not guess.
    else document.body.requestFullscreen().catch(() => {});
  }
  fsBtn.addEventListener('click', toggleFullscreen);
  document.addEventListener('fullscreenchange', () => {
    fsBtn.textContent = document.fullscreenElement ? '⛶ Exit' : '⛶ Fullscreen';
    scheduleResize();
  });
  // F11 would otherwise fullscreen the BROWSER, which leaves the canvas its
  // old size inside a bigger window.
  window.addEventListener('keydown', (e) => {
    if (e.key === 'F11') { e.preventDefault(); toggleFullscreen(); }
  }, true);

  var Module = {
    canvas: canvas,
    // Keep the canvas the size the page gives it. Without this emscripten
    // resizes the ELEMENT to whatever the C++ side asks for, and the CSS
    // 100vw/100vh above stops meaning anything.
    setCanvasSize: null,
    setStatus: function (text) {
      if (!text) {
        overlay.classList.add('gone');
        canvas.focus();
        return;
      }
      msg.textContent = text;
      const m = /([\d.]+)\s*\/\s*([\d.]+)/.exec(text);
      if (m && bar) bar.style.width = (100 * parseFloat(m[1]) / parseFloat(m[2])) + '%';
    },
    onRuntimeInitialized: function () {
      resize();
      // Cleared once the first frame is actually up, not here: the editor
      // still has its own Init() to run and a blank canvas behind a removed
      // overlay reads as a hang.
      setTimeout(function () { Module.setStatus(''); }, 300);
    },
    printErr: function (text) { console.error(text); }
  };
  globalThis.Module = Module;

  resize();
})();
