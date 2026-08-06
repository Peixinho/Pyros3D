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

  if (C.renderer === "deferred") {
    buildDeferredRenderer(P, G, width, height);
  } else {
    G.renderer = new P.ForwardRenderer(width, height);
  }

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

    G.renderer.preRender(G.camera, G.scene);
    G.renderer.renderScene(G.projection, G.camera, G.scene);
    app.draw();

    setStatus(
      `Neon Pulse — ${Game.state} | score ${Game.score} | lives ${Game.lives} | L${Game.level} (${C.renderer})`
    );
    requestAnimationFrame(frame);
  }

  requestAnimationFrame(frame);
}

function buildDeferredRenderer(P, G, width, height) {
  const target = (dataType) => {
    const t = new P.Texture();
    t.createEmptyTextureFull(
      P.TextureType_Texture,
      dataType,
      width,
      height,
      false,
      0,
      0
    );
    t.setRepeat3(
      P.TextureRepeat_ClampToEdge,
      P.TextureRepeat_ClampToEdge,
      P.TextureRepeat_ClampToEdge
    );
    return t;
  };

  G.gbuffer = {
    depth: target(P.TextureDataType_DepthComponent),
    albedo: target(P.TextureDataType_RGBA),
    specular: target(P.TextureDataType_RGBA),
    normal: target(P.TextureDataType_RGBA32F),
    matrough: target(P.TextureDataType_RGBA),
  };

  G.gbufferFBO = new P.FrameBuffer();
  G.gbufferFBO.initTex(
    P.FrameBufferAttachmentFormat_Depth_Attachment,
    P.TextureType_Texture,
    G.gbuffer.depth
  );
  G.gbufferFBO.addAttachTex(
    P.FrameBufferAttachmentFormat_Color_Attachment0,
    P.TextureType_Texture,
    G.gbuffer.albedo
  );
  G.gbufferFBO.addAttachTex(
    P.FrameBufferAttachmentFormat_Color_Attachment1,
    P.TextureType_Texture,
    G.gbuffer.specular
  );
  G.gbufferFBO.addAttachTex(
    P.FrameBufferAttachmentFormat_Color_Attachment2,
    P.TextureType_Texture,
    G.gbuffer.normal
  );
  G.gbufferFBO.addAttachTex(
    P.FrameBufferAttachmentFormat_Color_Attachment3,
    P.TextureType_Texture,
    G.gbuffer.matrough
  );

  G.renderer = new P.DeferredRenderer(width, height, G.gbufferFBO);
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
