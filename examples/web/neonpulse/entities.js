/** Neon Pulse entities — port of assets/neonpulse/entities.lua */

const PARK_Z = -4000;

export function createEntities(P, G, C, Arena, Audio) {
  const E = { keep: [] };

  function keep(obj) {
    E.keep.push(obj);
    return obj;
  }

  E.levels = [
    ["XXXXXXXXXXX", "XXXXXXXXXXX", "xxxxxxxxxxx", "xxxxxxxxxxx", "xxxxxxxxxxx", "xxxxxxxxxxx"],
    ["XX.XXXXX.XX", "XX.XXXXX.XX", "xxxxxxxxxxx", "x.x.x.x.x.x", "xxxxxxxxxxx", ".xx.xxx.xx."],
    [".....X.....", "....XXX....", "...xxxxx...", "..xxxxxxx..", ".xxxxxxxxx.", "xxxxxxxxxxx"],
    ["X.X.X.X.X.X", ".x.x.x.x.x.", "X.X.X.X.X.X", ".x.x.x.x.x.", "xxx.xxx.xxx", ".x.x.x.x.x."],
  ];

  E.POWERUP_KINDS = ["wide", "slow", "multi"];

  function buildPaddle() {
    const p = C.paddle;
    const a = C.arena;
    const go = new P.GameObject();
    const mesh = keep(Arena.cube(p.width, p.height, p.depth));
    const mat = Arena.lit(C.color.paddle, 70);
    const rc = new P.RenderingComponent(mesh, mat);
    go.addRenderingComponent(rc);
    go.setPosition(new P.Vec3(0, p.y, a.playZ));
    G.scene.add(go);
    keep(go);
    keep(rc);

    const glowGO = new P.GameObject();
    const glowMesh = keep(Arena.cube(p.width - 2, 1.1, p.depth + 0.6));
    const glowRC = new P.RenderingComponent(glowMesh, Arena.unlit(C.color.paddleGlow));
    glowGO.addRenderingComponent(glowRC);
    glowGO.setPosition(new P.Vec3(0, p.height * 0.5 + 0.2, 0));
    go.addGameObject(glowGO);
    G.scene.add(glowGO);
    keep(glowGO);
    keep(glowRC);

    E.paddle = {
      go,
      glow: glowGO,
      x: 0,
      halfW: p.width * 0.5,
      halfH: p.height * 0.5,
      scale: 1,
      wideTimer: 0,
    };
  }

  function setPaddleWidth(scale) {
    E.paddle.scale = scale;
    E.paddle.halfW = C.paddle.width * 0.5 * scale;
    E.paddle.go.setScale(new P.Vec3(scale, 1, 1));
  }

  function movePaddle(x) {
    const limit = C.arena.halfW - E.paddle.halfW;
    if (x < -limit) x = -limit;
    if (x > limit) x = limit;
    E.paddle.x = x;
    E.paddle.go.setPosition(new P.Vec3(x, C.paddle.y, C.arena.playZ));
  }

  function buildBalls() {
    E.balls = [];
    for (let i = 0; i < C.ball.maxCount; i++) {
      const go = new P.GameObject();
      const mesh = keep(new P.Sphere(C.ball.radius, 22, 22));
      const rc = new P.RenderingComponent(mesh, Arena.unlit(C.color.ball));
      go.addRenderingComponent(rc);

      const light = new P.PointLight(C.color.ballLight, 120);
      light.setLightIntensity(1.5 * Arena.lightGain);
      go.addPointLight(light);

      const d = new P.ParticleSystemDesc();
      d.maxParticles = 96;
      d.texture = G.sprite;
      d.looping = true;
      d.emissionRate = 55;
      d.burstCount = 1;
      d.minLifetime = 0.3;
      d.maxLifetime = 0.48;
      d.direction = new P.Vec3(0, 0, 1);
      d.spreadAngle = 3.14;
      d.minSpeed = 0;
      d.maxSpeed = 5;
      d.gravity = new P.Vec3(0, 0, 0);
      d.damping = 1.6;
      d.startSize = 5.5;
      d.endSize = 0.4;
      d.startColor = new P.Vec4(0.45, 0.95, 1.0, 1);
      d.endColor = new P.Vec4(0.25, 0.35, 1.0, 0);
      d.fadeInFraction = 0.05;
      d.fadeOutFraction = 0.35;
      d.blendMode = P.ParticleBlendMode_Additive;
      const trail = new P.ParticleSystem(d);
      go.addParticleSystem(trail);

      const hum = null; // attached in Audio.ensureReady() after user gesture
      // (browser autoplay policy — creating AudioSource too early fails silently)

      G.scene.add(go);
      keep(go);
      keep(rc);
      keep(light);
      keep(trail);
      keep(d);

      const ball = { go, light, trail, hum, x: 0, y: 0, vx: 0, vy: 0, active: false };
      E.balls.push(ball);
      deactivateBall(ball);
    }
  }

  function activateBall(b, x, y, vx, vy) {
    b.active = true;
    b.x = x;
    b.y = y;
    b.vx = vx;
    b.vy = vy;
    b.go.setPosition(new P.Vec3(x, y, C.arena.ballZ));
    b.go.refreshTransformation();
    b.light.setLightIntensity(1.5 * Arena.lightGain);
    b.trail.play();
    if (b.hum) {
      b.hum.resetVelocityTracking();
      b.hum.setVolume(C.audio.humVolume);
      b.hum.play();
    }
  }

  function deactivateBall(b) {
    b.active = false;
    b.go.setPosition(new P.Vec3(0, 0, PARK_Z));
    b.go.refreshTransformation();
    b.light.setLightIntensity(0);
    b.trail.stop();
    b.trail.clear();
    if (b.hum) {
      b.hum.stop();
      b.hum.resetVelocityTracking();
    }
  }

  function activeBallCount() {
    let n = 0;
    for (const b of E.balls) if (b.active) n++;
    return n;
  }

  function buildBricks() {
    const b = C.bricks;
    E.bricks = [];
    const stepX = b.width + b.gapX;
    const stepY = b.height + b.gapY;
    const originX = -((b.cols - 1) * stepX) * 0.5;

    for (let row = 0; row < b.rows; row++) {
      for (let colIdx = 0; colIdx < b.cols; colIdx++) {
        const go = new P.GameObject();
        const mesh = keep(Arena.cube(b.width, b.height, b.depth));
        const mat = Arena.lit(C.color.rows[row], 34);
        const rc = new P.RenderingComponent(mesh, mat);
        go.addRenderingComponent(rc);
        G.scene.add(go);
        keep(go);
        keep(rc);
        E.bricks.push({
          go,
          mat,
          row,
          x: originX + colIdx * stepX,
          y: b.topRowY - row * stepY,
          halfW: b.width * 0.5,
          halfH: b.height * 0.5,
          alive: false,
          hp: 0,
          flash: 0,
          pop: 0,
        });
      }
    }
  }

  function loadLevel(index) {
    const layout = E.levels[(index - 1) % E.levels.length];
    const b = C.bricks;
    let remaining = 0;
    let i = 0;
    for (let row = 0; row < b.rows; row++) {
      for (let colIdx = 0; colIdx < b.cols; colIdx++) {
        const brick = E.bricks[i++];
        const ch = layout[row][colIdx];
        if (ch === "x" || ch === "X") {
          brick.alive = true;
          brick.hp = ch === "X" ? 2 : 1;
          brick.flash = 0;
          brick.pop = 0;
          brick.go.setScale(new P.Vec3(1, 1, 1));
          brick.go.setRotation(new P.Vec3(0, 0, 0));
          brick.go.setPosition(new P.Vec3(brick.x, brick.y, C.arena.playZ));
          brick.mat.setColor(brick.hp > 1 ? C.color.armoured : C.color.rows[row]);
          remaining++;
        } else {
          brick.alive = false;
          brick.pop = 0;
          brick.go.setPosition(new P.Vec3(0, 0, PARK_Z));
        }
      }
    }
    return remaining;
  }

  function updateBricks(dt) {
    for (const brick of E.bricks) {
      if (brick.flash > 0) {
        brick.flash -= dt;
        if (brick.flash <= 0) {
          brick.mat.setColor(brick.hp > 1 ? C.color.armoured : C.color.rows[brick.row]);
        }
      }
      if (brick.pop > 0) {
        brick.pop -= dt;
        if (brick.pop <= 0) {
          brick.go.setScale(new P.Vec3(1, 1, 1));
          brick.go.setRotation(new P.Vec3(0, 0, 0));
          brick.go.setPosition(new P.Vec3(0, 0, PARK_Z));
        } else {
          const t = brick.pop / C.bricks.popTime;
          brick.go.setScale(new P.Vec3(t, t, t));
          brick.go.setRotation(new P.Vec3(0, 0, (1 - t) * 2.4));
        }
      }
    }
  }

  function buildPowerups() {
    E.powerups = [];
    const s = C.powerup.size;
    for (let i = 0; i < C.powerup.poolSize; i++) {
      const go = new P.GameObject();
      const mesh = keep(Arena.cube(s, s, s));
      const mat = Arena.lit(C.color.powerup.wide, 90);
      const rc = new P.RenderingComponent(mesh, mat);
      go.addRenderingComponent(rc);
      go.setPosition(new P.Vec3(0, 0, PARK_Z));
      G.scene.add(go);
      keep(go);
      keep(rc);
      E.powerups.push({ go, mat, kind: "wide", x: 0, y: 0, spin: 0, active: false });
    }
  }

  function spawnPowerup(x, y) {
    for (const p of E.powerups) {
      if (!p.active) {
        p.kind = E.POWERUP_KINDS[Math.floor(Math.random() * E.POWERUP_KINDS.length)];
        p.mat.setColor(C.color.powerup[p.kind]);
        p.x = x;
        p.y = y;
        p.spin = 0;
        p.active = true;
        p.go.setPosition(new P.Vec3(x, y, C.arena.playZ));
        return p;
      }
    }
    return null;
  }

  function despawnPowerup(p) {
    p.active = false;
    p.go.setPosition(new P.Vec3(0, 0, PARK_Z));
  }

  function buildFX() {
    E.fx = { pool: [], next: 0, armed: [] };
    for (let i = 0; i < 10; i++) {
      const go = new P.GameObject();
      const d = new P.ParticleSystemDesc();
      d.maxParticles = 34;
      d.texture = G.sprite;
      d.looping = false;
      d.burstCount = 26;
      d.minLifetime = 0.28;
      d.maxLifetime = 0.6;
      d.direction = new P.Vec3(0, 0, 1);
      d.spreadAngle = 3.14;
      d.minSpeed = 26;
      d.maxSpeed = 78;
      d.gravity = new P.Vec3(0, -55, 0);
      d.damping = 1.1;
      d.startSize = 6.0;
      d.endSize = 0.5;
      d.fadeInFraction = 0.02;
      d.fadeOutFraction = 0.25;
      d.blendMode = P.ParticleBlendMode_Additive;
      const ps = new P.ParticleSystem(d);
      go.addParticleSystem(ps);
      G.scene.add(go);
      keep(go);
      keep(ps);
      keep(d);
      E.fx.pool.push({ go, ps });
    }
  }

  function burst(x, y, color, scale = 1) {
    const slot = E.fx.pool[E.fx.next];
    E.fx.next = (E.fx.next + 1) % E.fx.pool.length;
    slot.go.setPosition(new P.Vec3(x, y, C.arena.playZ + 3));
    slot.ps.setColors(color, new P.Vec4(color.x * 0.4, color.y * 0.4, color.z * 0.9, 0));
    slot.ps.setSizes(6.0 * scale, 0.5, 0.35);
    E.fx.armed.push(slot);
  }

  function fireQueuedBursts() {
    for (const slot of E.fx.armed) {
      slot.go.refreshTransformation();
      slot.ps.play();
    }
    E.fx.armed.length = 0;
  }

  function pullToward(x, y, z, k) {
    const eye = C.camera.pos;
    return new P.Vec3(
      eye.x + (x - eye.x) * k,
      eye.y + (y - eye.y) * k,
      eye.z + (z - eye.z) * k
    );
  }

  function placeLabel(entry) {
    let x = entry.x;
    if (entry.centred) x = x - entry.text.length * entry.size * 0.3;
    entry.go.setPosition(pullToward(x, entry.y, C.arena.hudZ, C.hud.pullIn));
  }

  function setLabel(entry, text) {
    if (text == null || text === "") {
      if (!entry.hidden) {
        entry.hidden = true;
        entry.go.setPosition(new P.Vec3(0, 0, PARK_Z));
      }
      return;
    }
    if (entry.text !== text) {
      entry.text = text;
      entry.mesh.updateText(text, entry.color);
    } else if (!entry.hidden) {
      return;
    }
    entry.hidden = false;
    placeLabel(entry);
  }

  function buildHUD() {
    const a = C.arena;
    G.font = new P.Font("assets/verdana.ttf", 48);
    G.font.createText(
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .,:!?-+*/()[]<>'"
    );

    const mat = new P.GenericShaderMaterial(P.ShaderUsage_TextRendering);
    mat.setTextFont(G.font);
    keep(mat);
    E.hudMat = mat;

    function label(text, x, y, size, color, centred = false) {
      const mesh = keep(
        new P.Text(G.font, text, size * C.hud.pullIn, size * C.hud.pullIn, color, true)
      );
      const go = new P.GameObject();
      const rc = new P.RenderingComponent(mesh, mat);
      go.addRenderingComponent(rc);
      G.scene.add(go);
      keep(go);
      keep(rc);
      const entry = { go, mesh, text, color, x, y, size, centred, hidden: false };
      placeLabel(entry);
      return entry;
    }

    E.hud = {
      score: label("SCORE 0", -a.halfW + 3, a.deadY - 6, 8, C.color.hud),
      level: label("LEVEL 1", -11, a.deadY - 6, 8, C.color.hudDim),
      lives: label("LIVES 3", a.halfW - 40, a.deadY - 6, 8, C.color.hud),
      banner: label("NEON PULSE", 0, 52, 15, C.color.hud, true),
      hint: label("space or click to begin", 0, 38, 6, C.color.hudDim, true),
    };
  }

  function build() {
    buildPaddle();
    buildBalls();
    buildBricks();
    buildPowerups();
    buildFX();
    buildHUD();
  }

  E.build = build;
  E.setPaddleWidth = setPaddleWidth;
  E.movePaddle = movePaddle;
  E.activateBall = activateBall;
  E.deactivateBall = deactivateBall;
  E.activeBallCount = activeBallCount;
  E.loadLevel = loadLevel;
  E.updateBricks = updateBricks;
  E.spawnPowerup = spawnPowerup;
  E.despawnPowerup = despawnPowerup;
  E.burst = burst;
  E.fireQueuedBursts = fireQueuedBursts;
  E.setLabel = setLabel;
  E.placeLabel = placeLabel;
  return E;
}
