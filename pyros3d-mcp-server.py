#!/usr/bin/env python3
"""MCP server for Pyros3D — replicates the editor's file-level operations.

Every format here is matched to what the C++ editor (PyrosBuilder) actually
reads and writes, verified against the editor source:

  project.json            ProjectManager::WriteProjectJson
  scenes/<Name>.json      SceneSerializer::SaveScene
                          (version, mainScript, materials[], roots[])
  game object             name, position[3], rotation[3] euler radians,
                          scale[3], static, tags[], children[], components[]
  scene sidecar           <scene>.json.editor.json (cameras fov/near/far)
  Lua templates           ProjectManager::BuildLuaSnippet (exact)
  assets/ tree            models (p3dm packages), textures, sounds, shaders,
                          lua, materials — ProjectManager::ImportAssetFile
  model conversion        AssimpImporter --model <in> <outBase> + p3dm
                          texture packaging (ProjectManager::ImportModel)

Runtime features (play mode, log tab, viewport) are available through the
LIVE EDITOR tools (editor_status / editor_screenshot / editor_log /
play_mode / reload_scene): when PyrosBuilder is running it exposes a local
loopback command server and publishes <tempdir>/pyros3d-editor.json
({port, token, pid}); the bridge drives it in real time and every
scene-editing tool prefers the live editor when it has the same scene open,
falling back to these file operations when the editor is closed.
"""

import base64
import json
import math
import os
import shutil
import socket
import struct
import subprocess
import tempfile
from pathlib import Path
from typing import Any

from mcp.server.fastmcp import FastMCP

ROOT = Path(os.environ.get("PYROS_ROOT", str(Path(__file__).parent))).resolve()
if not (ROOT / "CMakeLists.txt").exists():
    for p in Path(__file__).resolve().parents:
        if (p / "CMakeLists.txt").exists():
            ROOT = p
            break

mcp = FastMCP("pyros3d-editor")

# --------------------------------------------------------------------------
# Extension tables — mirror ProjectManager (editor/src/editor/ProjectManager.cpp)
# --------------------------------------------------------------------------

MODEL_SOURCE_EXTS = {".obj", ".fbx", ".dae", ".gltf", ".glb", ".blend",
                     ".3ds", ".ase", ".ifc", ".x", ".md5mesh", ".smd", ".stl", ".ply"}
TEXTURE_EXTS = {".png", ".jpg", ".jpeg", ".tga", ".bmp", ".gif", ".hdr", ".exr", ".webp"}
SOUND_EXTS = {".wav", ".ogg", ".mp3", ".flac", ".aiff"}
SHADER_EXTS = {".glsl", ".vert", ".frag", ".comp", ".hlsl", ".metal", ".spv"}
MATERIAL_EXTS = {".mat", ".material"}
MODEL_PACKAGE_EXTS = {".p3dm"}

ASSET_DIRS = {
    "models": "assets/models",
    "textures": "assets/textures",
    "sounds": "assets/sounds",
    "shaders": "assets/shaders",
    "lua": "assets/lua",
    "materials": "assets/materials",
}

# GenericMaterial defaults — exactly what the editor creates for new
# primitives (SceneObjects::GenericMaterial / Default.json material 0).
DEFAULT_MATERIAL = {
    "kind": "generic",
    "options": 23553,  # Color|Diffuse|DirectionalShadow|PointShadow|SpotShadow
    "color": [1.0, 1.0, 1.0, 1.0],
    "specular": [0.0, 0.0, 0.0, 1.0],
    "displacementHeight": 0.05,
    "reflectivity": 0.0,
    "shininess": 50.0,
    "metallic": 0.0,
    "roughness": 0.0,
    "ssrEnabled": False,
    "alphaCutoff": 0.5,
    "opacity": 1.0,
    "transparent": False,
    "cullFace": 0,
    "wireframe": False,
    "castingShadows": True,
    "depthTest": True,
    "depthWrite": True,
    "blending": False,
    "blendSFactor": 0,
    "blendDFactor": 0,
    "blendEquation": 0,
    "depthBias": False,
    "depthBiasFactor": 0.0,
    "depthBiasUnits": 0.0,
}

CAMERA_TAG = "PyrosEditor.Camera"


# --------------------------------------------------------------------------
# Path / project helpers
# --------------------------------------------------------------------------

def _fail(msg: str) -> str:
    return f"ERROR: {msg}"


def _rel(path: Path) -> str:
    """Path relative to ROOT when possible, absolute otherwise."""
    try:
        return path.resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return str(path)


def _resolve_project(project_path: str) -> tuple[Path | None, str]:
    p = Path(project_path) if os.path.isabs(project_path) else ROOT / project_path
    p = p.resolve()
    if (p / "project.json").exists():
        return p, ""
    if (p / "scenes").exists():
        return p, ""
    return None, f"Not a Pyros3D project (no project.json or scenes/): {project_path}"


def _project_json(proj: Path) -> Path:
    return proj / "project.json"


def _read_project(proj: Path) -> dict:
    with open(_project_json(proj)) as f:
        return json.load(f)


def _write_project(proj: Path, data: dict) -> None:
    with open(_project_json(proj), "w") as f:
        json.dump(data, f, indent=4)


# --------------------------------------------------------------------------
# LIVE EDITOR CLIENT
#
# When a PyrosBuilder editor instance is running, it exposes a local
# loopback TCP command server (AgentServer) and publishes
#   <tempdir>/pyros3d-editor.json  ->  {"port": N, "token": "...", "pid": N}
# so we can drive the *running* editor in real time (create objects, read
# the live hierarchy, take screenshots, play/stop, reload from disk).
#
# These helpers are best-effort: any failure falls back to the file-based
# operations below, which always work.
# --------------------------------------------------------------------------

def _pid_alive(pid: int) -> bool:
    try:
        os.kill(pid, 0)
        return True
    except (ProcessLookupError, PermissionError):
        return False
    except Exception:
        return True


def _editor_endpoint():
    """Return (port, token) of a running editor, or None if not available."""
    cands = []
    tmpdir = os.environ.get("TMPDIR") or tempfile.gettempdir()
    cands.append(os.path.join(tmpdir, "pyros3d-editor.json"))
    for env in ("TEMP", "TMP"):
        v = os.environ.get(env)
        if v:
            cands.append(os.path.join(v, "pyros3d-editor.json"))
    for path in cands:
        try:
            with open(path) as f:
                d = json.load(f)
            pid = int(d.get("pid", 0))
            port = int(d.get("port", 0))
            token = str(d.get("token", ""))
            if port and token and _pid_alive(pid):
                return port, token
        except Exception:
            continue
    return None


def _editor_call(cmd: str, args: dict | None = None, timeout: float = 30.0):
    """Send one command to the running editor.

    Returns (True, result_dict) on success or (False, error_string) on any
    failure (editor not running, refused, timeout, or the editor reporting
    an error). Never raises.
    """
    ep = _editor_endpoint()
    if not ep:
        return False, "editor not running (no discovery file)"
    port, token = ep
    msg = json.dumps({"id": 1, "token": token, "cmd": cmd, "args": args or {}}) + "\n"
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(timeout)
    try:
        s.connect(("127.0.0.1", port))
        s.sendall(msg.encode("utf-8"))
        buf = b""
        while b"\n" not in buf:
            chunk = s.recv(65536)
            if not chunk:
                break
            buf += chunk
        line = buf.split(b"\n", 1)[0].decode("utf-8", "replace")
        d = json.loads(line)
        if d.get("ok"):
            return True, d.get("result", {})
        return False, str(d.get("error", "unknown error"))
    except Exception as e:
        return False, f"editor call failed: {e}"
    finally:
        try:
            s.close()
        except Exception:
            pass


def _live_active_scene_path(res: dict) -> str:
    """Absolute path of the scene the editor reports as active, or "".

    The editor answers with a *project-relative* scenePath ("scenes/Foo.json")
    and one absolute anchor, projectPath - so the two have to be recombined
    here. A scene saved outside the project still comes back absolute (the
    editor cannot relativize it), and an older editor build reported every
    path absolute, so an already-absolute value is passed straight through.
    """
    active = str(res.get("scenePath", ""))
    if not active:
        return ""
    if os.path.isabs(active):
        return active
    root = str(res.get("projectPath", ""))
    if not root:
        return ""
    return os.path.join(root, active)


def _live_scene_matches(scene_file: Path) -> bool:
    """True if a running editor has exactly this scene file open."""
    ok, res = _editor_call("status", {})
    if not ok:
        return False
    active = _live_active_scene_path(res)
    if not active:
        return False
    try:
        return os.path.realpath(active) == os.path.realpath(str(scene_file))
    except Exception:
        return active == str(scene_file)


def _live_or_none(cmd: str, args: dict, scene_file: Path):
    """Run `cmd` against the running editor, but only if it has `scene_file` open.

    Returns:
      dict -> success (result payload from the editor)
      str  -> live failure (human-readable error)
      None -> no live editor for this scene (caller should fall back to files)
    """
    if not _live_scene_matches(scene_file):
        return None
    ok, res = _editor_call(cmd, args)
    return res if ok else str(res)


def _live_project_matches(proj: Path) -> bool:
    """True if a running editor already has exactly this project open."""
    ok, res = _editor_call("status", {})
    if not ok or not res.get("projectOpen"):
        return False
    active = str(res.get("projectPath", ""))
    if not active:
        return False
    try:
        return os.path.realpath(active) == os.path.realpath(str(proj))
    except Exception:
        return active == str(proj)


def _live_open_project(proj: Path) -> str:
    """Best-effort: make a running editor open `proj` live.

    Returns a short human-readable status line to append to a tool's
    output ("" if no editor is running - nothing to report).
    """
    if not _editor_endpoint():
        return ""
    if _live_project_matches(proj):
        return "\n(already open in the running editor)"
    ok, res = _editor_call("open_project", {"path": str(proj)})
    if ok:
        note = f"\nOpened live in the running editor (scene: {res.get('scene', 'none')})"
        return note
    return f"\nNOTE: could not open live in the running editor: {res}"


def _scene_file(proj: Path, scene_name: str) -> Path:
    name = scene_name.strip()
    if name.endswith(".json"):
        name = name[:-5]
    name = name.replace("scenes/", "")
    return proj / "scenes" / f"{name}.json"


def _load_scene(scene_file: Path) -> dict:
    with open(scene_file) as f:
        return json.load(f)


def _save_scene(scene_file: Path, data: dict) -> None:
    scene_file.parent.mkdir(parents=True, exist_ok=True)
    with open(scene_file, "w") as f:
        json.dump(data, f, indent=4)


def _scene_sidecar(scene_file: Path) -> Path:
    return Path(str(scene_file) + ".editor.json")


def _read_sidecar(scene_file: Path) -> dict:
    sc = _scene_sidecar(scene_file)
    if sc.exists():
        try:
            with open(sc) as f:
                return json.load(f)
        except (json.JSONDecodeError, OSError):
            pass
    return {"cameras": {}, "activeCamera": ""}


def _write_sidecar(scene_file: Path, data: dict) -> None:
    with open(_scene_sidecar(scene_file), "w") as f:
        json.dump(data, f, indent=4)


def _scene_error(scene_file: Path) -> str | None:
    if not scene_file.exists():
        return f"Scene not found: {scene_file}. Use list_scenes to see available scenes."
    return None


def _iter_objects(data: dict):
    for root in data.get("roots", []):
        stack = [root]
        while stack:
            node = stack.pop()
            if not isinstance(node, dict):
                continue
            yield node
            for child in node.get("children", []):
                stack.append(child)


def _find_object(data: dict, name: str) -> dict | None:
    for node in _iter_objects(data):
        if node.get("name") == name:
            return node
    return None


def _find_object_with_parent(data: dict, name: str) -> tuple[dict | None, dict | list | None]:
    def walk(node, parent):
        if isinstance(node, dict):
            if node.get("name") == name:
                return node, parent
            for child in node.get("children", []):
                found = walk(child, node)
                if found[0]:
                    return found
        return None, None
    for root in data.get("roots", []):
        found = walk(root, None)
        if found[0]:
            return found
    return None, None


def _new_game_object(name: str, position=None, rotation=None, scale=None) -> dict:
    return {
        "name": name,
        "position": list(position) if position else [0.0, 0.0, 0.0],
        "rotation": list(rotation) if rotation else [0.0, 0.0, 0.0],
        "scale": list(scale) if scale else [1.0, 1.0, 1.0],
        "static": False,
        "tags": [],
        "children": [],
        "components": [],
    }


def _add_to_scene(data: dict, obj: dict, parent_name: str | None) -> str | None:
    """Insert obj at root or under parent. Returns error string on failure."""
    if parent_name:
        parent = _find_object(data, parent_name)
        if parent is None:
            return f"Parent object '{parent_name}' not found"
        parent.setdefault("children", []).append(obj)
    else:
        data.setdefault("roots", []).append(obj)
    return None


def _unique_scene_name(data: dict, base: str) -> str:
    names = {n.get("name") for n in _iter_objects(data)}
    if base not in names:
        return base
    i = 1
    while f"{base} ({i})" in names or f"{base}({i})" in names:
        i += 1
    return f"{base} ({i})"


def _default_material_entry(mat_id: int) -> dict:
    mat = json.loads(json.dumps(DEFAULT_MATERIAL))  # deep copy
    mat["id"] = mat_id
    return mat


# --------------------------------------------------------------------------
# Lua — exact templates from ProjectManager::BuildLuaSnippet
# --------------------------------------------------------------------------

def _sanitize_lua_class_name(stem: str) -> str:
    out = []
    for c in stem:
        if c.isalnum() or c == "_":
            out.append(c)
        elif c in "- ":
            out.append("_")
    if not out or out[0].isdigit():
        out = list("Script_") + out
    return "".join(out)


def _build_lua_snippet(kind: str, class_name: str, file_label: str) -> str:
    if kind == "scene":
        return (
            f"-- {file_label}\n"
            "-- Scene main script (no GameObject owner).\n"
            "-- Created automatically next to the scene .json — not listed in Assets.\n"
            "\n"
            f"local {class_name} = class('{class_name}')\n"
            "\n"
            f"function {class_name}:initialize()\n"
            "end\n"
            "\n"
            f"function {class_name}:init(owner)\n"
            "\t-- owner is always nil for scene scripts\n"
            "end\n"
            "\n"
            f"function {class_name}:update(time)\n"
            "end\n"
            "\n"
            f"function {class_name}:destroy()\n"
            "end\n"
            "\n"
            f"return {class_name}\n"
        )
    return (
        f"-- {file_label}\n"
        "-- GameObject behavior. Attach via Properties → Script.\n"
        "-- Must return a middleclass class with :new().\n"
        "\n"
        f"local {class_name} = class('{class_name}')\n"
        "\n"
        f"function {class_name}:initialize()\n"
        "end\n"
        "\n"
        f"function {class_name}:init(owner)\n"
        "\tself.owner = owner\n"
        "end\n"
        "\n"
        f"function {class_name}:update(time)\n"
        "end\n"
        "\n"
        f"function {class_name}:destroy()\n"
        "end\n"
        "\n"
        f"function {class_name}:serialize()\n"
        "\treturn {}\n"
        "end\n"
        "\n"
        f"function {class_name}.deserialize(data)\n"
        f"\treturn {class_name}:new()\n"
        "end\n"
        "\n"
        f"return {class_name}\n"
    )


def _register_scene_in_project(proj: Path, scene_rel: str) -> None:
    """Mirror ProjectManager::SetActiveSceneRel."""
    if not scene_rel.endswith(".json"):
        scene_rel += ".json"
    data = _read_project(proj)
    scenes = data.setdefault("scenes", [])
    if scene_rel not in scenes:
        scenes.append(scene_rel)
    data["activeScene"] = scene_rel
    _write_project(proj, data)


# --------------------------------------------------------------------------
# p3dm model import — mirror ProjectManager::ImportModel + texture packaging
# --------------------------------------------------------------------------

def _find_assimp_importer() -> Path | None:
    candidates = [
        Path("AssimpImporter"),
        Path("tools/AssimpImporter"),
        Path("../tools/AssimpImporter/AssimpImporter"),
        Path("../../tools/AssimpImporter/AssimpImporter"),
        Path(ROOT.parent) / "tools/AssimpImporter/AssimpImporter",
    ]
    for c in candidates:
        if c.is_file():
            return c.resolve()
    for build_dir in sorted(ROOT.glob("build_*")):
        c = build_dir / "tools" / "AssimpImporter" / "AssimpImporter"
        if c.is_file():
            return c
    return None


def _unique_texture_dest_name(textures_dir: Path, source_file: Path) -> str:
    base = source_file.name or "texture.png"
    if not (textures_dir / base).exists():
        return base
    stem, ext = os.path.splitext(source_file.name)
    for n in range(1, 10000):
        candidate = f"{stem}_{n}{ext}"
        if not (textures_dir / candidate).exists():
            return candidate
    return f"{stem}_copy{ext}"


def _resolve_referenced_texture(stored_raw: str, roots: list[Path]) -> Path | None:
    stored = stored_raw.replace("\\", "/")
    while stored.startswith("./"):
        stored = stored[2:]
    if not stored or stored.startswith("*"):
        return None
    as_path = Path(stored)
    if as_path.is_absolute() and as_path.is_file():
        return as_path.resolve()
    for root in roots:
        if not root:
            continue
        for cand in (
            (root / as_path),
            root / as_path.name,
        ):
            cand = Path(os.path.normpath(cand))
            if cand.is_file():
                return cand.resolve()
        for sub in ("textures", "Textures", "maps", "Maps", "images", "Images"):
            for cand in (root / sub / as_path.name, Path(os.path.normpath(root / sub / as_path))):
                if cand.is_file():
                    return cand.resolve()
    return None


def _package_referenced_model_textures(p3dm: Path, original_source: Path, model_dir: Path) -> tuple[int, list[str]]:
    """Mirror ProjectManager::PackageReferencedModelTextures.

    p3dm layout: int32 materialsCount, then per material:
      int32 id, str name,
      4 × (u8 flag, 16 bytes),
      u8 wire, u8 doubleSided, 3 × float,
      3 × (u8 hasTexture, str texPath),
      u8 hasBones.
    str = int32 length + bytes.
    """
    data = bytearray(p3dm.read_bytes())
    if len(data) < 4:
        return 0, []

    def read_string(buf: bytearray, pos: int) -> tuple[str, int]:
        (size,) = struct.unpack_from("<i", buf, pos)
        pos += 4
        s = bytes(buf[pos:pos + size]).decode("utf-8", errors="replace")
        return s, pos + size

    def write_string(s: str) -> bytes:
        b = s.encode("utf-8")
        return struct.pack("<i", len(b)) + b

    pos = 0
    (count,) = struct.unpack_from("<i", data, pos)
    pos += 4
    if count < 0 or count > 100000:
        return 0, ["invalid materials count in p3dm"]

    out = bytearray(struct.pack("<i", count))
    roots = [model_dir, original_source.parent, p3dm.parent]
    textures_dir = model_dir / "textures"
    textures_dir.mkdir(parents=True, exist_ok=True)
    remapped: dict[str, str] = {}
    any_rewrite = False
    errors: list[str] = []

    for _ in range(count):
        # id + name
        (mid,) = struct.unpack_from("<i", data, pos)
        pos += 4
        name, pos = read_string(data, pos)
        out += struct.pack("<i", mid)
        out += write_string(name)
        # 4 × (u8 flag, 16 bytes vec4)
        out += bytes(data[pos:pos + 4 * 17])
        pos += 4 * 17
        # u8 wire, u8 double, 3 floats
        out += bytes(data[pos:pos + 2 + 12])
        pos += 14
        # 3 texture slots
        for _t in range(3):
            have = data[pos]
            pos += 1
            path, pos = read_string(data, pos)
            rewritten = path
            if have and path and not path.startswith("*"):
                resolved = _resolve_referenced_texture(path, roots)
                if resolved is None:
                    errors.append(f"model texture not found: {path}")
                else:
                    key = str(resolved)
                    if key in remapped:
                        rewritten = remapped[key]
                    else:
                        dest_name = _unique_texture_dest_name(textures_dir, resolved)
                        shutil.copy2(resolved, textures_dir / dest_name)
                        rel = f"textures/{dest_name}"
                        remapped[key] = rel
                        rewritten = rel
            if rewritten != path:
                any_rewrite = True
            out += struct.pack("<B", have)
            out += write_string(rewritten)
        # hasBones
        out += bytes(data[pos:pos + 1])
        pos += 1

    out += data[pos:]

    if any_rewrite or remapped:
        p3dm.write_bytes(bytes(out))
    return len(remapped), errors


def _copy_model_sidecars(source: Path, model_dir: Path) -> None:
    src_dir = source.parent
    stem = source.stem
    model_dir.mkdir(parents=True, exist_ok=True)
    try:
        for p in src_dir.iterdir():
            if not p.is_file():
                continue
            ext = p.suffix.lower()
            if ext in (".mtl", ".bin") and (p.stem == stem or p.name.startswith(stem)):
                shutil.copy2(p, model_dir / p.name)
    except OSError:
        pass
    for sub in ("textures", "Textures", "texture", "Texture",
                "maps", "Maps", "materials", "Materials", "images", "Images"):
        sub_dir = src_dir / sub
        if sub_dir.is_dir():
            shutil.copytree(sub_dir, model_dir / sub, dirs_exist_ok=True)


def _convert_model(source: Path, model_dir: Path) -> tuple[Path | None, str]:
    """Full editor model import: stage → convert → package textures."""
    stem = source.stem
    importer = _find_assimp_importer()
    if importer is None:
        return None, "AssimpImporter not found (build the editor with -DBUILD_CONVERTER=ON)"

    model_dir.mkdir(parents=True, exist_ok=True)
    staged = model_dir / source.name
    shutil.copy2(source, staged)
    _copy_model_sidecars(source, model_dir)

    out_base = model_dir / stem
    cmd = [str(importer), "--model", str(staged), str(out_base)]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=300, cwd=str(model_dir))
    except (subprocess.TimeoutExpired, OSError) as e:
        return None, f"AssimpImporter failed: {e}"
    if proc.returncode != 0:
        return None, f"AssimpImporter failed ({proc.returncode}): {proc.stdout.strip() or proc.stderr.strip()}"

    p3dm = Path(str(out_base) + ".p3dm")
    if not p3dm.exists():
        return None, "Converter finished but output .p3dm is missing"

    _package_referenced_model_textures(p3dm, source, model_dir)
    return p3dm, ""


# --------------------------------------------------------------------------
# TOOLS — project level
# --------------------------------------------------------------------------

# --------------------------------------------------------------------------
# Animation (.p3da) helpers
# --------------------------------------------------------------------------
#
# A .p3da is a small binary of animation clips - the format AnimationLoader
# reads and (since the Animation Editor) writes:
#
#   int32 clipCount
#     int32 nameLen, char[nameLen] name
#     int32 channelCount
#     f32   duration
#     f32   ticksPerSecond
#     channelCount x:
#       int32 nodeNameLen, char[nodeNameLen] nodeName
#       int32 posCount,   posCount   x (f32 time, f32 x,y,z)
#       int32 rotCount,   rotCount   x (f32 time, f32 x,y,z,w)   <- Quaternion
#       int32 scaleCount, scaleCount x (f32 time, f32 x,y,z)
#
# Reading and writing it here (rather than only through the editor) is what
# lets these tools work with no editor running, the same way the scene tools
# work directly on scene JSON. The loader divides every time by
# ticksPerSecond and then reports it as 1, so anything written back out uses
# ticksPerSecond 1 and times already in seconds - see AnimationLoader::Save.
#
# Quaternion field order note: p3d::Quaternion declares `f32 w, x, y, z` in
# that order, so a raw struct write puts w FIRST. Everything below keeps the
# file's own w,x,y,z order and only reorders at the tool boundary, where
# rotations are exposed as [x, y, z, w] to match how quaternions are usually
# written down.

_P3DA_STRUCT_F = struct.Struct("<f")
_P3DA_STRUCT_I = struct.Struct("<i")


def _p3da_read_i32(buf: bytes, pos: int) -> tuple[int, int]:
    return _P3DA_STRUCT_I.unpack_from(buf, pos)[0], pos + 4


def _p3da_read_f32(buf: bytes, pos: int) -> tuple[float, int]:
    return _P3DA_STRUCT_F.unpack_from(buf, pos)[0], pos + 4


def _p3da_read_str(buf: bytes, pos: int) -> tuple[str, int]:
    n, pos = _p3da_read_i32(buf, pos)
    if n < 0 or pos + n > len(buf):
        raise ValueError("corrupt .p3da (bad string length)")
    return buf[pos:pos + n].decode("utf-8", "replace"), pos + n


_P3DA_MAGIC = b"P3DA"
_P3DA_VERSION = 1

# Per-key interpolation, mirroring p3d::InterpolationMode in
# include/Pyros3D/Utils/ModelLoaders/MultiModelLoader/AnimationLoader.h.
_P3DA_INTERP = ["linear", "step", "ease_in", "ease_out", "ease_both", "bezier"]

# Clip flags, mirroring p3d::AnimationFlags in the same header.
_P3DA_FLAG_LOOP = 1 << 0
_P3DA_FLAG_APPLY_SCALE = 1 << 1


def _p3da_read_u32(buf: bytes, pos: int) -> tuple[int, int]:
    return struct.unpack_from("<I", buf, pos)[0], pos + 4


def _p3da_read_key_interp(buf: bytes, pos: int, version: int) -> tuple[dict, int]:
    """Read the mode/tangent triple every key carries in v1+.

    v0 files have no such fields, and the defaults returned here are exactly
    the linear sampling those files were authored against.
    """
    if version < 1:
        return {"interp": "linear", "in_tangent": 1.0, "out_tangent": 1.0}, pos
    mode = buf[pos]
    pos += 1
    in_t, pos = _p3da_read_f32(buf, pos)
    out_t, pos = _p3da_read_f32(buf, pos)
    name = _P3DA_INTERP[mode] if mode < len(_P3DA_INTERP) else "linear"
    return {"interp": name, "in_tangent": in_t, "out_tangent": out_t}, pos


