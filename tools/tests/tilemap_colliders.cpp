// Checks TileMap2D's collider merge. Standalone, per prefab_roundtrip.cpp:
//
//   c++ -std=c++17 -I include tools/tests/tilemap_colliders.cpp -o /tmp/tmc \
//       -L build_gl -lPyrosEngine -Wl,-rpath,$PWD/build_gl && /tmp/tmc
//
// BuildColliderBoxes() is pure, so none of this needs a physics world. What it
// has to get right is that the merged rectangles cover EXACTLY the solid cells
// - no more (a phantom wall), no less (a floor you fall through) - and that
// they are few. Both are checked by rasterising the output back onto a grid
// and comparing it with the input, which catches an off-by-one that eyeballing
// a rectangle list would not.
#include <Pyros3D/Rendering/Components/TileMap2D/TileMap2D.h>

#include <cstdio>
#include <cmath>
#include <set>
#include <string>
#include <vector>

using namespace p3d;

static int failures = 0;
static void check(bool cond, const std::string &what)
{
	printf("%s  %s\n", cond ? "PASS" : "FAIL", what.c_str());
	if (!cond) failures++;
}

static TileSet2D MakeSet()
{
	TileSet2D s;
	s.image = "atlas.png";
	s.tileW = s.tileH = 16;
	s.SetImageSize(64, 64);
	s.tiles[1].solid = true;    // solid
	s.tiles[2].solid = true;    // solid
	// tile 0 and 3 stay non-solid: decoration.
	return s;
}

// Rasterise the merged rectangles back onto a cell grid and compare with the
// map's own solid cells. Also reports overlap, which would double up contacts.
static void coverageMatches(const TileMap2D &m, const std::string &what)
{
	const std::vector<Vec4> boxes = m.BuildColliderBoxes();
	const Vec2 ts = m.GetTileSize();

	std::set<std::pair<int, int> > want, got;
	int32 minX = 0, minY = 0, maxX = 0, maxY = 0;
	if (m.GetTileBounds(minX, minY, maxX, maxY))
		for (int32 y = minY; y <= maxY; y++)
			for (int32 x = minX; x <= maxX; x++)
			{
				const int32 t = m.GetTile(x, y);
				if (t >= 0 && m.GetTileSet().IsSolid(t))
					want.insert(std::make_pair((int)x, (int)y));
			}

	bool overlapped = false;
	for (size_t b = 0; b < boxes.size(); b++)
	{
		// Back from (centre, half-extent) in world units to cell indices.
		const int32 x0 = (int32)lroundf((boxes[b].x - boxes[b].z) / ts.x);
		const int32 y0 = (int32)lroundf((boxes[b].y - boxes[b].w) / ts.y);
		const int32 w = (int32)lroundf((boxes[b].z * 2.f) / ts.x);
		const int32 h = (int32)lroundf((boxes[b].w * 2.f) / ts.y);
		for (int32 y = 0; y < h; y++)
			for (int32 x = 0; x < w; x++)
				if (!got.insert(std::make_pair((int)(x0 + x), (int)(y0 + y))).second)
					overlapped = true;
	}

	check(got == want, what + ": covers exactly the solid cells");
	check(!overlapped, what + ": rectangles do not overlap");
}

