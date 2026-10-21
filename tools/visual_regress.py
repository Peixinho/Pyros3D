#!/usr/bin/env python3
"""Visual regression over the DemoLauncher: every demo is run in turn (one window at a time,
closed after), a frame captured a few seconds in, and compared with its baseline - downscaled to
96x54 and blurred, so animation and timing move nothing, while a demo gone black, a mesh gone,
a colour gone wrong, a mirror where a road was, does. macOS (Quartz window capture).

    /usr/bin/python3 tools/visual_regress.py check  [--build build_ed_mtl] [--only 3,7] [--wait 7]
    /usr/bin/python3 tools/visual_regress.py accept [--only ...]     (the current frames become the baselines)

Baselines: tests/visual/<backend>/<index>_<name>.png (small: 96x54). Exit status 1 if any demo
differs by more than --tolerance (mean per-channel difference, 0-255) or fails to show a window."""
import argparse, json, os, re, subprocess, sys, time
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VOLATILE = {"Motion Blur"}

def capture(pid, out):
    import Quartz, CoreFoundation
    best = None
    for w in Quartz.CGWindowListCopyWindowInfo(Quartz.kCGWindowListOptionAll, Quartz.kCGNullWindowID):
        if w.get("kCGWindowOwnerPID") != pid or w.get("kCGWindowLayer", 1) != 0: continue
        b = w["kCGWindowBounds"]; a = b["Width"] * b["Height"]
        if b["Height"] > 200 and (best is None or a > best[1]): best = (w["kCGWindowNumber"], a)
    if not best: return False
    img = Quartz.CGWindowListCreateImage(Quartz.CGRectNull, Quartz.kCGWindowListOptionIncludingWindow, best[0], Quartz.kCGWindowImageBoundsIgnoreFraming)
    if not img: return False
    d = Quartz.CGImageDestinationCreateWithURL(CoreFoundation.CFURLCreateWithFileSystemPath(None, out, 0, False), "public.png", 1, None)
    Quartz.CGImageDestinationAddImage(d, img, None); Quartz.CGImageDestinationFinalize(d)
    return True

def window_size(pid):
    """The demo's window's size in points, or None."""
    import Quartz
    for w in Quartz.CGWindowListCopyWindowInfo(Quartz.kCGWindowListOptionAll, Quartz.kCGNullWindowID):
        if w.get("kCGWindowOwnerPID") != pid or w.get("kCGWindowLayer", 1) != 0: continue
        b = w["kCGWindowBounds"]
        if b["Height"] > 200: return (int(b["Width"]), int(b["Height"]))
    return None

def fix_size(pid, w=1280, h=760):
    """The demo's window floated and sized (yabai, where it tiles windows; System Events otherwise)."""
    import shutil
    if shutil.which("yabai"):
        try:
            wins = json.loads(subprocess.run(["yabai", "-m", "query", "--windows"], capture_output=True, text=True).stdout or "[]")
            for wi in wins:
                if wi.get("pid") == pid:
                    if not wi.get("is-floating"): subprocess.run(["yabai", "-m", "window", str(wi["id"]), "--toggle", "float"], capture_output=True)
                    subprocess.run(["yabai", "-m", "window", str(wi["id"]), "--move", "abs:40:60"], capture_output=True)
                    subprocess.run(["yabai", "-m", "window", str(wi["id"]), "--resize", "abs:%d:%d" % (w, h)], capture_output=True)
                    return
        except Exception:
            pass
    subprocess.run(["osascript", "-e", 'tell application "System Events" to tell (first process whose unix id is %d) to set size of window 1 to {%d, %d}' % (pid, w, h)],
                   capture_output=True)

def thumb(path):
    from PIL import Image, ImageFilter
    im = Image.open(path).convert("RGB")
    W, H = im.size
    # (the launcher's list and profiler panels left out: the demo's own picture, right of the list)
    im = im.crop((int(W * 0.24), int(H * 0.05), W, int(H * 0.72)))
    return im.resize((96, 54), Image.BILINEAR).filter(ImageFilter.GaussianBlur(1.2))