def _read_p3da(path: Path) -> list[dict]:
    """Parse a .p3da into a list of clip dicts. Raises ValueError if malformed."""
    buf = path.read_bytes()
    pos = 0

    # Version sniff. A v0 file opens directly with its clip count, so the
    # first four bytes are a small positive int32 and cannot spell "P3DA".
    version = 0
    if buf[:4] == _P3DA_MAGIC:
        pos = 4
        version, pos = _p3da_read_u32(buf, pos)
        if version > _P3DA_VERSION:
            raise ValueError(
                f"'{path.name}' is .p3da version {version}; this server understands "
                f"up to {_P3DA_VERSION}"
            )

    clip_count, pos = _p3da_read_i32(buf, pos)
    if clip_count < 0 or clip_count > 100000:
        raise ValueError("corrupt .p3da (implausible clip count)")

    clips = []
    for _ in range(clip_count):
        name, pos = _p3da_read_str(buf, pos)

        guid = ""
        flags = 0
        authored_fps = 0.0
        if version >= 1:
            guid = buf[pos:pos + 16].hex()
            pos += 16
            flags, pos = _p3da_read_u32(buf, pos)
            authored_fps, pos = _p3da_read_f32(buf, pos)

        channel_count, pos = _p3da_read_i32(buf, pos)
        duration, pos = _p3da_read_f32(buf, pos)
        tps, pos = _p3da_read_f32(buf, pos)
        if tps == 0:
            tps = 1.0

        channels = []
        for _ in range(channel_count):
            node, pos = _p3da_read_str(buf, pos)

            n, pos = _p3da_read_i32(buf, pos)
            positions = []
            for _ in range(n):
                t, pos = _p3da_read_f32(buf, pos)
                x, pos = _p3da_read_f32(buf, pos)
                y, pos = _p3da_read_f32(buf, pos)
                z, pos = _p3da_read_f32(buf, pos)
                interp, pos = _p3da_read_key_interp(buf, pos, version)
                positions.append({"time": t / tps, "value": [x, y, z], **interp})

            n, pos = _p3da_read_i32(buf, pos)
            rotations = []
            for _ in range(n):
                t, pos = _p3da_read_f32(buf, pos)
                w, pos = _p3da_read_f32(buf, pos)
                x, pos = _p3da_read_f32(buf, pos)
                y, pos = _p3da_read_f32(buf, pos)
                z, pos = _p3da_read_f32(buf, pos)
                interp, pos = _p3da_read_key_interp(buf, pos, version)
                rotations.append({"time": t / tps, "value": [x, y, z, w], **interp})

            n, pos = _p3da_read_i32(buf, pos)
            scales = []
            for _ in range(n):
                t, pos = _p3da_read_f32(buf, pos)
                x, pos = _p3da_read_f32(buf, pos)
                y, pos = _p3da_read_f32(buf, pos)
                z, pos = _p3da_read_f32(buf, pos)
                interp, pos = _p3da_read_key_interp(buf, pos, version)
                scales.append({"time": t / tps, "value": [x, y, z], **interp})

            channels.append({
                "bone": node,
                "positions": positions,
                "rotations": rotations,
                "scales": scales,
            })

        clips.append({
            "name": name,
            "guid": guid,
            "loop": bool(flags & _P3DA_FLAG_LOOP),
            "apply_scale": bool(flags & _P3DA_FLAG_APPLY_SCALE),
            "authored_fps": authored_fps,
            "duration": duration / tps,
            "channels": channels,
        })
    return clips


def _write_p3da(path: Path, clips: list[dict]) -> None:
    """Write clips back out in the layout AnimationLoader::Load expects.

    Always writes the current version. Round-trips a v0 file that was read by
    _read_p3da into an equivalent v1 one - clips that had no guid are minted
    one here, which is what gives them a stable identity for scenes to save.
    """
    out = bytearray()

    def put_i32(v: int) -> None:
        out.extend(_P3DA_STRUCT_I.pack(int(v)))

    def put_u32(v: int) -> None:
        out.extend(struct.pack("<I", int(v)))

    def put_f32(v: float) -> None:
        out.extend(_P3DA_STRUCT_F.pack(float(v)))

    def put_str(v: str) -> None:
        enc = v.encode("utf-8")
        put_i32(len(enc))
        out.extend(enc)

    def put_key_interp(k: dict) -> None:
        name = str(k.get("interp", "linear"))
        mode = _P3DA_INTERP.index(name) if name in _P3DA_INTERP else 0
        out.append(mode)
        put_f32(k.get("in_tangent", 1.0))
        put_f32(k.get("out_tangent", 1.0))

    out.extend(_P3DA_MAGIC)
    put_u32(_P3DA_VERSION)

    put_i32(len(clips))
    for clip in clips:
        put_str(str(clip.get("name", "Clip")))

        guid = clip.get("guid") or ""
        try:
            raw = bytes.fromhex(guid)
        except ValueError:
            raw = b""
        if len(raw) != 16:
            raw = os.urandom(16)
        out.extend(raw)

        flags = 0
        if clip.get("loop"):
            flags |= _P3DA_FLAG_LOOP
        if clip.get("apply_scale"):
            flags |= _P3DA_FLAG_APPLY_SCALE
        put_u32(flags)
        put_f32(clip.get("authored_fps", 0.0))

        channels = clip.get("channels", [])
        put_i32(len(channels))
        put_f32(clip.get("duration", 1.0))
        put_f32(1.0)  # ticksPerSecond - times below are already seconds
        for ch in channels:
            put_str(str(ch.get("bone", "")))

            positions = ch.get("positions", [])
            put_i32(len(positions))
            for k in positions:
                put_f32(k["time"])
                v = k["value"]
                put_f32(v[0]); put_f32(v[1]); put_f32(v[2])
                put_key_interp(k)

            rotations = ch.get("rotations", [])
            put_i32(len(rotations))
            for k in rotations:
                put_f32(k["time"])
                v = k["value"]  # [x, y, z, w] at this boundary
                put_f32(v[3]); put_f32(v[0]); put_f32(v[1]); put_f32(v[2])
                put_key_interp(k)

            scales = ch.get("scales", [])
            put_i32(len(scales))
            for k in scales:
                put_f32(k["time"])
                v = k["value"]
                put_f32(v[0]); put_f32(v[1]); put_f32(v[2])
                put_key_interp(k)

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(bytes(out))


def _animation_file(proj: Path, animation_name: str) -> Path:
    """Resolve an animation argument to a file under the project."""
    name = animation_name.strip()
    if os.path.isabs(name):
        return Path(name)
    if not name.endswith(".p3da"):
        name += ".p3da"
    if "/" in name:
        return proj / name
    return proj / "assets" / "animations" / name


def _live_animation_open(anim_file: Path) -> bool:
    """True when the running editor has this .p3da open in an Animation Editor tab.

    Edits must go through the editor in that case: it holds the clips in
    memory and would write its own copy over anything changed on disk the
    moment the user pressed Save.

    Deliberately answered from list_animations' openDocuments, NOT by probing
    animation_state with a path: that command *opens* the file when it is not
    already open (which is what an agent naming a file usually wants), so
    using it as a test both answered "yes" every time and popped a tab open
    as a side effect of asking.
    """
    ok, res = _editor_call("list_animations", {})
    if not ok:
        return False
    try:
        target = os.path.realpath(str(anim_file))
    except Exception:
        target = str(anim_file)
    for doc in res.get("openDocuments", []):
        abs_path = doc.get("absolutePath", "")
        if not abs_path:
            continue
        try:
            if os.path.realpath(abs_path) == target:
                return True
        except Exception:
            if abs_path == str(anim_file):
                return True
    return False


def _euler_to_quat_xyzw(euler_degrees: list[float]) -> list[float]:
    """Euler XYZ in degrees -> [x, y, z, w], matching Quaternion::SetRotationFromEuler."""
    rx, ry, rz = (math.radians(v) for v in euler_degrees)
    cx, sx = math.cos(rx * 0.5), math.sin(rx * 0.5)
    cy, sy = math.cos(ry * 0.5), math.sin(ry * 0.5)
    cz, sz = math.cos(rz * 0.5), math.sin(rz * 0.5)
    return [
        sx * cy * cz - cx * sy * sz,
        cx * sy * cz + sx * cy * sz,
        cx * cy * sz - sx * sy * cz,
        cx * cy * cz + sx * sy * sz,
    ]


def _find_clip(clips: list[dict], clip: str | int | None) -> int:
    """Resolve a clip argument (index, name, or None -> 0) to an index."""
    if not clips:
        raise ValueError("this animation has no clips")
    if clip is None:
        return 0
    if isinstance(clip, int):
        if 0 <= clip < len(clips):
            return clip
        raise ValueError(f"clip index {clip} out of range (0..{len(clips) - 1})")
    text = str(clip).strip()
    if text.isdigit():
        idx = int(text)
        if 0 <= idx < len(clips):
            return idx
        raise ValueError(f"clip index {idx} out of range (0..{len(clips) - 1})")
    for i, c in enumerate(clips):
        if c.get("name") == text:
            return i
    raise ValueError(f"no clip named '{text}' (have: {', '.join(str(c.get('name')) for c in clips)})")


def _key_times(channel: dict) -> list[float]:
    times = [k["time"] for k in channel.get("positions", [])]
    times += [k["time"] for k in channel.get("rotations", [])]
    times += [k["time"] for k in channel.get("scales", [])]
    times.sort()
    uniq: list[float] = []
    for t in times:
        if not uniq or abs(t - uniq[-1]) > 0.001:
            uniq.append(t)
    return uniq


@mcp.tool()
def list_projects() -> str:
    """List all Pyros3D projects in the workspace (dirs containing project.json)."""
    results = []
    for proj_dir in sorted(ROOT.rglob("project.json")):
        proj = proj_dir.parent
        if any(part in (".git", "build_gl", "build_metal", "build_vulkan", "build_ed_mtl",
                        "build_editor", "build-editor", "__pycache__", "node_modules", ".fetchcontent-cache")
               for part in proj.parts):
            continue
        try:
            data = json.loads(proj_dir.read_text())
            active = data.get("activeScene", "")
            scenes = data.get("scenes", [])
            results.append(f"Project: {_rel(proj)}  (name={data.get('name', '?')}, "
                           f"scenes={len(scenes)}, active={active or 'none'})")
        except (json.JSONDecodeError, OSError) as e:
            results.append(f"Project: {_rel(proj)}  (unreadable project.json: {e})")
    return "\n".join(results) if results else "No projects found."


@mcp.tool()
def new_project(parent_dir: str, name: str) -> str:
    """Create a new Pyros3D project, exactly like the editor's File > New Project.

    Creates <parent_dir>/<name>/ with project.json, scenes/, and the standard
    assets/ tree (models, textures, sounds, shaders, lua, materials).
    The folder must not already exist.
    """
    parent = Path(parent_dir) if os.path.isabs(parent_dir) else ROOT / parent_dir
    parent = parent.resolve()
    if not parent.is_dir():
        return _fail(f"Parent directory does not exist: {parent_dir}")
    target = parent / name
    if target.exists():
        return _fail(f"Folder already exists: {target}")

    for d in ("assets", "assets/models", "assets/textures", "assets/sounds",
              "assets/shaders", "assets/lua", "assets/materials", "scenes"):
        (target / d).mkdir(parents=True, exist_ok=True)

    data = {
        "name": name,
        "version": 1,
        "activeScene": "",
        "scenes": [],
        "settings": {"defaultMainScript": ""},
    }
    with open(target / "project.json", "w") as f:
        json.dump(data, f, indent=4)

    live_note = _live_open_project(target)
    return f"Created project '{name}' at {target} (relative: {_rel(target)}){live_note}"


@mcp.tool()
def open_project(project_path: str) -> str:
    """Open a project and show its state (name, scenes, active scene, settings)."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    data = _read_project(proj)
    out = {
        "path": str(proj),
        "name": data.get("name", ""),
        "scenes": data.get("scenes", []),
        "activeScene": data.get("activeScene", ""),
        "settings": data.get("settings", {}),
    }
    live_note = _live_open_project(proj)
    return json.dumps(out, indent=2) + live_note


@mcp.tool()
def set_active_scene(project_path: str, scene_name: str) -> str:
    """Set the project's active scene (editor: File > Open Scene / SetActiveSceneRel).

    scene_name may be 'MyScene' or 'scenes/MyScene.json'.
    Also registers the scene in the project's scenes list if missing.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    if not scene_file.exists():
        return _fail(f"Scene not found: {scene_file}")
    rel = scene_file.relative_to(proj).as_posix()
    _register_scene_in_project(proj, rel)

    live_note = ""
    if _live_project_matches(proj):
        ok, res = _editor_call("load_scene", {"path": str(scene_file)})
        live_note = "\nLoaded live in the running editor" if ok else \
            f"\nNOTE: could not load live in the running editor: {res}"

    return f"Active scene set to '{rel}'{live_note}"


