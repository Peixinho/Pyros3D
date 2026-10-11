//============================================================================
// Name        : TreeTextures.cpp
// Description : See Tree.h. The pictures a species is drawn with, painted
//               here: a spray of its leaves on nothing (the alpha is the
//               cut-out), and bark that tiles.
//============================================================================

#include <Pyros3D/Assets/Renderable/Trees/Tree.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <vector>

namespace p3d {

	namespace {

		const f32 kPi = 3.14159265358979f;

		struct Dice
		{
			uint32 s;
			explicit Dice(const uint32 seed) : s(seed * 2654435761u + 0x9E3779B9u) { if (s == 0) s = 1; Next(); Next(); }
			uint32 Next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
			f32 Unit() { return (f32)(Next() & 0xFFFFFF) / (f32)0x1000000; }
			f32 In(const f32 a, const f32 b) { return a + (b - a) * Unit(); }
		};

		struct Canvas
		{
			int w, h;
			std::vector<f32> px;        // r g b a, alpha not premultiplied; painted back to front
			Canvas(const int w, const int h) : w(w), h(h), px((size_t)w * h * 4, 0.f) {}
			void Put(const int x, const int y, const Vec3 &c, const f32 a)
			{
				if (x < 0 || y < 0 || x >= w || y >= h || a <= 0.f) return;
				f32* p = &px[((size_t)y * w + x) * 4];
				const f32 keep = p[3] * (1.f - a), all = a + keep;
				if (all <= 1e-6f) return;
				p[0] = (c.x * a + p[0] * keep) / all; p[1] = (c.y * a + p[1] * keep) / all; p[2] = (c.z * a + p[2] * keep) / all;
				p[3] = std::min(1.f, all);
			}
		};

		// A leaf: from (x, y), `length` long in the direction `angle`, `width` across at its widest.
		// `shape` 0 is a blade (a pointed oval), 1 a lobed leaf (an oak's), 2 a needle.
		void Leaf(Canvas &c, const f32 x, const f32 y, const f32 angle, const f32 length, const f32 width, const Vec3 &colour, const int shape, Dice &dice)
		{
			const f32 ca = cosf(angle), sa = sinf(angle);
			const f32 reach = length + width;
			const int x0 = (int)floorf((x - reach) * c.w), x1 = (int)ceilf((x + reach) * c.w);
			const int y0 = (int)floorf((y - reach) * c.h), y1 = (int)ceilf((y + reach) * c.h);
			const f32 soft = 1.6f / (f32)c.w;
			const f32 lobes = 3.f + floorf(dice.Unit() * 2.f), lobePhase = dice.Unit() * 6.f;
			const f32 light = dice.In(.82f, 1.12f);
			for (int py = y0; py <= y1; py++)
				for (int px = x0; px <= x1; px++)
				{
					const f32 dx = ((f32)px + .5f) / (f32)c.w - x, dy = ((f32)py + .5f) / (f32)c.h - y;
					const f32 u = (dx * ca + dy * sa) / length;          // along: 0 the stalk, 1 the tip
					const f32 v = (-dx * sa + dy * ca) / (width * .5f);  // across: -1..1 at its widest
					if (u < -.02f || u > 1.02f) continue;
					f32 half;
					if (shape == 2) half = .9f * (1.f - powf(std::max(0.f, u), 3.f));
					else
					{
						half = powf(sinf(kPi * powf(std::min(1.f, std::max(0.f, u)), .72f)), .78f);
						if (shape == 1) half *= .70f + .30f * (.5f + .5f * cosf(u * lobes * 2.f * kPi + lobePhase));
					}
					const f32 edge = (half - fabsf(v)) * (width * .5f);
					if (edge <= -soft) continue;
					const f32 a = std::min(1.f, (edge + soft) / (2.f * soft));
					// lit along its length, a paler rib, a darker rim: a leaf, not a green blot
					f32 k = light * (.74f + .30f * std::min(1.f, std::max(0.f, u)));
					if (shape != 2)
					{
						k *= 1.f - .22f * powf(std::min(1.f, fabsf(v) / std::max(.05f, half)), 3.f);
						if (fabsf(v) < .10f) k *= 1.16f;
						// (veins: faint, off the rib)
						k *= 1.f + .05f * sinf((u * 9.f + fabsf(v) * 2.2f) * 2.f * kPi);
					}
					c.Put(px, py, Vec3(colour.x * k, colour.y * k, colour.z * k), a);
				}
		}

