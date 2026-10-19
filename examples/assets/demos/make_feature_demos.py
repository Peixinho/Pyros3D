#!/usr/bin/env python3
"""The DemoLauncher's rendering-feature demos: one courtyard, written once per feature with only
the sun's settings changed, so a feature and the same scene without it sit side by side in the
list. A colonnade casting long shadows across a glossy floor, a low sun behind it, cubes and
spheres of every finish, something tall off to the side for reflections to find off the screen.

    python3 examples/assets/demos/make_feature_demos.py      (writes Courtyard_*.json + manifest entries)
"""
import json, math, os
HERE = os.path.dirname(os.path.abspath(__file__))
PBR = 0x1 | 0x4000 | 0x400000       # Color Diffuse PBR (the renderer adds what its passes need - shadow bits here broke the G-buffer: all red)

def mat(i, color, rough, metal=0.0, ssr=False, refl=0.0):
    return {"blendDFactor": 0, "blendEquation": 0, "blendSFactor": 0, "blending": False, "castingShadows": True,
            "color": list(color) + [1.0], "cullFace": 0, "depthBias": False, "depthBiasFactor": 0.0, "depthBiasUnits": 0.0,
            "depthTest": True, "depthWrite": True, "displacementHeight": 0.05, "id": i, "kind": "generic",
            "metallic": metal, "opacity": 1.0, "options": PBR, "reflectivity": refl, "roughness": rough,
            "shininess": 0.0, "specular": [0, 0, 0, 0], "ssrEnabled": ssr, "transparent": False, "wireframe": False}

MATS = [mat(0, (0.42, 0.42, 0.45), 0.06, ssr=True, refl=1.0),          # the glossy floor
        mat(1, (0.78, 0.72, 0.62), 0.8),                                 # stone: columns, walls
        mat(2, (0.85, 0.12, 0.08), 0.25, metal=1.0, ssr=True, refl=1.0), # red metal
        mat(3, (0.95, 0.75, 0.3), 0.12, metal=1.0, ssr=True, refl=1.0),  # gold
        mat(4, (0.1, 0.35, 0.8), 0.45),                                  # blue plastic
        mat(5, (0.92, 0.92, 0.9), 0.5)]                                  # white

roots = []
def obj(name, pos, comps, rot=(0, 0, 0)):
    roots.append({"name": name, "tags": [], "position": list(pos), "rotation": list(rot), "scale": [1, 1, 1],
                  "static": True, "children": [], "components": comps})
def cube(name, pos, half, m, rot=(0, 0, 0)):
    obj(name, pos, [{"type": "RenderingComponent", "castingShadows": True, "cullTest": True, "material": m,
                     "renderable": {"kind": "primitive", "shape": "Cube", "width": half[0], "height": half[1], "depth": half[2],
                                    "smooth": False, "flip": False, "tangentBitangent": False}}], rot)
def sphere(name, pos, r, m):
    obj(name, pos, [{"type": "RenderingComponent", "castingShadows": True, "cullTest": True, "material": m,
                     "renderable": {"kind": "primitive", "shape": "Sphere", "radius": r, "segmentsW": 32, "segmentsH": 20,
                                    "smooth": True, "flip": False, "tangentBitangent": False}}])

def build():
    roots.clear()
    roots.append({"name": "Camera", "tags": ["Camera"], "position": [0, 4.5, 24], "rotation": [0, 0, 0], "scale": [1, 1, 1],
                  "static": False, "children": [], "components": [{"type": "LuaComponent", "scriptFile": "demos/scripts/camera_fly.lua",
                                                                    "data": {"yaw": 0, "pitch": -8}}]})
    cube("Floor", (0, -0.5, 0), (40, 0.5, 40), 0)                          # (half extents)
    # the colonnade along the back, the sun low behind it: shafts between the columns, long shadows forward
    for i in range(-6, 7):
        cube("Column_%d" % (i + 6), (i * 3.0, 4.0, -10), (0.45, 4.0, 0.45), 1)
    cube("Lintel", (0, 8.4, -10), (19.5, 0.4, 0.8), 1)
    cube("BackWall", (0, 1.25, -20), (24, 1.25, 0.5), 1)     # (low: the sun is behind it)
    # contact shadows: a thin slab standing on the floor, a sphere resting on it, a cube on its edge
    cube("Slab", (-6, 1.5, 4), (0.08, 1.5, 2.0), 5)
    sphere("RedBall", (-2.5, 1.2, 6), 1.2, 2)
    sphere("GoldBall", (2.0, 1.0, 3), 1.0, 3)
    cube("BlueCube", (6, 1.0, 6), (1.0, 1.0, 1.0), 4, rot=(0, 0.6, 0))
    cube("TiltedCube", (0, 0.9, 10), (0.9, 0.9, 0.9), 5, rot=(0.6, 0.4, 0.3))
    # tall things off to either side and behind the eye: what only a traced reflection can find
    cube("Tower", (-22, 8, 8), (2.0, 8.0, 2.0), 2)
    cube("Pillar", (22, 6, 14), (1.5, 6.0, 1.5), 3)
    cube("Arch", (0, 7, 34), (8.0, 1.0, 1.0), 4)

