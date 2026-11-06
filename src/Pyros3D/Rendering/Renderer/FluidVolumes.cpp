//============================================================================
// Name        : FluidVolumes.cpp
// Description : Smoke and fire simulated as a fluid on the GPU
//============================================================================

#include <Pyros3D/Rendering/Renderer/FluidVolumes.h>
#include <Pyros3D/Rendering/Volumetrics/VolumetricSmoke.h>
#include <Pyros3D/Physics/PhysicsEngines/IPhysics.h>
#include <Pyros3D/Utils/Jobs/JobSystem.h>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <queue>

namespace p3d {

	namespace {
		const uint32 kPoolCells = 10u << 20;      // (the render pool: 10M cells, 80 MB - eight grids, the biggest kind 1.3M)
		const uint32 kPressureIterations = 16;    // (warm: each step starts from the last one's pressure)
		// A step every 1/30 s of the clouds' clock (smoke moves slowly: it does not show, and it halves the cost), two
		// at most a frame; a frame further behind lets the smoke slow down rather than the frame
		const f32 kStep = 1.f / 30.f;
		const uint32 kMaxStepsAFrame = 2;

		// The fluid.comp DrawData
		struct SimData
		{
			f32 dims[4], grid[4], step[4], flags[4], source[4], inject[4], jet[4], forces[4], decay[4], wind[4], sun[4], emit[4], turb[4], extra[4];     // (extra: x 1 where the box's sides hold the smoke in - under a roof)
			f32 pool[4];     // (x 1: a fire burns its spilled fuel - Sim::pool - not a ball round its source; y how deep the flames start (m) over it; w where it starts in the solid buffer (words))
		};
		static_assert(sizeof(SimData) == 240, "fluid.comp's DrawData");

		uint32 Groups(const uint32 n) { return (n + 3) / 4; }
	}

	void FluidVolumes::Create(gfx::Device* d, gfx::Shader fluidShader)
	{
		device = d;
		gfx::ComputePipelineDesc cp;
		cp.compute = fluidShader;
		for (uint32 b = 1; b <= 7; b++) cp.material.push_back({ b, gfx::BindingKind::StorageReadWrite, false, false, true });
		cp.material.push_back({ 8, gfx::BindingKind::StorageRead, false, false, true });
		cp.drawDataBytes = sizeof(SimData);
		cp.debugName = "Renderer.Fluid";
		pipeline = device->CreatePipeline(cp);
		gfx::BufferDesc b; b.usage = gfx::BufStorage; b.memory = gfx::Memory::GpuOnly;
		b.size = (uint64)kPoolCells * 8; b.debugName = "Renderer.FluidRender";
		pool = device->CreateBuffer(b);
		poolCells = kPoolCells;
	}

	void FluidVolumes::Destroy()
	{
		for (Sim &s : sims) Forget(s);
		sims.clear();
		volumes.clear();
		if (pool) { device->Destroy(pool); pool = gfx::Buffer(); }
	}

	void FluidVolumes::Forget(Sim &s)
	{
		for (gfx::Buffer* b : { &s.vel0, &s.vel1, &s.scal0, &s.scal1, &s.pressure, &s.aux, &s.solid })
			if (*b) { device->Destroy(*b); *b = gfx::Buffer(); }
		if (s.group) { device->Destroy(s.group); s.group = gfx::BindGroup(); }
		s.cloud = -1;
	}

	uint32 FluidVolumes::Allocate(const uint32 cells)
	{
		// (first fit among the grids alive, kept in the order of where they start)
		std::vector<std::pair<uint32, uint32> > used;
		for (const Sim &s : sims) if (s.cloud >= 0 && s.cells) used.push_back(std::make_pair(s.offset, s.cells));
		std::sort(used.begin(), used.end());
		uint32 at = 0;
		for (const std::pair<uint32, uint32> &u : used)
		{
			if (u.first >= at + cells) break;
			at = std::max(at, u.first + u.second);
		}
		return at + cells <= poolCells ? at : 0xFFFFFFFFu;
	}

