//============================================================================
// Name        : TerrainOcclusion.cpp
// Description : What the ground hides from where the camera stands
//               (see the header).
//============================================================================

#include <Pyros3D/Rendering/Terrain/TerrainOcclusion.h>
#include <Pyros3D/Assets/Renderable/Terrains/TerrainEditor.h>
#include <Pyros3D/Assets/Renderable/Terrains/Heightfield.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <cmath>
#include <algorithm>

namespace p3d {

	namespace {
		bool g_enabled = true;
		const f32 kFirst = 6.f;              // metres to the first step
		const f32 kGrowth = 1.075f;          // each step this much further than the last (72 steps: 1.1 km)
		const f32 kMoved = 1.0f;             // metres the eye may move before the horizons are made again
		const f32 kNothing = -1e9f;
		// A sphere is hidden only under a horizon this much steeper than its top:
		// a slope's worth, and a couple of metres of ground at its distance. (The
		// ground between two directions, and between two steps, was not looked at.)
		const f32 kSlopeMargin = 0.012f;
		const f32 kMetresMargin = 3.0f;
		const f32 kRidgeSlack = 0.75f;       // metres a ridge is taken to be lower, or the eye higher, than was worked out
		const f32 kStale = 4.0f;             // metres from where the horizons were made beyond which they are not used
	}

	void TerrainOcclusion::SetEnabled(const bool on) { g_enabled = on; }
	bool TerrainOcclusion::GetEnabled() { return g_enabled; }

	// `directions` more of the set being made; true when it is whole.
	bool TerrainOcclusion::Build(SceneGraph* scene, const int directions)
	{
		for (int n = 0; n < directions && nextDirection < Directions; n++, nextDirection++)
		{
			const int a = nextDirection;
			const f32 ang = (f32)a / (f32)Directions * 6.28318530718f;
			const f32 dx = sinf(ang), dz = cosf(ang);
			f32 top = kNothing, topAt = kFirst;
			f32* row = &next[(size_t)a * Steps];
			f32* rowAt = &nextRidge[(size_t)a * Steps];
			for (int s = 0; s < Steps; s++)
			{
				f32 h = 0.f;
				// (where the terrain has no answer the horizon stays as it was: nothing hides more)
				if (TerrainEditor::HeightAt(scene, nextEye.x + dx * stepAt[s], nextEye.z + dz * stepAt[s], h))
				{
					const f32 sl = (h - nextEye.y) / stepAt[s];
					if (sl > top) { top = sl; topAt = stepAt[s]; }
				}
				row[s] = top;
				rowAt[s] = topAt;
			}
		}
		return nextDirection >= Directions;
	}

	bool TerrainOcclusion::Update(SceneGraph* scene, const Vec3 &at)
	{
		if (!g_enabled || !scene) { ready = false; nextDirection = -1; return false; }
		const uint32 gen = Heightfield::Generation();
		if (scene != forScene || gen != generation) { ready = false; nextDirection = -1; forScene = scene; generation = gen; }

		// Under the ground - in a cave - there is no horizon to hide behind: the
		// heights all round are over the eye, every direction reads as a wall of
		// hill, and everything in the cave with you a few metres off was "behind"
		// it. (Crates, a table and whatever lay on the floor were not drawn.)
		{
			f32 over = 0.f;
			under = TerrainEditor::HeightAt(scene, at.x, at.z, over) && at.y < over - 0.5f;
			if (under) return false;
		}

		if (nextDirection < 0 && (!ready || at.distanceSQR(eye) > kMoved * kMoved))
		{
			f32 ground = 0.f;
			if (!TerrainEditor::HeightAt(scene, at.x, at.z, ground)) { ready = false; return false; }
			f32 d = kFirst;
			for (int s = 0; s < Steps; s++) { stepAt[s] = d; d *= kGrowth; }
			next.assign((size_t)Directions * Steps, kNothing);
			nextRidge.assign((size_t)Directions * Steps, kFirst);
			nextEye = at;
			nextDirection = 0;
		}
		// (the first set all at once - there is nothing to draw by until it is
		// there - and after that an eighth of it a frame)
		if (nextDirection >= 0 && Build(scene, ready ? Directions / 8 : Directions))
		{
			slope.swap(next);
			ridge.swap(nextRidge);
			eye = nextEye;
			nextDirection = -1;
			ready = true;
		}
		// (a set made from too far back hides nothing: the eye was put somewhere else)
		drift = ready ? sqrtf(at.distanceSQR(eye)) : 0.f;
		if (ready && drift > kStale) return false;
		return ready;
	}

	bool TerrainOcclusion::GroundBetween(SceneGraph* scene, const Vec3 &from, const Vec3 &to)
	{
		const Vec3 d = to - from;
		const f32 len = sqrtf(d.x * d.x + d.z * d.z);
		const int n = (int)(len) + 1;
		for (int i = 1; i < n; i++)
		{
			const f32 t = (f32)i / (f32)n;
			f32 h = 0.f;
			if (TerrainEditor::HeightAt(scene, from.x + d.x * t, from.z + d.z * t, h) && h > from.y + d.y * t) return true;
		}
		return false;
	}

	bool TerrainOcclusion::Hidden(const Vec3 &c, const f32 radius) const
	{
		if (!ready || !g_enabled || under) return false;
		const f32 dx = c.x - eye.x, dz = c.z - eye.z;
		const f32 d = sqrtf(dx * dx + dz * dz);
		const f32 nearEdge = d - radius;
		if (nearEdge < stepAt[1]) return false;                     // round the eye, or over it
		// the last step that is wholly nearer than the sphere
		int s = (int)(logf(nearEdge / kFirst) / logf(kGrowth));
		if (s >= Steps) s = Steps - 1;
		while (s > 0 && stepAt[s] > nearEdge) s--;
		if (s < 1) return false;
		// how steeply its highest point is seen (from the nearest it can be)
		const f32 topY = c.y + radius - eye.y;
		const f32 topSlope = topY / (topY > 0.f ? nearEdge : (d + radius));
		const f32 need = topSlope + kSlopeMargin + kMetresMargin / nearEdge;
		// every direction it spans, and one more to either side
		const f32 mid = atan2f(dx, dz);
		const f32 half = asinf(std::min(1.f, radius / d));
		const f32 per = 6.28318530718f / (f32)Directions;
		int a0 = (int)floorf((mid - half) / per) - 1, a1 = (int)ceilf((mid + half) / per) + 1;
		if (a1 - a0 >= Directions) return false;
		for (int a = a0; a <= a1; a++)
		{
			const int k = ((a % Directions) + Directions) % Directions;
			// The ridge that hides it, as it is seen now: the eye has moved on since
			// the horizons were made, and sees over a ridge from a little further up
			// or along - by the more the nearer the ridge is. (A bump ten metres off
			// hides a far hillside from one step and not from the next.)
			const f32 sl = slope[(size_t)k * Steps + s];
			const f32 seen = sl - ((drift + kRidgeSlack) * (1.f + fabsf(sl))) / ridge[(size_t)k * Steps + s];
			if (seen < need) return false;
		}
		return true;
	}

}
