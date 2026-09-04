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
    // The VISUAL viewport when there is one: with a soft keyboard up, the
    // layout viewport still reports full height and the bottom of the editor
    // would sit behind the keyboard.
    const vv = window.visualViewport;
    const cssH = vv ? Math.min(canvas.clientHeight, vv.height) : canvas.clientHeight;
    const w = Math.max(320, Math.floor(canvas.clientWidth * dpr));
    const h = Math.max(240, Math.floor(cssH * dpr));
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

  // ---- on-screen keyboard (phones/tablets) --------------------------------
  // A canvas is not editable, so a touch device never offers a keyboard. The
  // hidden input in index.html is what gets focused; everything below is the
  // plumbing between it and the app. See editor/src/editor/WebTextInput.cpp.
  const kb = document.getElementById('softkb');
  const isTouch = matchMedia('(pointer: coarse)').matches || 'ontouchstart' in window;
  let keys = null;   // ImGuiKey values, read from the module once it is up

  function keyboardWanted() {
    return !!(globalThis.Module && Module.pyrosWantsTextInput);
  }

  function showKeyboard() {
    // focus() only raises the keyboard when called synchronously inside a user
    // gesture. Called from a timer, a frame callback or an async continuation,
    // iOS ignores it silently - which is the usual reason this "does not work".
    kb.value = '';
    kb.focus({ preventScroll: true });
  }
  function hideKeyboard() { kb.blur(); }

  if (isTouch) {
    // On touchEND, not start: iOS treats the gesture as complete there, and a
    // focus during touchstart is undone by the scroll/zoom handling that
    // follows. The app has already processed the previous frame, so
    // pyrosWantsTextInput tells us whether the widget under the finger takes
    // text - one frame late, which is why a first tap opens the keyboard and
    // a second is not needed.
    canvas.addEventListener('touchend', function () {
      if (keyboardWanted()) showKeyboard(); else hideKeyboard();
    }, { passive: true });

    // The app can also stop wanting text (the field lost focus inside ImGui),
    // and then the keyboard should go away on its own.
    setInterval(function () {
      if (document.activeElement === kb && !keyboardWanted()) hideKeyboard();
    }, 250);

    // Text, not keystrokes. An iOS soft keyboard does not emit usable keydown
    // - it reports keyCode 229, or nothing at all - but it does emit `input`
    // carrying what was typed, including autocorrect and dictation.
    kb.addEventListener('input', function () {
      const text = kb.value;
      kb.value = '';
      if (!text || !globalThis.Module || !Module.ccall) return;
      Module.ccall('PyrosWebInputText', null, ['string'], [text]);
    });

    // The few that arrive as key events rather than as text.
    kb.addEventListener('keydown', function (e) {
      if (!globalThis.Module || !Module.ccall) return;
      if (!keys) {
        keys = {
          Backspace: Module.ccall('PyrosWebKeyBackspace', 'number', [], []),
          Enter:     Module.ccall('PyrosWebKeyEnter',     'number', [], []),
          Tab:       Module.ccall('PyrosWebKeyTab',       'number', [], []),
          ArrowLeft: Module.ccall('PyrosWebKeyLeft',      'number', [], []),
          ArrowRight:Module.ccall('PyrosWebKeyRight',     'number', [], [])
        };
      }
      const k = keys[e.key];
      if (k === undefined) return;
      e.preventDefault();
      // Down and up together: ImGui reads these as edges, and a soft keyboard
      // gives no keyup for backspace on iOS - the key would stay stuck down
      // and repeat forever.
      Module.ccall('PyrosWebInputKey', null, ['number', 'number'], [k, 1]);
      setTimeout(function () {
        Module.ccall('PyrosWebInputKey', null, ['number', 'number'], [k, 0]);
      }, 0);
    });

    // The keyboard covers the bottom of the page. visualViewport reports what
    // is left, so the canvas can shrink to it instead of the text field being
    // hidden behind the very keyboard typing into it.
    if (window.visualViewport) {
      window.visualViewport.addEventListener('resize', scheduleResize);
    }
  }

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
