//============================================================================
// Name        : GrassTuft.cpp
// Description : See GrassTuft.h.
//============================================================================

#include <Pyros3D/Assets/Renderable/Plants/GrassTuft.h>
#include <algorithm>
#include <cmath>

namespace p3d {

	namespace {
		struct Dice
		{
			uint32 s;
			explicit Dice(const uint32 seed) : s(seed * 2654435761u + 0x9E3779B9u) { if (s == 0) s = 1; Next(); Next(); }
			uint32 Next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
			f32 Unit() { return (f32)(Next() & 0xFFFFFF) / (f32)0x1000000; }
			f32 In(const f32 a, const f32 b) { return a + (b - a) * Unit(); }
		};
	}

	GrassTuft::GrassTuft(const uint32 blades_, const f32 height_, const f32 spread_, const uint32 seed_, const Vec3 &base_, const Vec3 &tip_,
		const uint32 flowers_, const Vec3 &flower_, const f32 width_, const bool simple_)
		: blades(std::max<uint32>(1, blades_)), seed(seed_), flowers(std::min(flowers_, std::max<uint32>(1, blades_))), height(height_), spread(spread_), width(width_),
		simple(simple_), base(base_), tip(tip_), flower(flower_)
	{
		isFlipped = false;
		isSmooth = false;
		calculateTangentBitangent = false;

		Dice dice(seed * 9781u + 3u);
		PrimitiveGeometry* g = geometry;
		auto vertex = [g](const Vec3 &p, const Vec3 &n, const Vec3 &c, const f32 v) {
			g->tVertex.push_back(p); g->tNormal.push_back(n); g->tTexcoord.push_back(Vec2(.5f, v)); g->tColor.push_back(Vec4(c.x, c.y, c.z, 1.f));
		};
		const f32 pi = 3.14159265f;
		f32 top = 0.f;
		for (uint32 b = 0; b < blades; b++)
		{
			const bool stem = b < flowers;
			// where it stands (evenly over the patch, not heaped in the middle), and the way it faces and leans
			const f32 r = sqrtf(dice.Unit()) * spread, a = dice.Unit() * 2.f * pi;
			const Vec3 root(cosf(a) * r, 0.f, sinf(a) * r);
			const f32 face = dice.Unit() * 2.f * pi;
			const Vec3 across(cosf(face), 0.f, sinf(face));
			const Vec3 lean(-sinf(face), 0.f, cosf(face));
			const f32 h = height * (stem ? dice.In(.85f, 1.25f) : dice.In(.50f, 1.15f));
			const f32 w = width * dice.In(.75f, 1.3f) * (stem ? .45f : 1.f);
			const f32 bend = stem ? dice.In(.02f, .12f) : dice.In(.10f, .55f);
			const f32 shade = dice.In(.84f, 1.14f), warm = dice.In(-.08f, .10f);
			const Vec3 c0 = Vec3(base.x * (1.f + warm), base.y, base.z * (1.f - warm)) * shade;
			const Vec3 c2 = Vec3(tip.x * (1.f + warm), tip.y, tip.z * (1.f - warm)) * shade;
			const Vec3 c1 = c0.Lerp(c2, .55f);
			// (lit as the ground it grows from, leaning a little its own way: a meadow is one surface)
			const Vec3 n = (Vec3(0.f, 1.f, 0.f) + lean * .22f).normalize();
			const Vec3 mid = root + Vec3(0.f, h * .52f, 0.f) + lean * (h * bend * .28f);
			const Vec3 end = root + Vec3(0.f, h, 0.f) + lean * (h * bend);
			top = std::max(top, end.y);
			const uint32 i = (uint32)g->tVertex.size();
			if (simple)
			{
				// (one triangle, root to tip - the same random numbers drawn, so every blade is where
				// the full tuft has it)
				(void)mid; (void)c1;
				vertex(root - across * w, n, c0, 0.f);
				vertex(root + across * w, n, c0, 0.f);
				vertex(end, n, c2, 1.f);
				g->index.push_back(i); g->index.push_back(i + 1); g->index.push_back(i + 2);
			}
			else
			{
				vertex(root - across * w, n, c0, 0.f);
				vertex(root + across * w, n, c0, 0.f);
				vertex(mid - across * (w * .74f), n, c1, .5f);
				vertex(mid + across * (w * .74f), n, c1, .5f);
				vertex(end, n, c2, 1.f);
				g->index.push_back(i); g->index.push_back(i + 1); g->index.push_back(i + 3);
				g->index.push_back(i); g->index.push_back(i + 3); g->index.push_back(i + 2);
				g->index.push_back(i + 2); g->index.push_back(i + 3); g->index.push_back(i + 4);
			}
			if (stem)
			{
				// a head: a ring of petals round a yellow eye, tipped a little toward wherever it leans
				const f32 s = height * dice.In(.13f, .19f);
				const Vec3 up(0.f, 1.f, 0.f);
				const Vec3 fc = flower * dice.In(.88f, 1.08f);
				const Vec3 face = (up + lean * dice.In(.1f, .5f) + across * dice.In(-.3f, .3f)).normalize();
				Vec3 px = across - face * across.dotProduct(face);
				if (px.magnitudeSQR() < 1e-6f) px = Vec3(1.f, 0.f, 0.f);
				px = px.normalize();
				const Vec3 pz = face.cross(px);
				const int petals = 5 + (int)(dice.Unit() * 3.f);
				const f32 turn = dice.In(0.f, 6.2831853f);
				for (int k = 0; k < petals; k++)
				{
					const f32 a = turn + 6.2831853f * (f32)k / (f32)petals, h = 3.1415927f / (f32)petals * .82f;
					const Vec3 d0 = px * cosf(a) + pz * sinf(a), dl = px * cosf(a - h) + pz * sinf(a - h), dr = px * cosf(a + h) + pz * sinf(a + h);
					const uint32 j = (uint32)g->tVertex.size();
					vertex(end + d0 * (s * .16f), up, fc * .82f, 1.f);
					vertex(end + dl * (s * .72f) + face * (s * .10f), up, fc, 1.f);
					vertex(end + d0 * s + face * (s * .04f), up, fc, 1.f);
					vertex(end + dr * (s * .72f) + face * (s * .10f), up, fc, 1.f);
					g->index.push_back(j); g->index.push_back(j + 1); g->index.push_back(j + 2);
					g->index.push_back(j); g->index.push_back(j + 2); g->index.push_back(j + 3);
					g->index.push_back(j); g->index.push_back(j + 2); g->index.push_back(j + 1);
					g->index.push_back(j); g->index.push_back(j + 3); g->index.push_back(j + 2);
				}
				{
					const Vec3 eye(.95f, .72f, .12f);
					const uint32 j = (uint32)g->tVertex.size();
					vertex(end + face * (s * .07f), up, eye, 1.f);
					for (int k = 0; k < 6; k++) { const f32 a = 1.0471976f * (f32)k; vertex(end + (px * cosf(a) + pz * sinf(a)) * (s * .24f) + face * (s * .05f), up, eye * .85f, 1.f); }
					for (int k = 0; k < 6; k++) { g->index.push_back(j); g->index.push_back(j + 1 + k); g->index.push_back(j + 1 + (k + 1) % 6); g->index.push_back(j); g->index.push_back(j + 1 + (k + 1) % 6); g->index.push_back(j + 1 + k); }
				}
				top = std::max(top, end.y + s);
			}
		}

		Build();

		const f32 reach = spread + height * .6f;
		minBounds = Vec3(-reach, 0.f, -reach);
		maxBounds = Vec3(reach, top, reach);
		BoundingSphereCenter = (minBounds + maxBounds) * .5f;
		BoundingSphereRadius = minBounds.distance(BoundingSphereCenter);
	}
};