@mcp.tool()
def set_project_settings(project_path: str, default_main_script: str | None = None, renderer_type: str | None = None) -> str:
    """Update project.json settings.

    Args:
        project_path: Path to project root
        default_main_script: Default main script (e.g. 'scenes/Default.lua')
        renderer_type: 'forward' (default) or 'deferred'
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    if renderer_type is not None and renderer_type not in ("forward", "deferred"):
        return _fail("renderer_type must be 'forward' or 'deferred'")

    data = _read_project(proj)
    settings = data.setdefault("settings", {})
    if default_main_script is not None:
        settings["defaultMainScript"] = default_main_script
    if renderer_type is not None:
        if renderer_type == "deferred":
            settings["rendererType"] = "deferred"
        else:
            settings.pop("rendererType", None)
    _write_project(proj, data)

    live_note = ""
    if renderer_type is not None and _live_project_matches(proj):
        ok, res = _editor_call("set_renderer", {"type": renderer_type})
        live_note = "\nApplied live in the running editor" if ok else \
            f"\nNOTE: could not apply live in the running editor: {res}"

    return f"Updated project settings: {json.dumps(settings)}{live_note}"


@mcp.tool()
def list_scenes(project_path: str) -> str:
    """List scenes in a project (scenes/*.json, excluding .editor.json sidecars)."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scenes_dir = proj / "scenes"
    if not scenes_dir.exists():
        return "No scenes/ directory."
    data = _read_project(proj)
    active = data.get("activeScene", "")
    results = []
    for f in sorted(scenes_dir.glob("*.json")):
        if ".editor.json" in f.name:
            continue
        rel = f.relative_to(proj).as_posix()
        marker = "  [active]" if rel == active else ""
        companion = " [lua]" if (f.parent / (f.stem + ".lua")).exists() else ""
        results.append(f"{rel}{marker}{companion}")
    return "\n".join(results) if results else "No scenes yet. Use create_scene."


# --------------------------------------------------------------------------
# TOOLS — scene lifecycle
# --------------------------------------------------------------------------

@mcp.tool()
def create_scene(project_path: str, name: str) -> str:
    """Create a new scene, exactly like the editor's Scene > New Scene.

    - writes scenes/<name>.json  (version, mainScript, materials[], roots[])
    - creates the companion Lua script scenes/<name>.lua with the editor's
      exact scene-script template
    - registers the scene in project.json (scenes list + activeScene)
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    if not name or "/" in name or "\\" in name:
        return _fail("Scene name must be a simple name (no path separators)")

    scene_file = proj / "scenes" / f"{name}.json"
    if scene_file.exists():
        return _fail(f"Scene already exists: {_rel(scene_file)}")

    # Editor's WriteEmptySceneFile
    root = {"version": 1, "materials": [], "roots": []}
    root["mainScript"] = f"scenes/{name}.lua"
    _save_scene(scene_file, root)

    # Editor's companion-script template (BuildLuaSnippet, Scene kind)
    lua_file = proj / "scenes" / f"{name}.lua"
    if not lua_file.exists():
        class_name = _sanitize_lua_class_name(name)
        lua_file.write_text(_build_lua_snippet("scene", class_name, f"scenes/{name}.lua"))

    rel = f"scenes/{name}.json"
    _register_scene_in_project(proj, rel)

    live_note = ""
    if _live_project_matches(proj):
        ok, res = _editor_call("load_scene", {"path": str(scene_file)})
        live_note = f"\nLoaded live in the running editor" if ok else \
            f"\nNOTE: could not load live in the running editor: {res}"

    return (f"Created scene '{name}':\n  - {_rel(scene_file)}\n  - {_rel(lua_file)}\n"
            f"  Registered in project.json (activeScene={rel}){live_note}")


@mcp.tool()
def open_scene(project_path: str, scene_name: str) -> str:
    """Open and display a scene: metadata + full object tree with components."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    data = _load_scene(scene_file)

    def tree(node, depth=0):
        comps = [c.get("type", "?") for c in node.get("components", []) if isinstance(c, dict)]
        line = "  " * depth + f"- {node.get('name', '?')}"
        if comps:
            line += f"  [{', '.join(comps)}]"
        out = [line]
        for child in node.get("children", []):
            out.extend(tree(child, depth + 1))
        return out

    object_tree = "\n".join(sum((tree(n) for n in data.get("roots", [])), [])) or "(empty scene)"
    info = {
        "file": _rel(scene_file),
        "size_bytes": scene_file.stat().st_size,
        "version": data.get("version"),
        "mainScript": data.get("mainScript", ""),
        "materials_count": len(data.get("materials", [])),
        "objects": object_tree,
    }
    sidecar = _scene_sidecar(scene_file)
    if sidecar.exists():
        info["editor_sidecar"] = _rel(sidecar)
    return json.dumps(info, indent=2)


@mcp.tool()
def get_scene_objects(project_path: str, scene_name: str) -> str:
    """List all game objects in a scene with their components (flat list).

    If a running editor has this scene open, returns the LIVE hierarchy
    (including runtime components and current transforms) instead of the
    on-disk file.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    live = _live_or_none("scene_state", {}, scene_file)
    if isinstance(live, dict):
        out = []
        def _walk(nodes):
            for n in nodes:
                if not isinstance(n, dict):
                    continue
                out.append({
                    "name": n.get("name", "unnamed"),
                    "components": [c.get("type", "?") for c in n.get("components", []) if isinstance(c, dict)],
                    "position": n.get("position"),
                    "scale": n.get("scale"),
                    "tags": n.get("tags", []),
                    "children_count": len(n.get("children", [])),
                })
                _walk(n.get("children", []))
        _walk(live.get("objects", []))
        return json.dumps(out[:200], indent=2) + "\n(live editor state)"

    data = _load_scene(scene_file)
    objects = []
    for node in _iter_objects(data):
        comps = [c.get("type", "?") for c in node.get("components", []) if isinstance(c, dict)]
        objects.append({
            "name": node.get("name", "unnamed"),
            "components": comps,
            "position": node.get("position"),
            "scale": node.get("scale"),
            "tags": node.get("tags", []),
            "children_count": len(node.get("children", [])),
        })
    return json.dumps(objects[:200], indent=2)


@mcp.tool()
def get_object_properties(project_path: str, scene_name: str, name: str) -> str:
    """Get the full serialized properties of a game object (transform, components, tags)."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    data = _load_scene(scene_file)
    node = _find_object(data, name)
    if node is None:
        return _fail(f"Object '{name}' not found in scene {scene_name}")
    return json.dumps(node, indent=2)


@mcp.tool()
def find_game_objects_with_component(project_path: str, scene_name: str, component_type: str) -> str:
    """Find all game objects that have a specific component type (e.g. 'RenderingComponent', 'PointLight')."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    data = _load_scene(scene_file)
    results = []
    for node in _iter_objects(data):
        for c in node.get("components", []):
            if isinstance(c, dict) and c.get("type") == component_type:
                results.append(node.get("name", "unnamed"))
                break
    return "\n".join(results) if results else f"No objects with {component_type} found"


# --------------------------------------------------------------------------
# TOOLS — object creation (editor Add menu)
# --------------------------------------------------------------------------

@mcp.tool()
def add_game_object(project_path: str, scene_name: str, name: str, parent_name: str | None = None, position: list[float] | None = None, rotation: list[float] | None = None, scale: list[float] | None = None) -> str:
    """Add a plain game object (editor: Add > Game Object).

    rotation is euler angles [x, y, z] in radians (the editor's format).
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    live = _live_or_none("add_object", {
        "name": name, "parent": parent_name or "",
        "position": position, "rotation": rotation, "scale": scale,
    }, scene_file)
    if live is not None:
        return _fail(live) if isinstance(live, str) else f"Added game object '{name}' (live editor)"

    data = _load_scene(scene_file)
    final_name = _unique_scene_name(data, name)
    obj = _new_game_object(final_name, position, rotation, scale)
    a_err = _add_to_scene(data, obj, parent_name)
    if a_err:
        return _fail(a_err)
    _save_scene(scene_file, data)
    return f"Added game object '{final_name}' to scene {scene_name}"


@mcp.tool()
def add_camera(project_path: str, scene_name: str, name: str = "Camera", parent_name: str | None = None, position: list[float] | None = None, rotation: list[float] | None = None, fov: float = 70.0, near: float = 0.1, far: float = 2000.0, active: bool = True) -> str:
    """Add a scene camera (editor: Add > Camera).

    Creates a GameObject tagged 'PyrosEditor.Camera' and records its
    projection (fov/near/far) in the scene's .editor.json sidecar, which is
    where the editor stores camera settings.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    live = _live_or_none("add_camera", {
        "name": name, "parent": parent_name or "",
        "position": position or [0.0, 10.0, 20.0],
        "fov": fov, "near": near, "far": far, "active": active,
    }, scene_file)
    if live is not None:
        return _fail(live) if isinstance(live, str) else f"Added camera '{name}' (live editor)"

    data = _load_scene(scene_file)
    final_name = _unique_scene_name(data, name)
    obj = _new_game_object(final_name, position or [0.0, 10.0, 20.0], rotation, None)
    a_err = _add_to_scene(data, obj, parent_name)
    if a_err:
        return _fail(a_err)
    if CAMERA_TAG not in obj["tags"]:
        obj["tags"].append(CAMERA_TAG)
    _save_scene(scene_file, data)

    sidecar = _read_sidecar(scene_file)
    sidecar.setdefault("cameras", {})[final_name] = {"fov": fov, "near": near, "far": far}
    if active:
        sidecar["activeCamera"] = final_name
    _write_sidecar(scene_file, sidecar)
    return f"Added camera '{final_name}' (fov={fov}, near={near}, far={far}, active={active})"


def _add_rendering_object(data: dict, name: str, parent_name: str | None, position, rotation, scale,
                          renderable: dict, material_overrides: dict | None = None) -> tuple[str, str | None]:
    """Create a GO with a RenderingComponent + pooled material (editor style)."""
    if "materials" not in data:
        data["materials"] = []
    mat = _default_material_entry(len(data["materials"]))
    if material_overrides:
        mat.update(material_overrides)
    data["materials"].append(mat)
    mat_id = mat["id"]

    obj = _new_game_object(name, position, rotation, scale)
    obj["components"].append({
        "type": "RenderingComponent",
        "cullTest": True,
        "castingShadows": True,
        "material": mat_id,
        "renderable": renderable,
    })
    err = _add_to_scene(data, obj, parent_name)
    return name, err


@mcp.tool()
def add_primitive(project_path: str, scene_name: str, name: str, shape: str = "Cube",
                 parent_name: str | None = None, position: list[float] | None = None,
                 rotation: list[float] | None = None, scale: list[float] | None = None,
                 color: list[float] | None = None, **params: Any) -> str:
    """Add a primitive mesh with a RenderingComponent (editor: Add > Mesh > Primitives).

    shapes: Cube, Sphere, Cone, Cylinder, Plane, Capsule, Torus, TorusKnot
    shape params (kwargs): width, height, depth, radius, tube, segmentsW, segmentsH,
                 numRings, openEnded, halfSphere, p, q, heightScale
    color: [r, g, b, a] (0..1). rotation: euler radians [x, y, z].
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    P = lambda key, default: float(params.get(key, default))

    valid = ["Cube", "Sphere", "Cone", "Cylinder", "Plane", "Capsule", "Torus", "TorusKnot"]
    if shape not in valid:
        return _fail(f"Invalid shape '{shape}'. Use: {', '.join(valid)}")

    base = {"kind": "primitive", "smooth": False, "flip": False, "tangentBitangent": False, "shape": shape}
    if shape == "Cube":
        base.update({"width": P("width", 1.0), "height": P("height", 1.0), "depth": P("depth", 1.0)})
    elif shape == "Sphere":
        base.update({"radius": P("radius", 1.0), "segmentsW": int(P("segmentsW", 8)),
                     "segmentsH": int(P("segmentsH", 6)), "halfSphere": bool(params.get("halfSphere", False))})
    elif shape in ("Cone", "Cylinder"):
        base.update({"radius": P("radius", 1.0), "height": P("height", 1.0),
                     "segmentsW": int(P("segmentsW", 8)), "segmentsH": int(P("segmentsH", 6)),
                     "openEnded": bool(params.get("openEnded", False))})
    elif shape == "Plane":
        base.update({"width": P("width", 1.0), "height": P("height", 1.0)})
    elif shape == "Capsule":
        base.update({"radius": P("radius", 1.0), "height": P("height", 1.0),
                     "numRings": int(P("numRings", 8)), "segmentsW": int(P("segmentsW", 8)),
                     "segmentsH": int(P("segmentsH", 6))})
    elif shape == "Torus":
        base.update({"radius": P("radius", 1.0), "tube": P("tube", 1.0),
                     "segmentsW": int(P("segmentsW", 8)), "segmentsH": int(P("segmentsH", 6))})
    elif shape == "TorusKnot":
        base.update({"radius": P("radius", 1.0), "tube": P("tube", 1.0),
                     "segmentsW": int(P("segmentsW", 8)), "segmentsH": int(P("segmentsH", 6)),
                     "p": P("p", 1.0), "q": P("q", 1.0), "heightScale": P("heightScale", 1.0)})

    live = _live_or_none("add_primitive", {
        "name": name, "shape": shape, "parent": parent_name or "",
        "color": color, "position": position, "rotation": rotation, "scale": scale,
        **{k: v for k, v in params.items()},
    }, scene_file)
    if live is not None:
        return _fail(live) if isinstance(live, str) else f"Added {shape} '{name}' (live editor)"

    data = _load_scene(scene_file)
    final_name = _unique_scene_name(data, name)
    overrides = {"color": list(color)} if color else None
    final_name, a_err = _add_rendering_object(data, final_name, parent_name, position, rotation, scale, base, overrides)
    if a_err:
        return _fail(a_err)
    _save_scene(scene_file, data)
    return f"Added {shape} '{final_name}' to scene {scene_name} (material #{len(data['materials']) - 1})"


@mcp.tool()
def add_sprite(project_path: str, scene_name: str, name: str, texture: str | None = None,
               parent_name: str | None = None) -> str:
    """Add a 2D sprite - a textured, alpha-blended quad (editor: Add > Sprite).

    A sprite is a RenderingComponent, not a component type of its own: the
    shortcut is that it gets its own material with blending on, double-sided
    culling, and a size taken from the texture's pixel aspect. Omit `texture`
    for a white placeholder you can texture later.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    live = _live_or_none("add_sprite", {
        "name": name, "texture": texture or "", "parent": parent_name or "",
    }, scene_file)
    if live is not None:
        return _fail(live) if isinstance(live, str) else f"Added sprite '{name}' (live editor)"
    return _fail("add_sprite needs the editor open on this scene - it builds a material, "
                 "which the offline scene-file path does not do.")


@mcp.tool()
def sprite_2d_lit(project_path: str, scene_name: str, name: str) -> str:
    """Switch a sprite to 2D lighting (editor: Material Settings > Use 2D Lighting).

    Distance falloff with no N.L term. A sprite is a flat quad with one normal,
    so a light lying in its own plane - which is where 2D authoring puts one -
    is at grazing incidence and leaves it unlit otherwise. Rebuilds the
    material, because ShaderUsage is fixed when a material is constructed.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    live = _live_or_none("sprite_2d_lit", {"name": name}, scene_file)
    if live is not None:
        return _fail(live) if isinstance(live, str) else f"'{name}' now uses 2D lighting (live editor)"
    return _fail("sprite_2d_lit needs the editor open on this scene - it rebuilds a material.")


def _live_2d_anim(project_path: str, scene_name: str, cmd: str, args: dict, ok_msg: str) -> str:
    """Shared body for the 2D animation tools.

    All of these touch live runtime state - a skeleton instance, a pose, a clip
    being played - which only exists inside a running editor. There is no
    offline file path for them the way there is for adding a component, so they
    say so plainly instead of silently doing nothing.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    live = _live_or_none(cmd, args, scene_file)
    if live is None:
        return _fail(f"{cmd} needs the editor open on this scene - it works on the live rig.")
    if isinstance(live, str):
        return _fail(live)
    return ok_msg


@mcp.tool()
def slice_spritesheet(project_path: str, scene_name: str, name: str, sheet: str,
                      cols: int = 1, rows: int = 1, fps: float = 12.0, loop: bool = True) -> str:
    """Cut a spritesheet into frames and play them on an object's sprite.

    Cells are taken row-major and written as real PNGs beside the sheet, so the
    scene stores frame paths rather than embedding every frame.
    """
    return _live_2d_anim(project_path, scene_name, "slice_spritesheet",
                         {"object": name, "sheet": sheet, "cols": cols, "rows": rows,
                          "fps": fps, "loop": loop},
                         f"sliced {sheet} into {cols * rows} frames on '{name}' at {fps} fps")


@mcp.tool()
def set_sprite_pivot(project_path: str, scene_name: str, name: str, x: float, y: float) -> str:
    """Set a sprite's pivot, normalized over its own bounds.

    (0.5, 0.5) is the middle, (0.5, 0) the bottom edge. The pivot is what a
    cutout limb rotates about, so it matters before binding sprites to bones.
    """
    return _live_2d_anim(project_path, scene_name, "set_pivot",
                         {"object": name, "pivot": [x, y]},
                         f"'{name}' pivot set to ({x}, {y})")


def _live_character2d(project_path: str, cmd: str, args: dict, ok_msg: str) -> str:
    """Shared body for the 2D character tools.

    Keyed to the PROJECT, not to a scene: a character is an asset
    (assets/characters/*.p3d2d) that owns its own bones, artwork and clips, and
    scenes only place one. It is edited in its own editor window, so all of
    these need a running editor with the project open - there is no offline
    file path for posing a rig or scrubbing a clip.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    if not _live_project_matches(proj):
        return _fail(f"{cmd} needs the editor open on this project - "
                     "2D characters are edited live, in the Character 2D window.")
    ok, res = _editor_call(cmd, args)
    if not ok:
        return _fail(str(res))
    return ok_msg


@mcp.tool()
def new_character2d(project_path: str, name: str = "NewCharacter") -> str:
    """Start a new 2D character (.p3d2d) and open it for editing.

    A character is bones + artwork + clips in ONE asset. Build it with
    add_bone2d -> add_sprite2d -> new_clip2d/pose_bone2d/key_pose2d, then
    save_character2d. Scenes place a finished one with add_character2d.
    """
    return _live_character2d(project_path, "new_character2d", {"name": name},
                             f"new 2D character '{name}' - remember to save_character2d")


@mcp.tool()
def open_character2d(project_path: str, path: str) -> str:
    """Open an existing .p3d2d for editing. `path` is project-relative."""
    return _live_character2d(project_path, "open_character2d", {"path": path},
                             f"opened character '{path}'")


@mcp.tool()
def save_character2d(project_path: str, path: str = "") -> str:
    """Write the open character to disk. `path` is optional once saved before."""
    args = {"path": path} if path else {}
    return _live_character2d(project_path, "save_character2d", args,
                             "character saved")


@mcp.tool()
def character2d_state(project_path: str) -> str:
    """Everything about the open 2D character: bones (rest AND posed), sprites,
    clips with their key counts, the playhead, and the default clip."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    if not _live_project_matches(proj):
        return _fail("character2d_state needs the editor open on this project.")
    ok, res = _editor_call("character2d_state", {})
    if not ok:
        return _fail(str(res))
    return json.dumps(res, indent=2)


@mcp.tool()
def add_bone2d(project_path: str, bone: str, parent: str = "",
               x: float = 0.0, y: float = 0.0) -> str:
    """Add a bone to the open character. Position is local to the parent bone.

    Leave `parent` empty for a root. Bones are what the artwork follows and
    what clips animate.
    """
    return _live_character2d(project_path, "add_bone2d",
                             {"bone": bone, "parent": parent, "pos": [x, y]},
                             f"added bone '{bone}'")


@mcp.tool()
def remove_bone2d(project_path: str, bone: str) -> str:
    """Remove a bone and everything under it.

    Sprites pinned to any of them are UNPINNED, not deleted - the artwork is
    still wanted, it just has nothing to follow.
    """
    return _live_character2d(project_path, "remove_bone2d", {"bone": bone},
                             f"removed bone '{bone}' and its descendants")


@mcp.tool()
def rename_bone2d(project_path: str, bone: str, to: str) -> str:
    """Rename a bone, following through into the artwork pinned to it and the
    clip channels animating it."""
    return _live_character2d(project_path, "rename_bone2d", {"bone": bone, "to": to},
                             f"renamed '{bone}' to '{to}'")


@mcp.tool()
def reparent_bone2d(project_path: str, bone: str, parent: str = "") -> str:
    """Change which bone drives another, keeping it where it is on screen.

    Rejects a cycle. Empty `parent` makes it a root.
    """
    return _live_character2d(project_path, "reparent_bone2d",
                             {"bone": bone, "parent": parent},
                             f"reparented '{bone}' under '{parent or '(root)'}'")


@mcp.tool()
def set_bone2d(project_path: str, bone: str, x: float = 0.0, y: float = 0.0,
               rotation: float = 0.0) -> str:
    """Move a bone's REST pose - the character's SHAPE, not its animation.

    Rotation is in degrees about z.
    """
    return _live_character2d(project_path, "set_bone2d",
                             {"bone": bone, "pos": [x, y], "rotation": rotation},
                             f"bone '{bone}' rest pose set")


@mcp.tool()
def add_sprite2d(project_path: str, name: str, texture: str = "", bone: str = "") -> str:
    """Pin artwork to a bone. This is what makes a character visible.

    `texture` is project-root relative and keeps the assets/ prefix
    ("assets/textures/torso.png") - the same form scenes use.
    """
    return _live_character2d(project_path, "add_sprite2d",
                             {"name": name, "texture": texture, "bone": bone},
                             f"added sprite '{name}'")


@mcp.tool()
def remove_sprite2d(project_path: str, name: str) -> str:
    """Remove one piece of artwork from the open character."""
    return _live_character2d(project_path, "remove_sprite2d", {"name": name},
                             f"removed sprite '{name}'")


@mcp.tool()
def set_sprite2d(project_path: str, name: str, bone: str = None, texture: str = None,
                 offset_x: float = None, offset_y: float = None,
                 scale_x: float = None, scale_y: float = None,
                 pivot_x: float = None, pivot_y: float = None,
                 z: float = None, lit: bool = None) -> str:
    """Change a sprite. Every field is optional; only what you pass is changed.

    `pivot` is where the artwork's own origin sits, normalized (0.5,0.5 is
    centred): put it on the joint the limb turns about, or the limb rotates
    about the middle of its texture. `z` is draw order within the character.
    """
    args = {"name": name}
    if bone is not None: args["bone"] = bone
    if texture is not None: args["texture"] = texture
    if offset_x is not None and offset_y is not None: args["offset"] = [offset_x, offset_y]
    if scale_x is not None and scale_y is not None: args["scale"] = [scale_x, scale_y]
    if pivot_x is not None and pivot_y is not None: args["pivot"] = [pivot_x, pivot_y]
    if z is not None: args["z"] = z
    if lit is not None: args["lit"] = lit
    return _live_character2d(project_path, "set_sprite2d", args, f"sprite '{name}' updated")


@mcp.tool()
def new_clip2d(project_path: str, clip: str, duration: float = 1.0) -> str:
    """Add an animation clip to the open character."""
    return _live_character2d(project_path, "new_clip2d",
                             {"clip": clip, "duration": duration},
                             f"added clip '{clip}' ({duration}s)")


@mcp.tool()
def remove_clip2d(project_path: str, clip: str) -> str:
    """Remove a clip from the open character."""
    return _live_character2d(project_path, "remove_clip2d", {"clip": clip},
                             f"removed clip '{clip}'")


@mcp.tool()
def rename_clip2d(project_path: str, clip: str, to: str) -> str:
    """Rename a clip, following through into the character's default clip.

    Scenes address a clip by NAME, so a scene naming the old one stops
    auto-playing - which is why this is worth doing here rather than by hand.
    """
    return _live_character2d(project_path, "rename_clip2d", {"clip": clip, "to": to},
                             f"renamed clip '{clip}' to '{to}'")


@mcp.tool()
def set_default_clip2d(project_path: str, clip: str = "", loop: bool = True) -> str:
    """The clip this character starts on wherever it is placed, unless a scene
    overrides it. Empty clears it."""
    return _live_character2d(project_path, "set_default_clip2d",
                             {"clip": clip, "loop": loop},
                             f"default clip set to '{clip or '(none)'}'")


@mcp.tool()
def pose_bone2d(project_path: str, bone: str, rotation: float) -> str:
    """Rotate a bone on the LIVE rig without keying it. Degrees about z.

    Follow with key_pose2d to store the pose into a clip.
    """
    return _live_character2d(project_path, "pose_bone2d",
                             {"bone": bone, "rotation": rotation},
                             f"posed '{bone}' to {rotation} degrees (not keyed yet)")


@mcp.tool()
def ik_solve2d(project_path: str, root: str, effector: str, x: float, y: float) -> str:
    """Pose a limb by dragging its tip: solves the chain from `root` to
    `effector` so the effector reaches (x, y) in the character's space.

    Root the chain at the LIMB (upper arm -> hand), not at the spine: rooting
    it at the body makes the solver rotate the torso and the character comes
    apart. An authoring aid - key_pose2d stores the result as ordinary
    rotation keys.
    """
    return _live_character2d(project_path, "ik_solve2d",
                             {"root": root, "effector": effector, "target": [x, y]},
                             f"solved {root} -> {effector} to ({x}, {y}) (not keyed yet)")


@mcp.tool()
def key_pose2d(project_path: str, clip: str = "", time: float = 0.0, bone: str = "") -> str:
    """Key whatever is posed right now into a clip at a time.

    `bone` restricts it to one bone; omit to key everything that has been
    posed. Keys rotation only by default - a cutout limb's offset from its
    joint is the character's shape, not its animation.
    """
    args = {"time": time}
    if clip: args["clip"] = clip
    if bone: args["bone"] = bone
    return _live_character2d(project_path, "key_pose2d", args,
                             f"keyed at {time}s")


@mcp.tool()
def delete_key2d(project_path: str, clip: str, bone: str, time: float) -> str:
    """Remove a bone's key at a time."""
    return _live_character2d(project_path, "delete_key2d",
                             {"clip": clip, "bone": bone, "time": time},
                             f"deleted '{bone}' key at {time}s in '{clip}'")


@mcp.tool()
def scrub_clip2d(project_path: str, clip: str = "", time: float = 0.0) -> str:
    """Move the playhead and pose the character from the clip there.

    This is how you check what a clip actually looks like - follow with
    character2d_state to read the posed bone positions back.
    """
    args = {"time": time}
    if clip: args["clip"] = clip
    return _live_character2d(project_path, "scrub_clip2d", args,
                             f"scrubbed '{clip or '(current clip)'}' to {time}s")


@mcp.tool()
def character2d_undo_redo(project_path: str, action: str = "undo") -> str:
    """Undo or redo the last edit to the open 2D character.

    One history for the whole character - bones, artwork AND keyframes - so
    this is the same Ctrl+Z the editor does. Separate from the scene's, the
    material editor's and the .p3da animation editor's, which each own theirs.
    """
    if action not in ("undo", "redo"):
        return _fail("action must be 'undo' or 'redo'")
    return _live_character2d(project_path, f"{action}_character2d", {},
                             f"{action} ok")


@mcp.tool()
def add_character2d(project_path: str, scene_name: str, character: str,
                    name: str = "", x: float = 0.0, y: float = 0.0, z: float = 0.0) -> str:
    """Place a finished 2D character in a scene.

    The scene stores only which character and which clip - everything about
    what the character IS lives in the .p3d2d.
    """
    return _live_2d_anim(project_path, scene_name, "add_character2d",
                         {"character": character, "name": name, "position": [x, y, z]},
                         f"placed character '{character}' as '{name or character}'")


@mcp.tool()
def set_autoplay2d(project_path: str, scene_name: str, name: str,
                   clip: str = "", loop: bool = True) -> str:
    """Choose which clip a placed character starts on when the game runs.

    Empty `clip` clears it, falling back to the character's own default.
    """
    return _live_2d_anim(project_path, scene_name, "set_autoplay2d",
                         {"object": name, "clip": clip, "loop": loop},
                         f"'{name}' will start on '{clip or '(the character default)'}'")
def _add_simple_component(project_path: str, scene_name: str, name: str,
                          cmd: str, component: dict, what: str) -> str:
    """Shared body for the 2D components that are pure data: live if the editor
    has the scene, otherwise appended straight to the scene file."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    live = _live_or_none(cmd, {"name": name}, scene_file)
    if live is not None:
        return _fail(live) if isinstance(live, str) else f"Added {what} to '{name}' (live editor)"

    data = _load_scene(scene_file)
    for root in data.get("roots", []):
        if root.get("name") == name:
            # One per object: a second Layer2D or Occluder2D is not a richer
            # setup, it is one of them being silently ignored.
            existing = root.setdefault("components", [])
            if any(c.get("type") == component["type"] for c in existing):
                return _fail(f"'{name}' already has a {component['type']}")
            existing.append(component)
            _save_scene(scene_file, data)
            return f"Added {what} to '{name}' in scene {scene_name}"
    return _fail(f"object '{name}' not found in scene {scene_name}")


@mcp.tool()
def add_layer2d(project_path: str, scene_name: str, name: str,
                parallax: list[float] | None = None, visible: bool = True) -> str:
    """Make an object's subtree a 2D layer (editor: Add > Layer 2D).

    Ordering is the object's own z - under the orthographic camera a 2D scene
    uses, z is draw order. parallax is [x, y]: 1 moves with the camera, 0 is
    pinned, 0.5 is half speed. The axes are independent, so a sky can scroll
    sideways and stay put vertically.
    """
    return _add_simple_component(project_path, scene_name, name, "add_layer2d", {
        "type": "Layer2D",
        "parallax": list(parallax) if parallax else [1.0, 1.0],
        "visible": bool(visible),
    }, "Layer2D")


@mcp.tool()
def add_physics2d(project_path: str, scene_name: str, name: str,
                  body_type: str = "Dynamic", shape: str = "Box",
                  size: list[float] | None = None, density: float = 1.0,
                  friction: float = 0.3, restitution: float = 0.0,
                  fixed_rotation: bool = False, casts_shadow: bool = True) -> str:
    """Add a Box2D rigid body (editor: Add > Physics 2D).

    body_type: Static (never moves), Kinematic (moved by script, ignores
    forces), Dynamic (moved by the solver). shape: Box or Circle.
    size is HALF-extents, matching Box2D - a 1x1 box is [0.5, 0.5]; for a
    circle only the first value is used, as the radius.
    """
    bt = {"static": 0, "kinematic": 1, "dynamic": 2}.get(body_type.strip().lower())
    if bt is None:
        return _fail("body_type must be Static, Kinematic or Dynamic")
    sh = {"box": 0, "circle": 1}.get(shape.strip().lower())
    if sh is None:
        return _fail("shape must be Box or Circle")
    return _add_simple_component(project_path, scene_name, name, "add_physics2d", {
        "type": "Physics2D", "bodyType": bt, "shape": sh,
        "size": list(size) if size else [0.5, 0.5],
        "density": density, "friction": friction, "restitution": restitution,
        "fixedRotation": bool(fixed_rotation), "castsShadow": bool(casts_shadow),
    }, "Physics2D")


@mcp.tool()
def add_occluder2d(project_path: str, scene_name: str, name: str,
                   shape: str = "Box", size: list[float] | None = None,
                   enabled: bool = True) -> str:
    """Mark a shape as blocking 2D light (editor: Add > Occluder 2D).

    No physics involved - a painted backdrop can cast a shadow without being
    solid to the simulation, and a trigger volume can be solid without casting.
    size is HALF-extents, as with add_physics2d.

    Budget: 32 segments per scene, four to a box and eight to a circle.
    Occluders past that cast nothing.
    """
    sh = {"box": 0, "circle": 1}.get(shape.strip().lower())
    if sh is None:
        return _fail("shape must be Box or Circle")
    return _add_simple_component(project_path, scene_name, name, "add_occluder2d", {
        "type": "Occluder2D", "shape": sh,
        "size": list(size) if size else [0.5, 0.5],
        "enabled": bool(enabled),
    }, "Occluder2D")


@mcp.tool()
def add_model(project_path: str, scene_name: str, model_file: str, name: str | None = None,
              parent_name: str | None = None, position: list[float] | None = None,
              rotation: list[float] | None = None, scale: list[float] | None = None) -> str:
    """Add a 3D model to the scene (editor: Add > Mesh > Import Model).

    model_file: .p3dm inside the project, or a source format (.obj/.fbx/.gltf/...)
    anywhere — source formats are imported through the editor's AssimpImporter
    pipeline into assets/models/<stem>/ first.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    src = Path(model_file) if os.path.isabs(model_file) else ROOT / model_file
    src = src.resolve()
    if not src.exists():
        return _fail(f"Model file not found: {model_file}")

    if src.suffix.lower() == ".p3dm":
        if proj in src.parents:
            p3dm_path = src.relative_to(proj).as_posix()
        else:
            dst_dir = proj / "assets" / "models" / src.stem
            dst_dir.mkdir(parents=True, exist_ok=True)
            dst = dst_dir / (src.stem + ".p3dm")
            shutil.copy2(src, dst)
            p3dm_path = dst.relative_to(proj).as_posix()
    else:
        if src.suffix.lower() not in MODEL_SOURCE_EXTS:
            return _fail(f"Unsupported model format: {src.suffix} (use .p3dm or one of {sorted(MODEL_SOURCE_EXTS)})")
        model_dir = proj / "assets" / "models" / src.stem
        p3dm, c_err = _convert_model(src, model_dir)
        if p3dm is None:
            return _fail(c_err)
        p3dm_path = p3dm.relative_to(proj).as_posix()

    live = _live_or_none("add_model", {
        "name": name or src.stem, "file": str(src), "parent": parent_name or "",
    }, scene_file)
    if live is not None:
        return _fail(live) if isinstance(live, str) else f"Added model '{name or src.stem}' (live editor)"

    data = _load_scene(scene_file)
    obj_name = name or src.stem
    final_name = _unique_scene_name(data, obj_name)
    renderable = {"kind": "model", "path": p3dm_path, "mergeMeshes": True}
    final_name, a_err = _add_rendering_object(data, final_name, parent_name, position, rotation, scale, renderable)
    if a_err:
        return _fail(a_err)
    _save_scene(scene_file, data)
    return f"Added model '{final_name}' ({p3dm_path}) to scene {scene_name}"


@mcp.tool()
def add_directional_light(project_path: str, scene_name: str, name: str = "Sun", parent_name: str | None = None,
                          position: list[float] | None = None, color: list[float] | None = None,
                          direction: list[float] | None = None, intensity: float = 1.0,
                          casting_shadows: bool = False) -> str:
    """Add a directional light (editor: Add > Lights > Directional)."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    live = _live_or_none("add_light", {
        "name": name, "type": "DirectionalLight", "parent": parent_name or "",
        "position": position, "color": color, "direction": direction,
        "intensity": intensity,
    }, scene_file)
    if live is not None:
        return _fail(live) if isinstance(live, str) else f"Added DirectionalLight '{name}' (live editor)"

    data = _load_scene(scene_file)
    final_name = _unique_scene_name(data, name)
    obj = _new_game_object(final_name, position, None, None)
    comp = {
        "type": "DirectionalLight",
        "color": list(color) if color else [1.0, 1.0, 1.0, 1.0],
        "direction": list(direction) if direction else [0.0, -1.0, 0.0],
        "intensity": intensity,
        "castingShadows": bool(casting_shadows),
    }
    obj["components"].append(comp)
    a_err = _add_to_scene(data, obj, parent_name)
    if a_err:
        return _fail(a_err)
    _save_scene(scene_file, data)
    return f"Added DirectionalLight '{final_name}' to scene {scene_name}"


@mcp.tool()
def add_point_light(project_path: str, scene_name: str, name: str = "PointLight", parent_name: str | None = None,
                    position: list[float] | None = None, color: list[float] | None = None,
                    radius: float = 10.0, intensity: float = 1.0, casting_shadows: bool = False,
                    volumetric_scattering: float = 0.0, volumetric_anisotropy: float = 0.6,
                    volumetric_steps: int = 32) -> str:
    """Add a point light (editor: Add > Lights > Point)."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    live = _live_or_none("add_light", {
        "name": name, "type": "PointLight", "parent": parent_name or "",
        "position": position, "color": color,
        "radius": radius, "intensity": intensity,
    }, scene_file)
    if live is not None:
        return _fail(live) if isinstance(live, str) else f"Added PointLight '{name}' (live editor)"

    data = _load_scene(scene_file)
    final_name = _unique_scene_name(data, name)
    obj = _new_game_object(final_name, position, None, None)
    comp = {
        "type": "PointLight",
        "color": list(color) if color else [1.0, 1.0, 1.0, 1.0],
        "intensity": intensity,
        "radius": radius,
        "castingShadows": bool(casting_shadows),
        "volumetricScattering": volumetric_scattering,
        "volumetricAnisotropy": volumetric_anisotropy,
        "volumetricSteps": volumetric_steps,
    }
    obj["components"].append(comp)
    a_err = _add_to_scene(data, obj, parent_name)
    if a_err:
        return _fail(a_err)
    _save_scene(scene_file, data)
    return f"Added PointLight '{final_name}' to scene {scene_name}"


@mcp.tool()
def add_spot_light(project_path: str, scene_name: str, name: str = "SpotLight", parent_name: str | None = None,
                   position: list[float] | None = None, color: list[float] | None = None,
                   radius: float = 10.0, direction: list[float] | None = None,
                   intensity: float = 1.0, inner_cone: float = 30.0, outer_cone: float = 45.0,
                   casting_shadows: bool = False, volumetric_scattering: float = 0.0,
                   volumetric_anisotropy: float = 0.6, volumetric_steps: int = 32) -> str:
    """Add a spot light (editor: Add > Lights > Spot)."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    live = _live_or_none("add_light", {
        "name": name, "type": "SpotLight", "parent": parent_name or "",
        "position": position, "color": color,
        "radius": radius, "direction": direction,
        "intensity": intensity, "inner": inner_cone, "outer": outer_cone,
    }, scene_file)
    if live is not None:
        return _fail(live) if isinstance(live, str) else f"Added SpotLight '{name}' (live editor)"

    data = _load_scene(scene_file)
    final_name = _unique_scene_name(data, name)
    obj = _new_game_object(final_name, position, None, None)
    comp = {
        "type": "SpotLight",
        "color": list(color) if color else [1.0, 1.0, 1.0, 1.0],
        "intensity": intensity,
        "radius": radius,
        "direction": list(direction) if direction else [0.0, -1.0, 0.0],
        "innerCone": inner_cone,
        "outterCone": outer_cone,  # editor's spelling (SceneSerializer)
        "castingShadows": bool(casting_shadows),
        "volumetricScattering": volumetric_scattering,
        "volumetricAnisotropy": volumetric_anisotropy,
        "volumetricSteps": volumetric_steps,
    }
    obj["components"].append(comp)
    a_err = _add_to_scene(data, obj, parent_name)
    if a_err:
        return _fail(a_err)
    _save_scene(scene_file, data)
    return f"Added SpotLight '{final_name}' to scene {scene_name}"


@mcp.tool()
def add_light(project_path: str, scene_name: str, name: str, light_type: str = "DirectionalLight",
              parent_name: str | None = None, position: list[float] | None = None,
              color: list[float] | None = None, intensity: float = 1.0, **params: Any) -> str:
    """Add a light. light_type: 'DirectionalLight' | 'PointLight' | 'SpotLight'.

    Extra params: radius (point/spot), direction (dir/spot), inner_cone, outer_cone.
    Prefer the dedicated add_directional_light / add_point_light / add_spot_light.
    """
    t = (light_type or "DirectionalLight").strip()
    if t in ("DirectionalLight", "directional"):
        return add_directional_light(project_path, scene_name, name, parent_name, position, color,
                                     params.get("direction"), intensity, params.get("casting_shadows", False))
    if t in ("PointLight", "point"):
        return add_point_light(project_path, scene_name, name, parent_name, position, color,
                               params.get("radius", 10.0), intensity)
    if t in ("SpotLight", "spot"):
        return add_spot_light(project_path, scene_name, name, parent_name, position, color,
                              params.get("radius", 10.0), params.get("direction"), intensity,
                              params.get("inner_cone", 30.0), params.get("outer_cone", 45.0))
    return _fail(f"Invalid light type '{light_type}'. Use DirectionalLight, PointLight or SpotLight.")


PARTICLE_PRESETS = {
    "default": {},
    "fire": {
        "looping": True, "emissionRate": 60.0, "burstCount": 1,
        "minLifetime": 0.6, "maxLifetime": 1.2,
        "direction": [0.0, 1.0, 0.0], "spreadAngle": math.radians(20.0),
        "minSpeed": 1.2, "maxSpeed": 2.4,
        "gravity": [0.0, 0.6, 0.0], "damping": 0.8,
        "startSize": 0.7, "endSize": 0.15,
        "startColor": [1.0, 0.75, 0.25, 1.0], "endColor": [0.9, 0.15, 0.05, 0.0],
        "fadeInFraction": 0.08, "fadeOutFraction": 0.45, "blendMode": 1,
    },
    "smoke": {
        "looping": True, "emissionRate": 12.0, "burstCount": 1,
        "minLifetime": 3.0, "maxLifetime": 6.0,
        "direction": [0.0, 1.0, 0.0], "spreadAngle": math.radians(25.0),
        "minSpeed": 0.3, "maxSpeed": 0.8,
        "gravity": [0.0, 0.15, 0.0], "damping": 0.3,
        "startSize": 0.6, "endSize": 2.5,
        "startColor": [0.5, 0.5, 0.55, 0.5], "endColor": [0.25, 0.25, 0.3, 0.0],
        "fadeInFraction": 0.2, "fadeOutFraction": 0.4,
        "minRotationSpeed": -0.4, "maxRotationSpeed": 0.4, "blendMode": 0,
    },
    "explosion": {
        "looping": False, "burstCount": 80,
        "minLifetime": 0.5, "maxLifetime": 1.4,
        "direction": [0.0, 1.0, 0.0], "spreadAngle": math.radians(180.0),
        "minSpeed": 3.0, "maxSpeed": 8.0,
        "gravity": [0.0, -4.0, 0.0], "damping": 1.6,
        "startSize": 0.8, "endSize": 0.2,
        "startColor": [1.0, 0.9, 0.6, 1.0], "endColor": [1.0, 0.3, 0.05, 0.0],
        "fadeInFraction": 0.02, "fadeOutFraction": 0.35, "blendMode": 1,
    },
}

# ParticleSystemDesc()'s own defaults, mirrored here so the file-writing path
# below produces the same component the engine would have built itself.
PARTICLE_DEFAULTS = {
    "maxParticles": 200, "looping": True, "emissionRate": 20.0, "burstCount": 1,
    "minLifetime": 2.0, "maxLifetime": 4.0,
    "direction": [0.0, 1.0, 0.0], "spreadAngle": math.radians(15.0),
    "minSpeed": 1.0, "maxSpeed": 2.0,
    "gravity": [0.0, 0.0, 0.0], "damping": 0.0,
    "startSize": 1.0, "endSize": 2.0, "sizeRandomJitter": 0.2,
    "startColor": [1.0, 1.0, 1.0, 1.0], "endColor": [1.0, 1.0, 1.0, 0.0],
    "fadeInFraction": 0.1, "fadeOutFraction": 0.6,
    "minRotationSpeed": -1.0, "maxRotationSpeed": 1.0,
    "blendMode": 0, "boundingSphereRadius": 0.0,
}


@mcp.tool()
def add_particle_system(project_path: str, scene_name: str, name: str | None = None,
                        parent_name: str | None = None, position: list[float] | None = None,
                        preset: str = "default", texture: str | None = None,
                        max_particles: int | None = None, overrides: dict | None = None) -> str:
    """Add a particle emitter (editor: Add > Particle System).

    preset: default, fire, smoke or explosion — the same four the editor's Add
    form offers. `overrides` applies on top, using ParticleSystemDesc field
    names (looping, emissionRate, burstCount, minLifetime, maxLifetime,
    direction, spreadAngle in RADIANS, minSpeed, maxSpeed, gravity, damping,
    startSize, endSize, sizeRandomJitter, startColor, endColor,
    fadeInFraction, fadeOutFraction, minRotationSpeed, maxRotationSpeed,
    blendMode 0=alpha 1=additive).

    texture: the particle sprite — a project asset path ('assets/textures/x.png')
    or an absolute path; files outside the project are imported into it first.
    Omit it for the editor's default soft round sprite.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    if preset not in PARTICLE_PRESETS:
        return _fail(f"Unknown preset '{preset}'. Use: {', '.join(PARTICLE_PRESETS)}")

    tex_rel = None
    tex_src = None
    if texture:
        src = Path(texture) if os.path.isabs(texture) else proj / texture
        if not src.exists() and not os.path.isabs(texture):
            src = ROOT / texture
        if not src.exists():
            return _fail(f"Texture file not found: {texture}")
        tex_src = src
        if proj in src.parents:
            tex_rel = src.relative_to(proj).as_posix()
        else:
            if src.suffix.lower() not in TEXTURE_EXTS:
                return _fail(f"Unsupported texture format: {src.suffix}")
            dst = proj / "assets" / "textures"
            dst.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst / src.name)
            tex_rel = f"assets/textures/{src.name}"

    final = name or "Particles"
    live_args = {"name": final, "parent": parent_name or "", "position": position,
                 "preset": preset}
    if tex_src is not None:
        live_args["texture"] = str(tex_src)
    if max_particles is not None:
        live_args["maxParticles"] = int(max_particles)
    # The editor takes the cone half-angle in degrees for readability; the
    # scene file (and `overrides` here) use radians, like the engine struct.
    for key, value in (overrides or {}).items():
        live_args["spreadAngleDegrees" if key == "spreadAngle" else key] = (
            math.degrees(value) if key == "spreadAngle" else value)

    live = _live_or_none("add_particles", live_args, scene_file)
    if live is not None:
        return _fail(live) if isinstance(live, str) else f"Added ParticleSystem '{final}' (live editor)"

    data = _load_scene(scene_file)
    final_name = _unique_scene_name(data, final)
    obj = _new_game_object(final_name, position, None, None)
    comp = {"type": "ParticleSystem"}
    comp.update(PARTICLE_DEFAULTS)
    comp.update(PARTICLE_PRESETS[preset])
    if max_particles is not None:
        comp["maxParticles"] = int(max_particles)
    comp.update(overrides or {})
    # No texture key at all means the emitter draws untextured squares, so a
    # file-written one without a sprite is worth flagging rather than shipping.
    if tex_rel:
        comp["texture"] = tex_rel
    obj["components"].append(comp)
    a_err = _add_to_scene(data, obj, parent_name)
    if a_err:
        return _fail(a_err)
    _save_scene(scene_file, data)
    note = "" if tex_rel else " (no sprite set - pass `texture` or set one in the editor)"
    return f"Added ParticleSystem '{final_name}' [{preset}] to scene {scene_name}{note}"


@mcp.tool()
def add_audio_source(project_path: str, scene_name: str, file: str, name: str | None = None,
                     parent_name: str | None = None, position: list[float] | None = None,
                     looping: bool = False, spatialized: bool = True, volume: float = 1.0,
                     stream: bool = False, pitch: float = 1.0, pan: float = 0.0,
                     attenuation_model: int = 2, min_distance: float = 1.0, max_distance: float = 100.0) -> str:
    """Add an audio source (editor: Add > Sound).

    file: sound file — a project asset path ('assets/sounds/x.ogg') or an
    absolute path; unknown files are imported into the project first.
    attenuation_model: 0=None, 1=Inverse, 2=Linear (default), 3=Exponential.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    src = Path(file) if os.path.isabs(file) else proj / file
    if not src.exists() and not os.path.isabs(file):
        src = ROOT / file
    if not src.exists():
        return _fail(f"Sound file not found: {file}")

    if proj in src.parents:
        file_rel = src.relative_to(proj).as_posix()
    else:
        if src.suffix.lower() not in SOUND_EXTS:
            return _fail(f"Unsupported sound format: {src.suffix}")
        dst = proj / "assets" / "sounds"
        dst.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst / src.name)
        file_rel = f"assets/sounds/{src.name}"

    live = _live_or_none("add_audio", {
        "name": name or src.stem, "file": str(src), "parent": parent_name or "",
        "position": position, "looping": looping, "spatialized": spatialized,
        "volume": volume, "stream": stream, "pitch": pitch, "pan": pan,
    }, scene_file)
    if live is not None:
        return _fail(live) if isinstance(live, str) else f"Added AudioSource '{name or src.stem}' (live editor)"

    data = _load_scene(scene_file)
    final_name = _unique_scene_name(data, name or src.stem)
    obj = _new_game_object(final_name, position, None, None)
    obj["components"].append({
        "type": "AudioSource",
        "file": file_rel,
        "looping": bool(looping),
        "spatialized": bool(spatialized),
        "volume": volume,
        "pitch": pitch,
        "pan": pan,
        "attenuationModel": attenuation_model,
        "minDistance": min_distance,
        "maxDistance": max_distance,
        "stream": bool(stream),
        "dopplerFactor": 1.0,
        "directionalAttenuation": 1.0,
        "playing": False,
    })
    a_err = _add_to_scene(data, obj, parent_name)
    if a_err:
        return _fail(a_err)
    _save_scene(scene_file, data)
    return f"Added AudioSource '{final_name}' ({file_rel}) to scene {scene_name}"


@mcp.tool()
def add_physics(project_path: str, scene_name: str, name: str, shape: str = "Box",
                parent_name: str | None = None, position: list[float] | None = None,
                mass: float | None = None, ghost: bool = False,
                width: float = 1.0, height: float = 1.0, depth: float = 1.0,
                radius: float = 0.5, normal: list[float] | None = None,
                constant: float = 0.0, points: list[list[float]] | None = None) -> str:
    """Add a physics component object (editor: Add > Physics > Box/Capsule/Cone/Cylinder/Sphere/Static Plane).

    shapes: Box (width/height/depth), Sphere (radius), Capsule/Cone/Cylinder (radius/height),
            StaticPlane (normal/constant), ConvexHull (points [[x,y,z],...])
    Editor menu defaults: Box 1×1×1 m=1; Capsule/Cone/Cylinder r=0.5 h=1 m=1; Sphere r=0.5 m=1;
    StaticPlane normal (0,1,0) constant 0 mass 0.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    if shape == "Box":
        comp = {"type": "Physics", "shape": "Box", "mass": mass if mass is not None else 1.0,
                "ghost": ghost, "width": width, "height": height, "depth": depth}
    elif shape == "Sphere":
        comp = {"type": "Physics", "shape": "Sphere", "mass": mass if mass is not None else 1.0,
                "ghost": ghost, "radius": radius}
    elif shape in ("Capsule", "Cone", "Cylinder"):
        comp = {"type": "Physics", "shape": shape, "mass": mass if mass is not None else 1.0,
                "ghost": ghost, "radius": radius, "height": height}
    elif shape == "StaticPlane":
        comp = {"type": "Physics", "shape": "StaticPlane", "mass": mass if mass is not None else 0.0,
                "ghost": ghost, "normal": list(normal) if normal else [0.0, 1.0, 0.0], "constant": constant}
    elif shape == "ConvexHull":
        if not points:
            return _fail("ConvexHull requires 'points' ([[x, y, z], ...])")
        comp = {"type": "Physics", "shape": "ConvexHull", "mass": mass if mass is not None else 1.0,
                "ghost": ghost, "points": [list(p) for p in points]}
    else:
        return _fail(f"Invalid physics shape '{shape}'. Use Box, Sphere, Capsule, Cone, Cylinder, StaticPlane, ConvexHull.")

    if shape != "ConvexHull":
        live = _live_or_none("add_physics", {
            "name": name, "shape": shape, "parent": parent_name or "",
            "position": position, "mass": mass if mass is not None else 1.0,
            "ghost": ghost, "width": width, "height": height, "depth": depth,
            "radius": radius, "normal": normal, "constant": constant,
        }, scene_file)
        if live is not None:
            return _fail(live) if isinstance(live, str) else f"Added Physics ({shape}) '{name}' (live editor)"

    data = _load_scene(scene_file)
    final_name = _unique_scene_name(data, name)
    obj = _new_game_object(final_name, position, None, None)
    if shape == "Box":
        obj["scale"] = [width, height, depth]
    obj["components"].append(comp)
    a_err = _add_to_scene(data, obj, parent_name)
    if a_err:
        return _fail(a_err)
    _save_scene(scene_file, data)
    return f"Added Physics ({shape}) '{final_name}' to scene {scene_name}"


# --------------------------------------------------------------------------
# TOOLS — components
# --------------------------------------------------------------------------

@mcp.tool()
def attach_lua_component(project_path: str, scene_name: str, object_name: str, script_file: str, initial_data: dict | None = None) -> str:
    """Attach a Lua script component to a game object (editor: Properties > Script).

    script_file: relative project path ('assets/lua/Foo.lua') or absolute path
    to an existing .lua file.
    initial_data: optional component data table (defaults to {}).
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    live = _live_or_none("attach_script", {
        "name": object_name, "scriptFile": script_file,
        "data": initial_data or {},
    }, scene_file)
    if live is not None:
        return _fail(live) if isinstance(live, str) else f"Attached LuaComponent to '{object_name}' (live editor)"

    src = Path(script_file) if os.path.isabs(script_file) else proj / script_file
    if not src.exists() and not os.path.isabs(script_file):
        src = ROOT / script_file
    if not src.exists():
        return _fail(f"Lua script not found: {script_file}. Use create_lua_script first.")

    if proj in src.parents:
        rel = src.relative_to(proj).as_posix()
    else:
        dst = proj / "assets" / "lua"
        dst.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst / src.name)
        rel = f"assets/lua/{src.name}"

    scene = _load_scene(scene_file)
    node = _find_object(scene, object_name)
    if node is None:
        return _fail(f"Object '{object_name}' not found in scene {scene_name}")

    comp_data = initial_data or {}
    for c in node.get("components", []):
        if isinstance(c, dict) and c.get("type") == "LuaComponent":
            c["scriptFile"] = rel
            c["data"] = comp_data
            _save_scene(scene_file, scene)
            return f"Updated LuaComponent on '{object_name}' -> {rel}"

    node.setdefault("components", []).append({"type": "LuaComponent", "scriptFile": rel, "data": comp_data})
    _save_scene(scene_file, scene)
    return f"Attached LuaComponent to '{object_name}' -> {rel}"


@mcp.tool()
def attach_component(project_path: str, scene_name: str, object_name: str, component_type: str, properties: dict | None = None) -> str:
    """Attach a raw component (any type) to a game object.

    component_type: RenderingComponent | DirectionalLight | PointLight | SpotLight |
                    AudioSource | LuaComponent | Physics
    properties: component fields (e.g. {"scriptFile": "assets/lua/Foo.lua"}).
    Prefer the dedicated tools (add_light, attach_lua_component, ...) which set
    correct defaults.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    data = _load_scene(scene_file)
    node = _find_object(data, object_name)
    if node is None:
        return _fail(f"Object '{object_name}' not found in scene {scene_name}")

    comp = {"type": component_type}
    if properties:
        comp.update(properties)
    node.setdefault("components", []).append(comp)
    _save_scene(scene_file, data)
    return f"Attached {component_type} to '{object_name}'"


@mcp.tool()
def detach_component(project_path: str, scene_name: str, object_name: str, component_type: str) -> str:
    """Detach the first component of a given type from a game object."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    live = _live_or_none("detach_component", {"name": object_name, "componentType": component_type}, scene_file)
    if live is not None:
        return _fail(live) if isinstance(live, str) else f"Detached {component_type} from '{object_name}' (live editor)"

    data = _load_scene(scene_file)
    node = _find_object(data, object_name)
    if node is None:
        return _fail(f"Object '{object_name}' not found in scene {scene_name}")

    comps = node.get("components", [])
    kept = [c for c in comps if not (isinstance(c, dict) and c.get("type") == component_type)]
    if len(kept) == len(comps):
        return _fail(f"No {component_type} found on '{object_name}'")
    node["components"] = kept
    _save_scene(scene_file, data)
    return f"Detached {component_type} from '{object_name}'"


# --------------------------------------------------------------------------
# TOOLS — object manipulation
# --------------------------------------------------------------------------

@mcp.tool()
def set_object_transform(project_path: str, scene_name: str, name: str,
                         position: list[float] | None = None,
                         rotation: list[float] | None = None,
                         scale: list[float] | None = None,
                         is_static: bool | None = None) -> str:
    """Set a game object's transform (editor: gizmo / properties tab).

    position: [x, y, z]
    rotation: euler angles [x, y, z] in radians (NOT a quaternion)
    scale: [x, y, z]
    is_static: optional physics-static flag
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    if rotation is not None and len(rotation) == 4:
        # Convenience: convert quaternion [x, y, z, w] to euler radians.
        import math
        x, y, z, w = rotation
        rotation = [
            math.atan2(2 * (w * x + y * z), 1 - 2 * (x * x + y * y)),
            math.asin(max(-1.0, min(1.0, 2 * (w * y - z * x)))),
            math.atan2(2 * (w * z + x * y), 1 - 2 * (y * y + z * z)),
        ]

    if is_static is None:
        live = _live_or_none("set_transform", {
            "name": name, "position": position,
            "rotation": rotation, "scale": scale,
        }, scene_file)
        if live is not None:
            return _fail(live) if isinstance(live, str) else f"Updated transform of '{name}' (live editor)"

    data = _load_scene(scene_file)
    node = _find_object(data, name)
    if node is None:
        return _fail(f"Object '{name}' not found in scene {scene_name}")

    if position is not None:
        node["position"] = list(position)
    if rotation is not None:
        node["rotation"] = list(rotation)
    if scale is not None:
        node["scale"] = list(scale)
    if is_static is not None:
        node["static"] = is_static
    _save_scene(scene_file, data)
    return f"Updated transform of '{name}': pos={node.get('position')} rot={node.get('rotation')} scale={node.get('scale')}"


@mcp.tool()
def set_object_tags(project_path: str, scene_name: str, name: str, add_tags: list[str] | None = None, remove_tags: list[str] | None = None) -> str:
    """Add and/or remove tags on a game object (e.g. 'PyrosEditor.Camera')."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    data = _load_scene(scene_file)
    node = _find_object(data, name)
    if node is None:
        return _fail(f"Object '{name}' not found in scene {scene_name}")

    tags = set(node.get("tags", []))
    for t in add_tags or []:
        tags.add(t)
    for t in remove_tags or []:
        tags.discard(t)
    node["tags"] = sorted(tags)
    _save_scene(scene_file, data)
    return f"Tags on '{name}': {node['tags']}"


@mcp.tool()
def rename_object(project_path: str, scene_name: str, old_name: str, new_name: str) -> str:
    """Rename a game object (editor: hierarchy rename)."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    data = _load_scene(scene_file)
    node = _find_object(data, old_name)
    if node is None:
        return _fail(f"Object '{old_name}' not found in scene {scene_name}")
    node["name"] = new_name
    _save_scene(scene_file, data)
    return f"Renamed '{old_name}' -> '{new_name}'"


@mcp.tool()
def reparent_object(project_path: str, scene_name: str, name: str, new_parent_name: str | None = None) -> str:
    """Reparent a game object (editor: drag in hierarchy). new_parent_name=None moves to root."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    data = _load_scene(scene_file)
    node, parent = _find_object_with_parent(data, name)
    if node is None:
        return _fail(f"Object '{name}' not found in scene {scene_name}")

    def is_descendant(ancestor, candidate):
        cur = candidate
        seen = set()
        while cur is not None and id(cur) not in seen:
            if cur is ancestor:
                return True
            seen.add(id(cur))
            cur = _find_object_with_parent(data, cur.get("name"))[1]
        return False

    new_parent = None
    if new_parent_name is not None:
        target = _find_object(data, new_parent_name)
        if target is None:
            return _fail(f"Parent '{new_parent_name}' not found")
        if target is node:
            return _fail("Cannot reparent an object under itself")
        if is_descendant(node, target):
            return _fail("Cannot reparent an object under its own descendant")
        new_parent = target

    if isinstance(parent, list):
        parent.remove(node)
    elif isinstance(parent, dict):
        parent["children"] = [c for c in parent.get("children", []) if c is not node]
    elif parent is None:
        data.get("roots", []).remove(node)

    if new_parent is None:
        data.setdefault("roots", []).append(node)
        where = "root"
    else:
        new_parent.setdefault("children", []).append(node)
        where = f"'{new_parent_name}'"
    _save_scene(scene_file, data)
    return f"Moved '{name}' under {where}"


@mcp.tool()
def duplicate_object(project_path: str, scene_name: str, name: str, new_name: str | None = None) -> str:
    """Duplicate a game object with its children (editor: Duplicate → '<name> Copy')."""
    import copy as _copy
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    data = _load_scene(scene_file)
    node = _find_object(data, name)
    if node is None:
        return _fail(f"Object '{name}' not found in scene {scene_name}")

    clone = _copy.deepcopy(node)
    final_name = new_name or f"{name} Copy"
    final_name = _unique_scene_name(data, final_name)
    clone["name"] = final_name

    a_err = _add_to_scene(data, clone, None)
    if a_err:
        return _fail(a_err)
    _save_scene(scene_file, data)
    return f"Duplicated '{name}' as '{final_name}'"


# ---------------------------------------------------------------------------
# Prefabs
#
# A .prefab is one GameObject subtree saved as a reusable asset; a scene
# stores instances of it as a reference plus that instance's own name,
# transform and tags, so editing the prefab updates every instance. Neither
# the engine nor this server invents the format - it is what
# SceneSerializer::SerializeSubtree() writes, and shared/PrefabResolver.h is
# what resolves it on both sides.
#
# Create and instantiate work with or without a running editor. Apply,
# revert, unpack and state need the live editor: they depend on which live
# object is linked to what, which is editor-side state (SceneObject::
# prefabSource) with no representation in the file.
# ---------------------------------------------------------------------------


def _prefab_rel(proj: Path, path: Path) -> str:
    """Project-relative, which is what a scene file stores (_rel is relative
    to the repo root, and would write a path the engine cannot resolve)."""
    return path.resolve().relative_to(proj.resolve()).as_posix()


def _prefab_path(proj: Path, prefab_name: str) -> Path:
    stem = prefab_name[:-7] if prefab_name.endswith(".prefab") else prefab_name
    stem = Path(stem).name
    return proj / "assets" / "prefabs" / f"{stem}.prefab"


def _material_ids_in(node) -> list:
    """Every material-pool index the subtree references, in first-seen order."""
    found = []

    def walk(n):
        if isinstance(n, dict):
            if isinstance(n.get("material"), int) and n["material"] not in found:
                found.append(n["material"])
            for v in n.values():
                walk(v)
        elif isinstance(n, list):
            for v in n:
                walk(v)

    walk(node)
    return found


def _renumber_materials(node, remap: dict) -> None:
    if isinstance(node, dict):
        if isinstance(node.get("material"), int) and node["material"] in remap:
            node["material"] = remap[node["material"]]
        for v in node.values():
            _renumber_materials(v, remap)
    elif isinstance(node, list):
        for v in node:
            _renumber_materials(v, remap)


@mcp.tool()
def create_prefab(project_path: str, scene_name: str, name: str, prefab_name: str | None = None) -> str:
    """Save a game object and its children as a reusable .prefab; the object becomes the first instance of it.

    Editing the prefab afterwards updates every instance of it in every scene.
    """
    import copy as _copy
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    live = _live_or_none("create_prefab", {"name": name, "prefabName": prefab_name or ""}, scene_file)
    if live is not None:
        if isinstance(live, str):
            return _fail(live)
        return f"Created prefab {live.get('path', '')} from '{name}' (live editor)"

    data = _load_scene(scene_file)
    roots = data.get("roots", [])
    index = next((i for i, r in enumerate(roots) if r.get("name") == name), None)
    if index is None:
        return _fail(f"Object '{name}' not found at the root of scene {scene_name} "
                     "(prefab instances are scene roots)")

    out = _prefab_path(proj, prefab_name or name)
    if out.exists():
        return _fail(f"{_prefab_rel(proj, out)} already exists - pick another name, "
                     "or apply_prefab to update it")
    out.parent.mkdir(parents=True, exist_ok=True)

    root = _copy.deepcopy(roots[index])
    root.pop("prefab", None)
    used = _material_ids_in(root)
    pool = data.get("materials", [])
    _renumber_materials(root, {old: i for i, old in enumerate(used)})
    out.write_text(json.dumps({
        "version": 1,
        "prefabVersion": 1,
        "root": root,
        "materials": [pool[m] for m in used if m < len(pool)],
    }, indent=4))

    ref = {"prefab": _prefab_rel(proj, out), "name": roots[index].get("name", name)}
    for field in ("position", "rotation", "scale", "tags"):
        if field in roots[index]:
            ref[field] = roots[index][field]
    roots[index] = ref
    _save_scene(scene_file, data)
    return f"Created prefab {_prefab_rel(proj, out)}; '{name}' is now an instance of it"


@mcp.tool()
def instantiate_prefab(project_path: str, scene_name: str, prefab_path: str,
                       name: str | None = None, position: list[float] | None = None) -> str:
    """Add an instance of a .prefab to a scene."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    rel = (prefab_path if prefab_path.startswith("assets/")
           else _prefab_rel(proj, _prefab_path(proj, prefab_path)))
    if not (proj / rel).exists():
        return _fail(f"Prefab not found: {rel}")

    live = _live_or_none("instantiate_prefab", {
        "path": rel, "position": position or [0.0, 0.0, 0.0],
    }, scene_file)
    if live is not None:
        if isinstance(live, str):
            return _fail(live)
        return f"Instantiated {rel} as '{live.get('name', '')}' (live editor)"

    data = _load_scene(scene_file)
    prefab = json.loads((proj / rel).read_text())
    final_name = _unique_scene_name(data, name or prefab.get("root", {}).get("name", "Prefab"))
    ref = {"prefab": rel, "name": final_name, "position": position or [0.0, 0.0, 0.0]}
    data.setdefault("roots", []).append(ref)
    _save_scene(scene_file, data)
    return f"Instantiated {rel} as '{final_name}' in scene {scene_name}"


@mcp.tool()
def prefab_state(project_path: str, scene_name: str) -> str:
    """List the prefab instances in the open scene and which of them have local changes.

    Needs a running editor with this scene open: whether an instance still
    matches its prefab is answered against the live objects.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)

    live = _live_or_none("prefab_state", {}, scene_file)
    if live is None:
        # Without the editor, the file still says which roots are references.
        data = _load_scene(scene_file)
        refs = [r for r in data.get("roots", []) if "prefab" in r]
        if not refs:
            return f"No prefab instances in scene {scene_name}"
        lines = [f"{r.get('name', '?')} → {r['prefab']}" for r in refs]
        return ("Prefab instances (from the scene file; open the scene in the editor "
                "to see which have local changes):\n" + "\n".join(lines))
    if isinstance(live, str):
        return _fail(live)

    instances = live.get("instances", [])
    if not instances:
        return f"No prefab instances in scene {scene_name}"
    lines = [f"{i.get('name', '?')} → {i.get('prefab', '?')}"
             f"{'  [modified]' if i.get('modified') else ''}" for i in instances]
    return "Prefab instances:\n" + "\n".join(lines)


def _live_prefab_op(cmd: str, project_path: str, scene_name: str, name: str, done: str) -> str:
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    live = _live_or_none(cmd, {"name": name}, scene_file)
    if live is None:
        return _fail(f"{cmd} needs the editor open on scene {scene_name} - it acts on the "
                     "live objects and their link to the prefab")
    if isinstance(live, str):
        return _fail(live)
    return done.format(name=name)


@mcp.tool()
def apply_prefab(project_path: str, scene_name: str, name: str) -> str:
    """Write an instance's current state back over its prefab, updating every instance that had no changes of its own.

    Not undoable: it rewrites a project asset that other scenes reference.
    """
    return _live_prefab_op("apply_prefab", project_path, scene_name, name,
                           "Applied '{name}' to its prefab")


@mcp.tool()
def revert_prefab(project_path: str, scene_name: str, name: str) -> str:
    """Discard an instance's local changes and rebuild it from its prefab, keeping its name, transform and tags."""
    return _live_prefab_op("revert_prefab", project_path, scene_name, name,
                           "Reverted '{name}' to its prefab")


@mcp.tool()
def unpack_prefab(project_path: str, scene_name: str, name: str) -> str:
    """Break an instance's link to its prefab. The objects stay; future prefab edits no longer reach them."""
    return _live_prefab_op("unpack_prefab", project_path, scene_name, name,
                           "Unpacked '{name}' from its prefab")


@mcp.tool()
def list_prefabs(project_path: str) -> str:
    """List the .prefab assets in a project."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    folder = proj / "assets" / "prefabs"
    if not folder.is_dir():
        return "No prefabs yet (assets/prefabs/ does not exist)"
    found = sorted(p for p in folder.glob("*.prefab"))
    if not found:
        return "No prefabs yet"
    return "Prefabs:\n" + "\n".join(_prefab_rel(proj, p) for p in found)


@mcp.tool()
def build_game(project_path: str, output_dir: str, startup_scene: str | None = None,
               title: str | None = None, width: int = 1280, height: int = 720,
               fullscreen: bool = False) -> str:
    """Export a runnable game folder: the player binary, engine shaders, and the project's scenes and assets.

    Needs a running editor with this project open - the build locates the
    player binary and the staged shaders relative to the running editor.
    Note that system libraries the player links (SDL2, Lua, FreeType) are not
    gathered; they must be present on whatever machine runs the build.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    if not _live_project_matches(proj):
        return _fail("build_game needs the editor open on this project")

    args = {"outputDir": output_dir, "width": width, "height": height, "fullscreen": fullscreen}
    if startup_scene:
        args["startupScene"] = startup_scene if startup_scene.startswith("scenes/") \
            else f"scenes/{startup_scene}.json"
    if title:
        args["title"] = title

    ok, res = _editor_call("build_game", args, timeout=120.0)
    if not ok:
        return _fail(str(res))
    lines = [f"Built {res.get('files', 0)} file(s) into {res.get('outputDir', output_dir)}"]
    lines += [f"  ! {w}" for w in res.get("warnings", [])]
    return "\n".join(lines)


@mcp.tool()
def remove_game_object(project_path: str, scene_name: str, name: str) -> str:
    """Remove a game object (and its children) from a scene."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    live = _live_or_none("delete_object", {"name": name}, scene_file)
    if live is not None:
        return _fail(live) if isinstance(live, str) else f"Removed '{name}' from scene (live editor)"

    data = _load_scene(scene_file)
    node, parent = _find_object_with_parent(data, name)
    if node is None:
        return _fail(f"Object '{name}' not found in scene {scene_name}")

    if isinstance(parent, list):
        parent.remove(node)
    elif isinstance(parent, dict):
        parent["children"] = [c for c in parent.get("children", []) if c is not node]
    elif parent is None:
        data.get("roots", []).remove(node)
    _save_scene(scene_file, data)
    return f"Removed '{name}' from scene {scene_name}"


# --------------------------------------------------------------------------
# TOOLS — materials
# --------------------------------------------------------------------------

@mcp.tool()
def list_materials(project_path: str, scene_name: str) -> str:
    """List all materials in a scene (id + key fields)."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    data = _load_scene(scene_file)
    mats = []
    for m in data.get("materials", []):
        mats.append({
            "id": m.get("id"),
            "kind": m.get("kind"),
            "options": m.get("options"),
            "color": m.get("color"),
            "opacity": m.get("opacity"),
            "transparent": m.get("transparent"),
            "wireframe": m.get("wireframe"),
            "colorMap": m.get("colorMap", ""),
        })
    return json.dumps(mats, indent=2)


@mcp.tool()
def set_material(project_path: str, scene_name: str, material_id: int,
                color: list[float] | None = None, specular: list[float] | None = None,
                opacity: float | None = None, shininess: float | None = None,
                reflectivity: float | None = None, metallic: float | None = None,
                roughness: float | None = None, alpha_cutoff: float | None = None,
                displacement_height: float | None = None, wireframe: bool | None = None,
                transparent: bool | None = None, casting_shadows: bool | None = None,
                cull_face: int | None = None, options: int | None = None,
                color_map: str | None = None, specular_map: str | None = None,
                normal_map: str | None = None, env_map: str | None = None) -> str:
    """Edit a scene material (editor: Material panel).

    material_id: index/id of the material in the scene's materials[] array.
    All fields optional — only provided ones change.
    color_map etc.: relative asset paths ('assets/textures/x.png') or absolute.
    cull_face: 0=Back, 1=Front, 2=DoubleSided.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    data = _load_scene(scene_file)
    mats = data.get("materials", [])
    mat = None
    for m in mats:
        if m.get("id") == material_id or mats.index(m) == material_id:
            mat = m
            break
    if mat is None:
        return _fail(f"Material #{material_id} not found in scene {scene_name}")

    def _rel(p: str) -> str:
        path = Path(p) if os.path.isabs(p) else proj / p
        if not path.exists() and not os.path.isabs(p):
            path = ROOT / p
        if path.exists() and proj in path.parents:
            return path.resolve().relative_to(proj).as_posix()
        return p

    updates = []
    if color is not None:
        mat["color"] = list(color); updates.append("color")
    if specular is not None:
        mat["specular"] = list(specular); updates.append("specular")
    if opacity is not None:
        mat["opacity"] = opacity; updates.append("opacity")
    if shininess is not None:
        mat["shininess"] = shininess; updates.append("shininess")
    if reflectivity is not None:
        mat["reflectivity"] = reflectivity; updates.append("reflectivity")
    if metallic is not None:
        mat["metallic"] = metallic; updates.append("metallic")
    if roughness is not None:
        mat["roughness"] = roughness; updates.append("roughness")
    if alpha_cutoff is not None:
        mat["alphaCutoff"] = alpha_cutoff; updates.append("alphaCutoff")
    if displacement_height is not None:
        mat["displacementHeight"] = displacement_height; updates.append("displacementHeight")
    if wireframe is not None:
        mat["wireframe"] = wireframe; updates.append("wireframe")
    if transparent is not None:
        mat["transparent"] = transparent; updates.append("transparent")
    if casting_shadows is not None:
        mat["castingShadows"] = casting_shadows; updates.append("castingShadows")
    if cull_face is not None:
        mat["cullFace"] = cull_face; updates.append("cullFace")
    if options is not None:
        mat["options"] = options; updates.append("options")
    for key, val in (("colorMap", color_map), ("specularMap", specular_map),
                     ("normalMap", normal_map), ("envMap", env_map)):
        if val is not None:
            mat[key] = _rel(val); updates.append(key)

    _save_scene(scene_file, data)
    return f"Updated material #{material_id}: {', '.join(updates) if updates else 'no fields given'}"


# --------------------------------------------------------------------------
# TOOLS — Material Editor (node graph / custom-shader materials)
#
# Unlike set_material above (which patches an already-baked GenericShader
# material's scalar fields directly in scene.json), these drive the actual
# Material Editor: project-level assets/materials/<name>.mat files, authored
# via a node graph and compiled to real GLSL. There is no file-based
# fallback for the mutating ones - the node graph -> GLSL compile step only
# exists inside the running engine (MaterialCodegen.cpp + real shader
# compilation), so a live PyrosBuilder editor is required. Read-only
# get_material_graph also requires it for simplicity/consistency, even
# though it could in principle read the .mat JSON directly.
# --------------------------------------------------------------------------

def _require_live_project(proj: Path) -> str:
    """Best-effort: ensure a running editor has `proj` open live.

    Returns "" if a live editor already/now has the project open, otherwise
    a human-readable error string explaining why not.
    """
    if not _editor_endpoint():
        return "No running editor detected. The Material Editor's node-graph " \
               "compiler only runs inside the live engine - start PyrosBuilder first."
    if _live_project_matches(proj):
        return ""
    ok, res = _editor_call("open_project", {"path": str(proj)})
    if not ok:
        return f"Could not open project '{proj}' in the running editor: {res}"
    return ""


@mcp.tool()
def create_material(project_path: str, name: str, kind: str = "generic") -> str:
    """Create a new material asset under assets/materials/<name>.mat and open
    it live in the running editor (editor: Assets panel > New Material).

    kind: 'generic' (fixed PBR property sliders, no shader code) or
          'custom' (node graph / hand-written GLSL - seeded with a small
          starter graph: Base Color -> Albedo, two Float nodes -> Metallic/
          Roughness). Use set_material_graph to build out a 'custom' graph,
          then apply_material to compile it and assign_material to put it
          on an object.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    live_err = _require_live_project(proj)
    if live_err:
        return _fail(live_err)
    ok, res = _editor_call("create_material", {"name": name, "kind": kind})
    if not ok:
        return _fail(str(res))
    return f"Created material: {res.get('path')} (kind={res.get('kind')})"


@mcp.tool()
def get_material_graph(project_path: str, material_path: str) -> str:
    """Read a Custom Shader material's current node graph (nodes + connections)
    plus its kind/editMode/compiled-shader-path, as JSON.

    material_path: 'assets/materials/Foo.mat' (relative) or absolute.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    live_err = _require_live_project(proj)
    if live_err:
        return _fail(live_err)
    ok, res = _editor_call("get_material_graph", {"path": material_path})
    if not ok:
        return _fail(str(res))
    return json.dumps(res, indent=2)


@mcp.tool()
def set_material_graph(project_path: str, material_path: str, nodes: list[dict], connections: list[dict] | None = None) -> str:
    """Replace a Custom Shader material's node graph and save it to disk
    (editor: Material Editor's Node Graph tab).

    Only applies to 'custom' kind materials (see create_material). Does NOT
    compile/apply the shader by itself - call apply_material afterward to
    make the change render live.

    nodes: list of {"id": int, "type": str, "name": str, "pos": [x, y],
      "userData": str, "texturePath": str}. `id`s must be unique positive
      ints; a graph needs exactly one node with type "Output". `type` is one
      of: Color, Float, Texture, Int, Bool, Vec2, Vec3, Vec4, Add, Subtract,
      Multiply, Divide, Power, Modulo, Negate, Abs, Sqrt, Sin, Cos, Tan, Min,
      Max, Clamp, Lerp, DotProduct, CrossProduct, Length, Normalize,
      Distance, Equal, NotEqual, GreaterThan, LessThan, And, Or, Not, Step,
      SmoothStep, SplitVec2, SplitVec3, SplitVec4, CombineVec2, CombineVec3,
      CombineVec4, Output, ObjectPosition, CameraPosition, UVCoordinate,
      NormalVector, TimeValue.
      userData holds constant values as comma-separated floats (e.g. a Color
      node's "1,0,0,1"; a Float node's "0.5"); leave "" for non-constant
      nodes. texturePath is only used by Texture nodes (path under
      assets/textures/, e.g. "brick.png").
    connections: list of {"fromNode": int, "fromPinIndex": int, "toNode": int,
      "toPinIndex": int}. Output's input pins are, in order: Albedo(0),
      Normal(1), Metallic(2), Roughness(3), Emissive(4), Occlusion(5).
      Color's output pins are R(0) G(1) B(2) A(3) RGBA(4).
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    live_err = _require_live_project(proj)
    if live_err:
        return _fail(live_err)
    ok, res = _editor_call("set_material_graph", {
        "path": material_path, "nodes": nodes, "connections": connections or [],
    })
    if not ok:
        return _fail(str(res))
    note = f"\nWARNING: saved, but the shader failed to compile: {res.get('applyWarning')}" \
        if res.get("applyWarning") else ""
    return f"Saved graph to {res.get('path')}{note}"


@mcp.tool()
def get_material_text(project_path: str, material_path: str) -> str:
    """Read a Custom Shader material's Text-mode GLSL snippet (the hand-written
    shader code, not the node graph) plus its named texture inputs, as JSON.

    Returns: {"kind","editMode","text","textures":[{"name","path"},...]}.
    Use this to see the current GLSL before editing it with set_material_text.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    live_err = _require_live_project(proj)
    if live_err:
        return _fail(live_err)
    ok, res = _editor_call("get_material_text", {"path": material_path})
    if not ok:
        return _fail(str(res))
    return json.dumps(res, indent=2)


@mcp.tool()
def set_material_text(project_path: str, material_path: str, text: str,
                      textures: list[dict] | None = None) -> str:
    """Write a Custom Shader material's Text-mode GLSL snippet (hand-written
    shader code) and switch it to Text mode, then compile it.

    This is the "Text" editing mode of the Material Editor: `text` is the
    user snippet with assignments for the Output pins (Albedo / Normal /
    Metallic / Roughness / Emissive / Occlusion). The editor wraps it in
    boilerplate and generates the full GLSL, so you write only the body.

    textures: optional list of named texture inputs [{"name": "uTexture",
      "path": "brick.png"}] - each becomes a `uniform sampler2D <name>` you
      can sample by name in the snippet. Omit to keep existing inputs.

    Does NOT put the material on any object - use assign_material for that.
    On a compile error the previous working shader is kept and the error is
    returned.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    live_err = _require_live_project(proj)
    if live_err:
        return _fail(live_err)
    ok, res = _editor_call("set_material_text", {
        "path": material_path, "text": text, "textures": textures or [],
    })
    if not ok:
        return _fail(str(res))
    note = f"\nWARNING: saved, but the shader failed to compile: {res.get('applyWarning')}" \
        if res.get("applyWarning") else ""
    return f"Saved GLSL text to {res.get('path')}{note}"


@mcp.tool()
def apply_material(project_path: str, material_path: str) -> str:
    """Compile a Custom Shader material's current graph/text into real GLSL
    and hot-swap it onto the live material (editor: Material Editor's
    Apply / Save Shader button).

    On a compile error, the material's previous working shader is left
    untouched and the error is returned - fix the graph (set_material_graph)
    or hand-edited text and call apply_material again.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    live_err = _require_live_project(proj)
    if live_err:
        return _fail(live_err)
    ok, res = _editor_call("apply_material", {"path": material_path})
    if not ok:
        return _fail(str(res))
    return f"Applied and compiled: {material_path}"


@mcp.tool()
def assign_material(project_path: str, scene_name: str, object_name: str, material_path: str, submesh: int = 0) -> str:
    """Assign a material asset onto a scene object's submesh, replacing
    whatever it had (editor: Properties panel > Edit Material, then picking
    a different material - this is the "attach" step; use create_material /
    set_material_graph / apply_material first to build the material itself).

    Requires the target scene to be open live in the running editor.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)
    live_err = _require_live_project(proj)
    if live_err:
        return _fail(live_err)
    if not _live_scene_matches(scene_file):
        ok, res = _editor_call("load_scene", {"path": str(scene_file)})
        if not ok:
            return _fail(f"Could not load scene '{scene_name}' live: {res}")
    ok, res = _editor_call("assign_material", {"object": object_name, "path": material_path, "submesh": submesh})
    if not ok:
        return _fail(str(res))
    return f"Assigned {material_path} to '{object_name}' submesh {submesh}"


# --------------------------------------------------------------------------
# TOOLS — scene scripts / main script
# --------------------------------------------------------------------------

@mcp.tool()
def get_scene_lua(project_path: str, scene_name: str) -> str:
    """Get the companion Lua script for a scene (scenes/<name>.lua)."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    lua_file = _scene_file(proj, scene_name).with_suffix(".lua")
    if not lua_file.exists():
        return _fail(f"No companion script found for scene '{scene_name}'")
    content = lua_file.read_text()
    return f"-- {_rel(lua_file)} ({len(content.splitlines())} lines)\n{content}"


@mcp.tool()
def set_scene_main_script(project_path: str, scene_name: str, script_path: str) -> str:
    """Set the scene's mainScript (the companion/main Lua file).

    script_path: relative project path ('scenes/MyScene.lua') or absolute path.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    p = Path(script_path) if os.path.isabs(script_path) else proj / script_path
    if not p.exists() and not os.path.isabs(script_path):
        p = ROOT / script_path
    if p.exists() and proj in p.parents:
        rel = p.resolve().relative_to(proj).as_posix()
    else:
        rel = script_path

    data = _load_scene(scene_file)
    data["mainScript"] = rel
    _save_scene(scene_file, data)
    return f"Set mainScript of '{scene_name}' to '{rel}'"


@mcp.tool()
def migrate_scene(project_path: str, scene_name: str) -> str:
    """Migrate a scene written by the old (broken) MCP server into the editor's format.

    Fixes: 'objects' → 'roots', nested 'transform' → flat position/rotation/scale,
    quaternion rotation → euler radians, bool cullFace → int, missing
    'static'/'tags' fields added. Material flag values are preserved.
    Run this on scenes created before the server rewrite; the editor cannot
    load the old format.
    """
    import math as _math
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    data = _load_scene(scene_file)
    fixes = []

    if "roots" not in data and "objects" in data:
        data["roots"] = data.pop("objects")
        fixes.append("objects -> roots")

    def quat_to_euler(q):
        x, y, z, w = q
        return [
            _math.atan2(2 * (w * x + y * z), 1 - 2 * (x * x + y * y)),
            _math.asin(max(-1.0, min(1.0, 2 * (w * y - z * x)))),
            _math.atan2(2 * (w * z + x * y), 1 - 2 * (y * y + z * z)),
        ]

    def fix_node(node):
        if not isinstance(node, dict):
            return
        if "transform" in node:
            t = node.pop("transform") or {}
            node.setdefault("position", t.get("position", [0.0, 0.0, 0.0]))
            rot = t.get("rotation", [0.0, 0.0, 0.0])
            node["rotation"] = quat_to_euler(rot) if isinstance(rot, list) and len(rot) == 4 else list(rot or [0.0, 0.0, 0.0])
            node.setdefault("scale", t.get("scale", [1.0, 1.0, 1.0]))
            fixes.append("transform flattened")
        if "static" not in node:
            node["static"] = False
            fixes.append("added static")
        if "tags" not in node:
            node["tags"] = []
            fixes.append("added tags")
        rot = node.get("rotation")
        if isinstance(rot, list) and len(rot) == 4:
            node["rotation"] = quat_to_euler(rot)
            fixes.append("quaternion -> euler")
        for child in node.get("children", []):
            fix_node(child)

    for root in data.get("roots", []):
        fix_node(root)

    for m in data.get("materials", []):
        if isinstance(m.get("cullFace"), bool):
            m["cullFace"] = 0 if m["cullFace"] else 2
            fixes.append("cullFace bool -> int")

    if fixes:
        _save_scene(scene_file, data)
        return f"Migrated {scene_file.name}: {', '.join(sorted(set(fixes)))}"
    return "Scene already in editor format — nothing to do."


@mcp.tool()
def save_scene(project_path: str, scene_name: str) -> str:
    """Save a scene (re-serializes it cleanly, editor-style indent 4).

    If a running editor has this scene open, saves the LIVE in-memory scene
    (so unsaved editor changes are persisted) and marks it clean.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    live = _live_or_none("save_scene", {}, scene_file)
    if live is not None:
        if isinstance(live, str):
            return _fail(live)
        return f"Saved scene (live editor): {live.get('path', scene_file)}"

    data = _load_scene(scene_file)
    _save_scene(scene_file, data)
    return f"Saved scene: {_rel(scene_file)} ({scene_file.stat().st_size} bytes)"


# --------------------------------------------------------------------------
# TOOLS — assets
# --------------------------------------------------------------------------

@mcp.tool()
def list_assets(project_path: str, category: str = "all") -> str:
    """List project assets (editor asset browser).

    category: models | textures | sounds | shaders | lua | materials | all
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)

    cats = list(ASSET_DIRS.keys()) if category == "all" else [category]
    if category != "all" and category not in ASSET_DIRS:
        return _fail(f"Unknown category '{category}'. Use: {', '.join(ASSET_DIRS)} or 'all'")

    results = []
    for cat in cats:
        base = proj / ASSET_DIRS[cat]
        if not base.exists():
            continue
        entries = []
        for p in sorted(base.rglob("*")):
            rel = p.relative_to(proj).as_posix()
            parts = p.relative_to(base).parts
            if any(part == ".thumbnails" for part in parts):
                continue
            entries.append((p.is_dir(), rel))
        entries.sort(key=lambda e: (not e[0], e[1]))  # dirs first, then name
        for is_dir, rel in entries:
            results.append(f"{cat:>9} {'[dir] ' if is_dir else ''}{rel}")

    # scenes as pseudo-asset
    if category in ("all", "scenes"):
        scenes_dir = proj / "scenes"
        if scenes_dir.exists():
            for f in sorted(scenes_dir.glob("*.json")):
                if ".editor.json" in f.name:
                    continue
                companion = " [with .lua]" if (f.parent / (f.stem + ".lua")).exists() else ""
                results.append(f"{'scenes':>9} {f.relative_to(proj).as_posix()}{companion}")

    return "\n".join(results[:200]) if results else "No assets found."


@mcp.tool()
def import_asset(project_path: str, source_file: str) -> str:
    """Import a file into the project (editor asset browser drag-drop).

    Routes by extension exactly like the editor:
      models (.obj/.fbx/.gltf/...) → converted to .p3dm package in assets/models/<stem>/
      textures → assets/textures/    sounds → assets/sounds/
      shaders → assets/shaders/      .lua → assets/lua/
      materials → assets/materials/  anything else → assets/
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)

    src = Path(source_file) if os.path.isabs(source_file) else ROOT / source_file
    src = src.resolve()
    if not src.exists():
        return _fail(f"File not found: {source_file}")
    if src.is_dir():
        return _fail("Import individual files (folders not supported)")

    if proj in src.parents:
        return f"Already inside project: {src.relative_to(proj).as_posix()}"

    ext = src.suffix.lower()
    if ext in MODEL_SOURCE_EXTS or ext == ".p3dm":
        if ext == ".p3dm":
            if proj in src.parents:
                rel = src.relative_to(proj).as_posix()
                if rel.startswith("assets/models/"):
                    return f"Already inside project: {rel}"
            dst_dir = proj / "assets" / "models" / src.stem
            dst_dir.mkdir(parents=True, exist_ok=True)
            dst = dst_dir / (src.stem + ".p3dm")  # editor renames to <stem>.p3dm
            shutil.copy2(src, dst)
            return f"Imported model package: {dst.relative_to(proj).as_posix()}"
        model_dir = proj / "assets" / "models" / src.stem
        p3dm, c_err = _convert_model(src, model_dir)
        if p3dm is None:
            return _fail(c_err)
        return f"Imported model: {p3dm.relative_to(proj).as_posix()} (converted from {src.name})"

    if ext in TEXTURE_EXTS:
        dest_dir = proj / "assets" / "textures"
    elif ext in SOUND_EXTS:
        dest_dir = proj / "assets" / "sounds"
    elif ext in SHADER_EXTS:
        dest_dir = proj / "assets" / "shaders"
    elif ext == ".lua":
        dest_dir = proj / "assets" / "lua"
    elif ext in MATERIAL_EXTS:
        dest_dir = proj / "assets" / "materials"
    elif ext == ".json":
        dest_dir = proj / "scenes"
    else:
        dest_dir = proj / "assets"

    dest_dir.mkdir(parents=True, exist_ok=True)
    dst = dest_dir / src.name
    shutil.copy2(src, dst)
    return f"Imported {dst.relative_to(proj).as_posix()}"


@mcp.tool()
def import_model(project_path: str, model_file: str) -> str:
    """Import/convert a 3D model file into the project's assets/models/ (as a .p3dm package).

    Runs the editor's AssimpImporter pipeline: stages the source + sidecars
    (mtl/bin/textures) into assets/models/<stem>/, converts to .p3dm, and
    packages referenced textures into assets/models/<stem>/textures/.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    src = Path(model_file) if os.path.isabs(model_file) else ROOT / model_file
    src = src.resolve()
    if not src.exists():
        return _fail(f"Model file not found: {model_file}")
    if src.suffix.lower() not in MODEL_SOURCE_EXTS:
        return _fail(f"Unsupported model format: {src.suffix} (use one of {sorted(MODEL_SOURCE_EXTS)})")

    model_dir = proj / "assets" / "models" / src.stem
    p3dm, c_err = _convert_model(src, model_dir)
    if p3dm is None:
        return _fail(c_err)
    return f"Imported model to {_rel(p3dm)}"


@mcp.tool()
def delete_asset(project_path: str, asset_path: str) -> str:
    """Delete an asset (editor semantics).

    - Deleting a .p3dm removes its whole package folder (textures, staged source).
    - Non-empty folders are refused; project.json is protected.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)

    rel = asset_path.replace("\\", "/")
    if not rel or rel == "." or ".." in rel:
        return _fail(f"Invalid asset path: {asset_path}")
    if rel == "project.json":
        return _fail("Cannot delete project.json")

    if _live_project_matches(proj):
        ok, res = _editor_call("delete_asset", {"path": rel})
        if ok:
            return f"Deleted (moved to .trash/, live editor - Ctrl+Z to undo): {rel}"
        # Live editor is running but the call itself failed (e.g. folder
        # not empty) - surface that instead of silently falling through to
        # a permanent, non-undoable file-based delete.
        return _fail(str(res))

    path = (proj / rel).resolve()
    if not path.exists():
        return _fail(f"Asset not found: {asset_path}")
    if proj not in path.parents and path != proj:
        return _fail(f"Path is outside the project: {asset_path}")

    # p3dm package deletion (editor behavior): assets/models/<stem>/<stem>.p3dm
    if path.suffix.lower() == ".p3dm":
        parent = path.parent
        rel_parent = parent.relative_to(proj).as_posix()
        if parent.name == path.stem and rel_parent.startswith("assets/models/"):
            shutil.rmtree(parent)
            return f"Deleted model package {rel_parent}/ (incl. textures)"

    if path.is_dir():
        if any(path.iterdir()):
            return _fail("Folder is not empty (delete its contents first)")
        path.rmdir()
    else:
        path.unlink()
    return f"Deleted: {path.relative_to(proj).as_posix()}"


@mcp.tool()
def copy_asset(project_path: str, source_path: str, dest_path: str) -> str:
    """Copy an asset (or folder) to a new location inside/outside the project."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)

    src = Path(source_path) if os.path.isabs(source_path) else proj / source_path
    dst = Path(dest_path) if os.path.isabs(dest_path) else proj / dest_path
    if not src.exists():
        return _fail(f"Source not found: {source_path}")
    dst.parent.mkdir(parents=True, exist_ok=True)
    if src.is_dir():
        shutil.copytree(src, dst, dirs_exist_ok=True)
    else:
        shutil.copy2(src, dst)
    return f"Copied {src} -> {dst}"


# --------------------------------------------------------------------------
# TOOLS — Lua scripts (code editor)
# --------------------------------------------------------------------------

@mcp.tool()
def create_lua_script(project_path: str, name: str, kind: str = "gameobject", template_content: str | None = None) -> str:
    """Create a new Lua script with the editor's exact template.

    kind: 'gameobject' → assets/lua/<name>.lua (component script)
          'scene'      → scenes/<name>.lua (scene main script)
    name must not contain path separators (editor rule).
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)

    stem = name.strip()
    if not stem:
        return _fail("Script name is empty")
    if "/" in stem or "\\" in stem:
        return _fail("Name cannot contain path separators")
    if not stem.endswith(".lua"):
        stem += ".lua"

    if kind == "scene":
        rel = f"scenes/{stem}"
    elif kind == "gameobject":
        rel = f"assets/lua/{stem}"
    else:
        return _fail(f"Unknown kind '{kind}'. Use 'gameobject' or 'scene'.")

    path = proj / rel
    if path.exists():
        return _fail(f"Script already exists: {_rel(path)}")

    if template_content is None:
        class_name = _sanitize_lua_class_name(Path(stem).stem)
        content = _build_lua_snippet("scene" if kind == "scene" else "gameobject", class_name, rel)
    else:
        content = template_content

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)
    return f"Created script: {_rel(path)}"


@mcp.tool()
def read_lua_script(project_path: str, script_path: str) -> str:
    """Read a Lua script (or any text file) in the project."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    path = Path(script_path) if os.path.isabs(script_path) else proj / script_path
    if not path.exists() and not os.path.isabs(script_path):
        path = ROOT / script_path
    if not path.exists():
        return _fail(f"Script not found: {script_path}")
    content = path.read_text()
    try:
        rel = _rel(path)
    except ValueError:
        rel = str(path)
    return f"-- {rel} ({len(content.splitlines())} lines)\n{content}"


@mcp.tool()
def write_lua_script(project_path: str, script_path: str, content: str) -> str:
    """Write (overwrite) a Lua script — the editor's Save in the code editor."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    path = Path(script_path) if os.path.isabs(script_path) else proj / script_path
    if not path.exists() and not os.path.isabs(script_path):
        path = ROOT / script_path
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)
    try:
        rel = _rel(path)
    except ValueError:
        rel = str(path)
    return f"Wrote script: {rel} ({len(content.splitlines())} lines)"


@mcp.tool()
def edit_lua_script(project_path: str, script_path: str, search: str, replace: str) -> str:
    """Edit a Lua script by replacing an exact text occurrence."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    path = Path(script_path) if os.path.isabs(script_path) else proj / script_path
    if not path.exists() and not os.path.isabs(script_path):
        path = ROOT / script_path
    if not path.exists():
        return _fail(f"Script not found: {script_path}")

    content = path.read_text()
    if search not in content:
        return _fail(f"'{search}' not found in {path}")
    count = content.count(search)
    path.write_text(content.replace(search, replace))
    return f"Replaced {count} occurrence(s) in {path}"


@mcp.tool()
def list_lua_scripts(project_path: str) -> str:
    """List all Lua scripts in a project (scene scripts + component scripts)."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)

    results = []
    scenes_dir = proj / "scenes"
    if scenes_dir.exists():
        for f in sorted(scenes_dir.glob("*.lua")):
            companion = " [scene]" if (f.parent / (f.stem + ".json")).exists() else " [orphan]"
            results.append(f"scene   {f.relative_to(proj).as_posix()}{companion}")
    lua_dir = proj / "assets" / "lua"
    if lua_dir.exists():
        for f in sorted(lua_dir.rglob("*.lua")):
            results.append(f"script  {f.relative_to(proj).as_posix()}")
    return "\n".join(results) if results else "No Lua scripts found."


# --------------------------------------------------------------------------
# TOOLS — live editor (running PyrosBuilder instance)
#
# These talk to the editor's local command server (AgentServer). They only
# work while the editor is open; the file-based tools above always work.
# --------------------------------------------------------------------------

@mcp.tool()
def editor_status() -> str:
    """Report whether a PyrosBuilder editor is running, and if so: port,
    open project, active scene, dirty/playing state.

    This is the first tool to call to check if live editing is possible.
    """
    ep = _editor_endpoint()
    if not ep:
        return "No running editor detected. File-based tools will be used.\n" + \
               "Start the editor (PyrosBuilder) to enable live tools " \
               "(editor_screenshot, play_mode, editor_log, reload_scene)."
    ok, res = _editor_call("status", {})
    if not ok:
        return f"Editor discovery file present but unreachable: {res}"
    lines = [f"EDITOR RUNNING (live tools available)"]
    lines.append(f"  port:          {res.get('port', ep[0])}")
    if res.get("projectOpen"):
        lines.append(f"  project:       {res.get('projectPath')}")
    else:
        lines.append("  project:       (none open - editor is on the welcome screen)")
        lines.append("  Use open_project/new_project to load a project into this editor.")
    if res.get("scenePath"):
        lines.append(f"  active scene:  {res.get('scenePath')}")
        lines.append(f"  scene name:    {res.get('scene')}")
        lines.append(f"  dirty:         {res.get('dirty')}")
        lines.append(f"  playing:       {res.get('playing')}")
    return "\n".join(lines)


@mcp.tool()
def editor_screenshot(save_path: str | None = None, live: bool = False) -> str:
    """Capture the editor's 3D viewport as a PNG image.

    Returns the path to the saved PNG. If save_path is omitted, saves to a
    temp file named pyros3d-screenshot.png.

    By default this re-renders the view through an offscreen forward
    renderer, which is stable but is NOT what a deferred project actually
    looks like. Pass live=True to read back the texture the Scene View is
    really showing, including canvas mode - that is the one to use when the
    question is "what is on screen".
    """
    ok, res = _editor_call("screenshot", {"live": bool(live)})
    if not ok:
        return _fail(f"Screenshot failed: {res}")
    b64 = res.get("pngBase64", "")
    if not b64:
        return _fail("Editor returned an empty screenshot")
    png = base64.b64decode(b64)
    out = Path(save_path) if save_path else Path(tempfile.gettempdir()) / "pyros3d-screenshot.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(png)
    return f"Saved editor screenshot to {out} ({len(png)} bytes)"


@mcp.tool()
def editor_log(lines: int = 50) -> str:
    """Read the last N lines from the running editor's log (its Log tab).

    Useful to see import errors, Lua errors, or scene load messages.
    """
    ok, res = _editor_call("log", {"lines": lines})
    if not ok:
        return _fail(f"Could not read editor log: {res}")
    text = res.get("log", "")
    return text if text.strip() else "(editor log is empty)"


@mcp.tool()
def play_mode(action: str = "start") -> str:
    """Start or stop play mode in the running editor.

    action: 'start' or 'stop'. Lets the agent actually run the game to
    verify behaviour, then stop it.
    """
    cmd = "play" if action == "start" else "stop_play"
    ok, res = _editor_call(cmd, {})
    if not ok:
        return _fail(f"Play mode '{action}' failed: {res}")
    return f"Play mode: {res.get('playing', action)}"


@mcp.tool()
def reload_scene() -> str:
    """Ask the running editor to reload the active scene from disk.

    The editor only reloads if the scene file's mtime is newer than its last
    load and there are no unsaved in-editor changes. Use this after writing
    scene files directly (e.g. with the file-based tools) to sync a running
    editor.
    """
    ok, res = _editor_call("reload", {})
    if not ok:
        return _fail(f"Reload failed: {res}")
    if res.get("reloaded"):
        return "Editor reloaded the scene from disk."
    return "Editor did not reload (file unchanged, unsaved changes, or play mode active). " + \
           "Check editor_log for details."


@mcp.tool()
def undo_redo(action: str = "undo") -> str:
    """Undo or redo the last scene edit in the running editor's active document.

    action: 'undo' or 'redo'. Only meaningful against a live editor session -
    there is no file-based equivalent, unlike most other scene-editing tools
    here (undo history only exists in the running editor's memory).
    """
    cmd = "undo" if action == "undo" else "redo"
    ok, res = _editor_call(cmd, {})
    if not ok:
        return _fail(f"{cmd.capitalize()} failed: {res}")
    return f"{cmd.capitalize()} ok"


# --------------------------------------------------------------------------
# Animation tools
# --------------------------------------------------------------------------
#
# The clip-editing tools work on the .p3da file directly, so they function
# with no editor running - except when the running editor already has that
# very file open in an Animation Editor tab, in which case they are routed
# through the editor instead. That is not a nicety: the editor holds the
# clips in memory and writes its whole copy on Save, so a file edited behind
# its back would be silently reverted by the next Ctrl+S.
#
# The posing/playback/skeleton tools are live-only by nature - a pose exists
# in a running viewport, not in a file.


def _animation_target(project_path: str, animation: str) -> tuple[Path | None, Path | None, str]:
    proj, err = _resolve_project(project_path)
    if not proj:
        return None, None, _fail(err)
    anim = _animation_file(proj, animation)
    if not anim.exists():
        return None, None, _fail(f"Animation not found: {_rel(anim)}")
    return proj, anim, ""


@mcp.tool()
def list_animations(project_path: str) -> str:
    """List the animation clips (.p3da files) in a project.

    Shows each file, the clips inside it with their runtime ids, durations
    and key counts, and which rig (model) the editor previews it on.
    """
    proj, err = _resolve_project(project_path)
    if not proj:
        return _fail(err)

    bindings = {}
    proj_json = _project_json(proj)
    if proj_json.exists():
        try:
            data = json.loads(proj_json.read_text())
            bindings = (data.get("settings") or {}).get("animationBindings") or {}
        except Exception:
            bindings = {}

    files = sorted(proj.glob("assets/**/*.p3da"))
    if not files:
        return "No animations in this project. Use import_animation to convert an fbx/dae, " \
               "or create_animation to start an empty one."

    lines = [f"Animations in {_rel(proj)}:"]
    for f in files:
        rel = f.relative_to(proj).as_posix()
        try:
            clips = _read_p3da(f)
        except Exception as e:
            lines.append(f"  {rel}  (unreadable: {e})")
            continue
        rig = bindings.get(rel, "")
        lines.append(f"  {rel}{'  [rig: ' + rig + ']' if rig else ''}")
        for i, c in enumerate(clips):
            keys = sum(len(ch['positions']) + len(ch['rotations']) + len(ch['scales'])
                       for ch in c["channels"])
            lines.append(f"    [{i}] {c['name']}  {c['duration']:.3f}s  "
                         f"{len(c['channels'])} bones, {keys} keys")
    lines.append("\nClip id is the index shown - that is what Play(id) refers to at runtime.")
    return "\n".join(lines)


@mcp.tool()
def get_animation_info(project_path: str, animation: str, clip: str | None = None,
                       bone: str | None = None) -> str:
    """Inspect one animation file: its clips, animated bones and keyframe times.

    animation: file name ('walk'), project-relative path, or absolute path.
    clip:      clip name or index; omitted means all clips.
    bone:      restrict the per-bone listing to one bone (and show its key values).

    Reads the file on disk. If the editor has this animation open with unsaved
    edits, those are NOT reflected here - use open_animation / list_animations
    to see the live document's state, or save_animation first.
    """
    proj, anim, err = _animation_target(project_path, animation)
    if err:
        return err

    try:
        clips = _read_p3da(anim)
    except Exception as e:
        return _fail(f"Could not read {_rel(anim)}: {e}")

    if clip is not None:
        try:
            indices = [_find_clip(clips, clip)]
        except ValueError as e:
            return _fail(str(e))
    else:
        indices = list(range(len(clips)))

    lines = [f"{_rel(anim)} - {len(clips)} clip(s)"]
    for i in indices:
        c = clips[i]
        lines.append(f"\n[{i}] '{c['name']}'  duration {c['duration']:.4f}s  "
                     f"{len(c['channels'])} animated bone(s)")
        for ch in c["channels"]:
            if bone and ch["bone"] != bone:
                continue
            times = _key_times(ch)
            lines.append(f"  {ch['bone']}: {len(ch['positions'])} pos, "
                         f"{len(ch['rotations'])} rot, {len(ch['scales'])} scale keys")
            if bone:
                for t in times:
                    parts = []
                    for k in ch["positions"]:
                        if abs(k["time"] - t) < 0.001:
                            parts.append("pos(%.4f, %.4f, %.4f)" % tuple(k["value"]))
                    for k in ch["rotations"]:
                        if abs(k["time"] - t) < 0.001:
                            parts.append("rot(x=%.4f, y=%.4f, z=%.4f, w=%.4f)" % tuple(k["value"]))
                    lines.append(f"    t={t:.4f}s  {'  '.join(parts)}")
            elif times:
                shown = ", ".join(f"{t:.3f}" for t in times[:12])
                more = "" if len(times) <= 12 else f", ... (+{len(times) - 12})"
                lines.append(f"    key times: {shown}{more}")
    return "\n".join(lines)


@mcp.tool()
def import_animation(project_path: str, source_file: str, name: str | None = None,
                     open_in_editor: bool = True) -> str:
    """Convert a source file's animation tracks into assets/animations/<name>.p3da.

    source_file: fbx / dae / gltf / blend / ... (anything Assimp reads), or an
                 existing .p3da to copy in.
    name:        output file stem; defaults to the source's.

    Note the converter writes EVERY clip the source contains into the one
    .p3da, in source order - and that order fixes each clip's runtime id.
    Requires a running editor (the converter is invoked by it).
    """
    proj, err = _resolve_project(project_path)
    if not proj:
        return _fail(err)
    note = _live_open_project(proj)

    src = source_file if os.path.isabs(source_file) else str((ROOT / source_file).resolve())
    if not os.path.exists(src):
        return _fail(f"Source file not found: {source_file}")

    ok, res = _editor_call("import_animation",
                           {"source": src, "name": name or "", "open": open_in_editor},
                           timeout=180.0)
    if not ok:
        return _fail(f"Import failed: {res}")

    out = res.get("path", "")
    lines = [f"Imported animation: {out}"]
    if open_in_editor and res.get("opened"):
        lines.append("Opened in the Animation Editor.")
    lines.append(note.strip() if note.strip() else "")
    return "\n".join(l for l in lines if l)


@mcp.tool()
def create_animation(project_path: str, name: str, clip_name: str = "Clip",
                     duration: float = 1.0) -> str:
    """Create an empty animation file with one empty clip.

    Writes assets/animations/<name>.p3da directly - no editor needed. Add
    keys with set_animation_keyframe, or open it in the editor and pose the
    rig by hand.
    """
    proj, err = _resolve_project(project_path)
    if not proj:
        return _fail(err)

    anim = _animation_file(proj, name)
    if anim.exists():
        return _fail(f"Already exists: {_rel(anim)}")
    if duration <= 0:
        return _fail("duration must be positive")

    try:
        _write_p3da(anim, [{"name": clip_name, "duration": duration, "channels": []}])
    except Exception as e:
        return _fail(f"Could not write {_rel(anim)}: {e}")
    return (f"Created {anim.relative_to(proj).as_posix()} with clip [0] '{clip_name}' "
            f"({duration:.3f}s, no keys yet).")


def _mutate_animation(project_path: str, animation: str, fn) -> str:
    """Load a .p3da, apply fn(clips) -> str, write it back.

    Refuses when a running editor has the file open, since the editor's
    in-memory copy would overwrite the change on its next save. The message
    points at the live tool that does work in that case.
    """
    proj, anim, err = _animation_target(project_path, animation)
    if err:
        return err

    if _live_animation_open(anim):
        return _fail(
            f"{_rel(anim)} is open in the running editor - editing the file behind it would be "
            f"overwritten on its next save. Use the live tools (pose_animation_bone, "
            f"key_animation_pose, animation_keyframe_live) or close that tab first.")

    try:
        clips = _read_p3da(anim)
    except Exception as e:
        return _fail(f"Could not read {_rel(anim)}: {e}")

    try:
        message = fn(clips)
    except ValueError as e:
        return _fail(str(e))

    try:
        _write_p3da(anim, clips)
    except Exception as e:
        return _fail(f"Could not write {_rel(anim)}: {e}")
    return message


@mcp.tool()
def add_animation_clip(project_path: str, animation: str, name: str,
                       duration: float = 1.0) -> str:
    """Append a new empty clip to an animation file.

    The new clip's runtime id is its index, i.e. the number of clips that
    were already there.
    """
    def apply(clips):
        if any(c.get("name") == name for c in clips):
            raise ValueError(f"a clip named '{name}' already exists")
        clips.append({"name": name, "duration": max(0.05, duration), "channels": []})
        return f"Added clip [{len(clips) - 1}] '{name}' ({duration:.3f}s)."
    return _mutate_animation(project_path, animation, apply)


@mcp.tool()
def remove_animation_clip(project_path: str, animation: str, clip: str) -> str:
    """Delete a clip from an animation file.

    Later clips shift down one id, but scenes save each clip's guid and
    resolve through SkeletonAnimation::ResolveAnimationID, so they follow the
    move. Only scenes that referenced THIS clip are affected, and those warn
    on load rather than silently playing something else.
    """
    def apply(clips):
        idx = _find_clip(clips, clip)
        removed = clips.pop(idx)
        tail = "" if idx == len(clips) else \
            " Later clips shifted id; scenes resolve by guid, so they follow."
        return f"Removed clip [{idx}] '{removed['name']}'.{tail}"
    return _mutate_animation(project_path, animation, apply)


@mcp.tool()
def rename_animation_clip(project_path: str, animation: str, clip: str, new_name: str) -> str:
    """Rename a clip. Its id (index) is unchanged, so scenes keep working."""
    def apply(clips):
        idx = _find_clip(clips, clip)
        old = clips[idx]["name"]
        clips[idx]["name"] = new_name
        return f"Renamed clip [{idx}] '{old}' -> '{new_name}'."
    return _mutate_animation(project_path, animation, apply)


@mcp.tool()
def set_animation_clip_duration(project_path: str, animation: str, clip: str,
                                duration: float) -> str:
    """Set a clip's length in seconds. Keys past the new end stop being reached."""
    def apply(clips):
        idx = _find_clip(clips, clip)
        if duration <= 0:
            raise ValueError("duration must be positive")
        clips[idx]["duration"] = duration
        lost = 0
        for ch in clips[idx]["channels"]:
            lost += sum(1 for t in _key_times(ch) if t > duration)
        note = f" ({lost} key column(s) now sit past the end)" if lost else ""
        return f"Clip [{idx}] '{clips[idx]['name']}' duration -> {duration:.3f}s.{note}"
    return _mutate_animation(project_path, animation, apply)


@mcp.tool()
def set_animation_keyframe(project_path: str, animation: str, bone: str, time: float,
                           clip: str | None = None,
                           position: list[float] | None = None,
                           rotation_euler: list[float] | None = None) -> str:
    """Write a keyframe for one bone at one time, directly into the .p3da.

    bone:           bone name exactly as the rig spells it (see
                    get_animation_skeleton against a live editor, or
                    get_animation_info for bones the clip already drives).
    time:           seconds from the clip's start.
    position:       [x, y, z] bone-LOCAL translation. Omit to leave the
                    position track alone.
    rotation_euler: [x, y, z] degrees, bone-LOCAL. Omit to leave rotation alone.

    Both are local to the bone's parent, which is what the format stores -
    the runtime composes the parent chain itself.
    """
    def apply(clips):
        idx = _find_clip(clips, clip)
        c = clips[idx]
        if position is None and rotation_euler is None:
            raise ValueError("pass position and/or rotation_euler")
        if time < 0:
            raise ValueError("time must be >= 0")

        ch = next((x for x in c["channels"] if x["bone"] == bone), None)
        if ch is None:
            ch = {"bone": bone, "positions": [], "rotations": [], "scales": []}
            c["channels"].append(ch)

        wrote = []
        if position is not None:
            if len(position) != 3:
                raise ValueError("position must be [x, y, z]")
            ch["positions"] = [k for k in ch["positions"] if abs(k["time"] - time) > 0.001]
            ch["positions"].append({"time": time, "value": [float(v) for v in position]})
            ch["positions"].sort(key=lambda k: k["time"])
            wrote.append("position")
        if rotation_euler is not None:
            if len(rotation_euler) != 3:
                raise ValueError("rotation_euler must be [x, y, z] in degrees")
            quat = _euler_to_quat_xyzw([float(v) for v in rotation_euler])
            ch["rotations"] = [k for k in ch["rotations"] if abs(k["time"] - time) > 0.001]
            ch["rotations"].append({"time": time, "value": quat})
            ch["rotations"].sort(key=lambda k: k["time"])
            wrote.append("rotation")

        # A key past the end would never be reached at playback, so the clip
        # grows to fit - same behaviour as the editor's own key action.
        grew = ""
        if time > c["duration"]:
            c["duration"] = time
            grew = f" Clip extended to {time:.3f}s."
        return f"Keyed {' + '.join(wrote)} for '{bone}' at {time:.3f}s in clip [{idx}] '{c['name']}'.{grew}"
    return _mutate_animation(project_path, animation, apply)


@mcp.tool()
def delete_animation_keyframe(project_path: str, animation: str, bone: str, time: float,
                              clip: str | None = None) -> str:
    """Remove every key (position/rotation/scale) for one bone at one time."""
    def apply(clips):
        idx = _find_clip(clips, clip)
        c = clips[idx]
        ch = next((x for x in c["channels"] if x["bone"] == bone), None)
        if ch is None:
            raise ValueError(f"clip [{idx}] has no keys for bone '{bone}'")
        before = len(ch["positions"]) + len(ch["rotations"]) + len(ch["scales"])
        for comp in ("positions", "rotations", "scales"):
            ch[comp] = [k for k in ch[comp] if abs(k["time"] - time) > 0.001]
        after = len(ch["positions"]) + len(ch["rotations"]) + len(ch["scales"])
        if before == after:
            raise ValueError(f"no key for '{bone}' at {time:.3f}s")
        if after == 0:
            c["channels"] = [x for x in c["channels"] if x is not ch]
        return f"Removed {before - after} key(s) for '{bone}' at {time:.3f}s."
    return _mutate_animation(project_path, animation, apply)


# ---- live editor tools ----------------------------------------------------

@mcp.tool()
def open_animation_editor(project_path: str, animation: str, mesh: str | None = None) -> str:
    """Open a .p3da (or a .p3dm, for a fresh clip) in the editor's Animation Editor.

    mesh: project-relative .p3dm to preview the clips on. Omitted, the editor
    keeps whatever rig is remembered for this file in project.json, or matches
    one by bone names.

    Requires a running editor.
    """
    proj, err = _resolve_project(project_path)
    if not proj:
        return _fail(err)
    note = _live_open_project(proj)

    target = animation
    if not os.path.isabs(target):
        if target.endswith(".p3dm"):
            target = str(proj / target) if "/" in target else target
        else:
            target = str(_animation_file(proj, target))

    args = {"path": target}
    if mesh:
        args["mesh"] = mesh
    ok, res = _editor_call("open_animation", args, timeout=120.0)
    if not ok:
        return _fail(f"Could not open animation: {res}")

    lines = [f"Animation Editor: {res.get('path')}"]
    if res.get("mesh"):
        lines.append(f"  rig: {res['mesh']} ({res.get('bones', 0)} bones)")
    elif res.get("meshError"):
        lines.append(f"  rig error: {res['meshError']}")
    else:
        lines.append("  no rig bound - pass mesh= to preview and pose it")
    for c in res.get("clips", []):
        lines.append(f"  [{c['id']}] {c['name']}  {c['duration']:.3f}s  "
                     f"{c['channels']} bones, {c['keys']} keys")
    if note.strip():
        lines.append(note.strip())
    return "\n".join(lines)


@mcp.tool()
def get_animation_skeleton(animation: str | None = None) -> str:
    """List the bones of the rig bound to the open animation document.

    Shows each bone's id, parent and whether the active clip animates it.
    This is the reliable way to learn exact bone names before keying.
    Requires a running editor with an animation open.
    """
    args = {"path": animation} if animation else {}
    ok, res = _editor_call("animation_skeleton", args)
    if not ok:
        return _fail(f"Could not read skeleton: {res}")
    bones = res.get("bones", [])
    if not bones:
        return "The bound rig has no bones."
    lines = [f"{len(bones)} bones:"]
    for b in bones:
        mark = "*" if b.get("animated") else " "
        lines.append(f" {mark} [{b['id']:>3}] {b['name']}  (parent {b['parent']})")
    lines.append("\n* = animated by the active clip")
    return "\n".join(lines)


@mcp.tool()
def pose_animation_bone(bone: str, position: list[float] | None = None,
                        rotation_euler: list[float] | None = None,
                        key: bool = False, animation: str | None = None) -> str:
    """Pose one bone in the live Animation Editor viewport.

    Values are bone-local; anything omitted keeps the bone's current value,
    so rotation can be set without disturbing position. The pose is visible
    immediately but not saved into the clip unless key=True (or you later
    call key_animation_pose) - scrubbing the playhead discards it, exactly
    like dragging the bone gizmo by hand.
    """
    args: dict[str, Any] = {"bone": bone, "key": key}
    if position is not None:
        args["position"] = position
    if rotation_euler is not None:
        args["rotation"] = rotation_euler
    if animation:
        args["path"] = animation
    ok, res = _editor_call("set_animation_pose", args)
    if not ok:
        return _fail(f"Pose failed: {res}")
    pos = res.get("position", [])
    rot = res.get("rotation", [])
    tail = f" and keyed at {res.get('time', 0):.3f}s" if res.get("keyed") else " (not keyed)"
    return (f"Posed '{bone}' to position ({pos[0]:.3f}, {pos[1]:.3f}, {pos[2]:.3f}), "
            f"rotation ({rot[0]:.2f}, {rot[1]:.2f}, {rot[2]:.2f}) degrees{tail}.")


@mcp.tool()
def key_animation_pose(time: float | None = None, all_bones: bool = False,
                       animation: str | None = None) -> str:
    """Commit the live viewport's current pose into the active clip as keys.

    time:      seconds; defaults to where the playhead already is.
    all_bones: key every bone in the skeleton (blocking out a full pose)
               rather than only the bones posed since the last key.
    """
    args: dict[str, Any] = {"allBones": all_bones}
    if time is not None:
        args["time"] = time
    if animation:
        args["path"] = animation
    ok, res = _editor_call("key_animation_pose", args)
    if not ok:
        return _fail(f"Key failed: {res}")
    n = res.get("keyed", 0)
    if not n:
        return "Nothing keyed - no bones were posed. Pose one first, or pass all_bones=True."
    return f"Keyed {n} bone(s) at {res.get('time', 0):.3f}s."


@mcp.tool()
def animation_playback(action: str = "scrub", time: float | None = None,
                       loop: bool | None = None, speed: float | None = None,
                       animation: str | None = None) -> str:
    """Drive the Animation Editor's transport.

    action: 'play', 'pause', 'stop' or 'scrub' (with time=seconds).
    Scrubbing re-poses the rig from the clip, which is how you check what a
    keyed pose actually looks like at a given moment.
    """
    args: dict[str, Any] = {"action": action}
    if time is not None:
        args["time"] = time
    if loop is not None:
        args["loop"] = loop
    if speed is not None:
        args["speed"] = speed
    if animation:
        args["path"] = animation
    ok, res = _editor_call("animation_playback", args)
    if not ok:
        return _fail(f"Playback failed: {res}")
    return f"Playhead at {res.get('playhead', 0):.3f}s, {'playing' if res.get('playing') else 'paused'}."


@mcp.tool()
def save_animation(save_as: str | None = None, animation: str | None = None) -> str:
    """Save the open animation document to its .p3da.

    save_as: write to assets/animations/<save_as>.p3da instead. Required for a
    document created with 'New Animation', which has no file yet.
    """
    args: dict[str, Any] = {}
    if save_as:
        args["as"] = save_as
    if animation:
        args["path"] = animation
    ok, res = _editor_call("save_animation", args)
    if not ok:
        return _fail(f"Save failed: {res}")
    return f"Saved {res.get('saved')}"


@mcp.tool()
def open_scene(project_path: str, scene_name: str) -> str:
    """Open a scene as ANOTHER document, beside whatever is already open.

    Distinct from set_active_scene / loading one, which replaces the current
    document. Two open documents each keep their own renderer, selection and
    undo history - which matters because rules like "a 2D scene always uses
    forward" are per document, not per project.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    if not _live_project_matches(proj):
        return _fail("open_scene needs the editor open on this project.")
    scene_file = _scene_file(proj, scene_name)
    try:
        rel = str(scene_file.relative_to(proj))
    except Exception:
        rel = f"scenes/{scene_name}.json"
    ok, res = _editor_call("open_scene", {"path": rel})
    if not ok:
        return _fail(str(res))
    return f"opened '{rel}' as a second document"


@mcp.tool()
def material_undo_redo(action: str = "undo", material: str | None = None) -> str:
    """Undo or redo the last edit in the open Material Editor document.

    Each material document owns its history, separate from the scene's - the
    editor routes Ctrl+Z by which document was last EDITED, and an agent has
    no focus to route by, so it says which one it means.
    """
    if action not in ("undo", "redo"):
        return _fail("action must be 'undo' or 'redo'")
    cmd = "undo_material" if action == "undo" else "redo_material"
    ok, res = _editor_call(cmd, {"path": material} if material else {})
    if not ok:
        return _fail(f"{action} failed: {res}")
    return (f"{action} ok. "
            f"{'more undo available' if res.get('canUndo') else 'nothing left to undo'}, "
            f"{'redo available' if res.get('canRedo') else 'no redo'}.")


@mcp.tool()
def animation_undo_redo(action: str = "undo", animation: str | None = None) -> str:
    """Undo or redo the last edit in the open Animation Editor document.

    Each document has its own history (keys, poses, clip changes) - separate
    from the scene's and the material editor's.
    """
    cmd = "undo_animation" if action == "undo" else "redo_animation"
    ok, res = _editor_call(cmd, {"path": animation} if animation else {})
    if not ok:
        return _fail(f"{action} failed: {res}")
    return (f"{action} ok. "
            f"{'more undo available' if res.get('canUndo') else 'nothing left to undo'}, "
            f"{'redo available' if res.get('canRedo') else 'no redo'}.")


# ---- blending -------------------------------------------------------------
#
# Blending is a RUNTIME feature: the engine plays several clips at once and the
# game drives their weights from gameplay state each frame
# (SkeletonAnimationInstance::ChangeProperties). What the editor owns is the
# setup and the preview - seeing what a mix looks like, and authoring the bone
# masks ("layers") that restrict a clip to part of the skeleton. The output is
# the Lua that reproduces the setup, via get_animation_blend_lua.


def _blend_report(res: dict) -> str:
    lines = [f"Blend mode: {'on' if res.get('blendMode') else 'off'}  "
             f"clock {res.get('clock', 0):.2f}s  "
             f"{'playing' if res.get('playing') else 'paused'}"]
    entries = res.get("entries", [])
    if entries:
        lines.append("Clips:")
        for e in entries:
            layer = e.get("layer") or "(whole body)"
            lines.append(f"  [{e['index']}] clip {e['clip']} '{e.get('clipName','')}'  "
                         f"weight {e['weight']:.2f}  speed {e['speed']:.2f}x  layer {layer}")
    else:
        lines.append("No clips in the blend yet.")
    layers = res.get("layers", [])
    if layers:
        lines.append("Layers:")
        for l in layers:
            bones = l.get("bones", [])
            preview = ", ".join(bones[:4]) + (f", ... (+{len(bones)-4})" if len(bones) > 4 else "")
            lines.append(f"  {l['name']}: {len(bones)} bones  [{preview}]")
    return "\n".join(lines)


@mcp.tool()
def animation_blend(action: str = "state", clip: str | None = None, index: int | None = None,
                    weight: float | None = None, speed: float | None = None,
                    layer: str | None = None, enabled: bool | None = None,
                    animation: str | None = None) -> str:
    """Configure and preview a multi-clip blend on the open animation document.

    action:
      'state'  - just report the current blend (default)
      'mode'   - turn blend preview on/off with enabled=
      'add'    - add clip= to the blend, optionally weight=/speed=/layer=
      'set'    - change index='s weight=/speed=/layer=
      'remove' - drop entry index=
      'clear'  - remove every entry and layer

    weight is 0..1 and means what you expect ("how much of this clip you see").
    The engine's own Play() argument is inverted from that; the editor converts.

    Requires a running editor with an animation open. Weights are previewed
    here but driven by your game code at runtime - see get_animation_blend_lua.
    """
    args: dict[str, Any] = {"action": action}
    if clip is not None:
        args["clip"] = int(clip) if str(clip).isdigit() else clip
    if index is not None:
        args["index"] = index
    if weight is not None:
        args["weight"] = weight
    if speed is not None:
        args["speed"] = speed
    if layer is not None:
        args["layer"] = layer
    if enabled is not None:
        args["enabled"] = enabled
    if animation:
        args["path"] = animation

    ok, res = _editor_call("animation_blend", args)
    if not ok:
        return _fail(f"Blend {action} failed: {res}")
    return _blend_report(res)


@mcp.tool()
def animation_blend_layer(layer: str, bone: str | None = None, children: bool = True,
                          animation: str | None = None) -> str:
    """Create a blend layer (bone mask) and add bones to it.

    layer:    layer name, created if it doesn't exist.
    bone:     bone to add; omit to just create an empty layer.
    children: also add every bone below it in the hierarchy (default true) -
              'Spine1' with children is the usual way to get an upper body.

    A layer restricts a clip to those bones, so the upper body can play one
    clip while the legs play another. This is the tedious part to hand-write:
    the runtime wants an explicit addBone() call per bone.
    """
    args: dict[str, Any] = {"action": "layer", "layer": layer, "children": children}
    if bone:
        args["bone"] = bone
    if animation:
        args["path"] = animation
    ok, res = _editor_call("animation_blend", args)
    if not ok:
        return _fail(f"Layer edit failed: {res}")
    return _blend_report(res)


# ---- rig sidecar ----------------------------------------------------------
# <model>.rig.json beside the .p3dm. Second implementation of the same schema
# as p3d::RigAsset (include/Pyros3D/AnimationManager/RigAsset.h) - keep the two
# in lockstep, a divergence here shows up as silently missing joint limits
# rather than a load error.
#
# Joint limit angles are DEGREES on disk and radians only inside the engine.


def _model_file(proj: Path, model_name: str) -> Path:
    """Resolve a model argument to a file under the project, like _animation_file."""
    name = model_name.strip()
    if os.path.isabs(name):
        return Path(name)
    if not name.endswith(".p3dm"):
        name += ".p3dm"
    if "/" in name:
        return proj / name
    return proj / "assets" / "models" / name


def _rig_path_for(model_path: Path) -> Path:
    """model.p3dm -> model.rig.json, matching RigAsset::SidecarPathFor."""
    return model_path.with_suffix(".rig.json")


def _read_rig(path: Path) -> dict:
    """Missing file is an empty rig, not an error - same as RigAsset::Load."""
    if not path.exists():
        return {"version": 1, "boneMasks": [], "jointLimits": [], "ikChains": []}
    with path.open() as fh:
        data = json.load(fh)
    if not isinstance(data, dict):
        raise ValueError(f"{path.name} is not a JSON object")
    data.setdefault("boneMasks", [])
    data.setdefault("jointLimits", [])
    data.setdefault("ikChains", [])
    return data


def _write_rig(path: Path, rig: dict) -> None:
    rig["version"] = 1
    rig["_comment"] = ("Rig data for the .p3dm beside this file. Keyed by bone name, "
                       "so models sharing a skeleton can share this file. "
                       "Joint limit angles are DEGREES.")
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w") as fh:
        json.dump(rig, fh, indent="\t", sort_keys=True)
        fh.write("\n")


@mcp.tool()
def get_rig(project_path: str, model: str) -> str:
    """Read a model's rig sidecar - bone masks, joint limits, IK chains.

    model: path to the .p3dm, project-relative or absolute. The rig is read
           from <model>.rig.json beside it.

    Rig data describes the SKELETON, not any one clip: an 'UpperBody' mask and
    a knee's limit mean the same thing for every animation played on that rig,
    and the file is shareable between models that share a skeleton.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    rig_path = _rig_path_for(_model_file(proj, model))
    try:
        rig = _read_rig(rig_path)
    except (ValueError, json.JSONDecodeError) as exc:
        return _fail(f"Could not read {rig_path.name}: {exc}")

    lines = [f"Rig: {rig_path.name}" + ("" if rig_path.exists() else "  (does not exist yet)")]
    lines.append(f"  bone masks : {len(rig['boneMasks'])}")
    for m in rig["boneMasks"]:
        lines.append(f"    {m.get('name','?')} ({len(m.get('bones', []))} bones)")
    lines.append(f"  joint limits: {len(rig['jointLimits'])}")
    for l in rig["jointLimits"]:
        state = "" if l.get("enabled", True) else "  (disabled)"
        lines.append(f"    {l.get('bone','?')}  min {l.get('minDeg')}  max {l.get('maxDeg')}{state}")
    lines.append(f"  ik chains  : {len(rig['ikChains'])}")
    for c in rig["ikChains"]:
        pole = f"  pole {c['pole']}" if c.get("usePole") else ""
        lines.append(f"    {c.get('name','?')}: {c.get('root','?')} -> {c.get('effector','?')}{pole}")
    return "\n".join(lines)


@mcp.tool()
def set_joint_limit(project_path: str, model: str, bone: str,
                    min_deg: list[float] | None = None,
                    max_deg: list[float] | None = None,
                    enabled: bool = True) -> str:
    """Set a bone's rotation limit in the model's rig sidecar.

    bone:    bone name, e.g. 'Bip01_L_Calf'.
    min_deg: [x, y, z] lower bounds in DEGREES. Defaults to fully open.
    max_deg: [x, y, z] upper bounds in DEGREES. Defaults to fully open.

    Limits are what stop an IK solver bending a knee backwards - both
    solutions reach the target, and without a limit nothing prefers the
    anatomically possible one. A knee is typically min [0,0,0] max [150,0,0]:
    it bends about one axis only.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    rig_path = _rig_path_for(_model_file(proj, model))
    try:
        rig = _read_rig(rig_path)
    except (ValueError, json.JSONDecodeError) as exc:
        return _fail(f"Could not read {rig_path.name}: {exc}")

    lo = list(min_deg) if min_deg else [-180.0, -180.0, -180.0]
    hi = list(max_deg) if max_deg else [180.0, 180.0, 180.0]
    if len(lo) != 3 or len(hi) != 3:
        return _fail("min_deg and max_deg must each be three numbers [x, y, z]")
    for axis in range(3):
        if lo[axis] > hi[axis]:
            return _fail(f"min_deg[{axis}] ({lo[axis]}) is above max_deg[{axis}] ({hi[axis]})")

    rig["jointLimits"] = [l for l in rig["jointLimits"] if l.get("bone") != bone]
    rig["jointLimits"].append({"bone": bone, "minDeg": lo, "maxDeg": hi, "enabled": enabled})
    _write_rig(rig_path, rig)
    return f"Set joint limit on '{bone}' in {rig_path.name}: min {lo} max {hi} deg."


@mcp.tool()
def set_ik_chain(project_path: str, model: str, name: str, root: str, effector: str,
                 pole: list[float] | None = None) -> str:
    """Define an IK chain in the model's rig sidecar.

    root/effector: bone names. The effector must descend from the root - a
                   three-bone chain (thigh/calf/foot) gets the exact
                   closed-form two-bone solve, longer chains use FABRIK.
    pole:          optional [x, y, z] model-space hint for which way the
                   knee or elbow points. It only steers the bend direction;
                   it cannot move the effector off the target.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    rig_path = _rig_path_for(_model_file(proj, model))
    try:
        rig = _read_rig(rig_path)
    except (ValueError, json.JSONDecodeError) as exc:
        return _fail(f"Could not read {rig_path.name}: {exc}")

    entry: dict[str, Any] = {"name": name, "root": root, "effector": effector,
                             "usePole": bool(pole)}
    if pole:
        if len(pole) != 3:
            return _fail("pole must be three numbers [x, y, z]")
        entry["pole"] = list(pole)

    rig["ikChains"] = [c for c in rig["ikChains"] if c.get("name") != name]
    rig["ikChains"].append(entry)
    _write_rig(rig_path, rig)
    return f"Set IK chain '{name}' in {rig_path.name}: {root} -> {effector}."


@mcp.tool()
def get_animation_blend_lua(animation: str | None = None) -> str:
    """Get the Lua that reproduces the current blend setup at runtime.

    Emits the createLayer/addBone/play calls the preview is making, ready to
    save as a GameObject script and attach to the rig. The weight-driving code
    is left as a commented example, since that is the part your game decides.
    """
    args = {"action": "lua"}
    if animation:
        args["path"] = animation
    ok, res = _editor_call("animation_blend", args)
    if not ok:
        return _fail(f"Could not build the snippet: {res}")
    return res.get("lua", "")


# ---------------------------------------------------------------------------
# Screen-space UI
#
# UI elements are ordinary GameObjects carrying UI components, so the offline
# path here is the same scene-JSON editing every other tool does. The two
# style tools are live-only: resolving a .uistyle means reading the palette,
# resolving "@name" colours and importing textures, and a second
# implementation of that in Python would be a second thing to disagree with
# the editor about.
# ---------------------------------------------------------------------------

UI_DEFAULT_RECT = {
    "type": "UIRect",
    "anchorMin": [0.5, 0.5], "anchorMax": [0.5, 0.5],
    "offsetMin": [-160.0, -48.0], "offsetMax": [160.0, 48.0],
    "pivot": [0.5, 0.5],
}


def _ui_find_font(proj: Path) -> str | None:
    """First .ttf/.otf already in the project, as a project-relative path.

    Same rule the editor uses. It does not import one here - a tool that
    silently copies files into a project is a surprise; the editor does that
    because a person asked it to add a Text and is watching.
    """
    assets = proj / "assets"
    if not assets.is_dir():
        return None
    for f in sorted(assets.rglob("*")):
        if f.suffix.lower() in (".ttf", ".otf"):
            # Project-relative, which is what a scene file stores.
            return f.relative_to(proj).as_posix()
    return None


def _ui_components(node: dict) -> list:
    return node.setdefault("components", [])


def _ui_has(node: dict, type_name: str) -> bool:
    return any(c.get("type") == type_name for c in node.get("components", []))


@mcp.tool()
def add_ui(project_path: str, scene_name: str, object_name: str, kind: str,
           font: str | None = None) -> str:
    """Add a screen-space UI component to an object.

    kind: canvas | rect | image | text | button | toggle | slider | input |
    list | dropdown | menu | popup. Image, text and button add a rect if the object has none;
    button also adds an image. A canvas is the root of a UI tree - put
    elements under it as children.

    The five widget kinds build child elements too (a checkbox's tick, a
    slider's fill and handle, a list's rows), so they need the live editor -
    writing the scene file directly would mean a second copy of those
    templates here, drifting from the editor's.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    k = (kind or "").strip().lower()
    widget_kinds = ("toggle", "slider", "input", "list", "dropdown", "menu", "popup")
    if k not in ("canvas", "rect", "image", "text", "button") + widget_kinds:
        return _fail(f"Invalid kind '{kind}'. Use canvas, rect, image, text, button, "
                     "toggle, slider, input, list, dropdown, menu or popup.")

    live = _live_or_none("add_ui", {"object": object_name, "kind": k, "font": font or ""}, scene_file)
    if live is not None:
        return _fail(live) if isinstance(live, str) else f"Added UI {k} to '{object_name}' (live editor)"

    if k in widget_kinds:
        return _fail(f"A UI {k} is a component plus the child elements it drives, so it needs "
                     "the live editor - open the project there and try again.")

    data = _load_scene(scene_file)
    node = _find_object(data, object_name)
    if node is None:
        return _fail(f"Object '{object_name}' not found in scene '{scene_name}'")
    comps = _ui_components(node)

    if k == "canvas":
        if _ui_has(node, "UICanvas"):
            return _fail(f"'{object_name}' already has a UICanvas")
        comps.append({"type": "UICanvas", "referenceWidth": 1920.0, "referenceHeight": 1080.0,
                      "scaleMode": "MatchWidth", "sortOrder": 0})
    else:
        if not _ui_has(node, "UIRect"):
            comps.append(dict(UI_DEFAULT_RECT))
        if k in ("image", "button") and not _ui_has(node, "UIImage"):
            tint = [0.16, 0.19, 0.25, 0.95] if k == "button" else [0.14, 0.16, 0.21, 0.92]
            comps.append({"type": "UIImage", "tint": tint, "border": [0.0, 0.0, 0.0, 0.0]})
        if k == "text":
            font_rel = font or _ui_find_font(proj)
            if not font_rel:
                return _fail("No font in this project. Pass font=<project-relative .ttf>, "
                             "or add the component in the editor, which imports one for you.")
            comps.append({"type": "UIText", "font": font_rel, "fontSize": 32.0, "size": 40.0,
                          "text": "Text", "color": [0.95, 0.96, 1.0, 1.0],
                          "align": "Center", "verticalAlign": "Middle"})
        if k == "button":
            comps.append({"type": "UIButton", "interactable": True, "transition": 0.12,
                          "states": {
                              "Hover": {"tint": [0.24, 0.30, 0.40, 0.98]},
                              "Pressed": {"tint": [0.22, 0.74, 0.98, 1.0], "offset": [0.0, 2.0]},
                              "Disabled": {"tint": [0.14, 0.15, 0.18, 0.6]},
                          }})

    _save_scene(scene_file, data)
    return f"Added UI {k} to '{object_name}'"


# Which component each property belongs to. Shared by the offline writer below
# and by the error message, so an unknown key is rejected the same way the
# editor rejects it rather than silently doing nothing.
UI_PROPERTY_OWNER = {
    "anchorMin": "UIRect", "anchorMax": "UIRect", "offsetMin": "UIRect",
    "offsetMax": "UIRect", "pivot": "UIRect",
    "tint": "UIImage", "border": "UIImage", "texture": "UIImage",
    "text": "UIText", "size": "UIText", "color": "UIText",
    "align": "UIText", "verticalAlign": "UIText", "wrap": "UIText",
    "sdf": "UIText",
    "referenceWidth": "UICanvas", "referenceHeight": "UICanvas",
    "scaleMode": "UICanvas", "sortOrder": "UICanvas",
    "interactable": "UIButton", "transition": "UIButton", "onClick": "UIButton",
    # The rest of the widget set. A node carries one widget, so "value",
    # "selected" and "text" never compete: an input's label and a list's rows
    # are children, not siblings of the component that drives them.
    "value": "UIToggle/UISlider", "check": "UIToggle", "group": "UIToggle",
    "min": "UISlider", "max": "UISlider", "step": "UISlider",
    "vertical": "UISlider", "fill": "UISlider", "handle": "UISlider",
    "placeholder": "UIInput/UIDropdown", "maxLength": "UIInput",
    "password": "UIInput", "readOnly": "UIInput", "filter": "UIInput",
    "blinkRate": "UIInput", "onSubmit": "UIInput/UIList",
    "items": "UIList", "itemHeight": "UIList", "selected": "UIList/UIDropdown",
    "options": "UIDropdown", "onChange": "any widget",
    "submenu": "UIMenuItem",
    "open": "UIPopup", "modal": "UIPopup", "closeOnEscape": "UIPopup",
    "closeOnOutside": "UIPopup", "dialogElement": "UIPopup", "onClose": "UIPopup",
}
UI_BUTTON_STATE_PROPERTIES = {
    "hoverTint": ("Hover", "tint"), "pressedTint": ("Pressed", "tint"),
    "disabledTint": ("Disabled", "tint"),
    "hoverTextColor": ("Hover", "textColor"), "pressedTextColor": ("Pressed", "textColor"),
    "disabledTextColor": ("Disabled", "textColor"),
    "pressedOffset": ("Pressed", "offset"),
}


@mcp.tool()
def set_ui(project_path: str, scene_name: str, object_name: str, properties: dict) -> str:
    """Set properties on an object's UI components, in one undoable edit.

    Rect: anchorMin/anchorMax/offsetMin/offsetMax/pivot ([x,y]).
    Image: tint/border ([r,g,b,a] / [left,top,right,bottom]), texture (path).
    Text: text, size, color, align, verticalAlign, wrap, sdf.
    Canvas: referenceWidth/referenceHeight, scaleMode, sortOrder.
    Button: interactable, transition, onClick, hoverTint/pressedTint/
    disabledTint, hoverTextColor/pressedTextColor/disabledTextColor,
    pressedOffset.
    Toggle: value, check, group, plus every button property.
    Slider: value, min, max, step, vertical, fill, handle.
    Input: text, placeholder, maxLength, password, readOnly, filter,
    blinkRate, onSubmit.
    List: items, selected, itemHeight, onSubmit.
    Dropdown: options, selected, placeholder.
    Menu item: submenu, plus every button property.
    Popup: open, modal, closeOnEscape, closeOnOutside, dialogElement,
    onClose.
    Any widget: interactable, onChange.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)
    if not isinstance(properties, dict) or not properties:
        return _fail("properties must be a non-empty object")

    live = _live_or_none("set_ui", {"object": object_name, "properties": properties}, scene_file)
    if live is not None:
        return _fail(live) if isinstance(live, str) else \
            f"Set {len(properties)} propert{'y' if len(properties) == 1 else 'ies'} on '{object_name}' (live editor)"

    data = _load_scene(scene_file)
    node = _find_object(data, object_name)
    if node is None:
        return _fail(f"Object '{object_name}' not found in scene '{scene_name}'")
    comps = _ui_components(node)
    by_type = {c.get("type"): c for c in comps}

    for key, value in properties.items():
        if key in UI_BUTTON_STATE_PROPERTIES:
            comp = by_type.get("UIButton")
            if comp is None:
                return _fail(f"'{object_name}' has no UIButton, so '{key}' has nowhere to go")
            state, field = UI_BUTTON_STATE_PROPERTIES[key]
            comp.setdefault("states", {}).setdefault(state, {})[field] = value
            continue
        owner = UI_PROPERTY_OWNER.get(key)
        if owner is None:
            return _fail(f"'{key}' is not a UI property")
        comp = by_type.get(owner)
        if comp is None:
            return _fail(f"'{object_name}' has no {owner}, so '{key}' has nowhere to go")
        comp[key] = value

    _save_scene(scene_file, data)
    return f"Set {len(properties)} propert{'y' if len(properties) == 1 else 'ies'} on '{object_name}'"