def sun(**extra):
    d = (-0.2, -0.42, 0.88)       # about 25 degrees up, from behind the colonnade toward the camera
    l = math.sqrt(sum(c * c for c in d)); d = [c / l for c in d]
    c = {"type": "DirectionalLight", "sun": False, "castingShadows": True, "cascades": 3, "color": [1.0, 0.93, 0.82, 1.0],
         "intensity": 2.2, "direction": d, "shadowWidth": 2048, "shadowHeight": 2048, "shadowNear": 0.1, "shadowFar": 90.0,
         "shadowBiasFactor": 2.0, "shadowBiasUnits": 1.0, "shadowNormalBias": 1.5, "shadowSoftness": 1.0}
    c.update(extra)
    return {"name": "Sun", "tags": [], "position": [0, 30, 0], "rotation": [0, 0, 0], "scale": [1, 1, 1],
            "static": False, "children": [], "components": [c]}

SHAFTS = {"volumetric": [0.008, 0.75, 0.05, 80.0], "volumetricTint": [1.0, 0.9, 0.75, 32.0]}
VARIANTS = [
    ("Courtyard_Base", "Courtyard (baseline)", "The courtyard every feature demo below is built on: shadow maps and screen-space reflections only.", {}),
    ("Courtyard_Shafts", "Sun Shafts", "Light scattered by the air, broken into shafts by the colonnade - the sun pass marching each view ray through the shadow map (DirectionalLight::SetVolumetric). Deferred.", SHAFTS),
    ("Courtyard_RTShadows", "Ray-Traced Shadows", "Every pixel traces a ray toward a point of the sun's disc against the scene's acceleration structure: exact contact shadows (the slab, the ball's foot), softening with distance, no shadow-map texels or acne. Needs a GPU that traces rays (Metal on Apple silicon, Vulkan RTX/RDNA2+); elsewhere this is the baseline. Compare with 'Courtyard (baseline)'.", {"rayTracedShadows": 120.0, "rayShadowSoftness": 0.003}),
    ("Courtyard_RTReflections", "Ray-Traced Reflections", "The glossy floor and metals trace their reflections against the scene: the tower and pillar beside the camera and the arch behind it reflect, which screen-space reflections cannot see. SSR (baseline) is still there and cheaper. Needs a GPU that traces rays.", {"rayTracedReflections": 80.0}),
    ("Courtyard_All", "Courtyard (everything)", "Sun shafts, ray-traced shadows and ray-traced reflections together.", dict(SHAFTS, rayTracedShadows=120.0, rayShadowSoftness=0.003, rayTracedReflections=80.0)),
    ("Courtyard_Clouds", "Volumetric Clouds", "The Sky effect's clouds marched through a slab of 3D noise 1.2-2.8 km up, each step lit by the sun through the cloud itself (Beer's law, powder, a forward-scattering phase) - against its flat cloud layer (uCloudVolume 0). Look up: they drift.", {},
     {"effects": [{"name": "Sky", "params": {"uCloudVolume": 1.0, "uCloudCover": 0.55, "uCloudSteps": 32.0}}], "background": None}, 18),
]

