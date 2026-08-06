// Neon Pulse — JS API port of the Lua game (assets/neonpulse).

const statusEl = document.getElementById("status");
const canvas = document.getElementById("canvas");
const createPyros3D = globalThis.createPyros3D;

function setStatus(msg) {
  if (statusEl) statusEl.textContent = msg;
}

async function main() {
  if (typeof createPyros3D !== "function") {
    setStatus("createPyros3D not found — build the Pyros3D emscripten target first.");
    return;
  }

  setStatus("Initializing WebAssembly…");
  const P = await createPyros3D({
    canvas,
    locateFile: (path) => `../${path}`,
  });

  const app = new P.Application(
    canvas.width,
    canvas.height,
    "Neon Pulse",
    P.WindowType_Close | P.WindowType_Resize
  );
  app.init();

  const { startNeonPulse } = await import("./neonpulse/main.js");
  await startNeonPulse(P, app, canvas, setStatus);
}

main().catch((err) => {
  console.error(err);
  setStatus(String(err && err.message ? err.message : err));
});
