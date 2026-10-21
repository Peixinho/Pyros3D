#!/usr/bin/env python3
"""Drive PyrosBuilder (the editor) headlessly, and check what it rendered.

The editor opens a loopback JSON-line command server and advertises it in
$TMPDIR/pyros3d-editor.json as {pid, port, token}. Every name in
Editor::HandleAgentCommand works (open_project, add_ui, set_renderer,
screenshot, play, save_scene, ...). This wraps that, plus the two things that
are otherwise painful: launching without racing the discovery file, and telling
whether the frame that came back is actually a render or a blank rectangle.

  driver.py launch [--build DIR]
  driver.py call <cmd> ['{"json":"args"}']
  driver.py shot <out.png> [--offscreen]
  driver.py stats <file.png>
  driver.py stop
  driver.py smoke [--project DIR] [--build DIR]

Stdlib only. No pip install.
"""

import base64
import collections
import json
import os
import signal
import socket
import struct
import subprocess
import sys
import time
import zlib

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))
DEFAULT_BUILD = os.path.join(REPO, "build_ed_vk")
# Our own copy of the handshake. The editor rewrites the shared one on every
# launch, so a second editor starting up would otherwise steal our port.
MINE = os.path.join(os.environ.get("TMPDIR", "/tmp"), "pyros3d-driver.json")


def discovery_path():
    return os.path.join(os.environ.get("TMPDIR", "/tmp"), "pyros3d-editor.json")


# ---------------------------------------------------------------- socket ----

def call(cmd, args=None, timeout=120.0):
    cfg = json.load(open(MINE))
    s = socket.create_connection(("127.0.0.1", cfg["port"]), timeout=timeout)
    s.settimeout(timeout)
    s.sendall((json.dumps({"id": 1, "token": cfg["token"],
                           "cmd": cmd, "args": args or {}}) + "\n").encode())
    buf = b""
    while not buf.endswith(b"\n"):
        chunk = s.recv(1 << 20)
        if not chunk:
            break
        buf += chunk
    s.close()
    reply = json.loads(buf.decode())
    if "error" in reply:
        raise RuntimeError("%s failed: %s" % (cmd, reply["error"]))
    return reply.get("result", reply)


# ---------------------------------------------------------------- launch ----

def launch(build=DEFAULT_BUILD, extra_env=None, wait=40):
    """Start the editor and block until *its* handshake file is on disk.

    cwd is build/editor deliberately - that directory holds the assets/ and
    shaders/ symlinks. Launched from the source editor/ dir the fonts resolve
    (so the panels and this socket all work) but the compiled shaders do not,
    and every 3D viewport renders blank. That failure looks exactly like a
    rendering regression and is not one.
    """
    build = os.path.abspath(build)     # (the editor runs from its own folder: a relative path is the caller's)
    editor_dir = os.path.join(build, "editor")
    exe = os.path.join(editor_dir, "PyrosBuilder")
    if not os.path.exists(exe):
        sys.exit("no editor at %s - build first (see SKILL.md)" % exe)

    try:
        os.remove(discovery_path())
    except OSError:
        pass

    env = dict(os.environ)
    env.update(extra_env or {})
    log = open(os.path.join(editor_dir, "driver-editor.log"), "w")
    proc = subprocess.Popen([exe], cwd=editor_dir, stdout=log, stderr=log, env=env)

    for _ in range(wait):
        time.sleep(1)
        try:
            d = json.load(open(discovery_path()))
        except Exception:
            continue
        # Match on pid: another editor starting up writes this same file.
        if d.get("pid") == proc.pid:
            json.dump(d, open(MINE, "w"))
            return d
    proc.kill()
    sys.exit("editor did not advertise itself within %ds - see %s/driver-editor.log"
             % (wait, editor_dir))


def stop():
    try:
        pid = json.load(open(MINE))["pid"]
    except Exception:
        return
    # SIGTERM does not stop PyrosBuilder. It has to be SIGKILL, and a survivor
    # then fights the next launch over the handshake file, which shows up as
    # every subsequent launch "timing out".
    try:
        os.kill(pid, signal.SIGKILL)
    except OSError:
        pass
    try:
        os.remove(MINE)
    except OSError:
        pass


# ------------------------------------------------------------------- png ----

def read_png(path):
    d = open(path, "rb").read()
    i, idat, w, h, ct = 8, b"", None, None, None
    while i < len(d):
        ln = struct.unpack(">I", d[i:i + 4])[0]
        typ, data = d[i + 4:i + 8], d[i + 8:i + 8 + ln]
        i += 12 + ln
        if typ == b"IHDR":
            w, h, _bd, ct = struct.unpack(">IIBB", data[:10])
        elif typ == b"IDAT":
            idat += data
        elif typ == b"IEND":
            break
    raw = zlib.decompress(idat)
    nch = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ct]
    stride = w * nch
    out, prev, pos = bytearray(), bytearray(stride), 0
    for _ in range(h):
        f = raw[pos]; pos += 1
        line = bytearray(raw[pos:pos + stride]); pos += stride
        for x in range(stride):
            a = line[x - nch] if x >= nch else 0
            b = prev[x]
            c = prev[x - nch] if x >= nch else 0
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pr) & 255
        out += line
        prev = line
    return w, h, nch, bytes(out)


