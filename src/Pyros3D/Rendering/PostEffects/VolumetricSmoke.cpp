//============================================================================
// Name        : VolumetricSmoke.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See VolumetricSmoke.h.
//============================================================================

#include <Pyros3D/Rendering/PostEffects/VolumetricSmoke.h>
#include <Pyros3D/Physics/PhysicsEngines/IPhysics.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Lights/PointLight/PointLight.h>
#include <Pyros3D/Rendering/Components/Lights/SpotLight/SpotLight.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <algorithm>
#include <cmath>
#include <queue>

namespace p3d {

	VolumetricSmoke::Cloud VolumetricSmoke::clouds[VolumetricSmoke::MaxClouds];
	std::vector<uchar> VolumetricSmoke::atlas;
	uint32 VolumetricSmoke::atlasVersion = 0;
	f32 VolumetricSmoke::cellSize = 0.6f;
	Vec3 VolumetricSmoke::lightDirection = Vec3(0.45f, 0.8f, 0.35f).normalize();
	Vec3 VolumetricSmoke::lightColor = Vec3(0.70f, 0.68f, 0.62f);
	Vec3 VolumetricSmoke::ambient = Vec3(0.28f, 0.30f, 0.34f);
	Vec3 VolumetricSmoke::wind = Vec3(0.25f, 0.06f, 0.12f);
	f32 VolumetricSmoke::screenFlash = 0.f;
	Vec3 VolumetricSmoke::screenFlashColor = Vec3(1.f, 1.f, 1.f);
	bool VolumetricSmoke::manualLight = false;
	bool VolumetricSmoke::manualAmbient = false;
	std::vector<VolumetricSmoke::LocalLight> VolumetricSmoke::localLights;

	namespace {

		// The value the shader treats as "no smoke" in the R channel. A voxel
		// whose centre is a fraction h of a cell from a wall stores
		// kWallFloor / (1 - h): interpolated toward the empty voxel behind
		// the wall, that reaches kWallFloor exactly at the wall. Must match
		// WALL_FLOOR in VolumetricSmokeEffect's shader.
		const f32 kWallFloor = 0.2f;


		// Smoke is not a surface. A white wall facing the sun returns all of
		// it; a cloud scatters it every way, and this much comes back.
		const f32 kSunScatter = 0.70f;
		const f32 kAmbientScatter = 0.55f;
		const f32 kLocalScatter = 0.60f;

		// How far a voxel looks toward the sun for something in the way.
		const f32 kSunRay = 60.f;

		// Rows of voxels kept below the start point, so smoke can run down a
		// step or a slope instead of being cut off flat at the grenade.
		const int32 kRowsBelow = 2;

		struct Node
		{
			f32 cost;      // what the fill is ordered by - see Reach()
			f32 path;      // distance walked to get here, cell to cell
			uint32 index;
			bool operator<(const Node &o) const { return cost > o.cost; }   // min-heap
		};

		// How far the smoke has effectively come to reach a voxel (dx,dy,dz)
		// cells from the start, having walked `path`.
		//
		// The fill only steps along the axes, so the distance it walks is a
		// Manhattan one, and ordering the fill by that grows an octahedron -
		// the first version's clouds were pyramids. In open air every
		// shortest walk is exactly the Manhattan distance, so the ratio of
		// the two says how much of a detour the walls forced: the straight-
		// line distance scaled by that ratio is round in the open and still
		// longer the further round a corner the smoke had to go.
		inline f32 Reach(const int32 dx, const int32 dy, const int32 dz, const f32 path, const f32 cell, const f32 kUpCost)
		{
			const f32 ax = (f32)(dx < 0 ? -dx : dx), az = (f32)(dz < 0 ? -dz : dz);
			const f32 ay = dy > 0 ? (f32)dy * kUpCost : (f32)(-dy);
			const f32 manhattan = (ax + ay + az) * cell;
			if (manhattan <= 0.f)
				return 0.f;
			const f32 straight = sqrtf(ax * ax + ay * ay + az * az) * cell;
			return straight * (path / manhattan);
		}

		// What makes one cloud different from the next, and none of them a
		// neat dome: a few directions it happens to push out further in,
		// and a slow turbulence over the whole grid that makes some pockets
		// of air easier to fill than others. Both only reorder the fill -
		// which voxel the smoke reaches next - so walls are respected
		// exactly as before and the amount of smoke is unchanged.
		struct Shape
		{
			enum { Lobes = 5 };
			Vec3 lobe[Lobes];
			f32 strength[Lobes];
			uint32 seed;
		};

