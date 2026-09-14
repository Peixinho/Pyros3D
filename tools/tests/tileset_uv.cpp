// Checks the .p3dt tileset's arithmetic and round-trip. Standalone on purpose -
// the repo has no test framework (see prefab_roundtrip.cpp). Build and run it
// against an existing engine build:
//
//   c++ -std=c++17 -I include tools/tests/tileset_uv.cpp -o /tmp/tileset_uv \
//       -L build_gl -lPyrosEngine -Wl,-rpath,$PWD/build_gl && /tmp/tileset_uv
//
// Covers the two things that are invisible until one specific tile is used:
// the cell-fit count on a sheet WITH margin and spacing, and the half-texel
// inset that keeps a cell's UVs off its neighbours' pixels. No window and no
// render device - TileSet2D is deliberately arithmetic over an image size, not
// over a Texture.
#include <Pyros3D/Assets/TileSet2D/TileSet2D.h>

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

int main()
{
	// --- a plain sheet: 64x64, 16px cells, no margin, no spacing ----------
	{
		TileSet2D s;
		s.image = "assets/textures/tiles.png";
		s.tileW = s.tileH = 16;
		s.SetImageSize(64, 64);

		check(s.Columns() == 4, "plain sheet: 4 columns");
		check(s.Rows() == 4, "plain sheet: 4 rows");
		check(s.TileCount() == 16, "plain sheet: 16 tiles");

		// Half-texel inset: tile 0 must NOT start at 0.
		const Vec4 t0 = s.UVRect(0);
		nearly(t0.x, 0.5f / 64.f, "tile 0 left is inset half a texel");
		nearly(t0.y, 0.5f / 64.f, "tile 0 top is inset half a texel");
		nearly(t0.z, 15.5f / 64.f, "tile 0 right is inset half a texel");

		// v runs DOWN: the tile on row 1 sits at a LARGER v than row 0.
		check(s.UVRect(4).y > s.UVRect(0).y, "v runs down (row 1 below row 0)");
		// ...and the tile in column 1 sits at a larger u than column 0.
		check(s.UVRect(1).x > s.UVRect(0).x, "u runs right (col 1 right of col 0)");

		// The last tile must stay inside the image.
		const Vec4 last = s.UVRect(15);
		check(last.z < 1.f && last.w < 1.f, "last tile stays inside [0,1]");
		check(last.x > 0.f && last.y > 0.f, "last tile has positive origin");
	}

	// --- the case that hides off-by-ones: margin AND spacing --------------
	// 4 cells of 16px with a 1px margin and a 2px gutter occupy
	// 1 + 4*16 + 3*2 + 1 = 72 px.
	{
		TileSet2D s;
		s.tileW = s.tileH = 16;
		s.margin = 1;
		s.spacing = 2;
		s.SetImageSize(72, 72);

		check(s.Columns() == 4, "margin+spacing: 4 columns fit in 72px");
		check(s.Rows() == 4, "margin+spacing: 4 rows fit in 72px");

		// Last cell starts at 1 + 3*(16+2) = 55 and ends at 71, inside 72.
		const Vec4 last = s.UVRect(15);
		nearly(last.x, 55.5f / 72.f, "margin+spacing: last cell left edge");
		nearly(last.z, 70.5f / 72.f, "margin+spacing: last cell right edge");
		check(last.z < 1.f, "margin+spacing: last cell stays inside [0,1]");

		// One pixel short and the last column must NOT be claimed.
		TileSet2D narrow = s;
		narrow.SetImageSize(71, 72);
		check(narrow.Columns() == 3, "a sheet 1px short drops the last column");
	}

	// --- an explicit column override, narrower than the sheet -------------
	{
		TileSet2D s;
		s.tileW = s.tileH = 16;
		s.columns = 3;
		s.SetImageSize(64, 64);
		check(s.Columns() == 3, "explicit columns wins over the derived count");
		check(s.TileCount() == 12, "explicit columns narrows the tile count");
		// Index 3 is row 1 col 0 under the override, not row 0 col 3.
		nearly(s.UVRect(3).x, 0.5f / 64.f, "override re-wraps the row");
	}

	// --- refusals and empties --------------------------------------------
	{
		TileSet2D s;
		s.tileW = s.tileH = 16;
		const Vec4 z = s.UVRect(0);
		check(z.x == 0.f && z.y == 0.f && z.z == 0.f && z.w == 0.f,
			"no image size yet -> zero rect, not a guess");
		check(s.TileCount() == 0, "no image size yet -> no tiles");

		s.SetImageSize(64, 64);
		const Vec4 oob = s.UVRect(16);
		check(oob.x == 0.f && oob.z == 0.f, "out-of-range index -> zero rect");
		const Vec4 neg = s.UVRect(-1);
		check(neg.x == 0.f && neg.z == 0.f, "negative index -> zero rect");
	}

	// --- solidity ---------------------------------------------------------
	{
		TileSet2D s;
		s.tiles[3].solid = true;
		s.tiles[7].tags.push_back("ice");
		check(s.IsSolid(3), "tile 3 is solid");
		check(!s.IsSolid(7), "a tile with tags but no solid flag is not solid");
		check(!s.IsSolid(99), "an unlisted tile is not solid");
		check(s.Find(99) == NULL, "an unlisted tile has no info");
	}

	// --- round-trip through the file format -------------------------------
	{
		TileSet2D s;
		s.image = "assets/tiles/forest.png";
		s.tileW = 16; s.tileH = 24;
		s.margin = 1; s.spacing = 2;
		s.columns = 5;
		s.tiles[3].solid = true;
		s.tiles[7].solid = true;
		s.tiles[7].tags.push_back("ice");

		std::string err;
		TileSet2D back;
		check(TileSet2DFromString(TileSet2DToString(s), back, &err),
			"round-trip parses: " + err);
		check(back.image == s.image, "round-trip keeps the image path verbatim");
		check(back.tileW == 16 && back.tileH == 24, "round-trip keeps cell size");
		check(back.margin == 1 && back.spacing == 2, "round-trip keeps margin/spacing");
		check(back.columns == 5, "round-trip keeps the column override");
		check(back.IsSolid(3) && back.IsSolid(7), "round-trip keeps solid flags");
		check(back.Find(7) && back.Find(7)->tags.size() == 1
			&& back.Find(7)->tags[0] == "ice", "round-trip keeps tags");

		// A cell carrying nothing is not written, so it does not come back.
		TileSet2D empties;
		empties.tiles[1] = TileInfo2D();
		TileSet2D back2;
		check(TileSet2DFromString(TileSet2DToString(empties), back2, &err),
			"round-trip of an all-default cell parses");
		check(back2.Find(1) == NULL, "a cell saying nothing is not written");
	}

	// --- malformed input is reported, not absorbed ------------------------
	{
		TileSet2D out;
		std::string err;
		check(!TileSet2DFromString("{ not json", out, &err), "malformed JSON is refused");
		check(!err.empty(), "malformed JSON reports why");
		check(!TileSet2DFromString("{\"tileW\":0}", out, &err), "tileW 0 is refused");
		check(!TileSet2DFromString("{\"margin\":-1}", out, &err), "negative margin is refused");
		check(!TileSet2DFromString("[]", out, &err), "a JSON array is not a tileset");
		check(TileSet2DFromString("{}", out, &err), "an empty object is a valid default tileset");
	}

	printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "OK",
		failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
