//============================================================================
// Name        : WalkPath2D.cpp
// Description : See the header.
//============================================================================

#include <Pyros3D/Utils/Navigation/WalkPath2D.h>
#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>

namespace p3d {

	namespace {

		struct Grid {
			const std::function<bool(int32, int32)> &solid;
			int32 minX, minY, maxX, maxY, height;

			bool Inside(const int32 x, const int32 y) const { return x >= minX && x <= maxX && y >= minY && y <= maxY; }
			// Room for the walker's body with its feet in (x, y).
			bool Clear(const int32 x, const int32 y) const
			{
				for (int32 k = 0; k < height; k++) if (solid(x, y + k)) return false;
				return true;
			}
			bool Standable(const int32 x, const int32 y) const
			{
				return Inside(x, y) && Clear(x, y) && solid(x, y - 1);
			}
			// A run of cells along one row, both ends included.
			bool ClearRow(int32 x0, int32 x1, const int32 y) const
			{
				if (x0 > x1) std::swap(x0, x1);
				for (int32 x = x0; x <= x1; x++) if (!Clear(x, y)) return false;
				return true;
			}
			// A run up or down one column, both ends included.
			bool ClearColumn(const int32 x, int32 y0, int32 y1) const
			{
				if (y0 > y1) std::swap(y0, y1);
				for (int32 y = y0; y <= y1; y++) if (!Clear(x, y)) return false;
				return true;
			}
		};

		uint64 Key(const int32 x, const int32 y) { return ((uint64)(uint32)x << 32) | (uint64)(uint32)y; }

		struct Open {
			f32 f;
			int32 x, y;
			bool operator<(const Open &o) const { return f > o.f; }      // smallest first
		};
		struct Node { f32 g; int32 px, py; uint8 move; bool closed; };

		// Down from where something is to where it would be standing.
		bool Settle(const Grid &g, int32 x, int32 &y, const int32 furthest)
		{
			for (int32 k = 0; k <= furthest; k++)
			{
				if (g.Standable(x, y - k)) { y -= k; return true; }
				// In the ground: up is the way out, not down.
				if (k == 0 && !g.Clear(x, y))
				{
					for (int32 u = 1; u <= furthest; u++)
						if (g.Standable(x, y + u)) { y += u; return true; }
					return false;
				}
			}
			return false;
		}
	}

	bool FindWalkPath2D(const std::function<bool(int32, int32)> &solid,
		const int32 fromX, const int32 fromY, const int32 toX, const int32 toY,
		const int32 minX, const int32 minY, const int32 maxX, const int32 maxY,
		const WalkPathOptions2D &o, std::vector<WalkStep2D> &out)
	{
		out.clear();
		const Grid g = { solid, minX, minY, maxX, maxY, o.height < 1 ? 1 : o.height };
		const int32 jumpH = std::max<int32>(0, o.jumpHeight), jumpD = std::max<int32>(1, o.jumpDistance);
		const int32 drop = std::max<int32>(0, o.maxDrop);

		int32 sx = fromX, sy = fromY, tx = toX, ty = toY;
		if (!g.Inside(sx, sy) || !g.Inside(tx, ty)) return false;
		if (!Settle(g, sx, sy, std::max<int32>(drop, 32)) || !Settle(g, tx, ty, std::max<int32>(drop, 32))) return false;

		std::unordered_map<uint64, Node> nodes;
		std::priority_queue<Open> open;
		const auto h = [tx, ty](const int32 x, const int32 y) {
			// Never more than the real cost: a jump covers several cells for
			// little more than one step's price.
			return 0.5f * (f32)std::max(abs(tx - x), abs(ty - y));
		};
		Node first = { 0.f, sx, sy, (uint8)WalkMove2D::Start, false };
		nodes[Key(sx, sy)] = first;
		Open so = { h(sx, sy), sx, sy };
		open.push(so);

		uint32 looked = 0;
		bool found = false;
		while (!open.empty())
		{
			const Open cur = open.top();
			open.pop();
			Node &n = nodes[Key(cur.x, cur.y)];
			if (n.closed) continue;
			n.closed = true;
			if (cur.x == tx && cur.y == ty) { found = true; break; }
			if (++looked > o.maxCells) break;
			const f32 gHere = n.g;

			const auto relax = [&](const int32 x, const int32 y, const f32 cost, const uint8 move) {
				std::unordered_map<uint64, Node>::iterator it = nodes.find(Key(x, y));
				const f32 gNew = gHere + cost;
				if (it != nodes.end() && (it->second.closed || it->second.g <= gNew)) return;
				Node nn = { gNew, cur.x, cur.y, move, false };
				nodes[Key(x, y)] = nn;
				Open on = { gNew + h(x, y), x, y };
				open.push(on);
			};

			for (int32 dx = -jumpD; dx <= jumpD; dx++)
			{
				if (dx == 0) continue;
				const int32 x = cur.x + dx;
				if (x < minX || x > maxX) continue;
				const int32 ax = abs(dx);

				// Along the ground.
				if (ax == 1 && g.Standable(x, cur.y)) { relax(x, cur.y, 1.f, WalkMove2D::Walk); }

				// Up: straight up beside where it stands until it is as high
				// as where it is going, then across at that height.
				for (int32 dy = 1; dy <= jumpH; dy++)
				{
					const int32 y = cur.y + dy;
					if (!g.Standable(x, y)) continue;
					if (!g.ClearColumn(cur.x, cur.y, y)) break;         // a ceiling: nothing higher either
					if (!g.ClearRow(cur.x, x, y)) continue;
					relax(x, y, (f32)(ax + dy) + 1.f, WalkMove2D::Jump);
				}

				// Across a gap, level: one row of headroom for the arc.
				if (ax >= 2 && g.Standable(x, cur.y) && g.ClearRow(cur.x, x, cur.y) && g.ClearRow(cur.x, x, cur.y + 1))
				{
					// Only a jump if there is something to jump OVER; over
					// unbroken ground the steps are cheaper and are taken.
					bool gap = false;
					for (int32 k = 1; k < ax && !gap; k++) gap = !g.solid(cur.x + (dx > 0 ? k : -k), cur.y - 1);
					if (gap) relax(x, cur.y, (f32)ax + 1.f, WalkMove2D::Jump);
				}

				// Down: out over the edge at the height it stands at, then
				// straight down to the first ground in that column.
				if (!g.ClearRow(cur.x, x, cur.y)) continue;
				// (stepping off needs an edge: the cell beside it has no floor)
				if (ax == 1 && g.solid(x, cur.y - 1)) continue;
				for (int32 dy = 1; dy <= drop; dy++)
				{
					const int32 y = cur.y - dy;
					if (y < minY) break;
					if (!g.Clear(x, y)) break;                           // landed on something above this
					if (g.solid(x, y - 1))
					{
						relax(x, y, (f32)ax + 0.5f * (f32)dy + 0.5f, ax == 1 ? WalkMove2D::Drop : WalkMove2D::Jump);
						break;
					}
				}
			}
		}
		if (!found) return false;

		// Back from the goal to the start, then turned round.
		int32 x = tx, y = ty;
		for (int guard = 0; guard < 1000000; guard++)
		{
			const Node &n = nodes[Key(x, y)];
			WalkStep2D s;
			s.x = x; s.y = y; s.move = n.move;
			out.push_back(s);
			if (n.move == WalkMove2D::Start) break;
			const int32 px = n.px, py = n.py;
			x = px; y = py;
		}
		std::reverse(out.begin(), out.end());
		return true;
	}

}