		inline f32 Hash01(uint32 x, uint32 y, uint32 z, uint32 seed)
		{
			uint32 h = x * 374761393u + y * 668265263u + z * 2147483647u + seed * 3266489917u;
			h = (h ^ (h >> 13)) * 1274126177u;
			h ^= h >> 16;
			return (f32)(h & 0xffffffu) / (f32)0xffffffu;
		}

		// Trilinear value noise over the voxel grid, `period` cells a lump.
		f32 Turbulence(const f32 x, const f32 y, const f32 z, const f32 period, const uint32 seed)
		{
			const f32 px = x / period + 64.f, py = y / period + 64.f, pz = z / period + 64.f;
			const uint32 ix = (uint32)px, iy = (uint32)py, iz = (uint32)pz;
			f32 fx = px - ix, fy = py - iy, fz = pz - iz;
			fx = fx * fx * (3.f - 2.f * fx); fy = fy * fy * (3.f - 2.f * fy); fz = fz * fz * (3.f - 2.f * fz);
			f32 v = 0.f;
			for (uint32 c = 0; c < 8; c++)
			{
				const uint32 ox = c & 1, oy = (c >> 1) & 1, oz = (c >> 2) & 1;
				v += Hash01(ix + ox, iy + oy, iz + oz, seed)
					* (ox ? fx : 1.f - fx) * (oy ? fy : 1.f - fy) * (oz ? fz : 1.f - fz);
			}
			return v;
		}

		Shape MakeShape(const uint32 seed)
		{
			Shape s;
			s.seed = seed;
			for (uint32 i = 0; i < Shape::Lobes; i++)
			{
				// Anywhere around, and from level to well up: a lobe aimed
				// at the ground would only be flattened against it.
				const f32 a = Hash01(i, 1, 0, seed) * 2.f * (f32)PI;
				const f32 up = Hash01(i, 2, 0, seed) * 0.6f;
				const f32 flat = sqrtf(1.f - up * up);
				s.lobe[i] = Vec3(cosf(a) * flat, up, sinf(a) * flat);
				s.strength[i] = 0.35f + 0.55f * Hash01(i, 3, 0, seed);
			}
			return s;
		}

		// Scales a voxel's reach: under 1 the smoke gets there sooner.
		f32 Irregularity(const Shape &s, const int32 dx, const int32 dy, const int32 dz)
		{
			const f32 len = sqrtf((f32)(dx * dx + dy * dy + dz * dz));
			if (len < 0.5f)
				return 1.f;
			const Vec3 dir((f32)dx / len, (f32)dy / len, (f32)dz / len);
			f32 push = 1.f;
			for (uint32 i = 0; i < Shape::Lobes; i++)
			{
				const f32 d = dir.x * s.lobe[i].x + dir.y * s.lobe[i].y + dir.z * s.lobe[i].z;
				if (d > 0.f) push += s.strength[i] * d * d * d * d;
			}
			const f32 coarse = Turbulence((f32)dx, (f32)dy, (f32)dz, 5.f, s.seed);
			const f32 fine = Turbulence((f32)dx, (f32)dy, (f32)dz, 2.2f, s.seed + 17u);
			return (0.72f + 0.40f * coarse + 0.22f * fine) / push;
		}

		inline uint32 VoxelIndex(const int32 x, const int32 y, const int32 z)
		{
			return (uint32)(x + VolumetricSmoke::NX * (z + VolumetricSmoke::NZ * y));
		}

		// Where voxel (x,y,z) of cloud `id` lives in the atlas, as a byte offset.
		inline size_t AtlasOffset(const uint32 id, const int32 x, const int32 y, const int32 z)
		{
			const uint32 tileX = (id % VolumetricSmoke::TilesPerRow) * VolumetricSmoke::TileSize;
			const uint32 tileY = (id / VolumetricSmoke::TilesPerRow) * VolumetricSmoke::TileSize;
			const uint32 px = tileX + (uint32)(y % VolumetricSmoke::SlicesPerRow) * VolumetricSmoke::NX + (uint32)x;
			const uint32 py = tileY + (uint32)(y / VolumetricSmoke::SlicesPerRow) * VolumetricSmoke::NZ + (uint32)z;
			return ((size_t)py * VolumetricSmoke::AtlasWidth + px) * 4;
		}
	}