@mcp.tool()
def canvas_drag(project_path: str, scene_name: str, object_name: str, handle: int,
                delta: list[float]) -> str:
    """Move or resize a UI element, the same edit dragging its handle makes.

    handle is row*3+column over the rect's handles: 0 top-left, 2 top-right,
    4 the body (moves it without resizing), 6 bottom-left, 8 bottom-right.
    delta is [x, y] in canvas units.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)
    if not isinstance(handle, int) or not 0 <= handle <= 8:
        return _fail("handle must be 0..8 (row*3+column, 4 = the body)")
    if not isinstance(delta, (list, tuple)) or len(delta) != 2:
        return _fail("delta must be [x, y] in canvas units")

    live = _live_or_none("canvas_drag", {"object": object_name, "handle": handle,
                                         "delta": list(delta)}, scene_file)
    if live is not None:
        return _fail(live) if isinstance(live, str) else f"Moved '{object_name}' (live editor)"

    data = _load_scene(scene_file)
    node = _find_object(data, object_name)
    if node is None:
        return _fail(f"Object '{object_name}' not found in scene '{scene_name}'")
    rect = next((c for c in node.get("components", []) if c.get("type") == "UIRect"), None)
    if rect is None:
        return _fail(f"'{object_name}' has no UIRect")

    # rect.x is anchorX0 + offsetMin.x and rect.right is anchorX1 + offsetMax.x,
    # so each edge maps to exactly one offset component - the same four lines
    # the editor applies.
    hx, hy, body = handle % 3, handle // 3, handle == 4
    o_min = list(rect.get("offsetMin", [0.0, 0.0]))
    o_max = list(rect.get("offsetMax", [0.0, 0.0]))
    if body or hx == 0: o_min[0] += float(delta[0])
    if body or hx == 2: o_max[0] += float(delta[0])
    if body or hy == 0: o_min[1] += float(delta[1])
    if body or hy == 2: o_max[1] += float(delta[1])
    rect["offsetMin"], rect["offsetMax"] = o_min, o_max
    _save_scene(scene_file, data)
    return f"Moved '{object_name}': offsetMin={o_min}, offsetMax={o_max}"


@mcp.tool()
def apply_ui_style(object_name: str, style: str) -> str:
    """Apply a .uistyle to a UI element and record the link (live editor only).

    The link is what makes later edits to the style - or a palette swap -
    reach this element on the next load. Styles carry look only, never text
    or layout.
    """
    ok, res = _editor_call("apply_ui_style", {"object": object_name, "style": style})
    if not ok:
        return _fail(f"Could not apply the style: {res}")
    return f"Applied '{style}' to '{object_name}'"


@mcp.tool()
def list_ui_styles() -> str:
    """List the .uistyle assets in the open project (live editor only).

    Worth calling before apply_ui_style: the names are whatever the project
    happens to use, not a convention to guess at.
    """
    ok, res = _editor_call("list_ui_styles", {})
    if not ok:
        return _fail(f"Could not list styles: {res}")
    styles = res.get("styles", [])
    if not styles:
        return "No .uistyle assets yet - extract_ui_style makes the first one."
    return "\n".join(styles)


@mcp.tool()
def revert_ui_style(object_name: str) -> str:
    """Put a UI element back under its style, dropping hand-edits (live editor).

    Editing a property the style set marks it an override, and every later
    re-apply - on load, or when the style file changes - skips it. This drops
    those marks and re-applies in full.
    """
    ok, res = _editor_call("revert_ui_style", {"object": object_name})
    if not ok:
        return _fail(f"Could not revert: {res}")
    return f"'{object_name}' is following its style again"


@mcp.tool()
def clear_ui_style(object_name: str) -> str:
    """Unlink a UI element from its style (live editor only).

    The element keeps the look it has and simply stops following the file,
    so this is 'stop tracking', not 'revert'.
    """
    ok, res = _editor_call("clear_ui_style", {"object": object_name})
    if not ok:
        return _fail(f"Could not unlink: {res}")
    return f"'{object_name}' no longer follows a style"


@mcp.tool()
def extract_ui_style(object_name: str, name: str | None = None) -> str:
    """Write a .uistyle from an element's current look (live editor only).

    Saves into assets/ui and links the element to it. Colours that match a
    palette entry are written back as @names, so extracting from a themed
    element does not bake the theme into the style.
    """
    ok, res = _editor_call("extract_ui_style", {"object": object_name, "name": name or ""})
    if not ok:
        return _fail(f"Could not extract a style: {res}")
    return f"Wrote {res.get('path', 'the style')} from '{object_name}' and linked it"


@mcp.tool()
def canvas_mode(on: bool = True) -> str:
    """Switch the editor viewport between the 3D scene and 2D canvas editing.

    Canvas mode hides the world, the floor grid and the axis widget, and
    shows the canvas bounds, a canvas-unit grid and the selected element's
    rect with its handles.
    """
    ok, res = _editor_call("canvas_mode", {"on": bool(on)})
    if not ok:
        return _fail(f"Could not switch canvas mode: {res}")
    return "Canvas (2D) mode on" if on else "Back to the 3D scene view"


@mcp.tool()
def select_object(name: str = "") -> str:
    """Select a game object in the running editor (empty name deselects).

    Selection is what the transform gizmo, the Properties panel and the
    canvas overlay all act on, so this is the prerequisite for anything that
    edits "the selected object".
    """
    ok, res = _editor_call("select", {"name": name})
    if not ok:
        return _fail(f"Could not select: {res}")
    return f"Selected '{name}'" if name else "Selection cleared"


@mcp.tool()
def set_camera(project_path: str, scene_name: str, name: str,
               projection: str | None = None, fov: float | None = None,
               size: float | None = None, near: float | None = None,
               far: float | None = None, active: bool | None = None) -> str:
    """Change an existing scene camera. Only the fields given are touched.

    projection: 'perspective' or 'orthographic'. fov applies to perspective;
    size is half the view height in world units and applies to orthographic.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    args: dict = {"name": name}
    for key, value in (("projection", projection), ("fov", fov), ("size", size),
                       ("near", near), ("far", far), ("active", active)):
        if value is not None:
            args[key] = value
    if len(args) == 1:
        return _fail("Nothing to set - pass at least one of projection, fov, size, near, far, active")

    live = _live_or_none("set_camera", args, scene_file)
    if live is not None:
        return _fail(live) if isinstance(live, str) else f"Updated camera '{name}' (live editor)"

    # Offline: camera settings live in the .editor.json sidecar, which is also
    # what the player reads.
    sidecar = _read_sidecar(scene_file)
    cams = sidecar.setdefault("cameras", {})
    if name not in cams:
        return _fail(f"'{name}' is not a camera in scene '{scene_name}'")
    cam = cams[name]
    if projection is not None:
        p = projection.strip().lower()
        if p in ("orthographic", "ortho"):
            cam["orthographic"] = True
        elif p == "perspective":
            cam["orthographic"] = False
        else:
            return _fail("projection must be 'perspective' or 'orthographic'")
    if fov is not None: cam["fov"] = float(fov)
    if size is not None: cam["orthoSize"] = float(size)
    if near is not None: cam["near"] = float(near)
    if far is not None: cam["far"] = float(far)
    if active:
        sidecar["activeCamera"] = name
    _write_sidecar(scene_file, sidecar)
    return f"Updated camera '{name}'"