int main()
{
	// --- a plain floor merges to ONE box -----------------------------------
	{
		TileMap2D m(Vec2(1.f, 1.f));
		m.SetTileSet(MakeSet());
		m.Fill(0, 0, 19, 0, 1);
		const std::vector<Vec4> b = m.BuildColliderBoxes();
		check(b.size() == 1, "a 20x1 floor is one box");
		if (b.size() == 1)
		{
			check(std::fabs(b[0].x - 10.f) < 1e-4f, "floor centre x is the middle of the run");
			check(std::fabs(b[0].y - 0.5f) < 1e-4f, "floor centre y is half a cell up");
			check(std::fabs(b[0].z - 10.f) < 1e-4f, "floor half-width is half the run");
			check(std::fabs(b[0].w - 0.5f) < 1e-4f, "floor half-height is half a cell");
		}
		coverageMatches(m, "floor");
	}

	// --- a solid block merges to ONE box ------------------------------------
	{
		TileMap2D m(Vec2(1.f, 1.f));
		m.SetTileSet(MakeSet());
		m.Fill(0, 0, 9, 4, 1);
		check(m.BuildColliderBoxes().size() == 1, "a 10x5 block is one box");
		coverageMatches(m, "block");
	}

	// --- two solid tile TYPES still merge together --------------------------
	{
		TileMap2D m(Vec2(1.f, 1.f));
		m.SetTileSet(MakeSet());
		m.Fill(0, 0, 9, 0, 1);
		m.Fill(10, 0, 19, 0, 2);
		check(m.BuildColliderBoxes().size() == 1,
			"different solid tiles merge - solidity is the only question");
		coverageMatches(m, "mixed solids");
	}

	// --- non-solid tiles contribute nothing ---------------------------------
	{
		TileMap2D m(Vec2(1.f, 1.f));
		m.SetTileSet(MakeSet());
		m.Fill(0, 0, 19, 2, 3);        // decoration everywhere
		check(m.BuildColliderBoxes().empty(), "a decorative layer has no colliders");
		m.Fill(5, 1, 8, 1, 1);         // one solid ledge inside it
		check(m.BuildColliderBoxes().size() == 1, "the solid ledge inside it is one box");
		coverageMatches(m, "decoration + ledge");
	}

	// --- a gap splits a run --------------------------------------------------
	{
		TileMap2D m(Vec2(1.f, 1.f));
		m.SetTileSet(MakeSet());
		m.Fill(0, 0, 4, 0, 1);
		m.Fill(7, 0, 11, 0, 1);        // a pit between them
		check(m.BuildColliderBoxes().size() == 2, "a pit splits the floor in two");
		coverageMatches(m, "floor with a pit");
	}

	// --- a platformer-shaped level stays in single digits --------------------
	{
		TileMap2D m(Vec2(1.f, 1.f));
		m.SetTileSet(MakeSet());
		m.Fill(0, 0, 39, 1, 1);        // ground, 2 deep
		m.Fill(6, 4, 10, 4, 1);        // ledges
		m.Fill(14, 6, 18, 6, 1);
		m.Fill(24, 5, 30, 5, 1);
		m.Fill(34, 3, 39, 8, 1);       // a wall at the end
		const size_t n = m.BuildColliderBoxes().size();
		printf("     (platformer level -> %d boxes)\n", (int)n);
		check(n <= 8, "a platformer level merges into single digits");
		coverageMatches(m, "platformer level");
	}

	// --- a checkerboard is the worst case, and must still be exact -----------
	{
		TileMap2D m(Vec2(1.f, 1.f));
		m.SetTileSet(MakeSet());
		int solidCells = 0;
		for (int32 y = 0; y < 8; y++)
			for (int32 x = 0; x < 8; x++)
				if (((x + y) & 1) == 0) { m.SetTile(x, y, 1); solidCells++; }
		const size_t n = m.BuildColliderBoxes().size();
		printf("     (8x8 checkerboard -> %d boxes for %d cells)\n", (int)n, solidCells);
		check((int)n == solidCells, "a checkerboard cannot merge, and does not pretend to");
		coverageMatches(m, "checkerboard");
	}

	// --- negative coordinates ------------------------------------------------
	{
		TileMap2D m(Vec2(1.f, 1.f));
		m.SetTileSet(MakeSet());
		m.Fill(-10, -4, -1, -3, 1);
		const std::vector<Vec4> b = m.BuildColliderBoxes();
		check(b.size() == 1, "a block in negative space is one box");
		if (b.size() == 1)
			check(b[0].x < 0.f && b[0].y < 0.f, "and its centre is negative too");
		coverageMatches(m, "negative block");
	}

	// --- cell size scales the boxes -----------------------------------------
	{
		TileMap2D m(Vec2(2.f, 0.5f));
		m.SetTileSet(MakeSet());
		m.Fill(0, 0, 3, 1, 1);
		const std::vector<Vec4> b = m.BuildColliderBoxes();
		check(b.size() == 1, "scaled cells still merge to one box");
		if (b.size() == 1)
		{
			check(std::fabs(b[0].z - 4.f) < 1e-4f, "half-width scales with cell width");
			check(std::fabs(b[0].w - 0.5f) < 1e-4f, "half-height scales with cell height");
		}
		coverageMatches(m, "scaled cells");
	}

	// --- an empty map, and a map with no tileset -----------------------------
	{
		TileMap2D empty(Vec2(1.f, 1.f));
		check(empty.BuildColliderBoxes().empty(), "an empty map has no colliders");

		TileMap2D noset(Vec2(1.f, 1.f));
		noset.SetTile(0, 0, 1);
		check(noset.BuildColliderBoxes().empty(),
			"with no tileset nothing is solid, so nothing collides");
	}

	printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "OK",
		failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
