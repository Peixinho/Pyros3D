#!/usr/bin/env python3
# The renderer draws a CustomShaderMaterial by its surface (CustomShaderMaterial::SetSurfaceSource): the Material
# Editor's codegen writes one beside every shader it generates (MaterialCodegen's BuildSurface). This gives the
# materials generated before that their surface, once, from what the codegen wrote then:
#
#   - a scene's custom material whose embedded "shaderSource" the node graph generated: a "surface" field added;
#   - a generated .glsl file on disk: a <file>.surface written beside it.
#
# The codegen's own output is the input here - the statements it put in the fragment stage's main(), the
# expressions it assigned to albedo/metallic/roughness/emissive/occlusion/opacity/N, the parameters and textures it
# declared, and the node helpers it carried - in the same form BuildSurface emits.
#
#   tools/migrate_material_surfaces.py <scene.json | shader.glsl | directory> ...

import json, os, re, sys

MARKER = "Material Editor's node graph"
GLSL_BUILTINS = set("vec2 vec3 vec4 ivec2 ivec3 ivec4 uvec2 uvec3 uvec4 mat2 mat3 mat4 float int uint bool if for while return "
    "texture texture_2D textureLod texelFetch mix clamp smoothstep step min max abs sign floor ceil fract mod pow exp exp2 log log2 sqrt "
    "inversesqrt length distance dot cross normalize reflect refract sin cos tan asin acos atan dFdx dFdy fwidth transpose inverse "
    "lessThan greaterThan any all not sampler2D".split())


def function_definitions(src, name, stage="FRAGMENT"):
    # Every definition of `name` (overloads too) in one stage's part of the file - the old template carries some in
    # both stages: from its return type to its matching closing brace, one per signature
    at = src.find("#ifdef " + stage)
    if at >= 0:
        src = src[at:]
        if stage == "VERTEX":
            end = src.find("#ifdef FRAGMENT")
            if end > 0: src = src[:end]
    out = []
    for m in re.finditer(r"^(?:void|vec[234]|ivec[234]|float|int|uint|bool|mat[234]|p3d_\w+)\s+" + re.escape(name) + r"\s*\([^)]*\)\s*\{", src, re.M):     # (a struct of its own returned too: p3d_Shore)
        depth, i = 0, m.end() - 1
        while i < len(src):
            if src[i] == "{": depth += 1
            elif src[i] == "}":
                depth -= 1
                if depth == 0: break
            i += 1
        d = src[m.start():i + 1]
        # (one per signature - the old template repeats some helpers, not always to the character: two copies of one
        # function do not compile)
        sig = re.sub(r"\s+", " ", d[:d.find("{")]).strip()
        if sig not in [re.sub(r"\s+", " ", o[:o.find("{")]).strip() for o in out]: out.append(d)
    return out