	const std::vector<uchar> &VolumetricSmoke::GetAtlas()
	{
		if (atlas.empty())
		{
			atlas.resize((size_t)AtlasWidth * AtlasHeight * 4, 0);
			for (size_t i = 0; i < atlas.size(); i += 4) { atlas[i + 1] = 255; atlas[i + 3] = 255; }
		}
		return atlas;
	}

	void VolumetricSmoke::ClearTile(const uint32 id)
	{
		GetAtlas();
		for (int32 y = 0; y < NY; y++)
			for (int32 z = 0; z < NZ; z++)
				for (int32 x = 0; x < NX; x++)
				{
					const size_t o = AtlasOffset(id, x, y, z);
					atlas[o] = 0; atlas[o + 1] = 255; atlas[o + 2] = 255;
				}
	}

	int32 VolumetricSmoke::Spawn(IPhysics* physics, const Vec3 &position, const Vec3 &color,
		const f32 radius, const f32 growTime, const f32 lifeTime, const f32 fadeTime,
		const f32 rise, const f32 emission, const f32 blast)
	{
		// What a step up costs against a step sideways. Under 1 the smoke
		// climbs more readily than it spreads, and open-air smoke stands up
		// as a ball on the ground; well over 1 it spreads into a low
		// mushroom cap, which is what 1.7 looked like - and is what burning
		// fuel on a floor should do.
		const f32 kUpCost = rise > 0.05f ? rise : 0.05f;
		int32 id = -1;
		for (int32 i = 0; i < MaxClouds; i++)
			if (!clouds[i].active) { id = i; break; }
		if (id < 0)
			return -1;

		const f32 cell = cellSize;
		const int32 sx = NX / 2, sy = kRowsBelow, sz = NZ / 2;

		Cloud &c = clouds[id];
		c = Cloud();
		// The start voxel's centre is the spawn position.
		c.boxMin = Vec3(position.x - (sx + 0.5f) * cell, position.y - (sy + 0.5f) * cell, position.z - (sz + 0.5f) * cell);
		c.color = color;
		c.growTime = growTime > 0.01f ? growTime : 0.01f;
		c.lifeTime = lifeTime > c.growTime ? lifeTime : c.growTime;
		c.fadeTime = fadeTime > 0.01f ? fadeTime : 0.01f;
		c.density = 1.f;
		c.emission = emission > 0.f ? emission : 0.f;
		c.blast = blast < 0.f ? 0.f : (blast > 1.f ? 1.f : blast);

		// How many voxels a dome of that radius is worth, and how far the
		// smoke may travel from the grenade. The second is what ends the
		// fill in a corridor, where the volume alone would send it on for
		// tens of metres - and it keeps the cloud inside its grid.
		const f32 r = radius > cell ? radius : cell;
		// Half an ellipsoid: r across the ground, r / kUpCost tall.
		const uint32 budget = (uint32)((2.0f / 3.0f) * (f32)PI * r * r * r / kUpCost / (cell * cell * cell));
		const f32 gridReach = (f32)(NX / 2 - 2) * cell;
		const f32 reach = std::min(r * 1.6f, gridReach * 0.72f);

		const uint32 total = (uint32)NX * NY * NZ;
		std::vector<f32> cost(total, -1.f);         // -1: not filled
		std::vector<f32> wall(total, 1.f);          // nearest blocked face, in cells
		std::priority_queue<Node> open;

		Node start; start.cost = 0.f; start.path = 0.f; start.index = VoxelIndex(sx, sy, sz);
		open.push(start);
		std::vector<f32> best(total, 1e30f);
		best[start.index] = 0.f;

		static uint32 spawnCounter = 0;
		const Shape shape = MakeShape(++spawnCounter * 7919u + (uint32)id);

		static const int32 dir[6][3] = { {1,0,0}, {-1,0,0}, {0,0,1}, {0,0,-1}, {0,1,0}, {0,-1,0} };

		uint32 filled = 0;
		f32 lastCost = 0.f;
		while (!open.empty() && filled < budget)
		{
			const Node n = open.top();
			open.pop();
			if (cost[n.index] >= 0.f)
				continue;
			cost[n.index] = n.cost;
			if (n.cost > lastCost) lastCost = n.cost;
			filled++;

			const int32 x = (int32)(n.index % NX);
			const int32 z = (int32)((n.index / NX) % NZ);
			const int32 y = (int32)(n.index / (NX * NZ));
			const Vec3 from(c.boxMin.x + (x + 0.5f) * cell, c.boxMin.y + (y + 0.5f) * cell, c.boxMin.z + (z + 0.5f) * cell);

			for (int32 d = 0; d < 6; d++)
			{
				const int32 nx = x + dir[d][0], ny = y + dir[d][1], nz = z + dir[d][2];
				// The outermost shell stays empty, so the shader can clamp
				// its lookups to the tile without reading a neighbour's.
				if (nx < 1 || ny < 1 || nz < 1 || nx > NX - 2 || ny > NY - 2 || nz > NZ - 2)
					continue;
				const uint32 ni = VoxelIndex(nx, ny, nz);
				if (cost[ni] >= 0.f)
					continue;

				if (physics != NULL)
				{
					const Vec3 to(from.x + dir[d][0] * cell, from.y + dir[d][1] * cell, from.z + dir[d][2] * cell);
					const RayCastHit hit = physics->RayCast(from, to);
					if (hit.hasHit)
					{
						const f32 h = (hit.point - from).magnitude() / cell;
						if (h < wall[n.index]) wall[n.index] = h;
						continue;
					}
				}

				const f32 step = cell * (dir[d][1] > 0 ? kUpCost : 1.f);
				const f32 np = n.path + step;
				const f32 nc = Reach(nx - sx, ny - sy, nz - sz, np, cell, kUpCost)
					* Irregularity(shape, nx - sx, ny - sy, nz - sz);
				if (nc > reach || nc >= best[ni])
					continue;
				best[ni] = nc;
				Node next; next.cost = nc; next.path = np; next.index = ni;
				open.push(next);
			}
		}
		c.cells = filled;

		ClearTile((uint32)id);
		const f32 norm = lastCost > 0.f ? lastCost : 1.f;
		int32 lo[3] = { NX, NY, NZ }, hi[3] = { -1, -1, -1 };
		for (int32 y = 0; y < NY; y++)
			for (int32 z = 0; z < NZ; z++)
				for (int32 x = 0; x < NX; x++)
				{
					const uint32 i = VoxelIndex(x, y, z);
					if (cost[i] < 0.f)
						continue;
					f32 amount = 1.f;
					if (wall[i] < 1.f)
					{
						const f32 open01 = 1.f - wall[i];
						amount = open01 > kWallFloor ? kWallFloor / open01 : 1.f;
					}
					if (x < lo[0]) lo[0] = x; if (x > hi[0]) hi[0] = x;
					if (y < lo[1]) lo[1] = y; if (y > hi[1]) hi[1] = y;
					if (z < lo[2]) lo[2] = z; if (z > hi[2]) hi[2] = z;
					const size_t o = AtlasOffset((uint32)id, x, y, z);
					atlas[o] = (uchar)(amount * 255.f + 0.5f);
					// 0..250: 255 is reserved for "never reached".
					atlas[o + 1] = (uchar)(cost[i] / norm * 250.f + 0.5f);
					if (physics != NULL)
					{
						const Vec3 centre(c.boxMin.x + (x + 0.5f) * cell, c.boxMin.y + (y + 0.5f) * cell, c.boxMin.z + (z + 0.5f) * cell);
						const RayCastHit shade = physics->RayCast(centre, centre + lightDirection * kSunRay);
						atlas[o + 2] = shade.hasHit ? 0 : 255;
					}
				}
		atlasVersion++;

		// Voxel centres, plus the one cell the interpolated field can
		// spread past the outermost of them.
		c.boundsMin = c.boxMin + Vec3((lo[0] - 0.5f) * cell, (lo[1] - 0.5f) * cell, (lo[2] - 0.5f) * cell);
		c.boundsMax = c.boxMin + Vec3((hi[0] + 1.5f) * cell, (hi[1] + 1.5f) * cell, (hi[2] + 1.5f) * cell);

		c.active = true;
		return id;
	}

