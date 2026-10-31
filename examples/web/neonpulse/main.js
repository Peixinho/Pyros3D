/** Neon Pulse bootstrap — port of assets/neonpulse/main.lua (standalone path). */

import { createConfig } from "./config.js";
import { createArena } from "./arena.js";
import { createAudio } from "./audio.js";
import { createEntities } from "./entities.js";
import { createGame } from "./game.js";

export async function startNeonPulse(P, app, canvas, setStatus) {
  const width = canvas.width;
  const height = canvas.height;
  const C = createConfig(P);
  const G = {};

  G.scene = new P.Scene();
  G.projection = new P.Projection();

  // (drawn by the engine's renderer onto the app's canvas, WebGPU)
  G.renderer = app.createRenderer();

  G.camera = new P.GameObject();
  G.camera.setPosition(C.camera.pos);
  G.camera.lookAtVec(C.camera.lookAt);
  G.scene.add(G.camera);

  // HTTP fetch → memory (no VFS / .data). File is copied next to this page at build time.
  G.sprite = await P.loadTextureFromUrl("./assets/smoke.png", {
    type: P.TextureType_Texture,
    mipmap: true,
  });

  const Arena = createArena(P, G, C);
  const Audio = createAudio(P, G, C, "assets/neonpulse/sfx/");
  const E = createEntities(P, G, C, Arena, Audio);
  const Game = createGame(P, G, C, Arena, E, Audio, { width, height });

  Arena.build();
  Audio.build();
  E.build();
  Game.bindInput(canvas);

  E.loadLevel(1);
  Game.setState("title");
  fitProjection(G, C, width, height);

  let lastTime = app.getTime();
  setStatus("Neon Pulse — space / click to begin");

  function frame() {
    if (!app.isRunning()) {
      setStatus("Stopped");
      return;
    }
    app.pollEvents();
    const time = app.getTime();
    let dt = time - lastTime;
    lastTime = time;
    if (dt > 0.05) dt = 0.05;

    Game.update(dt, time);
    G.scene.update(time);
    E.fireQueuedBursts();
    Audio.update(dt);

    G.renderer.renderScene(G.projection, G.camera, G.scene);
    app.draw();

    setStatus(
      `Neon Pulse — ${Game.state} | score ${Game.score} | lives ${Game.lives} | L${Game.level}`
    );
    requestAnimationFrame(frame);
  }

  requestAnimationFrame(frame);
}

function fitProjection(G, C, width, height) {
  const aspect = width / height;
  const cam = C.camera;
  let fov = cam.fov;
  const halfW = Math.tan((fov * 0.5 * Math.PI) / 180) * cam.planeDist * aspect;
  if (halfW < cam.minHalfW) {
    fov = ((Math.atan(cam.minHalfW / (cam.planeDist * aspect)) * 180) / Math.PI) * 2;
  }
  G.projection.perspective(fov, aspect, cam.near, cam.far);
}