def diff(a, b):
    import numpy as np
    return float(np.abs(np.asarray(a, dtype=float) - np.asarray(b, dtype=float)).mean())

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["check", "accept"])
    ap.add_argument("--build", default=os.path.join(REPO, "build_ed_mtl"))
    ap.add_argument("--only", default="")
    ap.add_argument("--wait", type=float, default=7.0)
    ap.add_argument("--tolerance", type=float, default=14.0)
    a = ap.parse_args()
    a.build = os.path.abspath(a.build)          # (the demos run from resources/: a relative path is the caller's)
    exe = os.path.join(a.build, "examples", "DemoLauncher")
    backend = "metal" if "mtl" in os.path.basename(a.build) else ("vulkan" if "vk" in os.path.basename(a.build) else "gl")
    demos = json.load(open(os.path.join(REPO, "examples", "assets", "demos", "manifest.json")))
    only = set(int(x) for x in a.only.split(",") if x.strip())
    base_dir = os.path.join(REPO, "tests", "visual", backend); os.makedirs(base_dir, exist_ok=True)
    tmp = os.path.join(os.environ.get("TMPDIR", "/tmp"), "pyros-visual"); os.makedirs(tmp, exist_ok=True)
    failed = []
    for i, d in enumerate(demos):
        if only and i not in only: continue
        name = re.sub(r"[^A-Za-z0-9]+", "_", d.get("name", str(i))).strip("_")
        shot = os.path.join(tmp, "%02d.png" % i)
        p = subprocess.Popen([exe], cwd=os.path.join(REPO, "resources"), env=dict(os.environ, PYROS_DEMO_INDEX=str(i)),
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        # (one size every time: a tiling window manager hands a new window a different tile each
        # launch, and the picture is framed by the window)
        time.sleep(2.0)
        # (until it takes: a demo that bakes on the CPU at start - the Cornell Box's GI on GL - has
        # no window yet at 2 s, the resize found nothing, and its picture came out at whatever
        # size the tiling manager chose: a different framing, and a "difference" every few runs)
        sized_at = time.time()
        for attempt in range(12):
            fix_size(p.pid)
            ws = window_size(p.pid)
            # (the bounds come back a title bar's worth either way of what was asked)
            if (ws and ws[0] == 1280 and abs(ws[1] - 760) <= 16) or p.poll() is not None: break
            time.sleep(0.5)
        time.sleep(max(1.0, a.wait - 2.0 - (time.time() - sized_at)))
        ok = capture(p.pid, shot)
        alive = p.poll() is None
        p.kill(); p.wait()
        if not ok or not alive:
            print("%2d %-34s FAIL (%s)" % (i, d.get("name"), "crashed" if not alive else "no window")); failed.append(i); continue
        t = thumb(shot)
        base = os.path.join(base_dir, "%02d_%s.png" % (i, name))
        if a.mode == "accept":
            t.save(base); print("%2d %-34s baseline saved" % (i, d.get("name"))); continue
        if not os.path.exists(base):
            print("%2d %-34s no baseline (run accept)" % (i, d.get("name"))); continue
        from PIL import Image
        e = diff(t, Image.open(base).convert("RGB"))
        # (a demo made of fast motion is a different picture every run: only gross breakage counts there)
        bad = e > (a.tolerance * 4 if d.get("name") in VOLATILE else a.tolerance)
        print("%2d %-34s %s  (%.1f)" % (i, d.get("name"), "DIFFERS" if bad else "ok", e))
        if bad:
            failed.append(i)
            t.save(os.path.join(tmp, "%02d_now.png" % i))
    if a.mode == "check":
        print("%d differ: %s" % (len(failed), ",".join(map(str, failed)) if failed else "none") + ("  (frames in %s)" % tmp if failed else ""))
    sys.exit(1 if failed and a.mode == "check" else 0)

main()