		void Line(Canvas &c, const f32 x0, const f32 y0, const f32 x1, const f32 y1, const f32 thick0, const f32 thick1, const Vec3 &colour)
		{
			const f32 len = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
			const int steps = std::max(2, (int)(len * c.w * 2.f));
			for (int i = 0; i <= steps; i++)
			{
				const f32 t = (f32)i / (f32)steps;
				const f32 x = x0 + (x1 - x0) * t, y = y0 + (y1 - y0) * t, r = (thick0 + (thick1 - thick0) * t) * .5f;
				const int rp = (int)ceilf(r * c.w) + 1;
				for (int oy = -rp; oy <= rp; oy++)
					for (int ox = -rp; ox <= rp; ox++)
					{
						const f32 d = sqrtf((f32)(ox * ox + oy * oy)) / (f32)c.w;
						const f32 a = std::min(1.f, std::max(0.f, (r - d) * c.w + .5f));
						if (a > 0.f) c.Put((int)(x * c.w) + ox, (int)(y * c.h) + oy, colour, a);
					}
			}
		}

		// What nothing was painted on takes the colour of the nearest paint (its alpha stays
		// nothing): a card drawn small averages its texels, and the green of a leaf's edge
		// must not be averaged with black.
		void Bleed(Canvas &c)
		{
			Vec3 mean(0.f, 0.f, 0.f); f32 weight = 0.f;
			for (size_t i = 0; i < c.px.size(); i += 4) { const f32 a = c.px[i + 3]; mean += Vec3(c.px[i], c.px[i + 1], c.px[i + 2]) * a; weight += a; }
			if (weight > 0.f) mean = mean / weight;
			for (int pass = 0; pass < 12; pass++)
			{
				std::vector<f32> next = c.px;
				bool any = false;
				for (int y = 0; y < c.h; y++)
					for (int x = 0; x < c.w; x++)
					{
						const size_t i = ((size_t)y * c.w + x) * 4;
						if (c.px[i + 3] > 0.f) continue;
						Vec3 sum(0.f, 0.f, 0.f); f32 n = 0.f;
						static const int ox[8] = { -1, 1, 0, 0, -1, 1, -1, 1 }, oy[8] = { 0, 0, -1, 1, -1, -1, 1, 1 };
						for (int k = 0; k < 8; k++)
						{
							const int xx = x + ox[k], yy = y + oy[k];
							if (xx < 0 || yy < 0 || xx >= c.w || yy >= c.h) continue;
							const size_t j = ((size_t)yy * c.w + xx) * 4;
							if (c.px[j + 3] > 0.f || next[j + 3] < 0.f) { sum += Vec3(c.px[j], c.px[j + 1], c.px[j + 2]); n += 1.f; }
						}
						if (n > 0.f) { const Vec3 v = sum / n; next[i] = v.x; next[i + 1] = v.y; next[i + 2] = v.z; next[i + 3] = -1.f; any = true; }
					}
				// (-1: coloured, still see-through; counted as paint by the next pass)
				for (size_t i = 0; i < next.size(); i += 4) if (next[i + 3] < 0.f) { c.px[i] = next[i]; c.px[i + 1] = next[i + 1]; c.px[i + 2] = next[i + 2]; c.px[i + 3] = 1e-6f; }
				if (!any) break;
			}
			for (size_t i = 0; i < c.px.size(); i += 4)
			{
				if (c.px[i + 3] <= 0.f) { c.px[i] = mean.x; c.px[i + 1] = mean.y; c.px[i + 2] = mean.z; }
				if (c.px[i + 3] <= 1e-5f) c.px[i + 3] = 0.f;
			}
		}

		std::shared_ptr<Texture> Upload(const Canvas &c, const bool repeat)
		{
			std::vector<uchar> bytes((size_t)c.w * c.h * 4);
			for (size_t i = 0; i < bytes.size(); i++) bytes[i] = (uchar)(std::min(1.f, std::max(0.f, c.px[i])) * 255.f + .5f);
			std::shared_ptr<Texture> t = std::make_shared<Texture>();
			t->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, c.w, c.h, true);
			t->UpdateData(&bytes[0]);
			t->UpdateMipmap();
			t->SetMinMagFilter(TextureFilter::LinearMipmapLinear, TextureFilter::Linear);
			if (repeat) t->SetRepeat(TextureRepeat::Repeat, TextureRepeat::Repeat);
			else t->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
			return t;
		}

