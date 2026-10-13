/**
 * HTTP asset helpers for the Pyros3D Embind library.
 * Install once after createPyros3D() — no VFS / .data required for textures.
 *
 *   const P = await createPyros3D({ canvas, locateFile: … });
 *   installPyrosAssets(P);
 *   const tex = new P.Texture();
 *   await tex.loadFromUrl("./assets/smoke.png");
 */

export function installPyrosAssets(P) {
  if (!P || !P.Texture) {
    throw new Error("installPyrosAssets: pass the createPyros3D module (P)");
  }
  if (P.__pyrosAssetsInstalled) return P;
  P.__pyrosAssetsInstalled = true;

  /**
   * @param {string} url relative or absolute HTTP URL
   * @param {number} [type] P.TextureType_* (default Texture)
   * @param {boolean} [mipmap=true]
   * @param {number} [level=0]
   * @returns {Promise<boolean>}
   */
  P.Texture.prototype.loadFromUrl = async function loadFromUrl(
    url,
    type,
    mipmap = true,
    level = 0
  ) {
    const res = await fetch(url);
    if (!res.ok) {
      throw new Error(`loadFromUrl: ${url} → HTTP ${res.status}`);
    }
    const bytes = new Uint8Array(await res.arrayBuffer());
    if (type === undefined || type === null) {
      return this.loadTextureFromMemory(bytes);
    }
    return this.loadTextureFromMemoryFull(bytes, type, mipmap, level);
  };

  /**
   * @param {string} url
   * @param {object} [opts]
   * @returns {Promise<InstanceType<P["Texture"]>>}
   */
  P.loadTextureFromUrl = async function loadTextureFromUrl(url, opts = {}) {
    const tex = new P.Texture();
    const ok = await tex.loadFromUrl(
      url,
      opts.type,
      opts.mipmap !== undefined ? opts.mipmap : true,
      opts.level !== undefined ? opts.level : 0
    );
    if (!ok) {
      throw new Error(`loadTextureFromUrl: decode failed for ${url}`);
    }
    return tex;
  };

  // ---- Files the engine reads by path: models, scenes, what they use ----
  //
  // A model or a scene is read from the module's own file system, so a page
  // first puts the files there. These fetch them over HTTP and do that.

  /** Fetches `url` and writes it at `path` in the module's file system. */
  P.fetchFile = async function fetchFile(url, path) {
    const res = await fetch(url);
    if (!res.ok) throw new Error(`fetchFile: ${url} → HTTP ${res.status}`);
    const bytes = new Uint8Array(await res.arrayBuffer());
    const dir = path.slice(0, path.lastIndexOf("/"));
    if (dir) P.FS.mkdirTree(dir);
    P.FS.writeFile(path, bytes);
    return path;
  };

  /**
   * A model (.p3dm) from a URL. `also` lists files it reads beside itself
   * (its textures), relative to the model's own folder.
   * @returns {Promise<InstanceType<P["Model"]>>}
   */
  P.loadModelFromUrl = async function loadModelFromUrl(url, also = []) {
    const base = url.slice(0, url.lastIndexOf("/") + 1);
    const dir = "/fetched/" + base.replace(/[^A-Za-z0-9_./-]/g, "_");
    const name = url.slice(url.lastIndexOf("/") + 1);
    await Promise.all(also.map((f) => P.fetchFile(base + f, dir + f)));
    await P.fetchFile(url, dir + name);
    return new P.Model(dir + name);
  };

  /**
   * A scene made in the editor, with everything it names under assets/.
   * `projectUrl` is the project's folder as served (it holds scenes/ and
   * assets/); `scenePath` is the scene inside it ("scenes/Plaza.json").
   * Scripts are not run - the page drives what it loads. `opts.also` adds
   * files the scene does not name itself (a model's textures, say), and
   * `opts.onProgress(done, total)` is called as files arrive.
   * @returns {Promise<string[]>} the files that could not be fetched
   */
  P.loadSceneFromUrl = async function loadSceneFromUrl(scene, projectUrl, scenePath, physics = null, opts = {}) {
    const base = projectUrl.endsWith("/") ? projectUrl : projectUrl + "/";
    const root = "/project/" + base.replace(/[^A-Za-z0-9_-]/g, "_") + "/";
    const res = await fetch(base + scenePath);
    if (!res.ok) throw new Error(`loadSceneFromUrl: ${base + scenePath} → HTTP ${res.status}`);
    const text = await res.text();
    const wanted = new Set(opts.also || []);
    const walk = (v) => {
      if (typeof v === "string") { if (/^assets\/[^\s]+\.[A-Za-z0-9]+$/.test(v)) wanted.add(v); }
      else if (Array.isArray(v)) v.forEach(walk);
      else if (v && typeof v === "object") Object.values(v).forEach(walk);
    };
    walk(JSON.parse(text));
    const missing = [];
    let done = 0;
    await Promise.all([...wanted].map(async (f) => {
      try { await P.fetchFile(base + f, root + f); } catch (e) { missing.push(f); }
      if (opts.onProgress) opts.onProgress(++done, wanted.size);
    }));
    const dir = (root + scenePath).slice(0, (root + scenePath).lastIndexOf("/"));
    P.FS.mkdirTree(dir);
    P.FS.writeFile(root + scenePath, text);
    if (!scene.load(root + scenePath, physics)) throw new Error(`loadSceneFromUrl: ${scenePath} did not load`);
    return missing;
  };

  return P;
}