# ---------------------------------------------------------------------------
# Post effects
#
# The chain lives in the scene file under "postEffects", runs in order, and
# each entry reads what the one before produced - so position is as much of
# the edit as the parameters are.
#
# Mirrors PostEffectChain::ListBuiltIn/ListBuiltInParams. The running editor
# is the authority (list_post_effects asks it when it has the scene open);
# this table is what the offline path checks a name against, so a typo is an
# error here rather than an effect that silently never builds.
_BUILTIN_POST_EFFECTS: dict[str, dict[str, tuple[float, float, float]]] = {
    "Bloom":        {"uThreshold": (0.8, 0.0, 4.0), "uKnee": (0.35, 0.0, 1.0),
                     "uIntensity": (1.0, 0.0, 4.0)},
    "BlurX":        {},
    "BlurY":        {},
    "Tonemap":      {},
    "Vignette":     {"uRADIUS": (0.5, 0.0, 1.5), "uSOFTNESS": (0.2, 0.0, 1.0)},
    "GammaEncode":  {},
    "SSAO":         {"uRadius": (0.2, 0.01, 2.0), "uStrength": (1.5, 0.0, 5.0),
                     "uTreshOld": (2.0, 0.0, 10.0), "uScale": (100.0, 1.0, 400.0)},
    "DepthOfField": {"uFocalPosition": (20.0, 0.0, 500.0), "uFocalRange": (2.0, 0.01, 100.0),
                     "uRatioL": (3.1, 0.0, 8.0), "uRatioH": (1.0, 0.0, 8.0)},
    "MotionBlur":   {"uTargetFPS": (60.0, 15.0, 240.0)},
}

