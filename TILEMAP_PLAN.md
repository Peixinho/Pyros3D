# Tilemaps: tileset assets, chunked map component, merged colliders

**Status: all nine phases done, 2026-09-15 - engine, Lua, undo/redo, agent commands, MCP,
the editor's paint mode and the on-ramp that reaches it.** Two gaps are called out below: a
stroke driven by a real mouse drag has not been exercised, only its logic (Phase 7), and
marking tiles solid still means hand-editing the `.p3dt` (Phase 8). Four
pre-existing engine bugs were found and fixed on the way - see Phase 4 - and one is left
open there as a deliberate decision rather than a side effect.

Design doc for the **engine half** of a tiling editor. Written 2026-09-14 and implemented
the same day. The editor half (paint mode, brushes, tile picker) is a separate document and
depends on Phases 1-4 of this one, which have landed.

Read "Things that will bite" before touching any of it. Six of the eight entries there were
found by reading the code for this plan and two by running it, and every one produces a
plausible-looking wrong result rather than an error.

Build/verify with: `cmake --build build_editor --target PyrosBuilder` (OpenGL + Lua,
Debug).

---

## What exists today

A 2D level is authored as one GameObject per piece of geometry. `projects/Platformer/scenes/Level1.json`
is the honest picture: 24 objects named `Ground0..4`, `Ledge0..7`, `Pillar0..4`, each a
stretched quad carrying a `RenderingComponent` + `Physics2D`, plus an `Occluder2D` on the
pillars. Ember hit the ceiling of that approach already - see
`memory/authoring-a-2d-game-in-the-editor.md`: *"a level with forty platforms must set
castsShadow false on all of them"*.

What the tilemap can build on:

