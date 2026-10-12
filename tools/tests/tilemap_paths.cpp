//============================================================================
// Name        : tilemap_paths.cpp
// Description : Ways across a tile map: for something that goes wherever a
//               cell is open, and for something that walks.
//
//   c++ -std=c++17 -I include tools/tests/tilemap_paths.cpp -o /tmp/tilemap_paths \
//       -L build_vulkan -lPyrosEngine -Wl,-rpath,$PWD/build_vulkan && /tmp/tilemap_paths
//============================================================================

#include <Pyros3D/Rendering/Components/TileMap2D/TileMap2D.h>
#include <Pyros3D/Utils/Navigation/WalkPath2D.h>
#include <cstdio>
#include <cstdlib>
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
	s.tiles[1].solid = true;
	return s;
}

// A level drawn as text, top row first: '#' solid, '.' open.
static void Paint(TileMap2D &m, const std::vector<std::string> &rows)
{
	const int32 h = (int32)rows.size();
	for (int32 r = 0; r < h; r++)
		for (int32 x = 0; x < (int32)rows[(size_t)r].size(); x++)
			m.SetTile(x, h - 1 - r, rows[(size_t)r][(size_t)x] == '#' ? 1 : 0);
}

static std::string Moves(const std::vector<WalkStep2D> &s)
{
	std::string out;
	for (size_t i = 0; i < s.size(); i++) out += "SWJD"[s[i].move <= 3 ? s[i].move : 0];
	return out;
}