def surface_from_generated(src):
    frag = src[src.find("#ifdef FRAGMENT"):]
    main = frag.find("void main() {")
    if main < 0: return None
    body = frag[main + len("void main() {"):]
    lines = body.split("\n")
    statements, out = [], {}
    clip = None
    for line in lines:
        t = line.strip()
        if not t: continue
        if t.startswith("#ifdef DEFERRED_GBUFFER") or t.startswith("#if ") or t.startswith("roughness = p3d_SpecularAA"): break
        m = re.match(r"vec3 albedo = \((.*)\)\.xyz;$", t)
        if m: out["albedo"] = m.group(1); continue
        m = re.match(r"float metallic = \((.*)\)\.x;$", t)
        if m: out["metallic"] = m.group(1); continue
        m = re.match(r"float roughness = clamp\(\((.*)\)\.x, 0\.03, 1\.0\);$", t)
        if m: out["roughness"] = m.group(1); continue
        m = re.match(r"vec3 emissive = \((.*)\)\.xyz;$", t)
        if m: out["emissive"] = m.group(1); continue
        m = re.match(r"float occlusion = \((.*)\)\.x;$", t)
        if m: out["occlusion"] = m.group(1); continue
        m = re.match(r"float opacity = clamp\(\((.*)\)\.x, 0\.0, 1\.0\) \* uOpacity;$", t)
        if m: out["opacity"] = m.group(1); continue
        m = re.match(r"float reflection = clamp\(\((.*)\)\.x, 0\.0, 1\.0\);$", t)
        if m: out["reflection"] = m.group(1); continue
        if t.startswith("float reflection ="): continue
        m = re.match(r"if \(opacity < \((.*)\)\.x\) discard;$", t)
        if m: clip = m.group(1); continue
        m = re.match(r"vec3 N = normalize\(\((.*)\)\.xyz\);$", t)
        if m: out["normal"] = m.group(1); continue
        if t == "vec3 N = p3d_N;": continue
        if t.startswith("vec3 p3d_N =") or t.startswith("vec3 p3d_V ="): continue
        if "albedo" in out: break        # (past the outputs: the template's own lighting)
        statements.append(t)
    if "albedo" not in out: return None
    params = re.findall(r"^uniform (float|vec4) p_(\w+);", src, re.M)
    # (one name or several to a line - "uniform sampler2D uOceanD0, uOceanD1;" - each a texture of its own)
    declared = [n.strip() for line in re.findall(r"^uniform sampler2D ([\w\s,]+);", frag, re.M) for n in line.split(",") if n.strip()]
    textures = [n for n in declared if not n.startswith("u") or n not in (
        "uDirectionalShadowMaps", "uSpotShadowMaps", "uSceneDepth", "uSkyPanorama")]
    textures = [n for n in textures if "Shadow" not in n and n not in ("uSkyPanorama",)]
    s = ["// Generated by the Pyros3D Material Editor's node graph - the renderer's surface (migrated)"]
    seen = set()
    for kind, name in params:
        if name in seen: continue
        seen.add(name)
        s.append("//@param %s %s" % (name, kind))
    for n in textures: s.append("//@texture %s" % n)
    # (every p3d_ function the surface calls - and those call - as the shader itself defines it)
    body = "\n".join(statements) + "\n".join(out.values()) + (clip or "")
    # (any function the file defines and the surface calls - p3d_ helpers and a material's own)
    calls = lambda text: set(n for n in re.findall(r"\b([A-Za-z_]\w*)\s*\(", text) if n not in GLSL_BUILTINS)
    wanted, order = set(n for n in calls(body) if function_definitions(src, n)), []
    queue = list(wanted)
    while queue:
        name = queue.pop(0)
        defs = function_definitions(src, name)
        for d in defs:
            for called in calls(d[d.find("{"):]):
                if called not in wanted and function_definitions(src, called): wanted.add(called); queue.append(called)
        order.append((name, defs))
    # (each function after every one it calls - GLSL wants a function declared before its use; the order they were
    # found in was not that, and a hash came before the helper it calls)
    deps = {name: set(c for d in defs for c in calls(d[d.find("{"):]) if c != name and c in wanted) for name, defs in order}
    placed, sorted_order = set(), []
    def place(name, seen=()):
        if name in placed or name in seen: return
        for c in sorted(deps.get(name, ())): place(c, seen + (name,))
        placed.add(name)
        sorted_order.append((name, dict(order)[name]))
    for name, _ in order: place(name)
    for name, defs in sorted_order:
        # (the old renderer's sky panorama texture - uSkyPanorama - is no material's to bind now: the sky itself, as the
        # renderer lights with it - SkyRadiance)
        if name == "p3d_skyPano":
            defs = ["vec4 p3d_skyPano(vec3 d) { return vec4(SkyRadiance(normalize(d)), 1.0); }"]
        if not defs and name == "p3d_ToLinear":
            defs = ["vec4 p3d_ToLinear(vec4 c) {\n\tvec3 x = max(c.rgb, vec3(0.0));\n\treturn vec4(mix(x / 12.92, pow((x + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), x)), c.a);\n}"]
        s.extend(defs)
    s.append("void Surface(inout SurfaceData s) {")
    s.append("\tvec3 p3d_N = s.normal;")
    s.append("\tvec3 p3d_V = s.view;")
    for st in statements: s.append("\t" + st)
    s.append("\ts.albedo = (%s).xyz;" % out["albedo"])
    s.append("\ts.metallic = (%s).x;" % out.get("metallic", "vec4(0.0)"))
    s.append("\ts.roughness = clamp((%s).x, 0.03, 1.0);" % out.get("roughness", "vec4(0.5)"))
    s.append("\ts.emission = (%s).xyz;" % out.get("emissive", "vec4(0.0)"))
    s.append("\ts.occlusion = (%s).x;" % out.get("occlusion", "vec4(1.0)"))
    s.append("\ts.alpha = clamp((%s).x, 0.0, 1.0) * uOpacity;" % out.get("opacity", "vec4(1.0)"))
    if "reflection" in out: s.append("\ts.reflection = clamp((%s).x, 0.0, 1.0);" % out["reflection"])     # (the old Reflection output: its mirror)
    if clip: s.append("\tif (s.alpha < (%s).x) discard;" % clip)
    if "normal" in out: s.append("\ts.normal = normalize((%s).xyz);" % out["normal"])
    s.append("}")
    return specialize_sampler_params(with_vertex_stage(src, "\n".join(s) + "\n"))