def stats(path):
    """Top colours by share. A viewport that failed shows up here as one
    colour at ~100% - blank grey, or pure white when deferred lighting blows
    out. Assert on this rather than trusting that a PNG was produced."""
    w, h, n, px = read_png(path)
    hist = collections.Counter()
    for y in range(0, h, 2):
        for x in range(0, w, 2):
            o = (y * w + x) * n
            hist[(px[o], px[o + 1], px[o + 2])] += 1
    total = sum(hist.values())
    top = [(c, 100.0 * k / total) for c, k in hist.most_common(4)]
    return w, h, top


def shot(out, offscreen=False):
    # live=True reads the texture the Scene View is really showing this frame:
    # DeferredRenderer::GetColorTexture() when the project is deferred,
    # EffectsManager->GetViewportColor() when it is forward. Without it,
    # AgentScreenshot re-renders through previewRenderer, which Init()
    # hardcodes as a ForwardRenderer - so a deferred project silently hands
    # back a *forward* image and any Forward-vs-Deferred comparison is void.
    r = call("screenshot", {} if offscreen else {"live": True})
    open(out, "wb").write(base64.b64decode(r["pngBase64"]))
    w, h, top = stats(out)
    flat = top[0][1] > 99.0
    print("%s %dx%d %s%s" % (out, w, h,
          " ".join("%s=%.1f%%" % (c, p) for c, p in top),
          "   <-- FLAT, viewport did not render" if flat else ""))
    return not flat


# ----------------------------------------------------------------- smoke ----

def smoke(project, build=DEFAULT_BUILD):
    """Launch, open a project, render in both renderers, drive the 2D UI, stop.
    Exits non-zero if any viewport came back flat."""
    ok = True
    d = launch(build)
    print("editor pid=%d port=%d" % (d["pid"], d["port"]))
    try:
        print("ping:", call("ping"))
        r = call("open_project", {"path": os.path.abspath(project)})
        print("opened %s -> scene %s" % (r["projectPath"], r.get("scene")))
        # Do not trust project.json's activeScene: it can point at a
        # ".editor.json" camera stub (a 19-byte {"cameras":{}}) rather than a
        # real scene, and then everything below renders an empty grid and
        # still passes. Load the first real scene by name.
        scenes = [s for s in call("list_scenes")["scenes"]
                  if not s.endswith(".editor.json")]
        if scenes:
            call("load_scene", {"path": scenes[0]})
            print("loaded scene %s" % scenes[0])
        time.sleep(3)

        for kind in ("deferred", "forward"):
            call("set_renderer", {"type": kind})
            time.sleep(2)
            ok &= shot(os.path.join(build, "editor", "smoke-%s.png" % kind))

        # 2D UI: the Canvas (2D) Mode item in the View menu stays disabled
        # until the scene actually has a UICanvas, so make one.
        call("add_object", {"name": "DriverUI"})
        call("add_ui", {"object": "DriverUI", "kind": "Canvas"})
        call("add_ui", {"object": "DriverUI", "kind": "Menu"})
        call("canvas_mode", {"on": True})
        time.sleep(2)
        # Stay in forward here: in canvas mode the scene UI is drawn into
        # EffectsManager's capture for forward but into its *transparent
        # overlay* for deferred, and `screenshot live` reads the deferred
        # colour target - so a deferred canvas screenshot is blank by design.
        ok &= shot(os.path.join(build, "editor", "smoke-canvas.png"))
        call("canvas_mode", {"on": False})

        print("log tail:", call("log", {"lines": 3})["log"].strip().splitlines()[-1:])
    finally:
        stop()
    print("SMOKE", "PASS" if ok else "FAIL")
    return 0 if ok else 1


# ------------------------------------------------------------------ main ----

def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    cmd, rest = sys.argv[1], sys.argv[2:]

    def opt(flag, default):
        return rest[rest.index(flag) + 1] if flag in rest else default

    if cmd == "launch":
        d = launch(opt("--build", DEFAULT_BUILD))
        print("pid=%d port=%d" % (d["pid"], d["port"]))
    elif cmd == "stop":
        stop()
    elif cmd == "call":
        args = json.loads(rest[1]) if len(rest) > 1 else {}
        print(json.dumps(call(rest[0], args))[:4000])
    elif cmd == "shot":
        sys.exit(0 if shot(rest[0], "--offscreen" in rest) else 1)
    elif cmd == "stats":
        w, h, top = stats(rest[0])
        print("%dx%d %s" % (w, h, " ".join("%s=%.1f%%" % (c, p) for c, p in top)))
    elif cmd == "smoke":
        sys.exit(smoke(opt("--project", os.path.join(REPO, "projects", "E2E")),
                       opt("--build", DEFAULT_BUILD)))
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