		// A spray of broad leaves: a few twigs out of the middle, leaves on them and between,
		// thick at the heart and broken at the rim - a card of it must not read as a square.
		void BroadSpray(Canvas &c, Dice &dice, const Vec3 &green, const Vec3 &twig, const f32 leafLength, const f32 leafWidth, const int leaves, const int shape, const f32 hangDown, const Vec3 &pale)
		{
			const int twigs = 5;
			for (int t = 0; t < twigs; t++)
			{
				const f32 a = (f32)t / (f32)twigs * 2.f * kPi + dice.In(-.4f, .4f), r = dice.In(.26f, .40f);
				Line(c, .5f, .5f, .5f + cosf(a) * r, .5f + sinf(a) * r, .012f, .004f, twig);
			}
			for (int i = 0; i < leaves; i++)
			{
				// more of them toward the middle; none at the very edge of the card
				const f32 r = powf(dice.Unit(), .62f) * .40f, a = dice.Unit() * 2.f * kPi;
				const f32 x = .5f + cosf(a) * r, y = .5f + sinf(a) * r;
				// pointing out from the middle - or, on a hanging twig, down (the picture's bottom: +y)
				f32 dir = a + dice.In(-.9f, .9f);
				if (hangDown > 0.f) dir = kPi * .5f + dice.In(-.7f, .7f) * (1.f - hangDown * .5f);
				const f32 shadeBy = .62f + .38f * ((f32)i / (f32)leaves);          // the ones underneath are in the others' shade
				const f32 warm = dice.In(-.10f, .10f);
				Vec3 col = Vec3(green.x * (1.f + warm), green.y, green.z * (1.f - warm)) * shadeBy;
				if (dice.Unit() < .12f) col = Vec3(pale.x, pale.y, pale.z) * shadeBy;
				const f32 len = leafLength * dice.In(.72f, 1.2f);
				Leaf(c, x - cosf(dir) * len * .3f, y - sinf(dir) * len * .3f, dir, len, leafWidth * dice.In(.8f, 1.2f), col, shape, dice);
			}
		}

		// A conifer's bough: a twig from the card's left edge to its right, side twigs off it
		// forward on both sides, each thick with needles. Tapered to the tip.
		void Bough(Canvas &c, Dice &dice, const Vec3 &green, const Vec3 &twig)
		{
			Line(c, .02f, .5f, .97f, .5f, .016f, .004f, twig);
			const int sides = 15;
			for (int s = 0; s < sides; s++)
			{
				const f32 u = .05f + .86f * (f32)s / (f32)(sides - 1);
				for (int k = -1; k <= 1; k += 2)
				{
					const f32 reach = (.40f * (1.f - u * .78f) + .03f) * dice.In(.8f, 1.1f);
					const f32 a = (f32)k * dice.In(.75f, 1.05f);
					const f32 ex = u + cosf(a) * reach, ey = .5f + sinf(a) * reach;
					Line(c, u, .5f, ex, ey, .008f, .003f, twig);
					const int needles = 16 + (int)(reach * 70.f);
					for (int n = 0; n < needles; n++)
					{
						const f32 t = dice.Unit();
						const f32 x = u + (ex - u) * t, y = .5f + (ey - .5f) * t;
						const f32 na = a + (dice.Unit() < .5f ? 1.f : -1.f) * dice.In(.5f, 1.2f);
						const f32 k2 = dice.In(.55f, 1.15f);
						Leaf(c, x, y, na, dice.In(.035f, .060f), .009f, Vec3(green.x * k2, green.y * k2, green.z * k2), 2, dice);
					}
				}
			}
			// needles along the bough itself, so its spine is not a bare stick
			for (int n = 0; n < 150; n++)
			{
				const f32 u = dice.In(.03f, .95f);
				const f32 na = (dice.Unit() < .5f ? 1.f : -1.f) * dice.In(.5f, 1.3f);
				const f32 k2 = dice.In(.6f, 1.15f);
				Leaf(c, u, .5f, na, dice.In(.03f, .05f), .009f, Vec3(green.x * k2, green.y * k2, green.z * k2), 2, dice);
			}
		}

