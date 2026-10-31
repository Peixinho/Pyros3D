#!/usr/bin/env python3
# A game's folder packed for the browser player (player/web): game.data - every file end to end - and game.files.json
# saying where each is, as the editor's Build Game writes them (ProjectManager.cpp, PackWebGame). With the web
# player's page and module beside them, the folder is the game's site.
#
# In two parts when that saves much: game.data with what the first scene reads (and every script, shader, surface and
# small file), game.more.data with the rest - fetched while the game runs; the player starts no other scene before it
# is there (/.pyros-files-coming). A file whose bytes are already packed points at that copy.
#
#   tools/pack_web_game.py <game folder> <site folder> [<built web player folder>]
import hashlib, json, os, re, shutil, sys

TREES = ("shaders", "lua", "scenes", "assets", "surfaces")
# Always in the first part: the trees a scene's scripts and materials are made of, and any file this small
SMALL = 256 * 1024
FIRST_TREES = ("lua/", "surfaces/")
# Worth a second part only when it is this much of the game
SPLIT_FROM = 0.2
# Paths named in a file's bytes (scene JSON, models, materials, scripts)
NAMED = re.compile(rb'[A-Za-z0-9_./\\ -]+\.[A-Za-z0-9]{1,8}')
# Files not looked inside: pictures, sounds, fonts
OPAQUE = (".png", ".jpg", ".jpeg", ".tga", ".dds", ".ktx", ".ktx2", ".hdr", ".exr", ".wav", ".ogg", ".mp3", ".ttf", ".otf", ".raw", ".r16")


def first_scene_files(game, files, startup):
    """What the startup scene reads: the files its own file names, and those they name, and so on - never into another
    scene's file (a script names every scene; their files are the second part's)."""
    known = {os.path.normpath(f): f for f in files}
    # (a name may be only the end of a path - a script's "heli_body.p3dm", put after its "models/" when it runs: the
    # files whose paths end so, found by their last part)
    by_base = {}
    for f in known:
        by_base.setdefault(os.path.basename(f), []).append(f)
    scenes = os.path.normpath(os.path.join(game, "scenes"))
    start = os.path.normpath(os.path.join(game, startup))
    # (and what every script names: a scene's scripts load models by their paths - Erangel's title its player and its
    # helicopter - and which scripts a scene runs is the scripts' own business)
    seen, todo = set(), [start] + [os.path.normpath(f) for f in files if f.endswith(".lua")]
    while todo:
        f = todo.pop()
        if f in seen or f not in known:
            continue
        seen.add(f)
        if f.lower().endswith(OPAQUE):
            continue
        data = open(f, "rb").read()
        for m in set(NAMED.findall(data)):
            name = m.decode("latin1").replace("\\", "/").strip().lstrip("./")
            if not name:
                continue
            ends = [k for k in by_base.get(os.path.basename(name), []) if k.replace(os.sep, "/").endswith("/" + name)]
            for c in [os.path.normpath(os.path.join(game, name)), os.path.normpath(os.path.join(os.path.dirname(f), name))] + ends:
                c = os.path.normpath(c)
                if c in known and c not in seen:
                    other_scene = c.startswith(scenes + os.sep) and c.endswith(".json") and not c.startswith(os.path.splitext(start)[0])
                    if not other_scene:
                        todo.append(c)
    return seen


def main():
    game, site = sys.argv[1], sys.argv[2]
    player = sys.argv[3] if len(sys.argv) > 3 else None
    os.makedirs(site, exist_ok=True)
    files = []
    for tree in TREES:
        root = os.path.join(game, tree)
        for d, _, names in os.walk(root, followlinks=True):
            files += [os.path.join(d, n) for n in names if n != ".DS_Store"]
    files.append(os.path.join(game, "game.json"))
    files.sort()
    manifest = json.load(open(os.path.join(game, "game.json")))

    # Which part each file is in
    part = {f: 0 for f in files}
    startup = manifest.get("startupScene", "")
    if startup and os.path.exists(os.path.join(game, startup)):
        first = first_scene_files(game, files, startup)
        for f in files:
            rel = os.path.relpath(f, game).replace(os.sep, "/")
            # (fonts too: the engine opens its own by name, which no file of the game says)
            font = rel.lower().endswith((".ttf", ".otf"))
            if os.path.normpath(f) not in first and not rel.startswith(FIRST_TREES) and rel != "game.json" and not font and os.path.getsize(f) > SMALL:
                part[f] = 1
        later = sum(os.path.getsize(f) for f in files if part[f] == 1)
        if later < SPLIT_FROM * sum(os.path.getsize(f) for f in files):
            part = {f: 0 for f in files}

    listing, written, offsets = [], {}, [0, 0]
    names = ("game.data", "game.more.data")
    outs = [open(os.path.join(site, names[0]), "wb"), None]
    if any(p == 1 for p in part.values()):
        outs[1] = open(os.path.join(site, names[1]), "wb")
    elif os.path.exists(os.path.join(site, names[1])):
        os.remove(os.path.join(site, names[1]))
    # (the first part's files first: a later file the same as one of them points into the first part)
    for p in (0, 1):
        for f in files:
            if part[f] != p:
                continue
            b = open(f, "rb").read()
            key = (len(b), hashlib.sha256(b).digest())
            there = written.get(key) if b else None
            if there is None:
                there = (p, offsets[p])
                outs[p].write(b)
                written[key] = there
                offsets[p] += len(b)
            entry = {"path": os.path.relpath(f, game).replace(os.sep, "/"), "offset": there[1], "size": len(b)}
            if there[0]:
                entry["part"] = 1
            listing.append(entry)
    for o in outs:
        if o:
            o.close()

    index = {"title": manifest.get("title", "Pyros3D"), "width": manifest.get("width", 0), "height": manifest.get("height", 0),
             "data": names[0], "size": offsets[0], "files": listing}
    if outs[1]:
        index["more"] = names[1]
        index["moreSize"] = offsets[1]
    json.dump(index, open(os.path.join(site, "game.files.json"), "w"))
    if player:
        for f in ("index.html", "player-shell.js", "manifest.webmanifest", "icon.png", "PyrosPlayer.js", "PyrosPlayer.wasm"):
            if os.path.exists(os.path.join(player, f)): shutil.copy(os.path.join(player, f), os.path.join(site, f))
    print("%s: %d files, %.1f MB first%s" % (site, len(files), offsets[0] / 1048576.0,
          (", %.1f MB after it has started" % (offsets[1] / 1048576.0)) if outs[1] else ""))


if __name__ == "__main__":
    main()
