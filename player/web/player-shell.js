// The page side of a game built for the web.
//
// Loaded BEFORE PyrosPlayer.js. It does three things the module cannot do
// for itself:
//   1. puts the game's files into the module's file system before main()
//      runs - game.data (every file, end to end) and game.files.json (where
//      each one is in it), both written by Build Game;
//   2. keeps the canvas the size of the window, in device pixels;
//   3. makes a phone behave: no scrolling or zooming under the game, the
//      full-screen button where the browser has one, and on an iPhone -
//      which has none - the one way there is (the home screen).
(function () {
  const canvas = document.getElementById('canvas');
  const overlay = document.getElementById('overlay');
  const msg = document.getElementById('msg');
  const bar = document.querySelector('#bar > i');
  const rotate = document.getElementById('rotate');
  const fsBtn = document.getElementById('fs');
  const tip = document.getElementById('tip');
  const isTouch = matchMedia('(pointer: coarse)').matches || 'ontouchstart' in window;

  // A phone's screen has three device pixels to a point. Drawing all of
  // them costs nine times the fill of one and shows nothing a game's art
  // has - two is the limit.
  const MAX_DPR = 2;
  let wantLandscape = false;

  function resize() {
    const dpr = Math.min(window.devicePixelRatio || 1, MAX_DPR);
    const w = Math.max(160, Math.round(canvas.clientWidth * dpr));
    const h = Math.max(120, Math.round(canvas.clientHeight * dpr));
    if (isTouch && wantLandscape) rotate.classList.toggle('gone', window.innerWidth >= window.innerHeight);
    if (canvas.width === w && canvas.height === h) return;
    // The game reads the canvas's size each frame and follows it. SDL's own
    // handler of the window's resize event runs before this (it sets the
    // canvas to the element's size in CSS pixels); this is debounced, so it
    // has the last word.
    canvas.width = w;
    canvas.height = h;
  }
  let pending = 0;
  function scheduleResize() {
    clearTimeout(pending);
    pending = setTimeout(resize, 60);
  }
  window.addEventListener('resize', scheduleResize);
  window.addEventListener('orientationchange', function () { setTimeout(resize, 250); });
  if (window.visualViewport) window.visualViewport.addEventListener('resize', scheduleResize);

  // A touch on the game is the game's. Left alone, the browser also scrolls
  // the page, zooms on a double tap, and selects the canvas on a long press.
  ['touchstart', 'touchmove', 'touchend', 'touchcancel'].forEach(function (type) {
    canvas.addEventListener(type, function (e) { e.preventDefault(); }, { passive: false });
  });
  ['gesturestart', 'gesturechange', 'gestureend', 'contextmenu', 'selectstart'].forEach(function (type) {
    document.addEventListener(type, function (e) { e.preventDefault(); });
  });

  // Sound. A browser keeps a page silent until it is touched, and the audio
  // library wakes its output on the first touch only - once, whether or not
  // that worked, and whether or not there was an output yet (a finger on the
  // loading screen used the one chance up). So every touch wakes whatever
  // is still asleep. And on an iPhone a page's sound follows the ring/silent
  // switch unless the page says it is playing something: a game is.
  try { if (navigator.audioSession) navigator.audioSession.type = 'playback'; } catch (e) {}
  function wakeAudio() {
    const m = window.miniaudio;
    if (!m || !m.devices) return;
    for (const d of m.devices) {
      if (d && d.webaudio && d.webaudio.state !== 'running') {
        const p = d.webaudio.resume();
        if (p && p.catch) p.catch(function () {});
      }
    }
  }
  ['touchstart', 'touchend', 'pointerdown', 'pointerup', 'click', 'keydown'].forEach(function (type) {
    document.addEventListener(type, wakeAudio, true);
  });
  document.addEventListener('visibilitychange', function () { if (!document.hidden) wakeAudio(); });

  // Full screen. A desktop browser, Android and an iPad can give the page
  // the screen; an iPhone cannot - there, a page is full screen only when
  // opened from the home screen, so that is said once, and not when it
  // already is.
  const fsTarget = document.documentElement;
  const canFullscreen = !!(fsTarget.requestFullscreen || fsTarget.webkitRequestFullscreen);
  const standalone = navigator.standalone === true || matchMedia('(display-mode: fullscreen), (display-mode: standalone)').matches;
  function toggleFullscreen() {
    const on = document.fullscreenElement || document.webkitFullscreenElement;
    if (on) (document.exitFullscreen || document.webkitExitFullscreen).call(document);
    else {
      const p = (fsTarget.requestFullscreen || fsTarget.webkitRequestFullscreen).call(fsTarget);
      if (p && p.catch) p.catch(function () {});
      // Where it can be asked for, the game is held the way it was made.
      if (wantLandscape && screen.orientation && screen.orientation.lock) screen.orientation.lock('landscape').catch(function () {});
    }
  }
  if (canFullscreen && !standalone) {
    fsBtn.classList.remove('gone');
    fsBtn.addEventListener('click', toggleFullscreen);
    ['fullscreenchange', 'webkitfullscreenchange'].forEach(function (type) {
      document.addEventListener(type, function () {
        fsBtn.classList.toggle('gone', !!(document.fullscreenElement || document.webkitFullscreenElement));
        scheduleResize();
      });
    });
  } else if (isTouch && !standalone) {
    let told = false;
    try { told = localStorage.getItem('pyros.homeScreenTip') === '1'; } catch (e) {}
    if (!told) {
      tip.classList.remove('gone');
      const hide = function () {
        tip.classList.add('gone');
        try { localStorage.setItem('pyros.homeScreenTip', '1'); } catch (e) {}
      };
      tip.addEventListener('click', hide);
      setTimeout(hide, 9000);
    }
  }

  function status(text, fraction) {
    msg.textContent = text;
    if (fraction !== undefined) bar.style.width = Math.round(100 * fraction) + '%';
  }
  function fail(text) {
    status(text);
    bar.parentElement.style.display = 'none';
    console.error(text);
  }

  // One request for the list, one for the bytes, with the bar following the
  // second: on a phone's connection a game's worth of small files is a game's
  // worth of round trips.
  async function loadGame(Module) {
    const list = await (await fetch('game.files.json', { cache: 'no-cache' })).json();
    if (list.title) document.title = list.title;
    wantLandscape = (list.width || 0) > (list.height || 0);
    resize();
    const bytes = await fetchPart(list.data || 'game.data', list.size, true);
    writeFiles(Module.FS, list, bytes, 0);
    // A game in two parts (game.more.data: what its first scene does not read): the rest comes after it has started,
    // and until it is written /.pyros-files-coming says so - the player starts no other scene meanwhile
    if (list.more) {
      Module.FS.writeFile('/.pyros-files-coming', '');
      moreToCome = list;
    }
    status('Starting…', 1);
  }

  let moreToCome = null;
  async function loadRest(Module) {
    const list = moreToCome;
    moreToCome = null;
    try {
      const bytes = await fetchPart(list.more, list.moreSize, false);
      writeFiles(Module.FS, list, bytes, 1);
      Module.FS.unlink('/.pyros-files-coming');
    } catch (e) {
      console.error('The rest of the game could not be loaded (' + (e && e.message ? e.message : e) + ')');
    }
  }

  // One part's bytes, the bar following them when it is shown
  async function fetchPart(url, size, showing) {
    const response = await fetch(url, { cache: 'no-cache' });
    if (!response.ok) throw new Error(url + ': ' + response.status);
    const total = size || Number(response.headers.get('Content-Length')) || 0;
    let bytes;
    if (response.body && total) {
      bytes = new Uint8Array(total);
      const reader = response.body.getReader();
      let got = 0;
      for (;;) {
        const part = await reader.read();
        if (part.done) break;
        if (got + part.value.length > bytes.length) {       // a server that lied about the length
          const bigger = new Uint8Array((got + part.value.length) * 2);
          bigger.set(bytes.subarray(0, got));
          bytes = bigger;
        }
        bytes.set(part.value, got);
        got += part.value.length;
        if (showing) status('Loading…', Math.min(1, got / total));
      }
      bytes = bytes.subarray(0, got);
    } else {
      bytes = new Uint8Array(await response.arrayBuffer());
    }
    return bytes;
  }

  // The files of one part (an entry's "part": 0, or none, the first) into the module's file system
  function writeFiles(FS, list, bytes, part) {
    for (const f of list.files) {
      if ((f.part || 0) !== part) continue;
      const at = f.path.lastIndexOf('/');
      if (at > 0) FS.mkdirTree('/' + f.path.slice(0, at));
      FS.writeFile('/' + f.path, bytes.subarray(f.offset, f.offset + f.size));
    }
  }

  const Module = {
    canvas: canvas,
    // The page sizes the canvas; the game is told, and does not get to.
    setCanvasSize: null,
    preRun: [function () {
      // The page's PYROS_* parameters as the game's environment (?PYROS_LOG_LEVEL=3&PYROS_RENDERER_DEBUG=albedo): the switches
      // a desktop run is given, for a build in a browser that nothing else reaches
      new URLSearchParams(location.search).forEach(function (value, name) {
        if (name.indexOf('PYROS_') === 0) Module.ENV[name] = value;
      });
      Module.addRunDependency('game-files');
      loadGame(Module).then(function () {
        Module.removeRunDependency('game-files');
      }).catch(function (e) {
        fail('This game could not be loaded (' + (e && e.message ? e.message : e) + ').');
      });
    }],
    setStatus: function () {},
    onRuntimeInitialized: function () { resize(); },
    // main() has returned: the game is set up and its first frame is next.
    postRun: [function () {
      if (moreToCome) loadRest(Module);
      setTimeout(function () { overlay.classList.add('gone'); canvas.focus(); resize(); }, 200);
    }],
    print: function (text) { console.log(text); },
    printErr: function (text) { console.error(text); },
    onAbort: function (what) { overlay.classList.remove('gone'); fail('The game stopped: ' + what); }
  };
  globalThis.Module = Module;
  resize();
})();
