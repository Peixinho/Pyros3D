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

  return P;
}
