/** Neon Pulse arena — port of assets/neonpulse/arena.lua */

export function createArena(P, G, C) {
  const A = { keep: [], mat: {}, deferred: false, lightGain: 1, rims: [] };

  function keep(obj) {
    A.keep.push(obj);
    return obj;
  }

  function lit(color, shininess = 26) {
    let usage = P.ShaderUsage_Color | P.ShaderUsage_Diffuse;
    if (A.deferred) usage |= P.ShaderUsage_DeferredRenderer_Gbuffer;
    const m = new P.GenericShaderMaterial(usage);
    m.setColor(color);
    m.setSpecular(new P.Vec4(0.55, 0.75, 0.95, 1));
    m.setShininess(shininess);
    return keep(m);
  }

  function unlit(color) {
    const m = new P.GenericShaderMaterial(P.ShaderUsage_Color);
    m.setColor(color);
    if (A.deferred) {
      m.setTransparencyFlag(true);
      m.enableDepthTest(0);
    }
    return keep(m);
  }

  /** Cube ctor uses half-extents; sizes in this game are full extents. */
  function cube(w, h, d) {
    return new P.Cube(w * 0.5, h * 0.5, d * 0.5);
  }

  function box(w, h, d, x, y, z, material) {
    const mesh = keep(cube(w, h, d));
    const go = new P.GameObject();
    const rc = new P.RenderingComponent(mesh, material);
    go.addRenderingComponent(rc);
    go.setPosition(new P.Vec3(x, y, z));
    keep(go);
    keep(rc);
    G.scene.add(go);
    return go;
  }

  function build() {
    const a = C.arena;
    const col = C.color;

    A.deferred = C.renderer === "deferred";
    A.lightGain = A.deferred ? C.deferredLightGain : 1.0;
    A.lit = lit;
    A.unlit = unlit;
    A.cube = cube;

    A.mat.panel = lit(col.panel, 8);
    A.mat.wall = lit(col.wall, 40);
    A.mat.trim = unlit(col.trim);
    A.mat.grid = unlit(col.grid);

    G.renderer.setBackground(col.background);
    G.renderer.setGlobalLight(new P.Vec4(0.24, 0.26, 0.36, 1));

    const panelW = a.halfW * 2 + a.wallT * 2;
    const panelH = a.topY + 46;
    box(panelW, panelH, 4, 0, panelH * 0.5 - 24, a.panelZ, A.mat.panel);

    const gz = a.panelZ + 2.3;
    const gridTop = a.topY + 4;
    const gridBottom = -24;
    for (let i = 0; i <= 12; i++) {
      const x = -a.halfW + a.halfW * 2 * (i / 12);
      box(0.5, gridTop - gridBottom, 0.5, x, (gridTop + gridBottom) * 0.5, gz, A.mat.grid);
    }
    for (let i = 0; i <= 9; i++) {
      const y = gridBottom + (gridTop - gridBottom) * (i / 9);
      box(a.halfW * 2, 0.5, 0.5, 0, y, gz, A.mat.grid);
    }

    const wallX = a.halfW + a.wallT * 0.5;
    const wallH = a.topY + 32;
    const wallCY = (a.topY - 20) * 0.5 + 4;
    box(a.wallT, wallH, a.depth, -wallX, wallCY, a.playZ, A.mat.wall);
    box(a.wallT, wallH, a.depth, wallX, wallCY, a.playZ, A.mat.wall);
    box(a.halfW * 2 + a.wallT * 2, a.wallT, a.depth, 0, a.topY + a.wallT * 0.5, a.playZ, A.mat.wall);

    const trimZ = a.playZ + a.depth * 0.5 + 0.4;
    box(1.4, wallH, 1.4, -a.halfW + 0.7, wallCY, trimZ, A.mat.trim);
    box(1.4, wallH, 1.4, a.halfW - 0.7, wallCY, trimZ, A.mat.trim);
    box(a.halfW * 2, 1.4, 1.4, 0, a.topY - 0.7, trimZ, A.mat.trim);
    box(a.halfW * 2, 0.8, 0.8, 0, a.deadY + 2, trimZ, A.mat.grid);

    const keyObj = new P.GameObject();
    const key = new P.DirectionalLight(
      new P.Vec4(0.82, 0.88, 1.0, 1),
      new P.Vec3(-0.35, -0.75, -0.55)
    );
    key.setLightIntensity(1.0 * A.lightGain);
    keyObj.addDirectionalLight(key);
    G.scene.add(keyObj);
    keep(keyObj);
    keep(key);
    A.keyLight = key;

    A.rims = [];
    for (let i = 0; i < 2; i++) {
      const obj = new P.GameObject();
      const light = new P.PointLight(new P.Vec4(0.42, 0.24, 0.85, 1), 150);
      obj.addPointLight(light);
      obj.setPosition(new P.Vec3(i === 0 ? -a.halfW : a.halfW, a.topY - 10, 40));
      G.scene.add(obj);
      keep(obj);
      keep(light);
      A.rims.push(light);
    }
  }

  function update(time) {
    for (let i = 0; i < A.rims.length; i++) {
      const phase = time * 0.9 + i * 2.1;
      A.rims[i].setLightIntensity((0.55 + 0.3 * Math.sin(phase)) * A.lightGain);
    }
  }

  A.build = build;
  A.update = update;
  A.lit = lit;
  A.unlit = unlit;
  A.cube = cube;
  return A;
}
