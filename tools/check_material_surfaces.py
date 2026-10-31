#!/usr/bin/env python3
# Compiles every material surface (<file>.surface, and a scene's "surface" fields) into the renderer's surface.frag the
# way SceneRenderer::SurfaceFor does - CUSTOM_SURFACE, the parameters' block, the textures, the surface - with glslc,
# and says which do not compile and why.   tools/check_material_surfaces.py <dir | file> ...
import json, os, re, subprocess, sys, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "resources", "shaders")

def resolve(name, done):
    if name in done: return ""
    done.add(name)
    out = []
    for line in open(os.path.join(SRC, name)):
        m = re.match(r'\s*#include "([^"]+)"', line)
        out.append(resolve(m.group(1), done) if m else line)
    return "".join(out)

def assemble(surface, stage="surface.frag"):
    params = re.findall(r"^//@param (\w+) (float|vec4)", surface, re.M)
    textures = re.findall(r"^//@texture (\w+)", surface, re.M)
    decl = "#define p3d_materialSampler uMaterialSampler\nlayout(set = 1, binding = 20, std140) uniform MaterialParams { vec4 v[%d]; } uParams;\n" % max(1, len(params))
    for i, (n, t) in enumerate(params): decl += "#define p_%s (uParams.v[%d]%s)\n" % (n, i, "" if t == "vec4" else ".x")
    for i, n in enumerate(textures): decl += "layout(set = 1, binding = %d) uniform texture2D p3d_texture%d;\n#define %s sampler2D(p3d_texture%d, p3d_materialSampler)\n" % (21 + i, i, n, i)
    text = resolve(stage, set())
    v = text.find("\n")
    if stage == "surface.vert":
        decl = decl.replace("#define p3d_materialSampler uMaterialSampler\n", "layout(set = 1, binding = 6) uniform sampler p3d_materialSampler;\n")
        text = text[:v + 1] + "#define CUSTOM_VERTEX\n" + text[v + 1:]
    else: text = text[:v + 1] + "#define CUSTOM_SURFACE\n" + text[v + 1:]
    text = text.replace("//P3D_SURFACE_DECLARATIONS", decl, 1).replace("//P3D_SURFACE\n", surface + "\n", 1)
    return text

def check(label, surface):
    ok = check_stage(label, surface, "surface.frag")
    if "void Vertex(" in surface: ok = check_stage(label + " (vertex)", surface, "surface.vert") and ok
    return ok

def check_stage(label, surface, stage):
    with tempfile.NamedTemporaryFile("w", suffix=stage[stage.rfind("."):], delete=False) as f: f.write(assemble(surface, stage)); path = f.name
    r = subprocess.run(["glslc", "--target-env=vulkan1.0", "-o", "/dev/null", path], capture_output=True, text=True)
    os.unlink(path)
    if r.returncode != 0:
        err = [l for l in r.stderr.splitlines() if "error" in l][:3]
        print("FAIL %s\n   %s" % (label, "\n   ".join(e.split(": ", 2)[-1] for e in err)))
        return False
    return True

ok = bad = 0
for arg in sys.argv[1:]:
    files = [arg] if os.path.isfile(arg) else [os.path.join(r, f) for r, _, fs in os.walk(arg, followlinks=True) for f in fs if f.endswith((".surface", ".json"))]
    for p in files:
        if p.endswith(".surface"):
            if check(p, open(p).read()): ok += 1
            else: bad += 1
        else:
            try: d = json.load(open(p))
            except Exception: continue
            for m in (d.get("materials", []) if isinstance(d, dict) else []):
                if "surface" in m:
                    if check("%s material %s" % (p, m.get("id")), m["surface"]): ok += 1
                    else: bad += 1
print("compiled: %d, failed: %d" % (ok, bad))