def destruction():
    """Crates, a wall and a column on a floor, broken one after another (scripts/destruction.lua)."""
    roots.clear()
    mats = [mat(0, (0.42, 0.42, 0.45), 0.6), mat(1, (0.55, 0.36, 0.2), 0.7), mat(2, (0.7, 0.68, 0.64), 0.85), mat(3, (0.85, 0.82, 0.76), 0.5)]
    roots.append({"name": "Camera", "tags": ["Camera"], "position": [0, 4.0, 16], "rotation": [0, 0, 0], "scale": [1, 1, 1],
                  "static": False, "children": [], "components": [{"type": "LuaComponent", "scriptFile": "demos/scripts/camera_fly.lua",
                                                                    "data": {"yaw": 0, "pitch": -12}}]})
    obj("Floor", (0, -0.5, 0), [{"type": "RenderingComponent", "castingShadows": True, "cullTest": True, "material": 0,
         "renderable": {"kind": "primitive", "shape": "Cube", "width": 30, "height": 0.5, "depth": 30, "smooth": False, "flip": False, "tangentBitangent": False}},
        {"type": "Physics", "shape": "Box", "width": 30, "height": 0.5, "depth": 30, "mass": 0.0, "ghost": False}])
    names = []
    for i, x in enumerate((-5.0, -2.5, 0.0, 2.5, 5.0)):
        cube("Crate_%d" % i, (x, 0.8, 0), (0.8, 0.8, 0.8), 1); names.append("Crate_%d" % i)
    cube("Crate_Top", (0, 2.4, 0), (0.8, 0.8, 0.8), 1); names.insert(2, "Crate_Top")
    cube("Wall", (0, 2.0, -5), (5.0, 2.0, 0.3), 2); names.append("Wall")
    obj("Column", (6, 2.5, -3), [{"type": "RenderingComponent", "castingShadows": True, "cullTest": True, "material": 3,
         "renderable": {"kind": "primitive", "shape": "Cylinder", "radius": 0.6, "height": 5.0, "segmentsW": 24, "segmentsH": 1,
                        "smooth": True, "flip": False, "tangentBitangent": False}}]); names.append("Column")
    obj("Director", (0, 0, 0), [{"type": "LuaComponent", "scriptFile": "demos/scripts/destruction.lua",
         "data": {"targets": names, "pieces": 14, "mass": 60, "blast": 6.0, "every": 2.2}}])
    roots.append(sun())
    json.dump({"version": 1, "materials": mats, "roots": roots}, open(os.path.join(HERE, "Destruction.json"), "w"), indent=1)
    return {"name": "Destruction", "description": "Run-time fracture (physics:fracture): a crate, then the stack, the wall and the column break one after another into Voronoi pieces packed round where they are hit - each piece keeps its faces and gets a cap over the cut, and a convex body of its own; a click breaks what is under the cursor.",
            "scene": "Destruction.json", "render": {"type": "deferred", "globalLight": [0.2, 0.21, 0.24, 1], "background": [0.42, 0.58, 0.82, 1],
                                                   "projection": {"fov": 70, "near": 0.1, "far": 400}, "effects": ["tonemap"]}}

def main():
    entries = []
    for v in VARIANTS:
        fname, title, desc, extra = v[:4]
        override = v[4] if len(v) > 4 else {}
        pitch = v[5] if len(v) > 5 else -8
        build()
        roots[0]["components"][0]["data"]["pitch"] = pitch
        roots.append(sun(**extra))
        json.dump({"version": 1, "materials": MATS, "roots": roots}, open(os.path.join(HERE, fname + ".json"), "w"), indent=1)
        entries.append({"name": title, "description": desc, "scene": fname + ".json",
                        "render": {"type": "deferred", "globalLight": [0.16, 0.17, 0.2, 1], "background": [0.42, 0.58, 0.82, 1], "ssr": True, "ssrDistances": [0.5, 60.0],
                                   "projection": {"fov": 70, "near": 0.1, "far": 4000}, "effects": ["tonemap"]}})
        for k, val in override.items():
            if val is None: entries[-1]["render"].pop(k, None)
            else: entries[-1]["render"][k] = val
    entries.append(destruction())
    mp = os.path.join(HERE, "manifest.json")
    man = [e for e in json.load(open(mp)) if not e.get("scene", "").startswith("Courtyard_") and e.get("scene") != "Destruction.json"]
    json.dump(man + entries, open(mp, "w"), indent=1)
    print("wrote", [e["scene"] for e in entries], "manifest:", len(man + entries))

main()
