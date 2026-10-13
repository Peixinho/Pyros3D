# A game in a browser

Build Game has a **web** platform: the game as a folder any web server can
serve - GitHub Pages included - that runs in a desktop browser and on a
phone.

```
index.html            the page
player-shell.js       loads the game, sizes the canvas, handles touch
PyrosPlayer.js/.wasm  the player (WebGL2)
game.data             every file of the game, end to end
game.files.json       where each one is in it
manifest.webmanifest, icon.png
```

**Making one.** *Build > Platform: web* in PyrosBuilder, or
`build_game {"outputDir": ..., "platform": "web"}`. It needs the web player
template once: `tools/fetch_templates.sh web` takes it from CI, or build it -

```
emcmake cmake -S . -B build_web_player -DCMAKE_BUILD_TYPE=Release -DBUILD_PLAYER=ON -DHAVE_LUA_BINDINGS=ON
cmake --build build_web_player --target PyrosPlayer
```

- and an editor run from this source tree finds it there.

**Trying it.** A browser will not load it from a file: serve the folder
(`python3 -m http.server` in it) and open that.

**What is different from a desktop build.**

- OpenGL ES 3 through WebGL2, one thread. The forward renderer and everything
  2D run; compute shaders do not exist there.
- The page owns the canvas: it is the size of the window in device pixels
  (two per point at most), and the game follows it. `game.json`'s width and
  height only say which way up the game was made - wider than tall, and a
  phone held upright is asked to turn.
- A touch is the mouse: the first finger moves the pointer and presses the
  left button. Scripts that read `isMouseButtonDown` and `view.mouseWorld()`
  need nothing else.
- Sound starts with the first touch or click - a browser's rule, not ours.
  The page wakes the audio output on every touch until it is running, and
  asks an iPhone to play it whatever the ring/silent switch says (Safari
  16.4 and later; before that, a silenced phone is a silent game).
- `store.read(name)` / `store.write(name, text)` - a game's own small files
  on a desktop - keep each name in the browser's local storage, so a best
  score is still there on the next visit (on that device, in that browser).
- No dedicated server and no sockets.
- Text drawn from a font built of overlapping shapes can show seams (the
  desktop removes them; in a browser that step is off).

**Full screen on a phone.** Android, iPads and desktop browsers get a
full-screen button. An iPhone gives a page the whole screen only when it is
opened from the home screen: *Share > Add to Home Screen*. The page says so
once. The canvas is kept clear of the notch and the home bar either way.
