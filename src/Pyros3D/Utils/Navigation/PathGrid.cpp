//============================================================================
// Name        : PathGrid.cpp
// Description : Ways across a grid of cells, found on the job system.
//============================================================================

#include <Pyros3D/Utils/Navigation/PathGrid.h>
#include <Pyros3D/Utils/Jobs/JobSystem.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <queue>

namespace p3d {

	PathGrid::PathGrid(const uint32 width, const uint32 height, const std::vector<uint8> &cost) : w(width), h(height)
	{
		std::vector<uint8> c(cost);
		c.resize((size_t)w * h, 0);
		costs = std::make_shared<const std::vector<uint8> >(std::move(c));
	}
	PathGrid::~PathGrid() {}

	uint32 PathGrid::Cost(const int32 i, const int32 j) const
	{
		if (i < 0 || j < 0 || (uint32)i >= w || (uint32)j >= h) return 0;
		return (*costs)[(size_t)j * w + i];
	}

	bool PathGrid::Find(const std::vector<uint8> &cost, const uint32 width, const uint32 height,
		const int32 si, const int32 sj, const int32 ti, const int32 tj, const uint32 maxCells, std::vector<uint32> &cells)
	{
		cells.clear();
		const int32 W = (int32)width, H = (int32)height;
		auto at = [&](const int32 i, const int32 j) -> uint32 {
			return (i < 0 || j < 0 || i >= W || j >= H) ? 0u : (uint32)cost[(size_t)j * W + i];
		};
		if (at(si, sj) == 0 || at(ti, tj) == 0) return false;
		const size_t n = (size_t)W * H;
		static const uint32 kNone = 0xFFFFFFFFu;
		std::vector<f32> g(n, -1.f);
		std::vector<uint32> from(n, kNone);
		std::vector<uint8> closed(n, 0);
		typedef std::pair<f32, uint32> Open;
		std::priority_queue<Open, std::vector<Open>, std::greater<Open> > open;
		const uint32 start = (uint32)(sj * W + si), goal = (uint32)(tj * W + ti);
		g[start] = 0.f;
		open.push(Open(0.f, start));
		static const int32 kStep[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };
		uint32 spent = 0;
		while (!open.empty())
		{
			const uint32 k = open.top().second;
			open.pop();
			if (closed[k]) continue;
			closed[k] = 1;
			spent++;
			if (k == goal)
			{
				for (uint32 c = k; c != kNone; c = from[c]) cells.push_back(c);
				std::reverse(cells.begin(), cells.end());
				return true;
			}
			// (a search that has looked at a great deal of the map is not going to find a way)
			if (spent > maxCells) return false;
			const int32 i = (int32)(k % (uint32)W), j = (int32)(k / (uint32)W);
			for (int s = 0; s < 8; s++)
			{
				const int32 ni = i + kStep[s][0], nj = j + kStep[s][1];
				const uint32 c = at(ni, nj);
				if (c == 0) continue;
				const bool diagonal = s >= 4;
				// (not across a corner between two cells that cannot be crossed)
				if (diagonal && (at(ni, j) == 0 || at(i, nj) == 0)) continue;
				const uint32 nk = (uint32)(nj * W + ni);
				if (closed[nk]) continue;
				const f32 ng = g[k] + (diagonal ? 1.414f : 1.f) * (0.6f + 0.4f * (f32)c);
				if (g[nk] >= 0.f && ng >= g[nk]) continue;
				g[nk] = ng;
				from[nk] = k;
				const f32 dx = (f32)std::abs(ni - ti), dz = (f32)std::abs(nj - tj);
				open.push(Open(ng + (std::max(dx, dz) + 0.414f * std::min(dx, dz)) * 1.02f, nk));
			}
		}
		return false;
	}

	uint32 PathGrid::Request(const int32 si, const int32 sj, const int32 ti, const int32 tj, const uint32 maxCells)
	{
		const uint32 ticket = ++lastTicket;
		std::shared_ptr<Answer> answer = std::make_shared<Answer>();
		asked[ticket] = answer;
		std::shared_ptr<const std::vector<uint8> > c = costs;
		const uint32 W = w, H = h;
		// (nobody waits for it: the counter is the job's own)
		std::shared_ptr<JobCounter> counter = std::make_shared<JobCounter>();
		JobSystem::Instance().Run([answer, c, W, H, si, sj, ti, tj, maxCells, counter]() {
			std::vector<uint32> cells;
			const bool found = Find(*c, W, H, si, sj, ti, tj, maxCells, cells);
			std::lock_guard<std::mutex> g(answer->lock);
			answer->cells.swap(cells);
			answer->state = found ? (uint32)Found : (uint32)NoWay;
		}, *counter);
		return ticket;
	}

	uint32 PathGrid::Poll(const uint32 ticket, std::vector<uint32> &cells)
	{
		std::unordered_map<uint32, std::shared_ptr<Answer> >::iterator at = asked.find(ticket);
		if (at == asked.end()) return Unknown;
		uint32 state;
		{
			std::lock_guard<std::mutex> g(at->second->lock);
			state = at->second->state;
			if (state == Found) cells.swap(at->second->cells);
		}
		if (state != Pending) asked.erase(at);
		return state;
	}

	void PathGrid::Cancel(const uint32 ticket)
	{
		asked.erase(ticket);       // (the search, if under way, finishes into an answer nobody holds)
	}

}