- **`SpriteRig2D.cpp`** - the working precedent for generating a `Renderable` at runtime.
  Subclass `PrimitiveGeometry`, fill `tVertex`/`tNormal`/`tTexcoord`/`index`, then
  `CreateBuffers(false)` + `SendBuffers()`. Its comment about that exact sequence
  ("CreateBuffers() alone builds the attribute list but never uploads it, and a mesh whose
  buffers were never sent crashes the renderer the first time it is bound") is the whole
  contract.
- **`IGeometry::buffersRevision`** (`Renderables.h`) - exists precisely so a geometry can be
  disposed and rebuilt in place without the renderer's per-shader VAO cache drawing through
  freed buffers. `Text::UpdateText()` (`Text.cpp:203`) is the working example of the rebuild
  sequence: `Dispose()`, clear the four vectors, refill, `CreateBuffers` + `SendBuffers`.
  A tile edit is exactly this operation on one chunk.
- **`Layer2D`** - already carries draw order and parallax for its subtree, and latches its
  authored position so repeated `ApplyParallax()` does not drift. This is *already* the
  tilemap-layer concept; the map should not reinvent it.
- **Per-geometry frustum culling** - `IRenderer::ShadowCasterVisible` / the cull test at
  `IRenderer.cpp:593` dispatches on `rmesh->CullingGeometry` per `RenderingMesh`, i.e. per
  geometry, not per component. This is what makes chunking pay: one GameObject can hold
  fifty chunk geometries and only the on-screen ones are submitted.
- **`Texture::LoadShared`** (`Texture.h:180`) - one decode per atlas instead of per user.
- **`Physics2DWorld`** (`Physics2DWorld.cpp:286`) - creates one `b2CreateBody` per `Physics2D`
  with one shape on it, stamps `bd.userData = p` so contact events find their way back, and
  reconciles bodies each frame against a fresh `CollectGameObjectsRecursive` scan.

## Correction to the sequencing I gave first

I said UV-rect on the sprite material was step one. It is not needed for the tilemap at all:
a chunk mesh bakes each tile's atlas rect straight into the vertex texcoords, which is both
simpler and strictly faster than a per-draw uniform. (`grep -rn 'uvScale|uvOffset|uvRect'`
over `src` and `include` still returns nothing, so a material UV-rect remains worth having
for *single* animated sprites - `OpSliceSpritesheet` currently writes one PNG per frame into
`<stem>_frames/`. That is a separate, independent cleanup, not a dependency of this plan.)

---

## Design

**One `TileMap2D` = one grid of one tileset.** Multiple map layers come from putting several
`TileMap2D` objects under `Layer2D` parents, which gets draw order and parallax for free and
keeps the component from growing its own layer stack. A background, a main layer and a
foreground are three objects, as they already are today.

**Storage is sparse chunks.** `std::unordered_map<int64, Chunk>` keyed by packed (cx, cy);
`Chunk` holds `std::vector<uint16> tiles` of `CHUNK*CHUNK`, where 0 means empty and any other
value is `tileIndex + 1`. Sparse so a level that grows leftwards or downwards costs nothing
and needs no origin rebasing. `CHUNK = 32` - chosen for cull granularity (a 32-tile chunk is
about half a screen at typical tile sizes), not for index width: `__INDEX_C_TYPE__` is
`uint32` (`Global.h:13`), so there is no 16-bit vertex ceiling to respect.

**One geometry per non-empty chunk, one material for the whole map.** The material is a
`GenericShaderMaterial` with `Color | Diffuse | Texture` (plus `Lighting2D` when lit), the
atlas as its colour map - the same recipe `BuildSpriteRig2D` uses, for the same reasons.

**Collision is merged, not per tile.** Tiles flagged `solid` in the tileset are greedy-merged
into a small set of rectangles (row runs first, then merged vertically where runs align) and
attached as multiple polygon shapes on a *single* static body. Platformer's hand-placed
level would come out as roughly the same dozen boxes it has today, which is the point: the
authoring gets cheaper without the solver getting more expensive.

**Tileset asset: `.p3dt`**, a JSON sidecar beside the atlas PNG.

```json
{ "version": 1, "image": "assets/textures/tiles.png",
  "tileW": 16, "tileH": 16, "margin": 0, "spacing": 0, "columns": 16,
  "tiles": [ { "i": 3, "solid": true }, { "i": 7, "solid": true, "tags": ["ice"] } ] }
```

Only non-default tiles are listed, so a decorative tileset is three lines. Keeping it beside
the PNG rather than inside the scene means two maps can share a tileset and a re-slice does
not touch any scene file.

---

## Phases

### Phase 1 - Tileset asset — **DONE** (2026-09-14)

`include/Pyros3D/Assets/TileSet2D/TileSet2D.h` + `src/Pyros3D/Assets/TileSet2D/TileSet2D.cpp`,
added to `cmake/PyrosSources.cmake`. Loads/saves `.p3dt`, exposes `UVRect(index) -> Vec4`
(left, top, right, bottom) and `IsSolid(index)`.

`UVRect` applies a **half-texel inset** on all four edges. Without it, neighbouring tiles
bleed into each other along every seam under any filtering - the classic tilemap artifact,
and the one that looks like a rendering bug rather than a UV bug.

Two things came out different from the sketch above, both deliberate:

- **The tileset holds no `Texture` and never touches GL.** The sketch had it resolving the
  atlas through `Texture::LoadShared`, which is wrong twice over. `LoadShared`'s own contract
  says callers that mutate per-instance texture state - filter, wrap - must not use it,
  because they would be editing every other user's texture; an atlas needs exactly that
  (`Nearest`, `ClampToEdge`). And keeping the asset GL-free is what makes its arithmetic
  testable with no window. Loading the atlas is Phase 2's job. See the open question below.
- **The image's pixel size is not in the file.** The PNG is the truth about its own
  dimensions, and a `.p3dt` that disagreed would cut the wrong cells while looking perfectly
  well-formed. `TileSet2DReadImageSize()` reads it via `stbi_info` without decoding, and
  `SetImageSize()` feeds it in. Until it is known, `TileCount()` is 0 and `UVRect` returns a
  zero rect rather than guessing.

An index outside `[0, TileCount())` returns a zero rect rather than clamping into a real
cell - drawing a degenerate quad is noticeable, silently drawing the wrong tile is not.

*Verified:* `tools/tests/tileset_uv.cpp` (standalone, per `prefab_roundtrip.cpp`'s
convention - the build line is in its header comment). 42 checks, all passing: cell-fit with
margin *and* spacing, the sheet-1px-short case that must drop its last column, the half-texel
inset, v-runs-down, the column override re-wrapping rows, out-of-range and unresolved-image
refusals, the format round-trip, and the four malformed-input rejections.
`TileSet2DReadImageSize` checked by hand against a real PNG (1280x720, agrees with `sips`).

**Open for Phase 2: where the atlas texture comes from.** `Texture::LoadShared`'s cache key
is `filename|type|mipmapping` (`Texture.cpp:479`) - filter and wrap are *not* in it. Loading
an atlas with `Mipmapping=false` therefore keeps it clear of a sprite's mipmapped load of the
same PNG, but two no-mip consumers of one PNG would share a texture and fight over its
filter. Recommendation: give `TileMap2D` a private `Texture` per tileset, cached by `.p3dt`
path at the map level. That is one decode per tileset - the actual goal - without mutating
anything shared.

### Phase 2 - The component and its mesh — **DONE** (2026-09-14), with one gap

`include/Pyros3D/Rendering/Components/TileMap2D/TileMap2D.{h}` +
`src/Pyros3D/Rendering/Components/TileMap2D/TileMap2D.cpp`, `TileMap2D` added to
`ComponentType` (`IComponent.h`), source added to `cmake/PyrosSources.cmake`.

API: `SetTile`/`GetTile`/`Fill`/`ClearTiles`, `TileToWorld`/`WorldToTile`, `SetTileSize`,
`SetTileSet`/`SetTileSetPath`, `SetLit`, `GetTileBounds`/`PaintedCount`/`NonEmptyChunks`,
`BuildChunkMesh`, `Rebuild`, and `NeedsRebuild`/`NeedsFullRebuild`/`DirtyChunkCount`.

**Tile coordinates are y-UP**, like the world the map sits in - not the y-down convention a
tileset indexes its own rows with. That one stays inside the atlas. Getting it backwards
mirrors a level vertically and nothing errors.

Three things worth knowing:

- **`RenderingComponent::AdoptGeneratedRenderable()` is new, and shared.** The sketch had
  `BuildTileMap2D` mirroring `BuildSpriteRig2D`, which would have meant a second copy of the
  unregister / delete-meshes / re-adopt / re-register dance inside `SetSpriteRig2D`. That
  sequence is subtle enough (the scene's render list holds raw `RenderingMesh*` and nothing
  else removes them) that two copies would drift, so it was extracted instead and
  `SetSpriteRig2D` now calls it.
- **Chunk bounds are the chunk's FULL rect, not the painted part.** Bounds that tracked the
  painted cells would change on every edit, which turns an in-place buffer refill into a
  bounds recompute and a re-adopt. A chunk's extent is fixed by the grid, so this is stable
  and an edit never moves it.
- **A chunk with no cells is never built.** `CreateBuffers()` indexes `tVertex[0]` and
  `SendBuffers()` indexes `index[0]`, so an empty mesh is not an empty draw, it is a crash.

Edits split by cost: a cell painted inside an existing chunk refills that chunk's buffers
via the `Text::UpdateText()` sequence; a cell that brings a chunk into existence or empties
the last one out of it changes the geometry list and forces a full re-adopt. `Update()`
applies whichever is pending, once, at the end of the frame.

*Verified:* `tools/tests/tilemap_mesh.cpp`, 75 checks, all passing - working at the
`BuildChunkMesh()` seam, which is pure. Covers the floor-division traps (tile -1 belongs to
chunk -1, and a point at -0.5 is tile -1 rather than sharing tile 0), chunk boundaries at 31
and 32, quad winding and the v-runs-down-while-y-runs-up UV mapping, bounds stability across
an edit, cell-size scaling, and the cheap-vs-full edit classification. The `SetSpriteRig2D`
extraction was checked against a real 2D character (`projects/Showcase2D`, Hero.p3d2d) in the
editor - head, torso and both legs still drawn by their own bones.

**Gap: nothing has drawn a tilemap yet.** `Rebuild()` uploads buffers and needs a render
device, and there is no way to get a map into a running scene until Phase 4 serializes one.
The draw-call measurement this phase was supposed to end with - `PYROS_PROFILE` a 200x60 map,
confirm one draw per visible chunk and the off-screen ones absent from `GroupAndSortAssets` -
is deferred to Phase 4, where it becomes cheap. Treat the mesh as structurally right and
visually unproven until then.

### Phase 3 - Merged colliders — **DONE** (2026-09-14)

`Physics2D` grew a compound shape list - `SetCompoundBoxes(const std::vector<Vec4>&)`, each
entry `(centreX, centreY, halfW, halfH)` in body-local space - rather than gaining a second
body type. When it is non-empty it replaces the single shape; `Physics2DWorld.cpp`'s creation
loop emits one `b2CreatePolygonShape` per entry via `b2MakeOffsetBox` instead of the single
`b2MakeBox`. `bd.userData`, contact dispatch and the tracked/destroy reconciliation are
untouched, so a tile collision reports the map component as the thing that was hit.

`TileMap2D::BuildColliderBoxes()` greedy-merges solid cells: grow right while the row holds,
then down while every column of that run holds. `SyncColliders()` pushes the result onto the
sibling `Physics2D` and `Update()` calls it at the frame boundary, after the mesh.

Three things the implementation added on top of the sketch:

- **`SetCompoundBoxes` compares before it assigns.** The merge is recomputed on any grid edit,
  and most edits do not change the collider set at all. Rebuilding an unchanged body would
  destroy and recreate every contact on it: anything standing on the floor would be re-seated
  and a body mid-contact would lose the events it was about to receive.
- **A changed shape list destroys and rebuilds the body**, at the frame boundary in the same
  reconciliation pass that creates new bodies - never mid-step. Box2D has no "replace every
  shape" call, and removing them one at a time costs the same as a fresh body.
- **`SyncColliders` forces `castsShadow` false** rather than leaving it to the author. Each
  merged rectangle is four occluder segments against a scene-wide budget of 32
  (`PYROS_MAX_OCCLUDERS_2D`), so a map that cast would spend the entire budget on itself and
  every prop in the scene would silently stop casting. A map that wants a shadow gets an
  explicit `Occluder2D`.

The compound list is derived from the tileset's `solid` flags at load, so nothing about it is
serialized - the scene stores the tileset reference and the grid, and the colliders follow.

*Verified:* `tools/tests/tilemap_colliders.cpp`, 38 checks, all passing. The merge is checked
by rasterising the output rectangles back onto a grid and comparing with the input, which
catches an off-by-one that reading a rectangle list would not - it asserts the cover is
*exact* (a short cover is a floor you fall through, a long one a wall you walk into) and
non-overlapping (overlap would double up contacts). A platformer-shaped level merges to **5
boxes**; an 8x8 checkerboard, the worst case, stays at 32 and does not pretend otherwise.

Confirmed against the solver in the editor with a free-fall probe (`projects/TileTest`,
`scenes/Phys.json`): a 1x1 dynamic body dropped over a ledge whose top is y=7.0 settles at
**y=7.500**, and one dropped clear of it onto a floor whose top is y=3.0 settles at
**y=3.500**. Both exact, which is what says the solver sees the merged rectangles where the
tiles actually are.

### Phase 4 - Serialization — **DONE** (2026-09-14)

`SceneSerializer.cpp`: a `TileMap2D` case on the write side, a `"TileMap2D"` branch on the
read side, and `EncodeTileChunk`/`DecodeTileChunk` beside `ResolveSceneAssetPath`.

Settings and grid are separate objects, as planned:

```json
{ "type": "TileMap2D",
  "settings": { "tileset": "assets/tiles/basic.p3dt", "tileSize": [1,1], "lit": false },
  "chunks": [ { "cx": 0, "cy": 0, "rle": [[-1,96],[12,4],[-1,28]] } ] }
```

The map's `RenderingComponent` carries a `"tileMap2D": true` marker and is given a
placeholder `Plane` on load, exactly as a 2D character is - generated geometry cannot be
described by `SerializeRenderable` or rebuilt by `DeserializeRenderable`. `DecodeTileChunk`
tolerates short or over-long run lists rather than trusting the file.

RLE earns its place: a hand-authored 32x32 chunk came to 25 runs for 1024 cells, and a
deliberately pathological 200x60 map with 15% random scatter - the worst case for run-length
- still only reached 143 KB.

**This phase closed Phase 2's gap: tilemaps now render.** Verified in the editor against
`projects/TileTest` (generated 4x4 atlas, one distinctly coloured cell per index with a black
notch in each cell's top-left):

- all sixteen atlas cells draw, in index order, with no colour bleeding along any seam -
  the half-texel inset holds;
- the notch lands top-left in every cell, so nothing is flipped, and `v` runs down the atlas
  while `y` runs up the world as intended;
- a 200x60 map, 2336 painted cells across 14 chunks in one object, renders whole. At 60 fps /
  16.7 ms it is pinned to the display's refresh cap with scene update at ~0.01 ms, so this
  establishes "far under budget" rather than a precise per-chunk cost.

Getting there cost three engine bugs, all of them pre-existing and none of them visible
without geometry that lives away from its object's origin:

1. **`ResolveSceneAssetPath` only works during a load.** `DeserializeScene` clears
   `g_sceneAssetRoot` when it finishes, so a resolver captured for the deferred rebuild
   resolved nothing a frame later - the atlas fell back to the default white texture and
   every cell drew blank. `TileMap2D::SetResolvedAtlasPath()` now takes an answer resolved
   while the root is still set.
2. **`GameObject` bounds were never recomputed after construction.** `Add()` aggregates a
   component's bounds once; nothing updates them when a component replaces its geometry, so
   a tilemap kept the 1x1 box of the placeholder `Plane` and was culled as soon as the origin
   left the view. Added `GameObject::RefreshComponentBounds()`, called from
   `AdoptGeneratedRenderable`.
3. **`CullingSphereTest` centres its sphere on the owner's world POSITION**, not on the
   bounding sphere's own centre (`IRenderer.cpp:1706`) - and `CullingGeometry::Sphere` is 0,
   which is what `RenderingMesh` constructs with. Geometry that does not straddle its origin
   is therefore tested against a sphere in the wrong place: a map painted out to x=160
   vanished from about x=116 onwards, and **the cutoff moved as the map grew**, because the
   radius grew with it. The map now asks for `CullingGeometry::Box`, which uses the real
   world-space box, and `AdoptGeneratedRenderable` carries the component's choice onto the
   meshes it creates (they construct as Sphere, and the caller cannot set it beforehand
   because they do not exist yet).

**Found and deliberately NOT fixed here: `IComponent`'s constructor leaves
`minBounds`/`maxBounds`/`BoundingSphereCenter` uninitialised**, and `GameObject::Add()`
merges every component's bounds into the owner's box - so any object carrying a component
with no geometry of its own (`Layer2D`, `Physics2D`, `Occluder2D`, every UI component) merges
stack garbage into the box it gets culled by. That is every 2D scene in the repo.

Zeroing them in the constructor looks like the obvious fix and **breaks the UI**: the driver's
canvas smoke test goes blank. The reasoning that it was "conservative - it can only cull less"
is wrong, because `GameObject::Add()` takes the FIRST component's bounds *unconditionally*
rather than merging into an empty box. An object whose only components carry no geometry - a
UICanvas - therefore collapses to a point at the origin and is culled, where the garbage had
been large enough to pass by accident.

The real fix is for components with no geometry not to contribute to the owner's bounds at
all, or for screen-space UI not to be frustum-culled against a world camera. Both are bigger
than this work and want their own measurement, so the UB is left in place and documented
rather than half-fixed.

**Still open, and not mine to decide:** bug 3's root cause is still in `CullingSphereTest`.
Using the bounding sphere's actual centre would fix it for every off-centre mesh in the
engine, but it changes culling for every object in every scene, so it is left as a
deliberate choice rather than folded into this work.

### Phase 5 - Lua — **DONE** (2026-09-14)

`TileMap2D` registered in `PyrosLuaPhysics.cpp` and added to
`GameObject_GetComponent`'s type list in `PyrosLuaHelpers.cpp` - that list is hand-written and
a component missing from it is simply unreachable from script (see
`memory/lua-getcomponent-type-list-is-short.md`).

```lua
local map = ground:getComponent("TileMap2D")
local tx, ty = map:worldToTile(player:getPosition())
if map:isSolidAt(tx, ty - 1) then ... end
map:setTile(tx, ty, -1)          -- dig
```

`getTile`, `setTile`, `fill`, `clearTiles`, `tileToWorld`, `worldToTile` (two returns, not
out-params), `getTileSize`/`setTileSize`, `paintedCount`, `getTileBounds` (a table, or nil
when nothing is painted - four zeroes would read as a real one-cell map at the origin),
`isSolidAt` and `getTileSetPath`.

`isSolidAt` takes tile coordinates and answers the question a script actually has, rather
than making every caller write `local t = map:getTile(x,y); t >= 0 and solid(t)`.

### Phase 6 - Editor undo/redo, agent commands, MCP — **DONE** (2026-09-14)

Not in the original plan; added because a feature the editor cannot drive is not finished.

**Undo/redo.** `SetTilesCommand` (`SceneCommands.{h,cpp}`) holds a delta of
`(x, y, before, after)` per changed cell. Deliberately not the subtree snapshot every other
component edit uses: a map's serialized form carries its whole grid, so a snapshot pair would
put two copies of the level into one undo entry and `UndoStack`'s 64 MB cap would evict
unrelated history every few strokes. **One command per operation, never one per cell** - a
24-cell fill is one entry, because `UndoStack` has no coalescing and a per-cell push would
fill the 200-deep stack with a single stroke. Cells that would not change are dropped from the
delta before it is stored, and an operation that changes nothing pushes no entry at all. The
map is re-resolved by object id on each Undo/Redo rather than held as a pointer, because an
earlier undo may have rebuilt that object.

**Agent commands** (`Editor.cpp`): `add_tilemap`, `set_tiles`, `fill_tiles`, `get_tiles`,
`tilemap_info`. `add_tilemap` loads the tileset eagerly and fails on a bad path rather than
leaving a map that draws nothing and says nothing, and adds a `RenderingComponent` if the
object has none. `fill_tiles` and `get_tiles` refuse absurd rects (1M / 64K cells) - a fill is
expanded cell by cell, and a typo'd rect would otherwise try to allocate for billions before
anything noticed. `tilemap_info` reports `colliderBoxes`, the number that says whether a level
is cheap or pathological for the solver; nothing else reports it.

**MCP tools** (`pyros3d-mcp-server.py`): `add_tilemap`, `fill_tiles`, `set_tiles`,
`get_tiles`, `tilemap_info`, each on the existing live-editor-or-scene-file dual path. The
file side reimplements the chunk RLE in Python, so it is a second implementation of the
format - checked against the first rather than assumed: encoding the scene the engine wrote
reproduces the engine's runs **byte for byte**, including on negative coordinates, and an
emptied map writes no chunks rather than 1024 empties.

*Verified* end to end through the agent socket: build a level with `add_object` +
`add_tilemap` + three `fill_tiles` (78 cells, 1 chunk, 2 collider boxes); undo twice
(78 -> 72 -> 48 painted, collider count following) and redo twice back to 78, each fill one
entry; `get_tiles` reads back the ledge exactly where it was painted; save, reload, and the
map comes back identical. Then the reverse direction - a scene authored entirely by the MCP
tools with no editor running, opened in the engine, which reports the same 27 painted cells,
2 chunks and bounds the Python side did, and 3 collider boxes for a floor split by an erased
gap plus a grass row.

### Deferred, deliberately

Animated tiles, per-tile tint, tile tags driving material variants, and **2D shadow
occluders from the map**. The last one is not a small addition: `PYROS_MAX_OCCLUDERS_2D` is
32 (`IRenderer.cpp:57`) for the whole scene, and `SetOccluders2D` truncates past it
(`IRenderer.cpp:1457`). Even perfectly merged, a level's silhouette exceeds that. Doing it
properly means making occluder selection view- or light-relative, which is its own change
with its own measurements. Until then `TileMap2D` contributes no occluders at all, and a
map that wants a shadow gets an explicit `Occluder2D` beside it.

---

### Phase 7 - The editor's paint mode — **DONE** (2026-09-14)

A THIRD viewport mode, beside the gizmo and Canvas (2D) Mode - not a reuse of the latter,
which edits a `UICanvas` in screen space. The two are mutually exclusive: both claim the left
button over the viewport, and a click that could mean either does the wrong one half the time.

- **Tile Palette window** (`SceneEditor::ShowTilePalette`): the map to paint into (skipped
  when the scene has only one), Brush / Rect, an Erase toggle, and the atlas cut into
  clickable cells at the tileset's own grid with the solid ones marked in their tooltip. It
  also shows painted count, chunk count and collider-box count, which are the numbers worth
  watching while building a level. Its own window rather than a Properties tab, because
  during a paint session Properties is showing nothing in particular.
- **Overlay** (`DrawTilePaintOverlay`, from `Draw2DReference`): the cell grid aligned to the
  MAP rather than to round world numbers - a tile editor's grid has to be the thing being
  painted - plus the cell under the cursor, red when the eraser is up, and the whole rectangle
  mid-drag. The grid is dropped past ~160 cells across the view, where it is a grey wash that
  hides the artwork.
- **One undo entry per stroke.** Cells accumulate from press to release and flush once through
  `EndTileStroke`.

Three things this cost:

- **`GetView2DExtent()` was factored out of `Draw2DReference`.** The grid, the cell cursor and
  the brush all have to agree about where the cursor is; three copies of that arithmetic would
  drift and the brush would paint one cell away from the highlight.
- **`viewportHovered` is useless after `UpdateViewportMouse()`** - that function CLEARS it at
  its start and never sets it again, so the value `ShowViewport` put there is already gone.
  Gating the brush on it meant the cursor was never over the viewport. `viewportMouseValid` is
  the flag that means what `viewportHovered` reads like.
- **The overlay had to move off z=0.** It and the tiles are both flat quads, so at the same z
  they z-fight and the cursor outline comes out as three washed-out edges and one bright one -
  which reads as a drawing bug rather than a depth one. Drawn at the map's z + 0.05, towards
  the camera.

**Agent commands:** `tile_paint_mode` (toggle, pick the map, set the brush and tool) and
`tile_stroke` (a brush or rect stroke in cell coordinates).

*Verified:* the cell grid and the cursor highlight screenshotted, grid-aligned, on the right
cell. `tile_stroke` run through the same accumulate-and-flush path the mouse uses - a 5-cell
brush stroke and a 4x4 rect, 78 -> 83 -> 99 painted, undone one stroke at a time back to 78
and redone to 99, so a 16-cell rect is one entry.

**Not verified: a stroke driven by an actual mouse drag.** Injected clicks never reach the
viewport in this environment - `MouseLeftPress` returns early because `viewportMouseValid` is
false, since SDL feeds the real cursor's position into ImGui every frame and the real cursor
is over another window. `tile_stroke` exists partly so the stroke logic is testable without
it, but the press/release wiring itself has only been read, not exercised by hand.

### Phase 8 - The on-ramp — **DONE** (2026-09-14)

Phase 7 shipped a paint mode with no way to reach it. Nothing in the UI created a tilemap,
and nothing anywhere created a `.p3dt` - so the only route in was hand-writing JSON and
calling agent commands, which is not a feature, it is a demo.

- **Assets > right-click an image > "Create Tile Set…"** writes `<image>.p3dt` beside it.
  The dialog reads the image's real pixel size and shows the grid the current numbers
  produce - "Grid: 4 x 4 = 16 tiles" - before committing, because "16" is a guess about
  someone else's art until it says that. It refuses a size that fits no whole tile.
- **GameObject > Tile Map 2D…** on the menu bar, plus **Add Component > Tile Map 2D…** on an
  object and a button in the Tile Palette: pick one of the project's tilesets from a list,
  set the cell size in world units, Add. Unlike the other entries in that menu it needs a
  form, because a map is nothing without a tileset and there is no sensible default for
  which one. It drops straight into paint mode with the new map selected - adding one and
  then hunting through the View menu to use it is a step with no decision in it.
- **View > Tile Paint Mode** toggles the viewport mode. Disabled outside a 2D scene, which
  is why it can look like it is missing.

Discoverability was got wrong twice. First the only entry point was a right-click >
Add Component submenu on an object that, in a fresh 2D scene, does not exist yet. Then
"Create Tile Set…" in Assets turned an image into a `.p3dt` and left you holding an asset
with nowhere to go - the step from tileset to a map in the scene existed nowhere in the UI,
which is the half of the job that actually matters. A feature
reachable only from a submenu of a thing you have not made is a feature nobody finds. It is
now on the menu bar, in that submenu, and as a button in the panel that reports having no
map - and every editor build in the repo carries it, not just the one that happened to get
rebuilt.

## How to use it

1. Put a tile sheet in the project (`assets/…/whatever.png`).
2. In **Assets**, right-click it > **Create Tile Set…**, set the cell size, check the grid
   line reads what you expect, Create. This makes a `.p3dt` **tile set** - the cut-up sheet.
   It is not yet a tile map; a map lives in a scene.
3. Right-click the `.p3dt` > **Edit Tile Set**. Click cells (shift-click for a range,
   ctrl-click to add), then Space or "Make Solid". Only solid cells become colliders. Save.
4. In a **2D scene**, put a map in it by any of:
   - **Assets**, right-click the `.p3dt` > **Create Tile Map in Scene** (shortest route -
     the tileset is already chosen);
   - menu bar > **GameObject > Tile Map 2D…**;
   - the **Create a Tile Map...** button in the Tile Palette when a scene has none;
   - right-click an existing object > **Add Component > Tile Map 2D…**.
   Any of them makes the object and drops straight into paint mode.

5. Pick a tile in the **Tile Palette** window, choose Brush or Rect, and paint. Erase with
   the Erase checkbox. Ctrl+Z undoes a whole stroke.
6. Add a **Physics 2D** component to the same object (body type Static) and the solid cells
   become merged colliders automatically.

### Phase 9 - The tile set editor — **DONE** (2026-09-15)

`editor/src/editor/TileSetDocument.{h,cpp}` + `editor/src/editor/UI/TileSetEditor.{h,cpp}`,
following `Character2DDocument` + `UI/Character2DEditor.cpp` - a dockable document with its
own undo stack and unsaved-changes marker, opened from Assets > right-click a `.p3dt` >
**Edit Tile Set** (or the `open_tileset` agent command).

This should have been built with the rest of it. A tile MAP is scene content, so painting it
in the scene is right; a tile SET is an asset, and every other asset type in this editor -
`.p3da`, `.p3d2d`, materials, scripts - opens as a document. Shipping the map painter and
leaving the set to be hand-edited as JSON was an inconsistency with the codebase's own
established pattern, and it was the half a person actually has to touch to make a level
collide.

The sheet is drawn as a clickable grid at the set's own cut. Solid is a filled wash over the
cell rather than a corner marker, because it has to be readable at a glance across a
256-tile sheet. Shift-click takes a range, ctrl-click adds, Space toggles the selection.
Re-cutting the sheet (tile size / margin / spacing / columns) clears the selection: every
index then means a different cell, and silently repointing a selection at different artwork
is worse than losing it.

Two bugs it cost:

- **ImGui aborts if `SetCursorPos` is used to extend a child's boundaries**
  (`ErrorCheckUsingSetCursorPosToExtendParentBoundaries`), which is how the scroll region
  was being sized - every cell is placed with `SetCursorScreenPos` and so contributes
  nothing to the content size. The assert kills the process rather than drawing wrong.
  Reserve with a `Dummy` instead.
- **The agent `undo`/`redo` commands called `sceneView->Undo()` unconditionally**, so they
  could not reach ANY document editor's stack - material, animation, character or tile set -
  and silently undid a scene edit instead. Now routed by focused document, the same way
  Ctrl+Z already was.

Agent commands: `open_tileset`, `set_tile_solid`, `save_tileset`.

## Where the editor half attaches

Checked against the editor before committing to the above. The engine design holds, with the
one amendment already folded into Phase 4.

- **"Canvas (2D) Mode" is not the mode this wants.** It is `uiEditMode`
  (`SceneEditor.cpp:4809`), which edits a `UICanvas` in *screen* space - anchors, pivots, y
  running down. A tile brush is world-space 2D, the same space `Layer2D` and `Physics2D`
  live in. Paint mode is a third mode beside those two, not a reuse of either.
- **Screen-to-world already works under the ortho view.** `Mouse3D::GenerateRay` with
  `projectionOrtho` is what the gizmo uses (`SceneEditor.cpp:6638`), including the two ortho
  corrections written up in the comment above it. A brush intersects that ray with the
  layer's z plane; nothing new is needed.
- **One undo command per stroke, never per tile.** `UndoStack` takes arbitrary
  `IUndoableCommand`s and reports cost through `MemoryCost()`, so a tile-delta command
  holding `(x, y, before, after)` for a whole drag is a clean fit. But there is no
  coalescing mechanism - the tool has to accumulate from mouse-down to mouse-up and push
  once. Pushing per tile means a 200-tile drag fills the 200-deep stack and silently evicts
  the rest of the session's history.
- **The grid overlay is a new drawable.** `Grid.h` is the XZ 3D grid, drawn at a fixed size
  and divisions. A tile grid is aligned to the map's origin and tile size, so it is either a
  second geometry of the same shape or a few `EditorDebugDraw::drawLine` calls - cheap
  either way, but not free.

---

## Things that will bite

1. **`PrimitiveGeometry::CalculateBounding()` is a no-op.** Concrete shapes set bounds by
   hand; anything deriving from it and not doing so gets uninitialised bounds and erratic
   culling. `SpriteRig2D`'s `QuadGeometry` has this bug today - it never sets bounds, and
   `SpriteRigRenderable::Finish()` then folds garbage into the model bounds. Worth fixing
   while the mechanism is fresh, separately from this work.
2. **Rebuild in place without bumping `buffersRevision` draws through freed buffers.** The
   index count updates (it is read CPU-side) while the vertex data does not - so an edited
   chunk renders with the *previous* tiles. `SendBuffers()` bumps it; a hand-rolled upload
   would not.
3. **Tile seams are a UV bug, not a filter bug.** Half-texel inset, `Nearest`, `ClampToEdge`,
   no mips. Reaching for a filter change first wastes the afternoon.
4. **`Physics2D.castsShadow` defaults to true** and spends four of the 32 occluder segments
   per body. `TileMap2D`'s body must set it false explicitly.
5. **The 2D ortho camera's far plane sits between 30 and 40 units** and a quad needing a
   scale factor in the hundreds is cull-tested out. Chunk meshes are built at world scale
   with the object at scale 1 - do not build a unit chunk and scale it up.
6. **Box2D bodies cannot be rebuilt mid-step.** Collider regeneration is a frame-boundary
   operation; an edit during play that rebuilds immediately will fault inside the solver.
7. **Objects a script adds at run time are not in the play-mode snapshot** and get written
   into the scene on the next save. If anything generates tiles at run time, it must undo
   that in `destroy()`.
8. **A 200x60 map is 12000 tiles.** Any code path that is per-tile rather than per-chunk -
   serialization, collider rebuild, a naive `Fill` that rebuilds once per `SetTile` - will
   be the thing that makes the editor feel slow, and it will not show up on a 20x10 test map.