def specialize_sampler_params(text):
    # A helper taking a sampler2D - p3d_bicubic(sampler2D t, ...) - cannot be handed one in the renderer's GLSL: its
    # textures are a texture and a sampler, put together where they are used, and that pair is no argument. Each such
    # helper copied once for every texture it is called with, the texture written in place of its parameter.
    for _ in range(4):
        sig = re.compile(r"^((?:void|vec[234]|float|int|bool)\s+)(\w+)\s*\(([^)]*\bsampler2D\s+\w+[^)]*)\)\s*\{", re.M)
        found = False
        for m in list(sig.finditer(text)):
            ret, name, params = m.group(1), m.group(2), m.group(3)
            plist = [x.strip() for x in params.split(",")]
            si = next(i for i, x in enumerate(plist) if x.startswith("sampler2D"))
            pname = plist[si].split()[-1]
            depth, i = 0, m.end() - 1
            while i < len(text):
                if text[i] == "{": depth += 1
                elif text[i] == "}":
                    depth -= 1
                    if depth == 0: break
                i += 1
            body = text[m.end():i]
            # every call: its texture argument (a plain name)
            calls = set(re.findall(r"\b" + name + r"\s*\(\s*" + (r"[^,()]+,\s*" * si) + r"(\w+)\s*[,)]", text))
            calls.discard(pname)
            if not calls: continue
            found = True
            copies = []
            for tex in sorted(calls):
                newp = ", ".join(x for k, x in enumerate(plist) if k != si)
                copies.append("%s%s_t_%s(%s) {%s}" % (ret, name, tex, newp, re.sub(r"\b" + pname + r"\b", tex, body)))
            text = text[:m.start()] + "\n".join(copies) + text[i + 1:]
            for tex in calls:
                pat = re.compile(r"\b" + name + r"\s*\(((?:\s*[^,()]+,){%d})\s*%s\s*(,|\))" % (si, tex))
                text = pat.sub(lambda mm: "%s_t_%s(%s%s" % (name, tex, mm.group(1).rstrip(",") if mm.group(2) == ")" else mm.group(1), mm.group(2) if mm.group(2) == ")" else ""), text)
            break
        if not found: break
    return text


STANDARD_VARYINGS = {"vTexcoord", "vNormal", "vNormalWorld", "vWorldPos", "vWorldPositionShadow", "vTangent", "vBitangent", "vColor"}