	// The cloud's obstacles in its grid, from the physics (and again when they change - a window broken: RefreshObstacles)
	void FluidVolumes::LayObstacles(Sim &s, const VolumetricSmoke::Cloud &c)
	{
		const uint32 words = (s.cells + s.nx * s.nz + 31) / 32;     // (a bit a cell, then a bit a column: under a roof)
		// The colliders: each column's rays down (the tops of what is in it, a ray again from just under each - a
		// ray ignores what it starts inside) and up from under the box (their bottoms); solid from each top down to
		// the nearest bottom under it, or to the box's floor
		std::vector<uint32> solid((size_t)words, 0u);
		// (the rays split across the cores - up to a quarter of a million of them for a smoke indoors, 100 ms on one
		// thread: the frame it was thrown in stalled. The physics only reads in a ray, and nothing steps it meanwhile;
		// each bit set with an atomic or, the columns of a row and the rows of a sweep sharing words)
		std::vector<std::atomic<uint32> > bits((size_t)words);
		for (std::atomic<uint32> &b : bits) b.store(0u, std::memory_order_relaxed);
		auto setBit = [&bits](size_t at) { bits[at >> 5].fetch_or(1u << (at & 31), std::memory_order_relaxed); };
		std::atomic<uint32> coverNear(0u), columnsNear(0u);
		if (c.physics)
		{
			const f32 top = s.origin.y + s.ny * s.cell, bottom = s.origin.y;
			JobSystem::Instance().ParallelFor(s.nz, 2, [&](uint32 zBegin, uint32 zEnd)
			{
			std::vector<f32> tops, bottoms;
			for (uint32 z = zBegin; z < zEnd; z++)
				for (uint32 x = 0; x < s.nx; x++)
				{
					const f32 wx = s.origin.x + (x + 0.5f) * s.cell, wz = s.origin.z + (z + 0.5f) * s.cell;
					tops.clear(); bottoms.clear();
					f32 from = top + 0.5f;
					for (int k = 0; k < 8; k++)
					{
						const RayCastHit h = c.physics->RayCast(Vec3(wx, from, wz), Vec3(wx, bottom - 0.5f, wz));
						if (!h.hasHit) break;
						tops.push_back(h.point.y);
						from = h.point.y - 0.01f;
					}
					{
						// (how much of the air round the canister has a roof over it: indoors the smoke is out of the wind and settles)
						const f32 ddx = wx - c.position.x, ddz = wz - c.position.z;
						if (ddx * ddx + ddz * ddz < 9.f)
						{
							columnsNear++;
							for (const f32 tp : tops) if (tp > c.position.y + 1.2f) { coverNear++; break; }
						}
					}
					if (!tops.empty() && tops.size() >= 2)
					{
						// (under a roof: something over the column above its lowest top - the ground or a floor - by more than a storey's
						// least height: the smoke there is indoors)
						const f32 ground = tops.back();
						if (tops.front() > ground + 1.8f) setBit(s.cells + (size_t)x + s.nx * z);
					}
					if (tops.empty()) continue;
					from = bottom - 0.5f;
					for (int k = 0; k < 8; k++)
					{
						const RayCastHit h = c.physics->RayCast(Vec3(wx, from, wz), Vec3(wx, top + 0.5f, wz));
						if (!h.hasHit) break;
						bottoms.push_back(h.point.y);
						from = h.point.y + 0.01f;
					}
					for (size_t ti = 0; ti < tops.size(); ti++)
					{
						const f32 t = tops[ti];
						f32 under = bottom - 1.f;
						bool closed = false;
						// (a closed solid's underside is a bottom below its top with no top between them. A surface that answers
						// rays from both sides - a house's floor or ceiling - is a top and a bottom at the same height; paired with the
						// next bottom down, a ceiling at 3.9 m and the floor under it at 0.9 m made the whole room solid)
						for (const f32 bo : bottoms) if (bo < t - 0.005f && bo > under) { under = bo; closed = true; }
						if (closed)
							for (size_t tj = ti + 1; tj < tops.size(); tj++)
								if (tops[tj] > under + 0.005f && tops[tj] < t - 0.005f) { closed = false; break; }
						bool thin = false;
						for (const f32 bo : bottoms) if (fabsf(bo - t) < 0.005f) thin = true;
						if (thin && ti + 1 < tops.size()) { closed = false; }
						// (a top with no underside found - a one-sided surface: a house's floor or roof, which a ray from below
						// passes - is that surface alone, not solid all the way down: filled to the box's floor, every room
						// under a roof was solid and smoke never went inside. The lowest top, the ground, still fills down.)
						static const bool oldSolid = getenv("PYROS_SMOKE_SOLID_OLD") != nullptr;
						if (!closed && ti + 1 < tops.size() && !oldSolid) under = t - s.cell;
						for (uint32 y = 0; y < s.ny; y++)
						{
							const f32 wy = s.origin.y + (y + 0.5f) * s.cell;
							if (wy <= t && wy >= under) setBit((size_t)x + s.nx * ((size_t)y + s.ny * z));
						}
					}
				}
			});
		}
		// (and the walls: a vertical ray meets a wall only at its top - rays across the box, along x and along z both ways,
		// mark the cells each surface they meet passes through)
		if (c.physics && !getenv("PYROS_SMOKE_SOLID_OLD") && !getenv("PYROS_SMOKE_NO_WALLS"))
		{
			auto mark = [&](const Vec3 &p) {
				const int x = (int)floorf((p.x - s.origin.x) / s.cell), y = (int)floorf((p.y - s.origin.y) / s.cell), z = (int)floorf((p.z - s.origin.z) / s.cell);
				if (x < 0 || y < 0 || z < 0 || x >= (int)s.nx || y >= (int)s.ny || z >= (int)s.nz) return;
				setBit((size_t)x + s.nx * ((size_t)y + s.ny * z));
			};
			auto sweep = [&](const Vec3 &a, const Vec3 &b) {
				const Vec3 dir = (b - a).normalize();
				Vec3 from = a;
				for (int k = 0; k < 12; k++)
				{
					const RayCastHit h = c.physics->RayCast(from, b);
					if (!h.hasHit) break;
					// (two cells thick, one each side of the surface: a wall one cell thin leaked - the solver's interpolation reads
					// across a single solid cell)
					mark(h.point + dir * 0.01f); mark(h.point - dir * (s.cell * 0.9f)); mark(h.point + dir * (s.cell * 0.9f));
					from = h.point + dir * 0.02f;
				}
			};
			const f32 sx = s.nx * s.cell, sz = s.nz * s.cell;
			JobSystem::Instance().ParallelFor(s.ny, 2, [&](uint32 yBegin, uint32 yEnd)
			{
			for (uint32 y = yBegin; y < yEnd; y++)
			{
				const f32 wy = s.origin.y + (y + 0.5f) * s.cell;
				for (uint32 z = 0; z < s.nz; z++)
				{
					const f32 wz = s.origin.z + (z + 0.5f) * s.cell;
					sweep(Vec3(s.origin.x - 0.5f, wy, wz), Vec3(s.origin.x + sx + 0.5f, wy, wz));
					sweep(Vec3(s.origin.x + sx + 0.5f, wy, wz), Vec3(s.origin.x - 0.5f, wy, wz));
				}
				for (uint32 x = 0; x < s.nx; x++)
				{
					const f32 wx = s.origin.x + (x + 0.5f) * s.cell;
					sweep(Vec3(wx, wy, s.origin.z - 0.5f), Vec3(wx, wy, s.origin.z + sz + 0.5f));
					sweep(Vec3(wx, wy, s.origin.z + sz + 0.5f), Vec3(wx, wy, s.origin.z - 0.5f));
				}
			}
			});
		}
		for (size_t w = 0; w < solid.size(); w++) solid[w] = bits[w].load(std::memory_order_relaxed);
		if (getenv("PYROS_SMOKE_SOLID_LOG"))
		{
			size_t n = 0; for (uint32 w : solid) for (uint32 v = w; v; v &= v - 1) n++;
			// (the solid share at each height, bottom to top)
			std::string prof;
			for (uint32 y = 0; y < s.ny; y += 4)
			{
				size_t k = 0;
				for (uint32 z = 0; z < s.nz; z++) for (uint32 x = 0; x < s.nx; x++) { const size_t at = (size_t)x + s.nx * ((size_t)y + s.ny * z); k += (solid[at >> 5] >> (at & 31)) & 1u; }
				char b[16]; snprintf(b, sizeof(b), " %d", (int)(100 * k / ((size_t)s.nx * s.nz))); prof += b;
			}
			fprintf(stderr, "[smoke] origin y %.2f, solid %% by height:%s\n", s.origin.y, prof.c_str());
			fprintf(stderr, "[smoke] grid %ux%ux%u cell %.2f: %zu solid\n", s.nx, s.ny, s.nz, s.cell, n);
		}
		// (what stops it that has no collider - panes: their cells solid)
		for (const std::pair<const int32, std::pair<Vec3, Vec3> > &b : VolumetricSmoke::GetBlocks())
		{
			const Vec3 &mn = b.second.first, &mx = b.second.second;
			if (mx.x < s.origin.x || mx.z < s.origin.z || mx.y < s.origin.y || mn.x > s.origin.x + s.nx * s.cell || mn.z > s.origin.z + s.nz * s.cell || mn.y > s.origin.y + s.ny * s.cell) continue;
			const int x0 = std::max(0, (int)floorf((mn.x - s.origin.x) / s.cell)), x1 = std::min((int)s.nx - 1, (int)floorf((mx.x - s.origin.x) / s.cell));
			const int y0 = std::max(0, (int)floorf((mn.y - s.origin.y) / s.cell)), y1 = std::min((int)s.ny - 1, (int)floorf((mx.y - s.origin.y) / s.cell));
			const int z0 = std::max(0, (int)floorf((mn.z - s.origin.z) / s.cell)), z1 = std::min((int)s.nz - 1, (int)floorf((mx.z - s.origin.z) / s.cell));
			for (int z = z0; z <= z1; z++) for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++)
			{ const size_t at = (size_t)x + s.nx * ((size_t)y + s.ny * z); solid[at >> 5] |= 1u << (at & 31); }
		}
		// (the openings the colliders lack - windows broken out: their cells cleared)
		for (const std::pair<Vec3, Vec3> &o : VolumetricSmoke::GetOpenings())
		{
			const int x0 = std::max(0, (int)floorf((o.first.x - s.origin.x) / s.cell)), x1 = std::min((int)s.nx - 1, (int)floorf((o.second.x - s.origin.x) / s.cell));
			const int y0 = std::max(0, (int)floorf((o.first.y - s.origin.y) / s.cell)), y1 = std::min((int)s.ny - 1, (int)floorf((o.second.y - s.origin.y) / s.cell));
			const int z0 = std::max(0, (int)floorf((o.first.z - s.origin.z) / s.cell)), z1 = std::min((int)s.nz - 1, (int)floorf((o.second.z - s.origin.z) / s.cell));
			for (int z = z0; z <= z1; z++) for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++)
			{ const size_t at = (size_t)x + s.nx * ((size_t)y + s.ny * z); solid[at >> 5] &= ~(1u << (at & 31)); }
		}
		// (PYROS_SMOKE_SLICE=<metres over the canister>: the solid cells at that height, as text - what stops the smoke where)
		if (const char* slices = getenv("PYROS_SMOKE_SLICE"))
		for (const char* sl = slices; sl && *sl; sl = strchr(sl, ',') ? strchr(sl, ',') + 1 : nullptr)
		{
			const int y = (int)floorf((c.position.y + (f32)atof(sl) - s.origin.y) / s.cell);
			if (y >= 0 && y < (int)s.ny)
			{
				fprintf(stderr, "[smoke] slice at %.2f m (x across, z down; # solid, R roofed, * canister):\n", s.origin.y + (y + 0.5f) * s.cell);
				const int cx = (int)((c.position.x - s.origin.x) / s.cell), cz = (int)((c.position.z - s.origin.z) / s.cell);
				for (uint32 z = 0; z < s.nz; z += 2)
				{
					std::string row;
					for (uint32 x = 0; x < s.nx; x += 2)
					{
						const size_t at = (size_t)x + s.nx * ((size_t)y + s.ny * z), col = s.cells + (size_t)x + s.nx * z;
						const bool so = (solid[at >> 5] >> (at & 31)) & 1u, roof = (solid[col >> 5] >> (col & 31)) & 1u;
						row += (abs((int)x - cx) <= 1 && abs((int)z - cz) <= 1) ? '*' : so ? '#' : roof ? 'R' : '.';
					}
					fprintf(stderr, "[smoke] %s\n", row.c_str());
				}
			}
		}
		const uint32 cover = coverNear.load(), columns = columnsNear.load();
		s.enclosed = columns ? (f32)cover / (f32)columns : 0.f;
		if (getenv("PYROS_SMOKE_SOLID_LOG")) fprintf(stderr, "[smoke] enclosed %.2f (%u of %u columns)\n", s.enclosed, cover, columns);
		device->Upload(s.solid, 0, solid.data(), (uint64)words * 4);
		if (!s.pool.empty()) device->Upload(s.solid, (uint64)words * 4, s.pool.data(), (uint64)s.pool.size() * 4);
		s.obstacleEpoch = VolumetricSmoke::GetObstacleEpoch();
	}

	void FluidVolumes::Make(Sim &s, const int32 cloud)
	{
		const VolumetricSmoke::Cloud &c = VolumetricSmoke::GetCloud((uint32)cloud);
		s = Sim();
		s.cloud = cloud;
		s.serial = c.serial;
		s.kind = c.plume ? 3 : c.emission <= 0.f ? 0 : (c.blast > 0.5f ? 2 : 1);
		s.clock = VolumetricSmoke::GetClock() - c.age;
		// The box and its cells, by kind: a grenade's smoke spreads wide and stays lowish; a fire is a tall column
		// (the flames and the smoke going up off them); a blast rolls up into a mushroom
		const f32 R = std::max(c.radius, 0.3f);
		// (under a roof? a few rays up round the canister: indoors the box is made wider - a room is larger than the cloud's
		// own size - and its sides hold the smoke in, the room's walls being what stops it. Its open sides took the smoke out
		// where they crossed the room: the cloud thinned half way, and stood cut off along a straight line)
		bool indoors = false;
		if (c.physics && c.emission <= 0.f && !c.plume)
		{
			int covered = 0, tried = 0;
			for (int k = 0; k < 9; k++)
			{
				const f32 a = (f32)k * 0.698f, r = k == 0 ? 0.f : 2.f;
				const Vec3 at(c.position.x + cosf(a) * r, c.position.y + 0.3f, c.position.z + sinf(a) * r);
				tried++;
				if (c.physics->RayCast(at, at + Vec3(0.f, 12.f, 0.f)).hasHit) covered++;
			}
			indoors = covered * 3 >= tried * 2;
		}
		f32 wide, tall, below; uint32 across, upMax;
		if (s.kind == 0) { wide = 2.6f * R + 2.f; tall = 1.3f * R + 2.f; below = 0.6f; across = 96; upMax = 56; }
		else if (s.kind == 1) { wide = 2.2f * R + 1.f; tall = 3.5f * R + 2.5f + 1.2f; below = 1.5f; across = 48; upMax = 112; }     // (below: a bottle broken on a wall a metre up - its fuel runs down to the floor)
		else if (s.kind == 3) { wide = 7.f; tall = 22.f; below = 0.4f; across = 40; upMax = 128; }     // (a plume: a narrow source, a tall thin column - room either side for the wind to lean it)
		else { wide = 4.5f * R + 2.f; tall = 6.f * R + 3.f; below = 0.6f; across = 64; upMax = 80; }
		// (indoors a storey down and up as well: thrown on a stair, the smoke stopped 0.6 m under the canister and under a
		// lid 5 m over it - "an invisible wall" on the stairs - and a room further off than the box's side never got any)
		if (s.kind == 0 && indoors) { wide *= 2.0f; across = 128; below = 3.2f; tall += 3.2f + 2.f; upMax = 80; }
		s.closedSides = s.kind == 0 && indoors;
		s.cell = wide / (f32)across;
		s.nx = s.nz = across;
		s.ny = std::min(upMax, (uint32)ceilf(tall / s.cell));
		s.cells = s.nx * s.ny * s.nz;
		s.origin = Vec3(c.position.x - wide * 0.5f, c.position.y - below, c.position.z - wide * 0.5f);
		s.offset = Allocate(s.cells);
		if (s.offset == 0xFFFFFFFFu) { s.cloud = -1; return; }

		gfx::BufferDesc b; b.usage = gfx::BufStorage; b.memory = gfx::Memory::GpuOnly;
		b.size = (uint64)s.cells * 8;     // (four half floats a cell)
		b.debugName = "Renderer.FluidVelocity"; s.vel0 = device->CreateBuffer(b); s.vel1 = device->CreateBuffer(b);
		b.debugName = "Renderer.FluidScalars"; s.scal0 = device->CreateBuffer(b); s.scal1 = device->CreateBuffer(b);
		b.debugName = "Renderer.FluidAux"; s.aux = device->CreateBuffer(b);
		b.debugName = "Renderer.FluidPressure"; s.pressure = device->CreateBuffer(b);     // (two floats a cell: the pair)
		const uint32 words = (s.cells + s.nx * s.nz + 31) / 32;     // (a bit a cell, then a bit a column: under a roof)
		if (s.kind == 1) SpillFuel(s, c);     // (a fire's fuel runs over what it lands on, as a liquid: after the bits)
		b.size = (uint64)(words + s.pool.size()) * 4; b.debugName = "Renderer.FluidSolid"; s.solid = device->CreateBuffer(b);

		{
			const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
			LayObstacles(s, c);
			if (getenv("PYROS_SMOKE_SOLID_LOG")) fprintf(stderr, "[smoke] obstacles laid in %.1f ms (%ux%ux%u)\n", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(), s.nx, s.ny, s.nz);
		}

		gfx::Device::BindGroupEntry e[8];
		const gfx::Buffer bufs[6] = { s.vel0, s.vel1, s.scal0, s.scal1, s.pressure, s.aux };
		for (uint32 k = 0; k < 6; k++) { e[k].binding = k + 1; e[k].buffer = bufs[k]; e[k].size = (uint64)s.cells * 8; }
		e[6].binding = 7; e[6].buffer = pool; e[6].size = (uint64)poolCells * 8;
		e[7].binding = 8; e[7].buffer = s.solid; e[7].size = (uint64)words * 4;
		s.group = device->CreateBindGroup(pipeline, e, 8);
		s.fresh = true;
	}

	void FluidVolumes::Prepare()
	{
		frame++;
		volumes.clear();
		if (!device || !device->PipelineReady(pipeline)) return;
		// (a grid whose cloud is gone, or whose slot is another cloud's now, let go)
		for (Sim &s : sims)
		{
			if (s.cloud < 0) continue;
			const VolumetricSmoke::Cloud &c = VolumetricSmoke::GetCloud((uint32)s.cloud);
			if (!c.active || c.serial != s.serial) { if (getenv("PYROS_SMOKE_SOLID_LOG")) fprintf(stderr, "[smoke] forget cloud %d (active %d serial %u/%u age %.1f)\n", s.cloud, (int)c.active, c.serial, s.serial, c.age); Forget(s); }
		}
		sims.erase(std::remove_if(sims.begin(), sims.end(), [](const Sim &s) { return s.cloud < 0; }), sims.end());
		// (the clouds without one, given one, while there is room: the youngest first - what was just thrown)
		std::vector<std::pair<f32, int32> > want;
		for (uint32 i = 0; i < VolumetricSmoke::MaxClouds; i++)
		{
			const VolumetricSmoke::Cloud &c = VolumetricSmoke::GetCloud(i);
			if (!c.active) continue;
			bool has = false;
			for (const Sim &s : sims) if (s.cloud == (int32)i) has = true;
			if (!has) want.push_back(std::make_pair(c.age, (int32)i));
		}
		std::sort(want.begin(), want.end());
		for (const std::pair<f32, int32> &w : want)
		{
			if (sims.size() >= MaxVolumes) break;
			Sim s;
			Make(s, w.second);
			if (getenv("PYROS_SMOKE_SOLID_LOG")) fprintf(stderr, "[smoke] make cloud %d age %.1f\n", w.second, w.first);
			if (s.cloud >= 0) sims.push_back(s);
		}
		// (their obstacles laid again where they changed - a pane broken out, a door opened: the smoke goes through)
		// (not in the frame it changed: a door's new collider is put in, and its old one taken out, by the script that asks
		// for this - the rays then still met the shut door, the epoch counted as laid, and an opened door held the smoke in for
		// good. Six frames on, the physics has both)
		for (Sim &s : sims)
		{
			if (s.obstacleEpoch == VolumetricSmoke::GetObstacleEpoch()) { s.epochSeenFrame = 0; continue; }
			if (s.epochSeenFrame == 0) s.epochSeenFrame = (uint32)frame;
			if ((uint32)frame - s.epochSeenFrame >= 6) { LayObstacles(s, VolumetricSmoke::GetCloud((uint32)s.cloud)); s.epochSeenFrame = 0; }
		}
		// What each is stepped by: the clouds' clock since it was last, in steps of kMaxStep at most (a frame that
		// would take more is let go: the smoke slows rather than the frame)
		const f64 now = VolumetricSmoke::GetClock();
		for (Sim &s : sims)
		{
			// (not cleared: several renderers share these grids, and a second Prepare in a frame comes before the first's
			// Record may have run - Record clears what it stepped)
			if (now - s.clock > kStep * kMaxStepsAFrame) s.clock = now - kStep * kMaxStepsAFrame;
			while (now - s.clock >= kStep && s.pendingSteps.size() < kMaxStepsAFrame) { s.pendingSteps.push_back(kStep); s.clock += kStep; }
			const VolumetricSmoke::Cloud &c = VolumetricSmoke::GetCloud((uint32)s.cloud);
			Volume v;
			v.origin = s.origin; v.cell = s.cell;
			v.nx = s.nx; v.ny = s.ny; v.nz = s.nz; v.offset = s.offset;
			v.color = c.color; v.emission = c.emission;
			const f32 left = c.lifeTime - c.age;
			v.fade = std::min(std::max(left / std::max(c.fadeTime, 0.01f), 0.f), 1.f);
			v.thickness = c.thickness;
			v.kind = s.kind == 3 ? 0.f : (f32)s.kind; v.pad = s.closedSides ? 1.f : 0.f;     // (pad: the box's sides closed - no rounding off at its edge, smoke.frag)
			volumes.push_back(v);
		}
	}

	namespace
	{
		// (a smooth noise over the ground, 0..1 - two octaves of value noise, a metre and a third of one - for SpillFuel's lobes)
		f32 Hash2(const int x, const int z, const f32 seed)
		{
			uint32 n = (uint32)x * 1597334673u ^ (uint32)z * 3812015801u ^ (uint32)(seed * 1000.f) * 2798796415u;
			n ^= n >> 15; n *= 2246822519u; n ^= n >> 13; n *= 3266489917u; n ^= n >> 16;
			return (f32)(n >> 8) * (1.f / 16777216.f);
		}
		f32 Value2(const f32 x, const f32 z, const f32 seed)
		{
			const int ix = (int)floorf(x), iz = (int)floorf(z);
			f32 fx = x - ix, fz = z - iz;
			fx = fx * fx * (3.f - 2.f * fx); fz = fz * fz * (3.f - 2.f * fz);
			const f32 a = Hash2(ix, iz, seed), b = Hash2(ix + 1, iz, seed), cc = Hash2(ix, iz + 1, seed), d = Hash2(ix + 1, iz + 1, seed);
			return (a + (b - a) * fx) + ((cc + (d - cc) * fx) - (a + (b - a) * fx)) * fz;
		}
		f32 Lumps(const f32 x, const f32 z, const f32 seed) { return Value2(x * 1.1f, z * 1.1f, seed) * 0.7f + Value2(x * 3.3f, z * 3.3f, seed + 7.f) * 0.3f; }
	}

	// A fire's fuel, spilled: it runs over what it lands on as a liquid would - from where the bottle broke, the lowest
	// ground next to what it already covers first (so it runs downhill and pools in a hollow, spreading evenly over a flat
	// floor), climbing no step higher than a few centimetres and nothing above where it started (a wall stops it, a
	// kerb too), down a step or a stair as far as it goes - until it covers the area the fuel would: the fire's own. Each
	// column it reaches gets when it reaches it, the spill running out over a second; the fire burns there, a layer over it.
	void FluidVolumes::SpillFuel(Sim &s, const VolumetricSmoke::Cloud &c)
	{
		s.pool.clear();
		if (!c.physics) return;
		const uint32 n = s.nx * s.nz;
		const f32 none = -1.0e9f;
		std::vector<f32> ground((size_t)n, none);
		const f32 from = c.position.y + 0.25f, to = s.origin.y + 0.5f * s.cell;
		JobSystem::Instance().ParallelFor(s.nz, 4, [&](uint32 zBegin, uint32 zEnd)
		{
			for (uint32 z = zBegin; z < zEnd; z++)
				for (uint32 x = 0; x < s.nx; x++)
				{
					const f32 wx = s.origin.x + (x + 0.5f) * s.cell, wz = s.origin.z + (z + 0.5f) * s.cell;
					const RayCastHit h = c.physics->RayCast(Vec3(wx, from, wz), Vec3(wx, to, wz));
					if (h.hasHit && h.normal.y > 0.5f) ground[(size_t)x + s.nx * z] = h.point.y;     // (a floor, not a wall's side)
				}
		});
		// (where it starts: the column under where it broke, or the nearest with ground under it)
		const int cx = std::min(std::max((int)((c.position.x - s.origin.x) / s.cell), 0), (int)s.nx - 1);
		const int cz = std::min(std::max((int)((c.position.z - s.origin.z) / s.cell), 0), (int)s.nz - 1);
		int start = -1; f32 best = 1e9f;
		// (and in sight of where it broke: the column under a bottle broken against a wall can be the wall's, or the floor
		// behind it - the spill then began on the far side)
		for (int dz = -4; dz <= 4; dz++) for (int dx = -4; dx <= 4; dx++)
		{
			const int x = cx + dx, z = cz + dz;
			if (x < 0 || z < 0 || x >= (int)s.nx || z >= (int)s.nz || ground[(size_t)x + s.nx * z] <= none) continue;
			const f32 d = (f32)(dx * dx + dz * dz);
			if (d >= best) continue;
			const Vec3 there(s.origin.x + (x + 0.5f) * s.cell, ground[(size_t)x + s.nx * z] + 0.08f, s.origin.z + (z + 0.5f) * s.cell);
			if (c.physics->RayCast(c.position, there).hasHit || c.physics->RayCast(there, c.position).hasHit) continue;
			best = d; start = x + (int)s.nx * z;
		}
		if (start < 0) return;
		const f32 R = std::max(c.radius, 0.3f);
		const uint32 target = std::min(n, (uint32)(3.14159f * R * R * 0.8f / (s.cell * s.cell)));
		const f32 startH = ground[(size_t)start];
		typedef std::pair<f32, int> Entry;
		std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry> > q;
		std::vector<uint8> seen((size_t)n, 0);
		std::vector<int> order;
		order.reserve(target);
		q.push(Entry(startH, start)); seen[(size_t)start] = 1;
		const int sx = start % (int)s.nx, sz = start / (int)s.nx;
		while (!q.empty() && order.size() < target)
		{
			const int i = q.top().second; q.pop();
			order.push_back(i);
			const int x = i % (int)s.nx, z = i / (int)s.nx;
			const f32 hi = ground[(size_t)i];
			const int nb[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
			for (const auto &o : nb)
			{
				const int jx = x + o[0], jz = z + o[1];
				if (jx < 0 || jz < 0 || jx >= (int)s.nx || jz >= (int)s.nz) continue;
				const int j = jx + (int)s.nx * jz;
				if (seen[(size_t)j]) continue;
				const f32 hj = ground[(size_t)j];
				if (hj <= none || hj > hi + 0.07f || hj > startH + 0.1f) continue;     // (no ground; a step up; above where it began)
				// (and nothing standing between the two, just over the ground: a column's ray down starts inside a wall taller
				// than where the bottle broke and goes through it - a ray ignores what it starts in - finding the floor beyond,
				// and the fuel ran under the wall into the next room)
				{
					const f32 y = std::max(hi, hj) + 0.06f;
					const Vec3 a(s.origin.x + (x + 0.5f) * s.cell, y, s.origin.z + (z + 0.5f) * s.cell);
					const Vec3 b(s.origin.x + (jx + 0.5f) * s.cell, y, s.origin.z + (jz + 0.5f) * s.cell);
					if (c.physics->RayCast(a, b).hasHit || c.physics->RayCast(b, a).hasHit) continue;
				}
				seen[(size_t)j] = 1;
				// (lowest first - downhill, into a hollow; on the flat it would take the nearest and spread as a disc - a spill
				// runs out in lobes and fingers, so on top of the distance a lumpy noise fixed in the world: where it is low the
				// fuel runs on, where high it holds back)
				const f32 dist = sqrtf((f32)((jx - sx) * (jx - sx) + (jz - sz) * (jz - sz))) * s.cell;
				const f32 wx = s.origin.x + (jx + 0.5f) * s.cell, wz = s.origin.z + (jz + 0.5f) * s.cell;
				q.push(Entry(hj + dist * 0.03f + Lumps(wx, wz, (f32)(c.serial % 61)) * 0.16f, j));
			}
		}
		uint32 noneBits; memcpy(&noneBits, &none, 4);
		s.pool.assign((size_t)n * 2, noneBits);
		const f32 spread = 1.1f;
		for (size_t k = 0; k < order.size(); k++)
		{
			const f32 top = ground[(size_t)order[k]], at = spread * (f32)k / (f32)std::max<size_t>(order.size(), 1);
			memcpy(&s.pool[(size_t)order[k] * 2], &top, 4);
			memcpy(&s.pool[(size_t)order[k] * 2 + 1], &at, 4);
		}
		if (getenv("PYROS_SMOKE_SOLID_LOG")) fprintf(stderr, "[smoke] fuel spilled over %zu columns of %u (wanted %u)\n", order.size(), n, target);
	}

	void FluidVolumes::Dispatch(gfx::CommandList* cl, Sim &s, const int pass, const f32 dt, const Vec3 &sunDir)
	{
		const VolumetricSmoke::Cloud &c = VolumetricSmoke::GetCloud((uint32)s.cloud);
		SimData d;
		memset(&d, 0, sizeof(d));
		d.dims[0] = (f32)s.nx; d.dims[1] = (f32)s.ny; d.dims[2] = (f32)s.nz; d.dims[3] = (f32)s.cells;
		d.grid[0] = s.origin.x; d.grid[1] = s.origin.y; d.grid[2] = s.origin.z; d.grid[3] = s.cell;
		d.step[0] = dt; d.step[1] = (f32)pass; d.step[2] = (f32)s.lightSlice; d.step[3] = s.age;
		d.flags[3] = (f32)s.lightAxis;
		d.flags[0] = s.scalCur ? 1.f : 0.f; d.flags[1] = s.velCur ? 1.f : 0.f; d.flags[2] = s.presCur ? 1.f : 0.f;
		const Vec3 &wind = VolumetricSmoke::GetWind();
		const f32 sheltered = s.closedSides ? 1.f - 0.95f * std::min(std::max(s.enclosed, 0.f), 1.f) : 1.f;     // (under a roof, out of the wind - outside, the wind as it always was)
		d.extra[0] = s.closedSides ? 1.f : 0.f;
		d.extra[2] = s.closedSides ? 0.12f : 0.f;     // (out of the house - by a door or a window - the open air thins it faster than the room;
		                                              // at 0.55 a second it was gone at the doorstep, and the smoke seemed never to leave the house)
		d.extra[3] = s.closedSides ? std::min(std::max(s.enclosed, 0.f), 1.f) : 0.f;     // (what the wind was cut by, given back outside)
		d.wind[0] = wind.x * sheltered; d.wind[1] = wind.y * sheltered; d.wind[2] = wind.z * sheltered; d.wind[3] = (f32)s.offset;
		d.sun[0] = sunDir.x; d.sun[1] = sunDir.y; d.sun[2] = sunDir.z;
		const f32 R = std::max(c.radius, 0.3f);
		const f32 left = c.lifeTime - c.age;
		d.emit[2] = std::min(std::max(left / std::max(c.fadeTime, 0.01f), 0.f), 1.f);
		d.emit[3] = (f32)(c.serial % 97) * 3.17f;
		if (s.kind == 0)
		{
			// A grenade: the canister on the ground spewing smoke sideways as it spins, for most of its growth; the
			// smoke slightly warm, so it rolls up and over before it settles
			const f32 spin = s.age * 2.3f + (f32)c.serial;
			const f32 spew = std::max(c.growTime * 2.2f, 1.f);
			d.source[0] = c.position.x; d.source[1] = c.position.y + 0.15f; d.source[2] = c.position.z; d.source[3] = 0.18f * R + 0.4f;
			d.inject[0] = 70.f * c.thickness; d.inject[1] = 3.5f; d.inject[2] = 0.f; d.inject[3] = 4.f + 0.8f * R;
			// (out of the canister all round, not one spinning jet: the jet swung the whole cloud from side to side - it grew,
			// went off one way, came back - where smoke should only ever spread. PYROS_SMOKE_SPIN brings the spin back)
			// (outside too, now: the spinning jet at 4 m/s and more, stirred hard, threw the smoke about - a churning cloud where
			// smoke should only spread. All round and slow; PYROS_SMOKE_SPIN brings the spinning jet back)
			static const bool spinWanted = getenv("PYROS_SMOKE_SPIN") != nullptr;
			const bool spinJet = spinWanted && !s.closedSides;
			const Vec3 jet = Vec3(cosf(spin), 0.35f, sinf(spin)).normalize();
			d.jet[0] = jet.x; d.jet[1] = jet.y; d.jet[2] = jet.z; d.jet[3] = spinJet ? 0.f : 1.f;
			if (!spinJet) d.inject[3] = 1.2f + 0.25f * R;     // (gentle, all round: pushed out at a walk, it spreads instead of being thrown)
			if (s.closedSides) d.inject[0] *= 0.45f;
			if (s.closedSides) d.inject[3] = 0.9f;     // (and pushed out gently: the canister's push carried the room's smoke out of every opening at once)     // (indoors none of it leaves by the box's sides: less poured in, the room fills about as a grenade's cloud is outside)
			d.forces[0] = 1.2f; d.forces[1] = 0.08f; d.forces[2] = 3.f; d.forces[3] = 0.6f;
			d.decay[0] = 0.015f; d.decay[1] = 0.f; d.decay[2] = 0.f; d.decay[3] = 0.f;
			d.emit[0] = s.age < spew ? 1.f : 0.f; d.emit[1] = 1.8f / std::max(R * 0.12f, 0.2f);
			d.sun[3] = 3.5f;
			d.turb[0] = 7.f; d.turb[1] = 0.9f; d.turb[2] = 0.45f; d.turb[3] = 18.f;
			if (!spinJet && !s.closedSides)
			{
				// (outside, calm: little lift - it lies low and spreads - a weak swirl kept from feeding itself, a slow stir in
				// big eddies changing slowly, and nothing in it faster than a brisk walk)
				d.forces[0] = 0.45f; d.forces[2] = 0.8f;
				d.turb[0] = 1.4f; d.turb[1] = 0.55f; d.turb[2] = 0.12f; d.turb[3] = 3.f;
				// (and once poured it settles: the air's drag takes what motion is left, the wind then drifting it as a whole)
				const f32 still = std::min(std::max((s.age - spew) / 5.f, 0.f), 1.f);
				if (still > 0.f) { d.jet[3] = -0.5f * still; d.turb[0] *= 1.f - 0.6f * still; }
			}
			// (once the canister has stopped and the cloud is at its size it settles: its stirring and swirl die away over a few
			// seconds - indoors almost to nothing, still air; outside some is kept, the wind's gusts)
			const f32 settle = s.closedSides ? std::min(std::max((s.age - spew) / 4.f, 0.f), 1.f) : 0.f;     // (indoors only: outside the cloud goes on as it always did)
			const f32 keep = 1.f - settle * (0.55f + 0.4f * std::min(std::max(s.enclosed, 0.f), 1.f));
			d.turb[0] *= keep; d.forces[2] *= keep; d.forces[0] *= (1.f - 0.6f * settle);
			if (s.closedSides) d.extra[1] = settle * 0.035f;
			// (indoors the air is slow: what leaves by a window broken out is a leak, drifting out and up - not the room emptying
			// itself into the street. The speed limit indoors is 4 m/s while the canister pours, down to
			// slow 1.6 m/s; the room's smoke was otherwise held under pressure by its walls and burst out of the first hole)
			if (s.closedSides) d.turb[3] = 2.2f * (1.f - settle) + 1.2f * settle;     // (indoors, once settled, the room's smoke thins slowly: a grey wall otherwise)
			if (settle > 0.f) d.jet[3] = -settle * (0.35f + 1.4f * std::min(std::max(s.enclosed, 0.f), 1.f));     // (drag once the canister is done - its jet is off then: indoors the air goes still)
		}
		else if (s.kind == 1)
		{
			// A fire: a bed of fuel on the ground, given out unevenly (the noise boiling up through it), lit by its own
			// heat; what burns lifts hard and makes soot that goes up off the flames
			d.source[0] = c.position.x; d.source[1] = c.position.y + 0.1f; d.source[2] = c.position.z; d.source[3] = 0.6f * R;
			d.inject[0] = 0.f; d.inject[1] = 1.5f; d.inject[2] = 5.f * c.emission; d.inject[3] = 1.f;
			d.jet[0] = 0.f; d.jet[1] = 1.f; d.jet[2] = 0.f; d.jet[3] = 0.f;
			d.forces[0] = 4.5f; d.forces[1] = 0.05f; d.forces[2] = 6.f; d.forces[3] = 2.6f;
			d.decay[0] = 0.3f; d.decay[1] = 5.f; d.decay[2] = 0.16f; d.decay[3] = 1.9f;
			d.emit[0] = left > c.fadeTime * 0.5f ? 1.f : 0.f; d.emit[1] = 4.5f;
			d.sun[3] = 6.f;
			d.turb[0] = 10.f; d.turb[1] = 2.6f; d.turb[2] = 2.2f; d.turb[3] = 9.f;
			// (its fuel spilled - SpillFuel: it burns where the liquid ran, a layer over it, not a ball round where it broke;
			// lifted by its own heat alone, no jet)
			if (!s.pool.empty())
			{
				d.pool[0] = 1.f; d.pool[1] = 0.22f; d.pool[3] = (f32)((s.cells + s.nx * s.nz + 31) / 32);
				d.inject[3] = 0.f;
				// (a burning spill is a low carpet of flames over the liquid, licking up here and there - not one tall fire: less
				// fuel fed at once, lifted less; the soot still goes up off it)
				d.inject[2] = 3.f * c.emission; d.forces[0] = 2.8f; d.turb[3] = 6.f;
			}
		}
		else if (s.kind == 3)
		{
			// A plume: a small source pouring a little warm smoke straight up, gently; it lifts on its warmth, cools, thins as
			// it goes and leans with the wind - narrow where it leaves the source, opening out as it climbs, a chimney's
			d.source[0] = c.position.x; d.source[1] = c.position.y + 0.05f; d.source[2] = c.position.z; d.source[3] = std::max(c.radius, 1.6f * s.cell);     // (at least a cell and a half: smaller, it fell between the cells and fed next to nothing in)
			d.inject[0] = 18.f * c.thickness; d.inject[1] = 1.4f; d.inject[2] = 0.f; d.inject[3] = 2.2f;
			d.jet[0] = 0.f; d.jet[1] = 1.f; d.jet[2] = 0.f; d.jet[3] = 0.f;
			d.forces[0] = 1.6f; d.forces[1] = 0.03f; d.forces[2] = 1.2f; d.forces[3] = 0.35f;
			d.decay[0] = 0.13f; d.decay[1] = 0.f; d.decay[2] = 0.f; d.decay[3] = 0.f;
			d.emit[0] = left > c.fadeTime * 0.5f ? 1.f : 0.f; d.emit[1] = 3.f;
			d.sun[3] = 3.5f;
			d.pool[2] = 7.f;     // (its stirring grows over its first 7 m up, and it thins as it climbs: fluid.comp)
			d.turb[0] = 4.f; d.turb[1] = 0.38f; d.turb[2] = 0.3f; d.turb[3] = 6.f;     // (big slow eddies: it opens out as it climbs, a funnel upside down)
		}
		else
		{
			// A blast: fuel, heat and a little smoke thrown out from its heart all at once, burning in a ball that
			// rolls up as it rises, leaving soot
			d.source[0] = c.position.x; d.source[1] = c.position.y + 0.3f * R; d.source[2] = c.position.z; d.source[3] = 0.55f * R;
			d.inject[0] = 4.f; d.inject[1] = 40.f; d.inject[2] = 60.f * c.emission; d.inject[3] = 7.f + 2.5f * R;
			d.jet[0] = 0.f; d.jet[1] = 1.f; d.jet[2] = 0.f; d.jet[3] = 1.f;
			d.forces[0] = 5.f; d.forces[1] = 0.08f; d.forces[2] = 7.f; d.forces[3] = 1.3f;
			d.decay[0] = 0.08f; d.decay[1] = 7.f; d.decay[2] = 0.45f; d.decay[3] = 1.4f;
			d.emit[0] = s.age < 0.14f ? 1.f : 0.f; d.emit[1] = 1.4f / std::max(R * 0.3f, 0.2f);
			d.sun[3] = 4.f;
			d.turb[0] = 18.f; d.turb[1] = 0.9f; d.turb[2] = 1.2f; d.turb[3] = 30.f;
		}
		cl->DrawData(&d, sizeof(d));
		if (pass == 11)
		{
			// (a slice: the two axes across the light's)
			const uint32 a = s.lightAxis == 0 ? s.ny : s.nx, b = s.lightAxis == 2 ? s.ny : s.nz;
			cl->Dispatch(Groups(a), Groups(b), 1);
		}
		else
			cl->Dispatch(Groups(s.nx), Groups(s.ny), Groups(s.nz));
		// (each pass reads what the one before wrote)
		gfx::Barrier b[7];
		const gfx::Buffer bufs[7] = { s.vel0, s.vel1, s.scal0, s.scal1, s.pressure, s.aux, pool };     // (the solid bits are only read)
		for (uint32 k = 0; k < 7; k++) { b[k].buffer = bufs[k]; b[k].from = gfx::Access::ShaderWrite; b[k].to = gfx::Access::ShaderWrite; }
		cl->Barriers(b, 7);
	}

	void FluidVolumes::Step(gfx::CommandList* cl, Sim &s, const f32 dt)
	{
		Dispatch(cl, s, 9, dt, sunNow);                              // (smoke, heat, fuel carried: a guess)
		Dispatch(cl, s, 1, dt, sunNow); s.scalCur = !s.scalCur;      // (... corrected, burnt, added)
		Dispatch(cl, s, 10, dt, sunNow);                             // (the velocity carried along itself: a guess)
		Dispatch(cl, s, 2, dt, sunNow); s.velCur = !s.velCur;        // (... corrected)
		Dispatch(cl, s, 3, dt, sunNow);                              // (its curl)
		Dispatch(cl, s, 4, dt, sunNow); s.velCur = !s.velCur;        // (buoyancy, confinement, wind, the jet)
		Dispatch(cl, s, 5, dt, sunNow);                              // (what flows out of each cell)
		for (uint32 k = 0; k < kPressureIterations; k++) { Dispatch(cl, s, 6, dt, sunNow); s.presCur = !s.presCur; }
		Dispatch(cl, s, 7, dt, sunNow); s.velCur = !s.velCur;        // (the pressure's push taken off)
		s.age += dt;
	}

	void FluidVolumes::Record(gfx::CommandList* cl, const Vec3 &sunDir)
	{
		if (sims.empty() || !device->PipelineReady(pipeline)) return;
		sunNow = sunDir;
		cl->SetPipeline(pipeline);
		for (Sim &s : sims)
		{
			cl->SetBindGroup(s.group, nullptr, 0);
			if (s.fresh) { Dispatch(cl, s, 0, 0.f, sunDir); s.fresh = false; s.scalCur = s.velCur = s.presCur = false; }
			// (its light baked again only where it moved: the sun is still for a step's time)
			const bool moved = !s.pendingSteps.empty() || s.fresh;
			for (const f32 dt : s.pendingSteps) Step(cl, s, dt);
			s.pendingSteps.clear();
			if (moved)
			{
				// The light, a slice at a time from the sun's side along its main axis, then the cells packed
				const f32 ax = fabsf(sunDir.x), ay = fabsf(sunDir.y), az = fabsf(sunDir.z);
				s.lightAxis = ax >= ay && ax >= az ? 0 : (ay >= az ? 1 : 2);
				const f32 toward = s.lightAxis == 0 ? sunDir.x : (s.lightAxis == 1 ? sunDir.y : sunDir.z);
				const uint32 slices = s.lightAxis == 0 ? s.nx : (s.lightAxis == 1 ? s.ny : s.nz);
				for (uint32 k = 0; k < slices; k++)
				{
					s.lightSlice = toward > 0.f ? slices - 1 - k : k;
					Dispatch(cl, s, 11, 0.f, sunDir);
				}
				Dispatch(cl, s, 8, 0.f, sunDir);
			}
		}
		gfx::Barrier after; after.buffer = pool; after.from = gfx::Access::ShaderWrite; after.to = gfx::Access::ShaderRead;
		cl->Barriers(&after, 1);
	}

}
