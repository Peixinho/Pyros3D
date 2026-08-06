/** Neon Pulse tunables — port of assets/neonpulse/config.lua */

export function createConfig(P) {
  const rgb = (r, g, b, a = 1) => new P.Vec4(r, g, b, a);

  return {
    // Deferred second-pass / G-buffer path still needs WebGL polish; Forward
    // matches the working cube demo and keeps Neon Pulse playable.
    renderer: "forward",
    deferredLightGain: 3.14159,

    arena: {
      halfW: 84,
      topY: 124,
      deadY: -14,
      wallT: 6,
      depth: 12,
      panelZ: -14,
      playZ: 0,
      ballZ: 2,
      hudZ: 6,
    },

    paddle: {
      y: 11,
      width: 30,
      height: 4.4,
      depth: 7,
      speed: 165,
      wideMul: 1.6,
      maxBounce: 1.15,
    },

    ball: {
      radius: 3.0,
      baseSpeed: 92,
      speedPerHit: 1.006,
      maxSpeed: 190,
      slowMul: 0.72,
      maxCount: 3,
      minVertical: 0.22,
    },

    bricks: {
      cols: 11,
      rows: 6,
      width: 13,
      height: 6,
      depth: 6,
      gapX: 2.2,
      gapY: 2.6,
      topRowY: 118,
      armouredRows: 2,
      popTime: 0.16,
      flashTime: 0.09,
    },

    powerup: {
      size: 7,
      fallSpeed: 46,
      chance: 0.16,
      poolSize: 6,
      wideTime: 14,
      slowTime: 9,
    },

    color: {
      background: rgb(0.02, 0.027, 0.075),
      panel: rgb(0.055, 0.07, 0.15),
      grid: rgb(0.075, 0.13, 0.265),
      wall: rgb(0.11, 0.135, 0.29),
      trim: rgb(0.35, 0.9, 1.0),
      paddle: rgb(0.6, 0.92, 1.0),
      paddleGlow: rgb(0.75, 1.0, 1.0),
      ball: rgb(0.88, 1.0, 1.0),
      ballLight: rgb(0.45, 0.85, 1.0),
      armoured: rgb(0.86, 0.92, 1.0),
      hud: rgb(0.62, 0.88, 1.0),
      hudDim: rgb(0.3, 0.44, 0.62),
      flash: rgb(1.0, 1.0, 1.0),
      rows: [
        rgb(0.72, 0.42, 1.0),
        rgb(0.48, 0.48, 1.0),
        rgb(0.3, 0.58, 1.0),
        rgb(0.2, 0.76, 1.0),
        rgb(0.18, 0.92, 0.92),
        rgb(0.28, 1.0, 0.72),
      ],
      powerup: {
        wide: rgb(0.3, 1.0, 0.78),
        slow: rgb(0.4, 0.7, 1.0),
        multi: rgb(0.78, 0.48, 1.0),
      },
    },

    audio: {
      master: 0.85,
      minDistance: 90,
      maxDistance: 340,
      volume: {
        paddle: 0.55,
        wall: 0.35,
        brick: 0.6,
        armoured: 0.5,
        powerup: 0.7,
        lost: 0.7,
        levelclear: 0.8,
        launch: 0.5,
      },
      ambienceVolume: 0.35,
      humVolume: 0.3,
      humPitchAtBaseSpeed: 0.85,
      humPitchAtMaxSpeed: 1.55,
      voices: { brick: 6, wall: 4, paddle: 3, default: 2 },
    },

    hud: { pullIn: 0.55 },

    score: {
      perRow: [90, 75, 60, 45, 30, 20],
      armouredBonus: 25,
      powerupCaught: 50,
      lives: 3,
    },

    camera: {
      pos: new P.Vec3(0, 72, 165),
      lookAt: new P.Vec3(0, 57, 0),
      fov: 55,
      near: 1,
      far: 600,
      planeDist: 178,
      minHalfW: 100,
    },
  };
}
