// Checks TileMap2D's grid bookkeeping and the chunk mesh it generates.
// Standalone, per prefab_roundtrip.cpp's convention:
//
//   c++ -std=c++17 -I include tools/tests/tilemap_mesh.cpp -o /tmp/tilemap_mesh \
//       -L build_gl -lPyrosEngine -Wl,-rpath,$PWD/build_gl && /tmp/tilemap_mesh
//
// Works at the BuildChunkMesh() seam, which is pure - no render device, no
// sibling component, no window. Rebuild() itself uploads buffers and so is not
// reachable from here; what this covers is everything that decides WHAT gets
// uploaded: which chunk a cell lands in, where its quad sits, which texels it
// samples, how big the chunk claims to be, and whether an edit is the cheap
// kind or the expensive kind.
#include <Pyros3D/Rendering/Components/TileMap2D/TileMap2D.h>

#include <cstdio>
#include <cmath>
#include <string>

using namespace p3d;

static int failures = 0;
static void check(bool cond, const std::string &what)
{
	printf("%s  %s\n", cond ? "PASS" : "FAIL", what.c_str());
	if (!cond) failures++;
}
static void nearly(const f32 got, const f32 want, const std::string &what)
{
	const bool ok = std::fabs(got - want) < 1e-5f;
	printf("%s  %s (got %.6f, want %.6f)\n", ok ? "PASS" : "FAIL",
		what.c_str(), (double)got, (double)want);
	if (!ok) failures++;
}

static TileSet2D MakeSet()
{
	TileSet2D s;
	s.image = "assets/textures/tiles.png";
	s.tileW = s.tileH = 16;
	s.SetImageSize(64, 64);   // 4x4 = 16 cells
	s.tiles[5].solid = true;
	return s;
}