def with_vertex_stage(src, surface):
    # The shader's own work in its vertex stage, where it has any - what the codegen's template put between the world
    # position and the normal (a sea's waves lifting its sheet): a Vertex(), the helpers and constants it uses; a
    # varying of its own (one vec2) carried in the texcoord. The rest of the surface then for the fragment stage alone.
    # Its constants (#define) too, in both: they were the template's, outside any function
    vsec = src[src.find("#ifdef VERTEX"):src.find("#ifdef FRAGMENT")] if "#ifdef VERTEX" in src else ""
    defines = []
    for m in re.finditer(r"^#define\s+(\w+)[^\n]*$", src, re.M):
        if m.group(1) in ("VERTEX", "FRAGMENT", "texture_2D", "attribute_in", "varying_in", "varying_out") or m.group(0) in defines: continue     # (the template's own: the surface's stages have theirs)
        if re.search(r"\b" + m.group(1) + r"\b", surface + vsec[vsec.find("void main()"):] if vsec else surface) and m.group(0) not in defines:
            defines.append(m.group(0))
    head, body = surface.split("\n", 1)
    tex_lines = [l for l in body.split("\n") if l.startswith("//@")]
    rest = "\n".join(l for l in body.split("\n") if not l.startswith("//@"))
    main_at = vsec.find("void main()")
    block = ""
    if main_at >= 0:
        vm = vsec[main_at:]
        a0 = vm.find("vec4 p3d_worldPos = uModelMatrix * p3d_localPos;")
        a1 = vm.find("vec3 p3d_worldNormal")
        if a0 >= 0 and a1 > a0: block = vm[a0 + len("vec4 p3d_worldPos = uModelMatrix * p3d_localPos;"):a1].strip()
    # (and the structs it names - a helper returning p3d_Shore - once each, in both stages)
    structs = []
    for m in re.finditer(r"^struct\s+(\w+)\s*\{[^}]*\};", src, re.M):
        if m.group(1) in [x.split()[1] for x in structs] or m.group(1) == "LIGHT": continue
        if re.search(r"\b" + m.group(1) + r"\b", rest + vsec[vsec.find("void main()"):] if vsec else rest): structs.append(m.group(0))
    out = [head] + tex_lines + defines + structs
    if not block:
        return "\n".join(out + [rest])
    extra = [n for n in re.findall(r"^varying_out vec2 (\w+);", vsec, re.M) if n not in STANDARD_VARYINGS and re.search(r"\b" + n + r"\b", block)]
    calls = lambda text: set(n for n in re.findall(r"\b([A-Za-z_]\w*)\s*\(", text) if n not in GLSL_BUILTINS)
    wanted = set(n for n in calls(block) if function_definitions(src, n, "VERTEX"))
    queue, defs_of = list(wanted), {}
    while queue:
        n = queue.pop(0)
        defs_of[n] = function_definitions(src, n, "VERTEX")
        for d in defs_of[n]:
            for c in calls(d[d.find("{"):]):
                if c not in wanted and function_definitions(src, c, "VERTEX"): wanted.add(c); queue.append(c)
    deps = {n: set(c for d in ds for c in calls(d[d.find("{"):]) if c != n and c in wanted) for n, ds in defs_of.items()}
    placed, order = set(), []
    def place(n, seen=()):
        if n in placed or n in seen: return
        for c in sorted(deps.get(n, ())): place(c, seen + (n,))
        placed.add(n); order.append(n)
    for n in sorted(wanted): place(n)
    v = ["#ifdef P3D_STAGE_VERTEX"]
    for n in order: v.extend(defs_of[n])
    v.append("void Vertex(inout VertexData p3d_v) {")
    v.append("\tvec4 p3d_worldPos = vec4(p3d_v.position, 1.0);")
    v.append("\tvec4 p3d_localPos = inverse(uModelMatrix) * p3d_worldPos;")
    for n in extra: v.append("\tvec2 %s = p3d_v.uv;" % n)
    v.append("\t" + block.replace("\n", "\n\t"))
    v.append("\tp3d_v.position = p3d_worldPos.xyz;")
    for n in extra[:1]: v.append("\tp3d_v.uv = %s;" % n)
    v.append("}")
    v.append("#endif")
    f = ["#ifdef P3D_STAGE_FRAGMENT"] + ["#define %s vTexcoord" % n for n in extra[:1]] + [rest, "#endif"]
    return "\n".join(out + v + f) + "\n"


def migrate_scene(path):
    text = open(path).read()
    try: data = json.loads(text)
    except ValueError: return 0
    if not isinstance(data, dict): return 0
    # (written back as it was laid out)
    layout = next(((i, nl) for i in (1, 2, 4, "\t") for nl in ("", "\n") if json.dumps(data, indent=i) + nl == text), None)
    n = 0
    for m in data.get("materials", []):
        if m.get("kind") != "custom" or "surface" in m or MARKER not in m.get("shaderSource", ""): continue
        surface = surface_from_generated(m["shaderSource"])
        if surface: m["surface"] = surface; n += 1
        else: print("  could not read the generated shader of material %s in %s" % (m.get("id"), path))
    if n:
        if layout is None: layout = (1, "")
        open(path, "w").write(json.dumps(data, indent=layout[0]) + layout[1])
    return n


def migrate_glsl(path):
    if os.path.exists(path + ".surface"): return 0
    src = open(path, errors="replace").read()
    if MARKER not in src: return 0
    surface = surface_from_generated(src)
    if not surface: print("  could not read %s" % path); return 0
    open(path + ".surface", "w").write(surface)
    return 1


def main():
    total = 0
    for arg in sys.argv[1:]:
        paths = [arg]
        if os.path.isdir(arg):
            paths = [os.path.join(r, f) for r, _, fs in os.walk(arg) for f in fs if f.endswith((".json", ".glsl"))]
        for p in paths:
            n = migrate_scene(p) if p.endswith(".json") else migrate_glsl(p)
            if n: print("%s: %d" % (p, n))
            total += n
    print("materials given a surface: %d" % total)


if __name__ == "__main__":
    main()
