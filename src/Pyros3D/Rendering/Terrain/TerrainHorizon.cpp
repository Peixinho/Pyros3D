//============================================================================
// Name        : TerrainHorizon.cpp
// Description : The terrain's own shadow, baked as a horizon map
//============================================================================

#include <Pyros3D/Rendering/Terrain/TerrainHorizon.h>
#include <Pyros3D/Assets/Renderable/Terrains/TerrainEditor.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Utils/Jobs/JobSystem.h>
#include <Pyros3D/Rendering/Components/Terrain/TerrainComponent.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <algorithm>
#include <cmath>

namespace p3d {

	TerrainHorizon::~TerrainHorizon()
	{
		delete texture;
	}

	f32 TerrainHorizon::ShadeAt(const f32 x, const f32 z, const Vec3 &toSun) const
	{
		if (resolution == 0) return 1.f;
		const f32 u = (x - rect.x) * rect.z, v = (z - rect.y) * rect.w;
		if (u < 0.f || v < 0.f || u > 1.f || v > 1.f) return 1.f;
		const int N = (int)resolution;
		const int i = std::min(N - 1, (int)(u * (f32)N)), j = std::min(N - 1, (int)(v * (f32)N));
		f32 turn = atan2f(toSun.z, toSun.x) / 6.28318531f;
		if (turn < 0.f) turn += 1.f;
		const f32 k = turn * (f32)Directions;
		const int k0 = ((int)floorf(k)) % Directions, k1 = (k0 + 1) % Directions;
		const unsigned char* h = &horizons[((size_t)j * N + i) * Directions];
		const f32 t = k - floorf(k);
		const f32 horizon = ((f32)h[k0] * (1.f - t) + (f32)h[k1] * t) / 255.f;
		const f32 lo = horizon - softness, hi = horizon + softness;
		const f32 s = std::max(0.f, std::min(1.f, (toSun.y - lo) / (hi - lo)));
		return s * s * (3.f - 2.f * s);
	}

	// What a bake reads of the ground before the slow part: the square the loaded tiles cover, and
	// the height at the middle of each texel. Read on the calling thread - the tiles are the scene's.
	struct TerrainHorizon::Ground
	{
		int N = 0;
		f32 minX = 0.f, minZ = 0.f, sizeX = 0.f, sizeZ = 0.f, stepX = 0.f, stepZ = 0.f, reach = 0.f;
		std::vector<f32> height;
	};

	// The square the loaded tiles cover - the quick half of reading the ground, on the calling thread
	static bool LoadedRect(SceneGraph* scene, f32 &minX, f32 &minZ, f32 &maxX, f32 &maxZ)
	{
		const std::vector<TerrainTile> tiles = TerrainEditor::FindTiles(scene);
		bool any = false;
		for (size_t i = 0; i < tiles.size(); i++)
		{
			const f32 size = tiles[i].Size();
			if (size <= 0.f) continue;
			const Vec3 &o = tiles[i].origin;
			if (!any) { minX = o.x; minZ = o.z; maxX = o.x + size; maxZ = o.z + size; any = true; }
			else { minX = std::min(minX, o.x); minZ = std::min(minZ, o.z); maxX = std::max(maxX, o.x + size); maxZ = std::max(maxZ, o.z + size); }
		}
		return any && maxX - minX >= 1.f && maxZ - minZ >= 1.f;
	}