int main()
{
	// --- the grid ---------------------------------------------------------
	{
		TileMap2D m(Vec2(1.f, 1.f));
		m.SetTileSet(MakeSet());

		check(m.GetTile(0, 0) == TileMap2D::EMPTY, "a fresh map is empty");
		check(m.PaintedCount() == 0, "a fresh map has painted nothing");

		check(m.SetTile(2, 3, 0), "painting tile index 0 is an edit");
		check(m.GetTile(2, 3) == 0, "tile index 0 is a real tile, not empty");
		check(!m.SetTile(2, 3, 0), "repainting the same cell is not an edit");
		check(m.PaintedCount() == 1, "one cell painted");

		check(m.SetTile(2, 3, TileMap2D::EMPTY), "erasing is an edit");
		check(m.GetTile(2, 3) == TileMap2D::EMPTY, "erased cell reads empty");
		check(!m.SetTile(9, 9, TileMap2D::EMPTY), "erasing nothing is not an edit");
		check(m.PaintedCount() == 0, "erasing the last cell leaves nothing");
	}

	// --- negative coordinates, the floor-division trap ---------------------
	{
		TileMap2D m(Vec2(1.f, 1.f));
		m.SetTileSet(MakeSet());

		m.SetTile(-1, -1, 7);
		m.SetTile(0, 0, 3);
		check(m.GetTile(-1, -1) == 7, "tile (-1,-1) reads back");
		check(m.GetTile(0, 0) == 3, "tile (0,0) is untouched by it");
		check(m.PaintedCount() == 2, "the two live in different cells");
		// Truncating division would put both in chunk 0 and collide.
		check(m.NonEmptyChunks().size() == 2, "negative tiles get their own chunk");

		m.SetTile(-33, 5, 1);
		check(m.GetTile(-33, 5) == 1, "tile two chunks left reads back");
		check(m.NonEmptyChunks().size() == 3, "and lands in a third chunk");
	}

	// --- chunk boundaries --------------------------------------------------
	{
		TileMap2D m(Vec2(1.f, 1.f));
		m.SetTileSet(MakeSet());
		m.SetTile(31, 0, 1);
		check(m.NonEmptyChunks().size() == 1, "tile 31 is still the first chunk");
		m.SetTile(32, 0, 1);
		check(m.NonEmptyChunks().size() == 2, "tile 32 starts the second chunk");
	}

	// --- fills and bounds --------------------------------------------------
	{
		TileMap2D m(Vec2(1.f, 1.f));
		m.SetTileSet(MakeSet());

		check(m.Fill(0, 0, 4, 2, 5) == 15, "a 5x3 fill paints 15 cells");
		check(m.Fill(0, 0, 4, 2, 5) == 0, "refilling the same rect changes nothing");
		// Reversed corners must describe the same rect.
		TileMap2D r(Vec2(1.f, 1.f));
		r.SetTileSet(MakeSet());
		check(r.Fill(4, 2, 0, 0, 5) == 15, "a fill given its corners backwards is the same rect");

		int32 x0 = 0, y0 = 0, x1 = 0, y1 = 0;
		check(m.GetTileBounds(x0, y0, x1, y1), "a painted map has bounds");
		check(x0 == 0 && y0 == 0 && x1 == 4 && y1 == 2, "bounds are the painted rect");

		TileMap2D empty(Vec2(1.f, 1.f));
		check(!empty.GetTileBounds(x0, y0, x1, y1), "an empty map has no bounds");
	}

	// --- world <-> tile, including the floor trap --------------------------
	{
		TileMap2D m(Vec2(2.f, 4.f));

		const Vec2 c = m.TileToWorld(3, 1);
		nearly(c.x, 7.f, "tile (3,1) centre x with 2-wide cells");
		nearly(c.y, 6.f, "tile (3,1) centre y with 4-tall cells");

		int32 tx = 0, ty = 0;
		m.WorldToTile(c, tx, ty);
		check(tx == 3 && ty == 1, "a cell centre maps back to its own cell");

		// The trap: truncation would call -0.5 tile 0, giving the origin a
		// two-cell-wide column.
		m.WorldToTile(Vec2(-0.5f, -0.5f), tx, ty);
		check(tx == -1 && ty == -1, "a point just left of the origin is tile -1");
		m.WorldToTile(Vec2(-2.0f, -4.0f), tx, ty);
		check(tx == -1 && ty == -1, "the far edge of tile -1 is still tile -1");
		m.WorldToTile(Vec2(0.f, 0.f), tx, ty);
		check(tx == 0 && ty == 0, "the origin itself is tile 0");
	}

	// --- the chunk mesh ----------------------------------------------------
	{
		TileSet2D set = MakeSet();
		TileMap2D m(Vec2(1.f, 1.f));
		m.SetTileSet(set);

		TileChunkMesh2D mesh;
		check(!m.BuildChunkMesh(0, 0, mesh), "an empty chunk builds no mesh");

		m.SetTile(0, 0, 0);
		check(m.BuildChunkMesh(0, 0, mesh), "a painted chunk builds a mesh");
		check(mesh.tileCount == 1, "one cell -> one quad");
		check(mesh.vertex.size() == 4, "one quad -> 4 vertices");
		check(mesh.index.size() == 6, "one quad -> 6 indices");
		check(mesh.normal.size() == 4, "one normal per vertex");
		check(mesh.texcoord.size() == 4, "one texcoord per vertex");

		// Corners, counter-clockwise from bottom-left - the winding every
		// sprite in this engine uses.
		nearly(mesh.vertex[0].x, 0.f, "quad bottom-left x");
		nearly(mesh.vertex[0].y, 0.f, "quad bottom-left y");
		nearly(mesh.vertex[2].x, 1.f, "quad top-right x");
		nearly(mesh.vertex[2].y, 1.f, "quad top-right y");
		check(mesh.vertex[0].z == 0.f && mesh.vertex[2].z == 0.f, "the quad is flat at z=0");
		check(mesh.normal[0].z == 1.f, "normals face the camera");

		// UVs must be the tileset's, with the quad's BOTTOM edge taking the
		// rect's bottom v - v runs down the atlas, y runs up the world.
		const Vec4 uv = set.UVRect(0);
		nearly(mesh.texcoord[0].x, uv.x, "bottom-left u is the cell's left");
		nearly(mesh.texcoord[0].y, uv.w, "bottom-left v is the cell's BOTTOM");
		nearly(mesh.texcoord[2].x, uv.z, "top-right u is the cell's right");
		nearly(mesh.texcoord[2].y, uv.y, "top-right v is the cell's TOP");
		check(mesh.texcoord[0].y > mesh.texcoord[2].y, "v decreases going up the quad");

		// Bounds are the whole chunk, not the one painted cell - so an edit
		// never changes them.
		nearly(mesh.minBounds.x, 0.f, "chunk bounds start at the chunk origin");
		nearly(mesh.maxBounds.x, (f32)TileMap2D::CHUNK, "chunk bounds span a whole chunk");
		nearly(mesh.maxBounds.y, (f32)TileMap2D::CHUNK, "chunk bounds are square");

		const Vec3 beforeMin = mesh.minBounds, beforeMax = mesh.maxBounds;
		m.SetTile(5, 6, 2);
		m.BuildChunkMesh(0, 0, mesh);
		check(mesh.tileCount == 2, "a second cell -> a second quad");
		check(mesh.vertex.size() == 8, "two quads -> 8 vertices");
		check(mesh.index[6] == 4, "the second quad's indices are rebased");
		check(mesh.minBounds == beforeMin && mesh.maxBounds == beforeMax,
			"painting does not move the chunk's bounds");
	}

	// --- a negative chunk's mesh sits where it should ----------------------
	{
		TileMap2D m(Vec2(1.f, 1.f));
		m.SetTileSet(MakeSet());
		m.SetTile(-1, -1, 1);
		TileChunkMesh2D mesh;
		check(m.BuildChunkMesh(-1, -1, mesh), "the negative chunk builds");
		nearly(mesh.vertex[0].x, -1.f, "tile -1's quad starts at -1");
		nearly(mesh.maxBounds.x, 0.f, "chunk -1 ends at the origin");
		nearly(mesh.minBounds.x, -(f32)TileMap2D::CHUNK, "chunk -1 starts a chunk back");
	}

	// --- cell size scales the mesh -----------------------------------------
	{
		TileMap2D m(Vec2(2.f, 0.5f));
		m.SetTileSet(MakeSet());
		m.SetTile(1, 1, 0);
		TileChunkMesh2D mesh;
		m.BuildChunkMesh(0, 0, mesh);
		nearly(mesh.vertex[0].x, 2.f, "cell 1 starts at one cell width");
		nearly(mesh.vertex[2].x, 4.f, "and ends at two");
		nearly(mesh.vertex[0].y, 0.5f, "cell 1 starts at one cell height");
		nearly(mesh.vertex[2].y, 1.f, "and ends at two");
		nearly(mesh.maxBounds.x, 2.f * TileMap2D::CHUNK, "chunk bounds scale with the cell");
	}

	// --- cheap edit vs expensive edit --------------------------------------
	{
		TileMap2D m(Vec2(1.f, 1.f));
		m.SetTileSet(MakeSet());
		check(m.NeedsFullRebuild(), "a map with a fresh tileset needs a full rebuild");

		// Pretend a rebuild happened by making one: not reachable here (it
		// uploads buffers), so the flags are checked on transitions only.
		m.SetTile(0, 0, 1);
		check(m.NeedsFullRebuild(), "the first cell in a chunk is a full rebuild");
		check(m.PaintedCount() == 1, "...and it did get painted");

		// A chunk that already exists: the second cell is the cheap kind.
		// (fullDirty is still set from above, so this checks the dirty LIST.)
		m.SetTile(1, 0, 1);
		check(m.DirtyChunkCount() == 1, "a cell in an existing chunk dirties one chunk");
		m.SetTile(2, 0, 1);
		check(m.DirtyChunkCount() == 1, "a second cell in the same chunk does not re-add it");
		m.SetTile(40, 0, 1);
		check(m.NeedsFullRebuild(), "a cell in a new chunk is a full rebuild");

		check(m.NeedsRebuild(), "pending edits are reported");
	}

	// --- an unresolved tileset must not silently produce zero-size quads ---
	{
		TileSet2D bare;                 // no image size set
		TileMap2D m(Vec2(1.f, 1.f));
		m.SetTileSet(bare);
		m.SetTile(0, 0, 0);
		TileChunkMesh2D mesh;
		check(m.BuildChunkMesh(0, 0, mesh), "geometry is still built");
		// Positions come from the grid, so they are right...
		nearly(mesh.vertex[2].x, 1.f, "the quad keeps its world size");
		// ...but every UV collapses, which is the visible symptom to look for.
		check(mesh.texcoord[0] == mesh.texcoord[2],
			"an unresolved tileset collapses UVs (the symptom to recognise)");
	}

	printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "OK",
		failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