	void VolumetricSmoke::Update(const f32 dt)
	{
		for (int32 i = 0; i < MaxClouds; i++)
		{
			Cloud &c = clouds[i];
			if (!c.active)
				continue;
			c.age += dt;
			if (c.age >= c.lifeTime)
			{
				Remove(i);
				continue;
			}
			// Fast out of the canister, slow to its full size.
			f32 g = c.age / c.growTime;
			if (g > 1.f) g = 1.f;
			g = 1.f - (1.f - g) * (1.f - g);
			f32 fade = (c.lifeTime - c.age) / c.fadeTime;
			if (fade > 1.f) fade = 1.f;
			// Thins from the outside in as it goes.
			c.growth = g * (0.7f + 0.3f * fade);
			c.density = fade * fade * (3.f - 2.f * fade) * c.thickness;
		}
	}

	void VolumetricSmoke::SetThickness(const int32 id, const f32 thickness)
	{
		if (id < 0 || id >= (int32)MaxClouds || !clouds[id].active) return;
		clouds[id].thickness = thickness < 0.f ? 0.f : (thickness > 1.f ? 1.f : thickness);
		clouds[id].density = clouds[id].thickness;
	}

	void VolumetricSmoke::Remove(const int32 id)
	{
		if (id < 0 || id >= MaxClouds)
			return;
		clouds[id].active = false;
		clouds[id].density = 0.f;
	}