	bool TerrainHorizon::ReadGround(SceneGraph* scene, const uint32 wanted, const f32 reach, Ground &g)
	{
		if (scene == NULL) return false;
		const std::vector<TerrainTile> tiles = TerrainEditor::FindTiles(scene);
		f32 minX = 0.f, minZ = 0.f, maxX = 0.f, maxZ = 0.f;
		bool any = false;
		for (size_t i = 0; i < tiles.size(); i++)
		{
			const f32 size = tiles[i].Size();
			if (size <= 0.f) continue;
			const Vec3 &o = tiles[i].origin;
			if (!any) { minX = o.x; minZ = o.z; maxX = o.x + size; maxZ = o.z + size; any = true; }
			else { minX = std::min(minX, o.x); minZ = std::min(minZ, o.z); maxX = std::max(maxX, o.x + size); maxZ = std::max(maxZ, o.z + size); }
		}
		if (!any || maxX - minX < 1.f || maxZ - minZ < 1.f) return false;

		const int N = (int)std::max(64u, std::min(2048u, wanted));
		g.N = N; g.minX = minX; g.minZ = minZ; g.sizeX = maxX - minX; g.sizeZ = maxZ - minZ;
		g.stepX = g.sizeX / (f32)N; g.stepZ = g.sizeZ / (f32)N; g.reach = reach;

		// the ground, once, at the middle of every texel - the rows shared between the cores. (The
		// frame's list of tiles is made first, so that each of them only reads it; the calling
		// thread waits in the loop meanwhile, so nothing changes the scene under them.)
		g.height.assign((size_t)N * N, 0.f);
		{
			f32 warm = 0.f;
			TerrainEditor::HeightAt(scene, minX, minZ, warm);
		}
		JobSystem::Instance().ParallelFor((uint32)N, 8, [&](uint32 j0, uint32 j1) {
			for (int j = (int)j0; j < (int)j1; j++)
				for (int i = 0; i < N; i++)
				{
					f32 h = 0.f;
					if (!TerrainEditor::HeightAt(scene, g.minX + ((f32)i + 0.5f) * g.stepX, g.minZ + ((f32)j + 0.5f) * g.stepZ, h)) h = -1e30f;       // (no tile here: filled in below)
					g.height[(size_t)j * N + i] = h;
				}
		});
		f32 lowest = 1e30f;
		for (size_t k = 0; k < g.height.size(); k++) if (g.height[k] > -1e29f) lowest = std::min(lowest, g.height[k]);
		if (lowest > 1e29f) return false;
		for (size_t k = 0; k < g.height.size(); k++) if (g.height[k] < -1e29f) g.height[k] = lowest;
		return true;
	}

	// The slow part: rows j0..j1 of every direction's horizon. Touches nothing but g and out - any thread.
	void TerrainHorizon::WorkOut(const Ground &g, TerrainHorizon &out, const int j0, const int j1)
	{
		const int N = g.N;
		// How far along a direction the ground is asked: close together near
		// to, further apart further off - a hill's edge matters most where it
		// is nearest.
		std::vector<f32> along;
		for (f32 d = 1.5f; d * std::min(g.stepX, g.stepZ) <= g.reach && along.size() < 40; d *= 1.3f) along.push_back(d);

		for (int k = 0; k < Directions; k++)
		{
			const f32 angle = (f32)k * (6.28318531f / (f32)Directions);
			const f32 dx = cosf(angle), dz = sinf(angle);
			// (a step of one texel in the direction, in texels of x and of z)
			const f32 tx = dx / g.stepX * std::min(g.stepX, g.stepZ), tz = dz / g.stepZ * std::min(g.stepX, g.stepZ);
			const f32 metresPerStep = std::min(g.stepX, g.stepZ);
			for (int j = j0; j < j1; j++)
				for (int i = 0; i < N; i++)
				{
					const f32 here = g.height[(size_t)j * N + i];
					f32 highest = 0.f;       // the sine of the horizon's height; the flat is 0
					for (size_t s = 0; s < along.size(); s++)
					{
						const int x = (int)((f32)i + tx * along[s] + 0.5f), z = (int)((f32)j + tz * along[s] + 0.5f);
						if (x < 0 || z < 0 || x >= N || z >= N) break;
						const f32 rise = g.height[(size_t)z * N + x] - here;
						if (rise <= 0.f) continue;
						const f32 run = along[s] * metresPerStep;
						const f32 sine = rise / sqrtf(rise * rise + run * run);
						if (sine > highest) highest = sine;
					}
					out.horizons[((size_t)j * N + i) * Directions + k] = (unsigned char)std::min(255.f, highest * 255.f + 0.5f);
				}
		}
	}

	std::shared_ptr<TerrainHorizon> TerrainHorizon::Start(const Ground &g)
	{
		std::shared_ptr<TerrainHorizon> out(new TerrainHorizon());
		out->resolution = (uint32)g.N;
		out->rect = Vec4(g.minX, g.minZ, 1.f / g.sizeX, 1.f / g.sizeZ);
		out->horizons.assign((size_t)g.N * g.N * Directions, 0);
		return out;
	}