		// A pine's tufts: needles out of a point, several points on a short twig.
		void Tufts(Canvas &c, Dice &dice, const Vec3 &green, const Vec3 &twig)
		{
			const int tufts = 9;
			for (int t = 0; t < tufts; t++)
			{
				const f32 a = (f32)t / (f32)tufts * 2.f * kPi + dice.In(-.3f, .3f), r = t == 0 ? 0.f : dice.In(.14f, .30f);
				const f32 x = .5f + cosf(a) * r, y = .5f + sinf(a) * r;
				Line(c, .5f, .5f, x, y, .012f, .006f, twig);
				const int needles = 95;
				for (int n = 0; n < needles; n++)
				{
					const f32 na = a + dice.In(-1.5f, 1.5f);
					const f32 k2 = dice.In(.5f, 1.15f);
					Leaf(c, x, y, na, dice.In(.09f, .17f), .008f, Vec3(green.x * k2, green.y * k2, green.z * k2), 2, dice);
				}
			}
		}

		// A palm's frond: the rib along the card, leaflets off it both sides, forward, long in
		// the middle of its length.
		void Frond(Canvas &c, Dice &dice, const Vec3 &green, const Vec3 &rib)
		{
			const int leaflets = 46;
			for (int i = 0; i < leaflets; i++)
			{
				const f32 u = .04f + .93f * (f32)i / (f32)(leaflets - 1);
				for (int k = -1; k <= 1; k += 2)
				{
					const f32 len = (.10f + .36f * sinf(kPi * powf(u, .7f))) * dice.In(.85f, 1.1f);
					const f32 a = (f32)k * dice.In(.85f, 1.15f);
					const f32 k2 = dice.In(.72f, 1.12f);
					Leaf(c, u, .5f, a, len, .030f, Vec3(green.x * k2, green.y * k2, green.z * k2), 0, dice);
				}
			}
			Line(c, .0f, .5f, .99f, .5f, .016f, .005f, rib);
		}

		f32 Hash(const int x, const int y, const uint32 seed)
		{
			uint32 h = (uint32)x * 374761393u + (uint32)y * 668265263u + seed * 2246822519u;
			h = (h ^ (h >> 13)) * 1274126177u;
			return (f32)((h ^ (h >> 16)) & 0xFFFFFF) / (f32)0x1000000;
		}
		// Noise that tiles: `px` cells across, `py` down.
		f32 Noise(const f32 x, const f32 y, const int px, const int py, const uint32 seed)
		{
			const f32 fx = x * (f32)px, fy = y * (f32)py;
			const int x0 = (int)floorf(fx), y0 = (int)floorf(fy);
			const f32 tx = fx - (f32)x0, ty = fy - (f32)y0;
			const f32 sx = tx * tx * (3.f - 2.f * tx), sy = ty * ty * (3.f - 2.f * ty);
			auto at = [&](const int ix, const int iy) { return Hash(((ix % px) + px) % px, ((iy % py) + py) % py, seed); };
			const f32 a = at(x0, y0), b = at(x0 + 1, y0), cc = at(x0, y0 + 1), d = at(x0 + 1, y0 + 1);
			return (a + (b - a) * sx) + ((cc + (d - cc) * sx) - (a + (b - a) * sx)) * sy;
		}

		std::shared_ptr<Texture> MakeBark(const uint32 species)
		{
			const int w = 256, h = 512;
			Canvas c(w, h);
			for (int y = 0; y < h; y++)
				for (int x = 0; x < w; x++)
				{
					const f32 u = ((f32)x + .5f) / (f32)w, v = ((f32)y + .5f) / (f32)h;
					Vec3 col;
					if (species == TreeSpecies::Birch)
					{
						// white, with the dark dashes that run round it and the odd scar
						f32 k = .86f + .10f * Noise(u, v, 6, 12, 1);
						const f32 dash = Noise(u, v, 6, 70, 2) * Noise(u, v, 14, 96, 3);
						Vec3 base(k, k * .98f, k * .93f);
						const f32 dark = std::min(1.f, std::max(0.f, (dash - .42f) * 9.f));
						const f32 scar = std::min(1.f, std::max(0.f, (Noise(u, v, 5, 9, 4) - .80f) * 9.f));
						col = base.Lerp(Vec3(.13f, .11f, .10f), std::max(dark, scar));
					}
					else if (species == TreeSpecies::Palm)
					{
						// rings: the scars of fronds gone
						const f32 ring = .5f + .5f * sinf((v * 26.f + Noise(u, v, 5, 8, 5) * .8f) * 2.f * kPi);
						const f32 k = .50f + .22f * powf(ring, .6f) + .12f * Noise(u, v, 24, 48, 6);
						col = Vec3(k * .78f, k * .66f, k * .50f);
					}
					else
					{
						// ridges up the stem, broken across; plates on a pine
						const f32 warp = (Noise(u, v, 4, 6, 7) - .5f) * .16f;
						f32 ridge = fabsf(Noise(u + warp, v, 14, 5, 8) * 2.f - 1.f);
						ridge = powf(ridge, .55f);
						const f32 fine = Noise(u, v, 48, 28, 9), crack = Noise(u, v, 7, 26, 10);
						f32 k = .30f + .50f * ridge + .14f * fine - .16f * std::max(0.f, crack - .6f) * 2.5f;
						Vec3 wood = Vec3(.46f, .38f, .30f);
						if (species == TreeSpecies::ScotsPine) wood = Vec3(.62f, .40f, .26f);
						if (species == TreeSpecies::Spruce) wood = Vec3(.40f, .33f, .29f);
						if (species == TreeSpecies::Dead) wood = Vec3(.50f, .48f, .45f);
						if (species == TreeSpecies::Bush) wood = Vec3(.40f, .34f, .26f);
						col = wood * (k * 1.45f);
						// (moss and lichen in the low places, on what lives)
						if (species != TreeSpecies::Dead && species != TreeSpecies::ScotsPine)
						{
							const f32 moss = std::min(1.f, std::max(0.f, (Noise(u, v, 5, 7, 11) - .58f) * 4.f)) * (1.f - ridge) * .55f;
							col = col.Lerp(Vec3(.30f, .38f, .20f), moss);
						}
					}
					f32* p = &c.px[((size_t)y * w + x) * 4];
					p[0] = col.x; p[1] = col.y; p[2] = col.z; p[3] = 1.f;
				}
			return Upload(c, true);
		}