	void VolumetricSmoke::Clear()
	{
		for (int32 i = 0; i < MaxClouds; i++)
			Remove(i);
	}

	uint32 VolumetricSmoke::GetActiveCount()
	{
		uint32 n = 0;
		for (int32 i = 0; i < MaxClouds; i++)
			if (clouds[i].active) n++;
		return n;
	}

	void VolumetricSmoke::SetCellSize(const f32 size)
	{
		if (GetActiveCount() == 0 && size > 0.05f)
			cellSize = size;
	}

	void VolumetricSmoke::CaptureLights(const std::vector<IComponent*> &lights, const Vec3 &sceneAmbient)
	{
		if (!manualAmbient)
			ambient = sceneAmbient * kAmbientScatter;

		localLights.clear();
		f32 brightest = -1.f;
		Vec3 sunDirection = lightDirection, sunColor(0.f, 0.f, 0.f);
		for (std::vector<IComponent*>::const_iterator i = lights.begin(); i != lights.end(); i++)
		{
			ILightComponent* light = (ILightComponent*)(*i);
			if (light->GetOwner() == NULL)
				continue;
			const Vec4 radiance = light->GetLightRadiance();
			const Vec3 color(radiance.x, radiance.y, radiance.z);
			switch (light->GetLightType())
			{
			case LIGHT_TYPE::DIRECTIONAL:
			{
				// Local to its owner, like every other reader of it.
				DirectionalLight* d = (DirectionalLight*)light;
				const f32 power = color.x + color.y + color.z;
				if (power > brightest)
				{
					brightest = power;
					const Vec3 travel = (d->GetOwner()->GetWorldTransformation() * Vec4(d->GetLightDirection(), 0.f)).xyz();
					if (travel.magnitudeSQR() > 1e-8f)
						sunDirection = travel.normalize() * -1.f;
					sunColor = color;
				}
				break;
			}
			case LIGHT_TYPE::POINT:
			{
				PointLight* p = (PointLight*)light;
				LocalLight l;
				l.position = p->GetOwner()->GetWorldPosition();
				l.radius = p->GetLightRadius();
				l.color = color * kLocalScatter;
				l.direction = Vec3(0.f, -1.f, 0.f);
				l.cosInner = l.cosOuter = -1.f;
				localLights.push_back(l);
				break;
			}
			case LIGHT_TYPE::SPOT:
			{
				SpotLight* sp = (SpotLight*)light;
				LocalLight l;
				l.position = sp->GetOwner()->GetWorldPosition();
				l.radius = sp->GetLightRadius();
				l.color = color * kLocalScatter;
				const Vec3 aim = (sp->GetOwner()->GetWorldTransformation() * Vec4(sp->GetLightDirection(), 0.f)).xyz();
				l.direction = aim.magnitudeSQR() > 1e-8f ? aim.normalize() : Vec3(0.f, -1.f, 0.f);
				l.cosInner = sp->GetLightCosInnerCone();
				l.cosOuter = sp->GetLightCosOutterCone();
				localLights.push_back(l);
				break;
			}
			}
		}
		if (!manualLight)
		{
			lightDirection = sunDirection;
			lightColor = sunColor * kSunScatter;
		}
	}

	void VolumetricSmoke::SetLight(const Vec3 &towardLight, const Vec3 &color)
	{
		if (towardLight.magnitudeSQR() > 1e-8f)
			lightDirection = towardLight.normalize();
		lightColor = color;
		manualLight = true;
	}

	void VolumetricSmoke::SetScreenFlash(const Vec3 &color, const f32 amount)
	{
		screenFlashColor = color;
		screenFlash = amount < 0.f ? 0.f : (amount > 1.f ? 1.f : amount);
	}

	void VolumetricSmoke::SetAmbient(const Vec3 &color) { ambient = color; manualAmbient = true; }
	void VolumetricSmoke::UseSceneLighting() { manualLight = manualAmbient = false; }
	void VolumetricSmoke::SetWind(const Vec3 &w) { wind = w; }

}
