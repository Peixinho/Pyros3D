#!/usr/bin/env python3
"""The demo's scene from Erangel's Island: its terrain (heightmaps and splat maps, 4 x 4 tiles of 128 m),
its trees where they stand (the scanned ones, tools/build_ph_trees.py in the Erangel repo) and its grass,
scattered from the terrain's own grass density maps. Writes demo_scene.json for renderer_demo."""
import json, math, os, random, sys
from PIL import Image

E = os.path.expanduser('~/dev/erangel/erangel')
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'demo_scene.json')
island = json.load(open(os.path.join(E, 'scenes', 'Island.json')))

terrain = None
def walk(o, f):
    f(o)
    for c in o.get('children', []): walk(c, f)
def find_terrain(o):
    global terrain
    for c in o.get('components', []):
        if c.get('type') == 'Terrain': terrain = c
for r in island['roots']: walk(r, find_terrain)
tiles_x, tiles_z, tile = terrain['tilesX'], terrain['tilesZ'], terrain['tileSize']
hf = [c for c in terrain['tileTemplate']['root']['components'] if c.get('renderable', {}).get('kind') == 'heightfield'][0]['renderable']

trees = []
def find_trees(o):
    comps = o.get('components', [])
    for c in comps:
        r = c.get('renderable') or {}
        p = r.get('path', '') if isinstance(r, dict) else ''
        if p.startswith('assets/models/ph/') and p.endswith('_cards.p3dm'):
            name = p.split('/')[3]
            trees.append({'name': name, 'pos': o['position'], 'yaw': o.get('rotation', [0, 0, 0])[1], 'scale': o.get('scale', [1, 1, 1])[0]})
for r in island['roots']: walk(r, find_trees)

# grass: from each tile's density map (Terrain/<x>_<z>_grass_density.png), a tuft group where it says so
rnd = random.Random(7)
grass = []
for tz in range(tiles_z):
    for tx in range(tiles_x):
        dp = os.path.join(E, terrain['directory'], '%d_%d_grass_density.png' % (tx, tz))
        dens = Image.open(dp).convert('L') if os.path.exists(dp) else None
        ox, oz = (tx - 0.5 * tiles_x) * tile, (tz - 0.5 * tiles_z) * tile
        n = int(tile * tile * 3.5)             # (groups a tile at full density - three tufts a group)
        for _ in range(n):
            u, v = rnd.random(), rnd.random()
            d = dens.getpixel((min(dens.width - 1, int(u * dens.width)), min(dens.height - 1, int(v * dens.height)))) / 255.0 if dens else 1.0
            if rnd.random() > d: continue
            grass.append([round(ox + u * tile, 2), round(oz + v * tile, 2), round(rnd.uniform(0, 6.283), 2), round(rnd.uniform(1.6, 2.8), 2), 1 if rnd.random() < 0.18 else 0])

scene = {
    'erangel': E,
    'terrain': {'dir': os.path.join(E, terrain['directory']), 'tilesX': tiles_x, 'tilesZ': tiles_z, 'tileSize': tile,
                'heightScale': hf['heightScale'], 'heightOffset': hf['heightOffset'],
                'layers': [os.path.join(E, 'assets/terrain/layers_nature/%s.png' % n) for n in ('grass', 'dirt', 'rock', 'sand')]},
    'trees': trees,
    'grass': grass,
    'treeCache': os.path.join(E, 'tools/cache/ph_trees'),
    'treeAssets': os.path.join(E, 'assets/models/ph'),
    'grassCache': os.path.join(E, 'tools/cache/ph_grass'),
    'grassAssets': os.path.join(E, 'assets/models/ph_grass'),
}
json.dump(scene, open(OUT, 'w'))
print('%s: %d trees, %d grass groups, terrain %dx%d tiles of %g m' % (OUT, len(trees), len(grass), tiles_x, tiles_z, tile))