		std::shared_ptr<Texture> MakeLeaves(const uint32 species)
		{
			Dice dice(species * 7907u + 13u);
			const Vec3 twig(.26f, .20f, .14f);
			if (species == TreeSpecies::Spruce)
			{
				Canvas c(512, 512);
				Bough(c, dice, Vec3(.24f, .42f, .24f), Vec3(.22f, .17f, .12f));
				Bleed(c);
				return Upload(c, false);
			}
			if (species == TreeSpecies::ScotsPine)
			{
				Canvas c(512, 512);
				Tufts(c, dice, Vec3(.30f, .48f, .26f), Vec3(.30f, .22f, .15f));
				Bleed(c);
				return Upload(c, false);
			}
			if (species == TreeSpecies::Palm)
			{
				Canvas c(1024, 512);
				Frond(c, dice, Vec3(.30f, .50f, .18f), Vec3(.50f, .52f, .26f));
				Bleed(c);
				return Upload(c, false);
			}
			Canvas c(512, 512);
			if (species == TreeSpecies::Birch) BroadSpray(c, dice, Vec3(.40f, .58f, .18f), twig, .090f, .062f, 150, 0, .8f, Vec3(.66f, .70f, .22f));
			else if (species == TreeSpecies::Bush) BroadSpray(c, dice, Vec3(.26f, .46f, .16f), twig, .105f, .055f, 190, 0, 0.f, Vec3(.42f, .58f, .20f));
			else BroadSpray(c, dice, Vec3(.25f, .46f, .15f), twig, .135f, .085f, 150, 1, 0.f, Vec3(.44f, .58f, .18f));
			Bleed(c);
			return Upload(c, false);
		}

		// (never destroyed: a texture owns something of the device's, and what is destroyed
		// as the process exits is destroyed after the device is gone)
		std::mutex g_lock;
		std::map<uint32, std::shared_ptr<Texture> > &Kept(const int which)
		{
			static std::map<uint32, std::shared_ptr<Texture> >* kept[2] = { new std::map<uint32, std::shared_ptr<Texture> >(), new std::map<uint32, std::shared_ptr<Texture> >() };
			return *kept[which];
		}
	}

	std::shared_ptr<Texture> TreeMesh::BarkTexture(const uint32 species_)
	{
		const uint32 species = species_ < TreeSpecies::Count ? species_ : (uint32)TreeSpecies::Oak;
		std::lock_guard<std::mutex> g(g_lock);
		std::shared_ptr<Texture> &t = Kept(0)[species];
		if (!t) t = MakeBark(species);
		return t;
	}

	std::shared_ptr<Texture> TreeMesh::LeafTexture(const uint32 species_)
	{
		const uint32 species = species_ < TreeSpecies::Count ? species_ : (uint32)TreeSpecies::Oak;
		std::lock_guard<std::mutex> g(g_lock);
		std::shared_ptr<Texture> &t = Kept(1)[species];
		if (!t) t = MakeLeaves(species);
		return t;
	}
};
