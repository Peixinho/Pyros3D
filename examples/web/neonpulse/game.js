/** Neon Pulse game logic — port of assets/neonpulse/game.lua */

export function createGame(P, G, C, Arena, E, Audio, screen) {
  const Game = {
    state: "title",
    stateTime: 0,
    score: 0,
    lives: C.score.lives,
    level: 1,
    remaining: 0,
    speedScale: 1,
    slowTimer: 0,
    shake: 0,
    keys: { left: false, right: false },
    mouseX: null,
  };

  const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
  const abs = Math.abs;

  const BANNERS = {
    title: ["NEON PULSE", "arrows or mouse to move - space to begin"],
    serve: ["READY", "space to launch"],
    over: ["GAME OVER", "space to play again"],
  };

  function setState(state) {
    Game.state = state;
    Game.stateTime = 0;
  }

  function start() {
    Game.score = 0;
    Game.lives = C.score.lives;
    Game.level = 1;
    Game.speedScale = 1;
    Game.slowTimer = 0;
    startLevel();
  }

  function startLevel() {
    Game.remaining = E.loadLevel(Game.level);
    for (const p of E.powerups) E.despawnPowerup(p);
    for (const b of E.balls) E.deactivateBall(b);
    E.setPaddleWidth(1);
    E.paddle.wideTimer = 0;
    Game.slowTimer = 0;
    E.movePaddle(0);
    setState("serve");
  }

  function ballSpeed() {
    let s = C.ball.baseSpeed * Game.speedScale;
    if (Game.slowTimer > 0) s *= C.ball.slowMul;
    return Math.min(s, C.ball.maxSpeed);
  }

  function launch() {
    const speed = ballSpeed();
    const ang = (Math.random() - 0.5) * 0.5;
    E.activateBall(
      E.balls[0],
      E.paddle.x,
      C.paddle.y + E.paddle.halfH + C.ball.radius + 0.5,
      Math.sin(ang) * speed,
      Math.cos(ang) * speed
    );
    Audio.playAt("launch", E.paddle.x, C.paddle.y);
    setState("play");
  }

  function hitBox(cx, cy, r, bx, by, hw, hh) {
    const dx = cx - bx;
    const dy = cy - by;
    const px = hw + r - abs(dx);
    const py = hh + r - abs(dy);
    if (px <= 0 || py <= 0) return null;
    if (px < py) return { side: dx < 0 ? "left" : "right", depth: px };
    return { side: dy < 0 ? "bottom" : "top", depth: py };
  }

  function enforceVertical(b) {
    const speed = Math.hypot(b.vx, b.vy);
    if (speed <= 0) return;
    const minVY = speed * C.ball.minVertical;
    if (abs(b.vy) < minVY) {
      b.vy = b.vy < 0 ? -minVY : minVY;
      const vx2 = speed * speed - b.vy * b.vy;
      b.vx = (b.vx < 0 ? -1 : 1) * Math.sqrt(Math.max(vx2, 0));
    }
  }

  function hitBrick(brick) {
    brick.hp -= 1;
    const rowColor = C.color.rows[brick.row];
    if (brick.hp > 0) {
      brick.mat.setColor(C.color.flash);
      brick.flash = C.bricks.flashTime;
      E.burst(brick.x, brick.y, C.color.armoured, 0.6);
      Audio.playAt("armoured", brick.x, brick.y);
      Game.score += C.score.armouredBonus;
      Game.shake = Math.max(Game.shake, 0.5);
      return;
    }
    brick.alive = false;
    brick.pop = C.bricks.popTime;
    Game.remaining -= 1;
    Game.score += C.score.perRow[brick.row];
    Game.shake = Math.max(Game.shake, 1.0);
    E.burst(brick.x, brick.y, rowColor, 1.0);
    Audio.playAt("brick", brick.x, brick.y, 0.92 + 0.05 * (C.bricks.rows - brick.row));
    Game.speedScale = Math.min(
      Game.speedScale * C.ball.speedPerHit,
      C.ball.maxSpeed / C.ball.baseSpeed
    );
    if (Math.random() < C.powerup.chance) E.spawnPowerup(brick.x, brick.y);
  }

  function stepBall(b, dt) {
    const a = C.arena;
    const r = C.ball.radius;
    let speed = Math.hypot(b.vx, b.vy);
    const want = ballSpeed();
    if (speed > 0.001 && abs(speed - want) > 0.5) {
      b.vx = (b.vx / speed) * want;
      b.vy = (b.vy / speed) * want;
      speed = want;
    }

    let steps = Math.ceil((speed * dt) / (r * 0.7));
    steps = clamp(steps, 1, 8);
    const sdt = dt / steps;

    for (let step = 0; step < steps; step++) {
      b.x += b.vx * sdt;
      b.y += b.vy * sdt;

      if (b.x < -a.halfW + r) {
        b.x = -a.halfW + r;
        b.vx = abs(b.vx);
        Audio.playAt("wall", b.x, b.y);
      } else if (b.x > a.halfW - r) {
        b.x = a.halfW - r;
        b.vx = -abs(b.vx);
        Audio.playAt("wall", b.x, b.y);
      }
      if (b.y > a.topY - r) {
        b.y = a.topY - r;
        b.vy = -abs(b.vy);
        Audio.playAt("wall", b.x, b.y);
      }
      if (b.y < a.deadY) return false;

      if (b.vy < 0) {
        const hit = hitBox(b.x, b.y, r, E.paddle.x, C.paddle.y, E.paddle.halfW, E.paddle.halfH);
        if (hit) {
          const t = clamp((b.x - E.paddle.x) / (E.paddle.halfW + r), -1, 1);
          const ang = t * C.paddle.maxBounce;
          b.vx = Math.sin(ang) * speed;
          b.vy = Math.cos(ang) * speed;
          b.y = C.paddle.y + E.paddle.halfH + r + 0.02;
          E.burst(b.x, C.paddle.y + E.paddle.halfH, C.color.paddleGlow, 0.45);
          Game.shake = Math.max(Game.shake, 0.35);
          Audio.playAt("paddle", b.x, C.paddle.y, 1.0 + t * 0.18);
        }
      }

      for (const brick of E.bricks) {
        if (!brick.alive) continue;
        const hit = hitBox(b.x, b.y, r, brick.x, brick.y, brick.halfW, brick.halfH);
        if (hit) {
          if (hit.side === "left") {
            b.x -= hit.depth;
            b.vx = -abs(b.vx);
          } else if (hit.side === "right") {
            b.x += hit.depth;
            b.vx = abs(b.vx);
          } else if (hit.side === "bottom") {
            b.y -= hit.depth;
            b.vy = -abs(b.vy);
          } else {
            b.y += hit.depth;
            b.vy = abs(b.vy);
          }
          hitBrick(brick);
          break;
        }
      }
      enforceVertical(b);
    }

    b.go.setPosition(new P.Vec3(b.x, b.y, a.ballZ));
    const span = C.ball.maxSpeed - C.ball.baseSpeed;
    Audio.setHumSpeed(b.hum, span > 0 ? clamp((speed - C.ball.baseSpeed) / span, 0, 1) : 0);
    return true;
  }

  function applyPowerup(kind) {
    Game.score += C.score.powerupCaught;
    if (kind === "wide") {
      E.setPaddleWidth(C.paddle.wideMul);
      E.paddle.wideTimer = C.powerup.wideTime;
    } else if (kind === "slow") {
      Game.slowTimer = C.powerup.slowTime;
    } else if (kind === "multi") {
      let source = null;
      for (const b of E.balls) {
        if (b.active) {
          source = b;
          break;
        }
      }
      if (source) {
        const speed = ballSpeed();
        const base = Math.atan2(source.vx, source.vy);
        const spread = [0.55, -0.55];
        let slot = 0;
        for (const b of E.balls) {
          if (!b.active && slot < spread.length) {
            const ang = base + spread[slot++];
            E.activateBall(b, source.x, source.y, Math.sin(ang) * speed, Math.cos(ang) * speed);
          }
        }
      }
    }
  }

  function updatePowerups(dt) {
    for (const p of E.powerups) {
      if (!p.active) continue;
      p.y -= C.powerup.fallSpeed * dt;
      p.spin += dt * 3.0;
      p.go.setPosition(new P.Vec3(p.x, p.y, C.arena.playZ));
      p.go.setRotation(new P.Vec3(p.spin * 0.7, p.spin, 0));

      const half = C.powerup.size * 0.5;
      const caughtX = abs(p.x - E.paddle.x) < E.paddle.halfW + half;
      const caughtY = abs(p.y - C.paddle.y) < E.paddle.halfH + half;
      if (caughtX && caughtY) {
        E.burst(p.x, p.y, C.color.powerup[p.kind], 1.2);
        Game.shake = Math.max(Game.shake, 0.8);
        Audio.playAt("powerup", p.x, p.y);
        applyPowerup(p.kind);
        E.despawnPowerup(p);
      } else if (p.y < C.arena.deadY) {
        E.despawnPowerup(p);
      }
    }
  }

  function confirm() {
    Audio.ensureReady(E);
    if (Game.state === "title" || Game.state === "over") start();
    else if (Game.state === "serve") launch();
  }

  function bindInput(canvas) {
    const onKey = (e, down) => {
      const k = e.code;
      if (k === "ArrowLeft" || k === "KeyA") Game.keys.left = down;
      if (k === "ArrowRight" || k === "KeyD") Game.keys.right = down;
      if (down && (k === "Space" || k === "Enter" || k === "NumpadEnter")) {
        e.preventDefault();
        confirm();
      }
    };
    window.addEventListener("keydown", (e) => onKey(e, true));
    window.addEventListener("keyup", (e) => onKey(e, false));
    canvas.addEventListener("mousedown", (e) => {
      if (e.button === 0) confirm();
    });
    canvas.addEventListener("mousemove", (e) => {
      const rect = canvas.getBoundingClientRect();
      const t = clamp((e.clientX - rect.left) / rect.width, 0, 1);
      Game.mouseX = (t - 0.5) * (C.arena.halfW * 2.15);
    });
    canvas.tabIndex = 0;
    canvas.focus();
  }

  function updatePaddle(dt) {
    let moved = false;
    let x = E.paddle.x;
    if (Game.keys.left !== Game.keys.right) {
      x += (Game.keys.left ? -1 : 1) * C.paddle.speed * dt;
      moved = true;
      Game.mouseX = null;
    } else if (Game.mouseX != null) {
      x = Game.mouseX;
      moved = true;
    }
    if (moved) E.movePaddle(x);
  }

  function updateHUD() {
    E.setLabel(E.hud.score, `SCORE ${Game.score}`);
    E.setLabel(E.hud.level, `LEVEL ${Game.level}`);
    E.setLabel(E.hud.lives, `LIVES ${Game.lives}`);

    if (Game.state === "clear") {
      E.setLabel(E.hud.banner, `LEVEL ${Game.level} CLEAR`);
      E.setLabel(E.hud.hint, null);
    } else if (Game.state === "lost") {
      E.setLabel(E.hud.banner, "BALL LOST");
      E.setLabel(E.hud.hint, `${Game.lives} left`);
    } else {
      const banner = BANNERS[Game.state];
      E.setLabel(E.hud.banner, banner ? banner[0] : null);
      E.setLabel(E.hud.hint, banner ? banner[1] : null);
    }
  }

  function updateCamera(dt, time) {
    Game.shake = Math.max(0, Game.shake - dt * 4.0);
    const base = C.camera.pos;
    const s = Game.shake * Game.shake * 2.2;
    const ox = (Math.random() - 0.5) * s;
    const oy = (Math.random() - 0.5) * s;
    const dx = Math.sin(time * 0.23) * 1.6;
    const dy = Math.cos(time * 0.17) * 1.0;
    G.camera.setPosition(new P.Vec3(base.x + ox + dx, base.y + oy + dy, base.z));
    G.camera.lookAtVec(C.camera.lookAt);
  }

  function update(dt, time) {
    Game.stateTime += dt;
    updatePaddle(dt);

    if (E.paddle.wideTimer > 0) {
      E.paddle.wideTimer -= dt;
      if (E.paddle.wideTimer <= 0) E.setPaddleWidth(1);
    }
    if (Game.slowTimer > 0) Game.slowTimer -= dt;

    if (Game.state === "serve") {
      const b = E.balls[0];
      if (!b.active) {
        E.activateBall(b, E.paddle.x, C.paddle.y + E.paddle.halfH + C.ball.radius + 0.5, 0, 0);
      }
      b.x = E.paddle.x;
      b.y = C.paddle.y + E.paddle.halfH + C.ball.radius + 0.5;
      b.go.setPosition(new P.Vec3(b.x, b.y, C.arena.ballZ));
    } else if (Game.state === "play") {
      for (const b of E.balls) {
        if (b.active && !stepBall(b, dt)) {
          E.burst(b.x, C.arena.deadY + 4, new P.Vec4(0.45, 0.55, 1.0, 1), 1.4);
          E.deactivateBall(b);
          Audio.play("lost");
        }
      }
      if (Game.remaining <= 0) {
        for (const b of E.balls) E.deactivateBall(b);
        Game.shake = 1.4;
        Audio.play("levelclear");
        setState("clear");
      } else if (E.activeBallCount() === 0) {
        Game.lives -= 1;
        Game.shake = 1.2;
        setState(Game.lives > 0 ? "lost" : "over");
      }
    } else if (Game.state === "lost") {
      if (Game.stateTime > 1.1) {
        E.setPaddleWidth(1);
        E.paddle.wideTimer = 0;
        Game.slowTimer = 0;
        setState("serve");
      }
    } else if (Game.state === "clear") {
      if (Game.stateTime > 1.8) {
        Game.level += 1;
        Game.speedScale = 1 + 0.12 * Math.floor((Game.level - 1) / E.levels.length);
        startLevel();
      }
    }

    updatePowerups(dt);
    E.updateBricks(dt);
    updateHUD();
    updateCamera(dt, time);
    Arena.update(time);
  }

  Game.setState = setState;
  Game.start = start;
  Game.startLevel = startLevel;
  Game.bindInput = bindInput;
  Game.update = update;
  return Game;
}