_EFFECT_PARAM_COMPONENTS = {"float": 1, "int": 1, "vec2": 2, "vec3": 3, "vec4": 4}


def _builtin_effect_name(name: str) -> str | None:
    """The built-in list's own spelling of `name`, or None."""
    for known in _BUILTIN_POST_EFFECTS:
        if known.lower() == name.strip().lower():
            return known
    return None


def _effect_assets(proj: Path) -> list[str]:
    """The project's .glsl effects, project-relative - assets/effects by convention."""
    d = proj / "assets" / "effects"
    if not d.is_dir():
        return []
    return sorted(f"assets/effects/{f.name}" for f in d.iterdir()
                  if f.is_file() and f.suffix == ".glsl")


def _parse_effect_asset(path: Path) -> tuple[str, list[dict], str]:
    """Name and parameters from a .glsl effect's `//!` header (see CustomEffect).

    Returns (name, params, error) - error non-empty means the file could not
    be read; a header with no directives is a valid effect with no knobs.
    """
    try:
        text = path.read_text()
    except OSError as exc:
        return "", [], str(exc)
    name = path.stem
    params: list[dict] = []
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped.startswith("//!"):
            continue
        words = stripped[3:].split()
        if not words:
            continue
        if words[0] == "effect" and len(words) >= 2:
            name = " ".join(words[1:])
        elif words[0] == "param" and len(words) >= 4:
            ptype = words[1]
            n = _EFFECT_PARAM_COMPONENTS.get(ptype)
            if n is None:
                continue
            try:
                default = [float(v) for v in words[3].split(",")][:n]
            except ValueError:
                default = [0.0] * n
            default += [0.0] * (n - len(default))
            p: dict = {"name": words[2], "type": ptype, "default": default}
            rest = words[4:]
            if n == 1 and len(rest) >= 2:
                try:
                    p["min"], p["max"] = float(rest[0]), float(rest[1])
                    rest = rest[2:]
                except ValueError:
                    pass
            p["label"] = " ".join(rest) if rest else words[2]
            params.append(p)
    return name, params, ""