	// Four pictures side by side, four directions each in their channels. The thread that draws.
	void TerrainHorizon::MakePicture()
	{
		const int N = (int)resolution;
		const int W = N * (Directions / 4);
		std::vector<unsigned char> pixels((size_t)W * N * 4);
		for (int j = 0; j < N; j++)
			for (int i = 0; i < N; i++)
			{
				const unsigned char* h = &horizons[((size_t)j * N + i) * Directions];
				for (int c = 0; c < Directions / 4; c++)
				{
					unsigned char* px = &pixels[((size_t)j * W + (size_t)c * N + i) * 4];
					px[0] = h[c * 4]; px[1] = h[c * 4 + 1]; px[2] = h[c * 4 + 2]; px[3] = h[c * 4 + 3];
				}
			}
		texture = new Texture();
		texture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, W, N, false);
		texture->UpdateData(&pixels[0]);
		texture->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
		texture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
	}

	std::shared_ptr<TerrainHorizon> TerrainHorizon::Bake(SceneGraph* scene, const uint32 wanted, const f32 reach)
	{
		Ground g;
		if (!ReadGround(scene, wanted, reach, g)) return std::shared_ptr<TerrainHorizon>();
		std::shared_ptr<TerrainHorizon> out = Start(g);
		JobSystem::Instance().ParallelFor((uint32)g.N, 8, [&](uint32 j0, uint32 j1) { WorkOut(g, *out, (int)j0, (int)j1); });
		out->MakePicture();
		return out;
	}

	// ---- a bake in the background
	struct TerrainHorizon::Later::State
	{
		Ground ground;
		std::shared_ptr<TerrainHorizon> out;
		JobCounter counter;
	};

	TerrainHorizon::Later::~Later()
	{
		// (the job holds the state it works on: it must be done before the counter it counts down goes)
		if (state) JobSystem::Instance().Wait(state->counter);
	}

	bool TerrainHorizon::Later::Ready() const
	{
		return state && state->counter.pending.load() == 0;
	}

	std::shared_ptr<TerrainHorizon> TerrainHorizon::Later::Finish()
	{
		if (!state) return std::shared_ptr<TerrainHorizon>();
		JobSystem::Instance().Wait(state->counter);
		std::shared_ptr<TerrainHorizon> out = state->out;
		state.reset();
		if (out) out->MakePicture();
		return out;
	}

	std::shared_ptr<TerrainHorizon::Later> TerrainHorizon::BakeLater(SceneGraph* scene, const uint32 wanted, const f32 reach)
	{
		std::shared_ptr<Later> later(new Later());
		std::shared_ptr<Later::State> st(new Later::State());
		// Where every terrain of the scene has its overview - the whole terrain's ground, fixed once it is
		// loaded - the ground is read from that, on the worker too: a million heights read here from the
		// loaded tiles were a sixth of a second on the game's thread every few hundred metres of flight.
		std::vector<const TerrainComponent*> terrains;
		bool overviews = scene != NULL;
		for (TerrainComponent* t : TerrainComponent::Instances())
			if (t->GetOwner() && t->GetOwner()->GetScene() == scene) { terrains.push_back(t); if (!t->HasOverview()) overviews = false; }
		f32 minX = 0.f, minZ = 0.f, maxX = 0.f, maxZ = 0.f;
		if (overviews && !terrains.empty() && LoadedRect(scene, minX, minZ, maxX, maxZ))
		{
			Ground &g = st->ground;
			const int N = (int)std::max(64u, std::min(2048u, wanted));
			g.N = N; g.minX = minX; g.minZ = minZ; g.sizeX = maxX - minX; g.sizeZ = maxZ - minZ;
			g.stepX = g.sizeX / (f32)N; g.stepZ = g.sizeZ / (f32)N; g.reach = reach;
			st->out = Start(g);
			later->state = st;
			Later::State* raw = st.get();
			JobSystem::Instance().Run([raw, terrains]() {
				Ground &g = raw->ground;
				const int N = g.N;
				g.height.assign((size_t)N * N, 0.f);
				f32 lowest = 1e30f;
				for (int j = 0; j < N; j++)
					for (int i = 0; i < N; i++)
					{
						f32 h = -1e30f, v = 0.f;
						for (size_t t = 0; t < terrains.size(); t++)
							if (terrains[t]->OverviewHeightAt(g.minX + ((f32)i + 0.5f) * g.stepX, g.minZ + ((f32)j + 0.5f) * g.stepZ, v)) h = std::max(h, v);
						g.height[(size_t)j * N + i] = h;
						if (h > -1e29f) lowest = std::min(lowest, h);
					}
				for (size_t k = 0; k < g.height.size(); k++) if (g.height[k] < -1e29f) g.height[k] = lowest < 1e29f ? lowest : 0.f;
				WorkOut(g, *raw->out, 0, N);
			}, st->counter);
			return later;
		}
		if (!ReadGround(scene, wanted, reach, st->ground)) return std::shared_ptr<Later>();
		st->out = Start(st->ground);
		later->state = st;
		// one worker, the whole of it: the game goes on meanwhile (the frame does not wait for it)
		Later::State* raw = st.get();
		JobSystem::Instance().Run([raw]() { WorkOut(raw->ground, *raw->out, 0, raw->ground.N); }, st->counter);
		return later;
	}

}
