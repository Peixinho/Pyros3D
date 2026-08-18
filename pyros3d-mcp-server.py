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


def _live_scene_matches(scene_file: Path) -> bool:
    """True if a running editor has exactly this scene file open."""
    ok, res = _editor_call("status", {})
    if not ok:
        return False
    active = str(res.get("scenePath", ""))
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
def editor_screenshot(save_path: str | None = None) -> str:
    """Capture the editor's 3D viewport as a PNG image.

    Returns the path to the saved PNG. If save_path is omitted, saves to a
    temp file named pyros3d-screenshot.png.
    """
    ok, res = _editor_call("screenshot", {})
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


if __name__ == "__main__":
    mcp.run()