def _effect_param_meta(proj: Path, effect: str, asset: str) -> tuple[dict[str, int] | None, str]:
    """{parameter name: how many numbers it takes} for one entry, or an error."""
    if effect:
        known = _builtin_effect_name(effect)
        if known is None:
            return None, (f"'{effect}' is not a built-in effect - there is "
                          f"{', '.join(_BUILTIN_POST_EFFECTS)}")
        return {name: 1 for name in _BUILTIN_POST_EFFECTS[known]}, ""
    file = proj / asset
    if not file.is_file():
        return None, f"Effect asset not found: {asset}"
    _, params, err = _parse_effect_asset(file)
    if err:
        return None, f"Could not read {asset}: {err}"
    return {p["name"]: _EFFECT_PARAM_COMPONENTS.get(p["type"], 1) for p in params}, ""


def _normalize_effect_params(params: dict | None, meta: dict[str, int], label: str) -> tuple[dict, str]:
    """Parameter overrides as {name: [numbers]}, checked against what the effect has.

    PostEffectChain::Build only logs an override it does not recognise, so a
    misspelled name would otherwise leave the value at its default with
    nothing said.
    """
    if not params:
        return {}, ""
    out: dict[str, list[float]] = {}
    for name, value in params.items():
        if name not in meta:
            return {}, (f"{label} has no parameter '{name}'"
                        + (f" - it takes {', '.join(meta)}" if meta else ", so it cannot be set"))
        if isinstance(value, (int, float)) and not isinstance(value, bool):
            nums = [float(value)]
        elif isinstance(value, (list, tuple)) and value and all(
                isinstance(v, (int, float)) and not isinstance(v, bool) for v in value):
            nums = [float(v) for v in value]
        else:
            return {}, f"Parameter '{name}' must be a number or a list of numbers"
        if len(nums) < meta[name]:
            return {}, f"{label}'s '{name}' needs {meta[name]} numbers"
        out[name] = nums[:4]
    return out, ""