int main()
{
	// --- anywhere a cell is open ----------------------------------------------
	{
		TileMap2D m(Vec2(1.f, 1.f));
		m.SetTileSet(MakeSet());
		std::vector<std::string> rows;
		rows.push_back("..........");
		rows.push_back("....#.....");
		rows.push_back("....#.....");
		rows.push_back("....#.....");
		rows.push_back("..........");
		Paint(m, rows);
		std::vector<std::pair<int32, int32> > way;
		check(m.FindPath(1, 2, 8, 2, way) && !way.empty(), "a way is found round a wall");
		check(way.front().first == 1 && way.front().second == 2 && way.back().first == 8 && way.back().second == 2,
			"from the start cell to the goal cell");
		bool through = false, jumps = false;
		for (size_t i = 0; i < way.size(); i++)
		{
			if (m.IsSolidCell(way[i].first, way[i].second)) through = true;
			if (i > 0 && (abs(way[i].first - way[i - 1].first) > 1 || abs(way[i].second - way[i - 1].second) > 1)) jumps = true;
		}
		check(!through, "through no solid cell");
		check(!jumps, "one neighbouring cell at a time");
		check(way.size() >= 8 && way.size() <= 11, "and not the long way round");
		check(!m.FindPath(1, 2, 4, 2, way), "there is no way INTO a solid cell");
		check(!m.FindPath(1, 2, 500, 2, way), "nor to somewhere off the map");

		// Walled in.
		TileMap2D box(Vec2(1.f, 1.f));
		box.SetTileSet(MakeSet());
		std::vector<std::string> br;
		br.push_back("#####....");
		br.push_back("#...#....");
		br.push_back("#...#....");
		br.push_back("#####....");
		Paint(box, br);
		check(!box.FindPath(2, 1, 7, 1, way, 0), "nor out of a closed room");

		std::vector<uint8> costs;
		int32 ox = 0, oy = 0;
		uint32 w = 0, h = 0;
		check(m.BuildPathCosts(costs, ox, oy, w, h, 1) && w == 12 && h == 7 && ox == -1 && oy == -1, "the map hands its cells over as a PathGrid's costs, with its margin");
		check(costs[(size_t)(3 + 1) * w + (4 + 1)] == 0 && costs[(size_t)(0 + 1) * w + (4 + 1)] == 1, "solid cells cost 0, open ones 1");
	}

	// --- something that walks ------------------------------------------------
	{
		// Floor along the bottom, a step up, a gap, a high ledge and a pit.
		//            0123456789012345678
		std::vector<std::string> rows;
		rows.push_back("...................");   // y 6
		rows.push_back("...................");   // y 5
		rows.push_back("...............####");   // y 4   a ledge: stand at y 5
		rows.push_back("...................");   // y 3
		rows.push_back("......###..........");   // y 2   a step: stand at y 3
		rows.push_back("...................");   // y 1
		rows.push_back("######...####..####");   // y 0   ground: stand at y 1; gaps at x 6-8 (under the step) and 13-14
		TileMap2D m(Vec2(1.f, 1.f));
		m.SetTileSet(MakeSet());
		Paint(m, rows);
		WalkPathOptions2D o;
		std::vector<WalkStep2D> s;

		check(m.FindWalkPath(0, 1, 5, 1, o, s) && Moves(s) == "SWWWWW", "along level ground it walks");
		check(m.FindWalkPath(0, 5, 3, 1, o, s) && s.front().y == 1, "a start in the air is brought down to the ground under it");

		check(m.FindWalkPath(4, 1, 7, 3, o, s), "it gets up onto a step two cells high");
		bool jumped = false, floats = false;
		for (size_t i = 0; i < s.size(); i++)
		{
			if (s[i].move == WalkMove2D::Jump) jumped = true;
			if (!m.IsSolidCell(s[i].x, s[i].y - 1) || m.IsSolidCell(s[i].x, s[i].y)) floats = true;
		}
		check(jumped, "by jumping");
		check(!floats, "and every cell of the way is one it can stand in");

		// Across the gap at x 13-14 (two cells wide): a jump of three.
		check(m.FindWalkPath(10, 1, 16, 1, o, s), "it crosses a two-cell gap");
		jumped = false;
		for (size_t i = 0; i < s.size(); i++) if (s[i].move == WalkMove2D::Jump && s[i].x >= 15) jumped = true;
		check(jumped, "by jumping it");
		WalkPathOptions2D shortLegs = o;
		shortLegs.jumpDistance = 2;
		check(!m.FindWalkPath(10, 1, 16, 1, shortLegs, s), "one that cannot jump that far does not");

		// The ledge at y 5 is four above the ground: out of reach of a jump of 3.
		check(!m.FindWalkPath(16, 1, 17, 5, o, s), "a ledge higher than it can jump is out of reach");
		WalkPathOptions2D strong = o;
		strong.jumpHeight = 4;
		check(m.FindWalkPath(16, 1, 17, 5, strong, s), "one that jumps higher reaches it");
		// Down from the ledge: off the edge and a fall of four.
		check(m.FindWalkPath(17, 5, 16, 1, o, s), "it comes down off a ledge");
		bool dropped = false;
		for (size_t i = 0; i < s.size(); i++) if (s[i].move == WalkMove2D::Drop || (s[i].move == WalkMove2D::Jump && i > 0 && s[i].y < s[i - 1].y)) dropped = true;
		check(dropped, "by dropping");
		WalkPathOptions2D careful = o;
		careful.maxDrop = 2;
		check(!m.FindWalkPath(17, 5, 16, 1, careful, s), "one that will not fall that far stays up");

		// Headroom: a walker two cells tall does not fit under the step (the
		// step's underside is at y 2, so only y 1 is clear beneath it)...
		// there is no floor there anyway; test a low ceiling over real floor.
		std::vector<std::string> low;
		low.push_back("..........");   // y 3
		low.push_back("...####...");   // y 2  a ceiling one cell above head height
		low.push_back("..........");   // y 1
		low.push_back("##########");   // y 0
		TileMap2D tunnel(Vec2(1.f, 1.f));
		tunnel.SetTileSet(MakeSet());
		Paint(tunnel, low);
		WalkPathOptions2D tall = o;
		tall.height = 2;
		check(tunnel.FindWalkPath(0, 1, 9, 1, o, s) && Moves(s) == "SWWWWWWWWW", "a walker one cell tall goes under a low ceiling");
		check(tunnel.FindWalkPath(0, 1, 9, 1, tall, s), "a taller one still gets there");
		bool under = false;
		for (size_t i = 0; i < s.size(); i++) if (s[i].y == 1 && s[i].x >= 3 && s[i].x <= 6) under = true;
		check(!under, "but over the top, not under it");
	}

	printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
