//============================================================================
// Name        : FluidVolumes.cpp
// Description : Smoke and fire simulated as a fluid on the GPU
//============================================================================

#include <Pyros3D/Rendering/Renderer/FluidVolumes.h>
#include <Pyros3D/Rendering/Volumetrics/VolumetricSmoke.h>
#include <Pyros3D/Physics/PhysicsEngines/IPhysics.h>
#include <algorithm>
#include <cmath>
#include <cstring>

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
			f32 dims[4], grid[4], step[4], flags[4], source[4], inject[4], jet[4], forces[4], decay[4], wind[4], sun[4], emit[4], turb[4];
		};
		static_assert(sizeof(SimData) == 208, "fluid.comp's DrawData");

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

	void FluidVolumes::Make(Sim &s, const int32 cloud)
	{
		const VolumetricSmoke::Cloud &c = VolumetricSmoke::GetCloud((uint32)cloud);
		s = Sim();
		s.cloud = cloud;
		s.serial = c.serial;
		s.kind = c.emission <= 0.f ? 0 : (c.blast > 0.5f ? 2 : 1);
		s.clock = VolumetricSmoke::GetClock() - c.age;
		// The box and its cells, by kind: a grenade's smoke spreads wide and stays lowish; a fire is a tall column
		// (the flames and the smoke going up off them); a blast rolls up into a mushroom
		const f32 R = std::max(c.radius, 0.3f);
		f32 wide, tall, below; uint32 across, upMax;
		if (s.kind == 0) { wide = 2.6f * R + 2.f; tall = 1.3f * R + 2.f; below = 0.6f; across = 96; upMax = 56; }
		else if (s.kind == 1) { wide = 2.2f * R + 1.f; tall = 3.5f * R + 2.5f; below = 0.3f; across = 48; upMax = 112; }
		else { wide = 4.5f * R + 2.f; tall = 6.f * R + 3.f; below = 0.6f; across = 64; upMax = 80; }
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
		const uint32 words = (s.cells + 31) / 32;
		b.size = (uint64)words * 4; b.debugName = "Renderer.FluidSolid"; s.solid = device->CreateBuffer(b);

		// The colliders: each column's rays down (the tops of what is in it, a ray again from just under each - a
		// ray ignores what it starts inside) and up from under the box (their bottoms); solid from each top down to
		// the nearest bottom under it, or to the box's floor
		std::vector<uint32> solid((size_t)words, 0u);
		if (c.physics)
		{
			const f32 top = s.origin.y + s.ny * s.cell, bottom = s.origin.y;
			std::vector<f32> tops, bottoms;
			for (uint32 z = 0; z < s.nz; z++)
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
						for (const f32 bo : bottoms) if (bo < t - 0.005f && bo > under) { under = bo; closed = true; }
						// (a top with no underside found - a one-sided surface: a house's floor or roof, which a ray from below
						// passes - is that surface alone, not solid all the way down: filled to the box's floor, every room
						// under a roof was solid and smoke never went inside. The lowest top, the ground, still fills down.)
						if (!closed && ti + 1 < tops.size()) under = t - s.cell;
						for (uint32 y = 0; y < s.ny; y++)
						{
							const f32 wy = s.origin.y + (y + 0.5f) * s.cell;
							if (wy <= t && wy >= under) { const size_t at = (size_t)x + s.nx * ((size_t)y + s.ny * z); solid[at >> 5] |= 1u << (at & 31); }
						}
					}
				}
		}
		// (and the walls: a vertical ray meets a wall only at its top - rays across the box, along x and along z both ways,
		// mark the cells each surface they meet passes through)
		if (c.physics)
		{
			auto mark = [&](const Vec3 &p) {
				const int x = (int)floorf((p.x - s.origin.x) / s.cell), y = (int)floorf((p.y - s.origin.y) / s.cell), z = (int)floorf((p.z - s.origin.z) / s.cell);
				if (x < 0 || y < 0 || z < 0 || x >= (int)s.nx || y >= (int)s.ny || z >= (int)s.nz) return;
				const size_t at = (size_t)x + s.nx * ((size_t)y + s.ny * z); solid[at >> 5] |= 1u << (at & 31);
			};
			auto sweep = [&](const Vec3 &a, const Vec3 &b) {
				const Vec3 dir = (b - a).normalize();
				Vec3 from = a;
				for (int k = 0; k < 12; k++)
				{
					const RayCastHit h = c.physics->RayCast(from, b);
					if (!h.hasHit) break;
					mark(h.point + dir * 0.01f);
					from = h.point + dir * 0.02f;
				}
			};
			const f32 sx = s.nx * s.cell, sz = s.nz * s.cell;
			for (uint32 y = 0; y < s.ny; y++)
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
		}
		device->Upload(s.solid, 0, solid.data(), (uint64)words * 4);

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
			if (!c.active || c.serial != s.serial) Forget(s);
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
			if (s.cloud >= 0) sims.push_back(s);
		}
		// What each is stepped by: the clouds' clock since it was last, in steps of kMaxStep at most (a frame that
		// would take more is let go: the smoke slows rather than the frame)
		const f64 now = VolumetricSmoke::GetClock();
		for (Sim &s : sims)
		{
			s.pendingSteps.clear();
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
			v.kind = (f32)s.kind; v.pad = 0.f;
			volumes.push_back(v);
		}
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
		d.wind[0] = wind.x; d.wind[1] = wind.y; d.wind[2] = wind.z; d.wind[3] = (f32)s.offset;
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
			const Vec3 jet = Vec3(cosf(spin), 0.35f, sinf(spin)).normalize();
			d.jet[0] = jet.x; d.jet[1] = jet.y; d.jet[2] = jet.z; d.jet[3] = 0.f;
			d.forces[0] = 1.2f; d.forces[1] = 0.08f; d.forces[2] = 3.f; d.forces[3] = 0.6f;
			d.decay[0] = 0.015f; d.decay[1] = 0.f; d.decay[2] = 0.f; d.decay[3] = 0.f;
			d.emit[0] = s.age < spew ? 1.f : 0.f; d.emit[1] = 1.8f / std::max(R * 0.12f, 0.2f);
			d.sun[3] = 3.5f;
			d.turb[0] = 7.f; d.turb[1] = 0.9f; d.turb[2] = 0.45f; d.turb[3] = 18.f;
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