def _match_post_effect(chain: list[dict], index: int | None,
                       effect: str | None, asset: str | None) -> tuple[int, str]:
    """Which entry a command means: index, or a built-in name, or an asset path."""
    if not chain:
        return -1, "This scene has no post effects"
    if index is not None:
        if index < 0 or index >= len(chain):
            return -1, f"index {index} is outside the chain ({len(chain)} effects)"
        return index, ""
    if not effect and not asset:
        return -1, ("Name the effect: index (0-based, and the only way to tell two of the "
                    "same effect apart), effect, or asset")
    for i, e in enumerate(chain):
        if effect and str(e.get("effect", "")).lower() == effect.strip().lower():
            return i, ""
        if asset and e.get("asset", "") == asset:
            return i, ""
    return -1, f"'{effect or asset}' is not in this scene's chain"


def _post_effect_label(entry: dict) -> str:
    return entry.get("effect") or entry.get("asset") or "effect"


@mcp.tool()
def list_post_effects(project_path: str, scene_name: str) -> str:
    """The scene's post-effect chain, and everything that could be added to it.

    Returns the chain in the order it runs (each effect reads what the one
    before produced), every built-in effect with the parameters it takes, and
    the project's .glsl effect assets. Read this before adding or tuning an
    effect: a name that does not exist builds nothing and says nothing.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    live = _live_or_none("post_effects", {}, scene_file)
    if isinstance(live, str):
        return _fail(live)
    if live is not None:
        return json.dumps(live, indent=2)

    data = _load_scene(scene_file)
    chain = []
    for i, e in enumerate(data.get("postEffects", []) or []):
        chain.append({"index": i, "effect": e.get("effect", ""), "asset": e.get("asset", ""),
                      "enabled": e.get("enabled", True), "params": e.get("params", {})})
    assets = []
    for rel in _effect_assets(proj):
        name, params, a_err = _parse_effect_asset(proj / rel)
        entry = {"path": rel, "name": name, "ok": not a_err, "params": params}
        if a_err:
            entry["error"] = a_err
        assets.append(entry)
    built_in = [{"name": name,
                 "params": [{"name": p, "type": "float", "default": [d], "min": lo, "max": hi}
                            for p, (d, lo, hi) in params.items()]}
                for name, params in _BUILTIN_POST_EFFECTS.items()]
    return json.dumps({"effects": chain, "builtIn": built_in, "assets": assets}, indent=2)


@mcp.tool()
def add_post_effect(project_path: str, scene_name: str, effect: str | None = None,
                    asset: str | None = None, index: int | None = None,
                    enabled: bool | None = None, params: dict | None = None) -> str:
    """Add a post effect to the scene's chain.

    Pass either effect (a built-in name - see list_post_effects) or asset (a
    project-relative .glsl). index is where in the chain it goes, appended by
    default; order is the edit, since each effect reads the previous one's
    output. params overrides the effect's defaults, {name: number or list}.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)
    if bool(effect) == bool(asset):
        return _fail("Pass exactly one of effect (a built-in name) or asset (a project-relative .glsl)")

    meta, m_err = _effect_param_meta(proj, effect or "", asset or "")
    if m_err:
        return _fail(m_err)
    label = _builtin_effect_name(effect) if effect else asset
    values, p_err = _normalize_effect_params(params, meta or {}, label or "")
    if p_err:
        return _fail(p_err)

    args: dict = {}
    if effect:
        args["effect"] = label
    else:
        args["asset"] = asset
    if index is not None:
        args["index"] = int(index)
    if enabled is not None:
        args["enabled"] = bool(enabled)
    if values:
        args["params"] = values

    live = _live_or_none("add_post_effect", args, scene_file)
    if isinstance(live, str):
        return _fail(live)
    if live is not None:
        return f"Added '{label}' at position {live.get('index', '?')} (live editor)"

    data = _load_scene(scene_file)
    chain = list(data.get("postEffects", []) or [])
    at = len(chain) if index is None else int(index)
    if at < 0 or at > len(chain):
        return _fail(f"index {at} is outside the chain (0..{len(chain)})")
    entry: dict = {"effect": label} if effect else {"asset": asset}
    if enabled is not None and not enabled:
        entry["enabled"] = False
    if values:
        entry["params"] = values
    chain.insert(at, entry)
    data["postEffects"] = chain
    _save_scene(scene_file, data)
    return f"Added '{label}' at position {at}"


@mcp.tool()
def set_post_effect(project_path: str, scene_name: str, index: int | None = None,
                    effect: str | None = None, asset: str | None = None,
                    enabled: bool | None = None, params: dict | None = None) -> str:
    """Turn one effect in the chain on/off and/or retune it.

    Name the entry by index (0-based, and the only way to tell two of the same
    effect apart), or by effect/asset. Parameters are merged, so setting one
    leaves the rest where they were.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)
    if enabled is None and not params:
        return _fail("Nothing to set - pass enabled and/or params")

    args: dict = {}
    if index is not None:
        args["index"] = int(index)
    if effect:
        args["effect"] = effect
    if asset:
        args["asset"] = asset
    if enabled is not None:
        args["enabled"] = bool(enabled)
    if params:
        args["params"] = params

    live = _live_or_none("set_post_effect", args, scene_file)
    if isinstance(live, str):
        return _fail(live)
    if live is not None:
        return "Updated the post effect (live editor)"

    data = _load_scene(scene_file)
    chain = list(data.get("postEffects", []) or [])
    at, m_err = _match_post_effect(chain, index, effect, asset)
    if m_err:
        return _fail(m_err)
    entry = dict(chain[at])
    meta, meta_err = _effect_param_meta(proj, entry.get("effect", ""), entry.get("asset", ""))
    if meta_err:
        return _fail(meta_err)
    values, p_err = _normalize_effect_params(params, meta or {}, _post_effect_label(entry))
    if p_err:
        return _fail(p_err)
    if enabled is not None:
        entry["enabled"] = bool(enabled)
    if values:
        merged = dict(entry.get("params", {}) or {})
        merged.update(values)
        entry["params"] = merged
    chain[at] = entry
    data["postEffects"] = chain
    _save_scene(scene_file, data)
    return f"Updated '{_post_effect_label(entry)}' at position {at}"


@mcp.tool()
def remove_post_effect(project_path: str, scene_name: str, index: int | None = None,
                       effect: str | None = None, asset: str | None = None) -> str:
    """Remove one effect from the scene's chain."""
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    args: dict = {}
    if index is not None:
        args["index"] = int(index)
    if effect:
        args["effect"] = effect
    if asset:
        args["asset"] = asset

    live = _live_or_none("remove_post_effect", args, scene_file)
    if isinstance(live, str):
        return _fail(live)
    if live is not None:
        return "Removed the post effect (live editor)"

    data = _load_scene(scene_file)
    chain = list(data.get("postEffects", []) or [])
    at, m_err = _match_post_effect(chain, index, effect, asset)
    if m_err:
        return _fail(m_err)
    label = _post_effect_label(chain[at])
    del chain[at]
    if chain:
        data["postEffects"] = chain
    else:
        # A scene with no chain has no "postEffects" key at all - that is what
        # the serializer writes, and it is what every scene without effects
        # already looks like.
        data.pop("postEffects", None)
    _save_scene(scene_file, data)
    return f"Removed '{label}'"


@mcp.tool()
def move_post_effect(project_path: str, scene_name: str, to: int, index: int | None = None,
                     effect: str | None = None, asset: str | None = None) -> str:
    """Move an effect to another position in the chain.

    What changes is what it reads: an effect sees whatever the one before it
    produced, so bloom before or after a tonemap are different pictures.
    """
    proj, err = _resolve_project(project_path)
    if err:
        return _fail(err)
    scene_file = _scene_file(proj, scene_name)
    s_err = _scene_error(scene_file)
    if s_err:
        return _fail(s_err)

    args: dict = {"to": int(to)}
    if index is not None:
        args["index"] = int(index)
    if effect:
        args["effect"] = effect
    if asset:
        args["asset"] = asset

    live = _live_or_none("move_post_effect", args, scene_file)
    if isinstance(live, str):
        return _fail(live)
    if live is not None:
        return f"Moved the post effect to position {to} (live editor)"

    data = _load_scene(scene_file)
    chain = list(data.get("postEffects", []) or [])
    at, m_err = _match_post_effect(chain, index, effect, asset)
    if m_err:
        return _fail(m_err)
    if to < 0 or to >= len(chain):
        return _fail(f"'to' is outside the chain (0..{len(chain) - 1})")
    entry = chain.pop(at)
    chain.insert(to, entry)
    data["postEffects"] = chain
    _save_scene(scene_file, data)
    return f"Moved '{_post_effect_label(entry)}' to position {to}"


if __name__ == "__main__":
    mcp.run()
