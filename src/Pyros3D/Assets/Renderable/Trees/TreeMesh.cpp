//============================================================================
// Name        : TreeMesh.cpp
// Description : See Tree.h. The growing of a tree: a stem, limbs off it by
//               the species' rules, twigs off those, and sprays of leaves
//               on the twigs - then the same skeleton skinned at a level of
//               detail.
//============================================================================

#include <Pyros3D/Assets/Renderable/Trees/Tree.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace p3d {

	namespace {

		const f32 kPi = 3.14159265358979f;

		// (a tree's own dice: the same seed is the same tree on every machine)
		struct Dice
		{
			uint32 s;
			explicit Dice(const uint32 seed) : s(seed * 2654435761u + 0x9E3779B9u) { if (s == 0) s = 1; Next(); Next(); }
			uint32 Next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
			f32 Unit() { return (f32)(Next() & 0xFFFFFF) / (f32)0x1000000; }                 // [0, 1)
			f32 In(const f32 a, const f32 b) { return a + (b - a) * Unit(); }
			f32 Around(const f32 v, const f32 part) { return v * (1.f + part * (Unit() * 2.f - 1.f)); }
			Vec3 OnSphere()
			{
				const f32 z = Unit() * 2.f - 1.f, a = Unit() * 2.f * kPi, r = sqrtf(std::max(0.f, 1.f - z * z));
				return Vec3(r * cosf(a), z, r * sinf(a));
			}
		};

		// The shape a crown fills: how long a limb is allowed to be, by how far up the
		// crown it leaves the stem (0 its bottom, 1 its top).
		namespace Crown { enum { Round = 0, Oval, Cone, Umbrella, Dome }; }

		struct Species
		{
			const char* name;
			f32 height, heightVar;          // metres, and the part of it that varies
			f32 trunkRadius;                // at the ground, as a part of the height
			f32 bare;                       // the part of the stem below the first limb
			f32 lean;                       // how far the stem wanders from straight up
			int levels;                     // limbs off the stem are level 1
			int count[4];                   // how many children a branch of each level has
			f32 angle[4];                   // degrees a child leaves its parent at
			f32 angleVar;
			f32 length[4];                  // a child's length, as a part of its parent's
			f32 up[4];                      // how a branch of each level bends as it grows: to the sky (+) or to the ground (-)
			f32 gnarl;                      // how crooked
			int crown;
			f32 spray, sprayVar;            // a leaf card's size, metres
			int sprays;                     // to a twig
			f32 round;                      // how far a leaf's normal is the crown's (1) and not straight up (0)
			Vec3 tint; f32 tintVar;         // what the leaf picture is multiplied by, and how far sprays differ
			f32 shade;                      // how dark the inside of the crown is (1: not at all)
		};

		const Species kSpecies[TreeSpecies::Count] = {
			// name        height  var   radius  bare  lean  lv  count           angle              var  length                   up                        gnarl crown           spray var  n  round  tint                       var   shade
			{ "Oak",        11.f,  .28f, .017f,  .26f, .07f, 3, { 7, 4, 4, 0 }, { 62, 48, 42, 0 }, 14,  { .52f, .56f, .46f, 0 }, { .05f, .16f, .10f, .0f },  .16f, Crown::Round,    0.92f, .30f, 13, .70f, Vec3(1.0f, 1.0f, 1.0f), .30f, .86f },
			{ "Birch",      12.f,  .25f, .011f,  .30f, .08f, 3, { 9, 4, 4, 0 }, { 42, 44, 50, 0 }, 12, { .34f, .52f, .52f, 0 }, { .16f, .0f, -.50f, .0f },  .10f, Crown::Oval,     0.80f, .30f, 12, .65f, Vec3(1.0f, 1.0f, 1.0f), .30f, .88f },
			{ "Spruce",     19.f,  .22f, .015f,  .06f, .01f, 1, { 64, 0, 0, 0 }, { 98, 0, 0, 0 },   6,  { .30f, .0f, 0, 0 },     { -.06f, .10f, .0f, .0f },  .04f, Crown::Cone,     1.f,   .15f, 5, .45f, Vec3(1.0f, 1.0f, 1.0f), .08f, .60f },
			{ "Scots pine", 21.f,  .20f, .014f,  .62f, .05f, 3, { 8, 4, 4, 0 }, { 66, 46, 40, 0 }, 14,  { .30f, .58f, .50f, 0 }, { .10f, .14f, .06f, .0f },  .20f, Crown::Umbrella, 0.85f, .30f, 9, .60f, Vec3(1.0f, 1.0f, 1.0f), .16f, .64f },
			{ "Palm",       11.f,  .25f, .018f,  1.0f, .22f, 0, { 0, 0, 0, 0 }, { 0, 0, 0, 0 },    0,   { 0, 0, 0, 0 },          { .0f, .0f, .0f, .0f },     .04f, Crown::Dome,     0.f,   .0f,  0, .35f, Vec3(1.0f, 1.12f, .74f), .12f, .80f },
			{ "Dead tree",  12.f,  .25f, .028f,  .30f, .08f, 3, { 6, 3, 3, 0 }, { 58, 46, 40, 0 }, 18,  { .46f, .52f, .40f, 0 }, { .08f, .10f, .04f, .0f },  .24f, Crown::Round,    0.f,   .0f,  0, .0f,  Vec3(1.f, 1.f, 1.f),    .0f,  1.f },
			{ "Bush",       1.9f,  .30f, .018f,  .0f,  .30f, 2, { 8, 5, 0, 0 }, { 44, 46, 0, 0 },  16,  { .74f, .56f, 0, 0 },    { .22f, .10f, .0f, .0f },   .16f, Crown::Dome,     0.66f, .25f, 10, .65f, Vec3(1.0f, 1.0f, 1.0f), .26f, .66f },
		};

		f32 CrownReach(const int shape, const f32 t)
		{
			const f32 x = std::min(1.f, std::max(0.f, t));
			switch (shape)
			{
			case Crown::Round:    return .36f + .64f * sinf(kPi * powf(x, .80f) * .88f + .10f);
			case Crown::Oval:     return .30f + .70f * sinf(kPi * powf(x, .62f) * .90f + .05f);
			case Crown::Cone:     return 1.f - .92f * powf(x, .86f);
			case Crown::Umbrella: return .55f + .45f * sinf(kPi * (.25f + x * .55f));
			default:              return .60f + .40f * sinf(kPi * x * .9f);
			}
		}

		// A branch: a line of points, each with the radius of the wood there.
		struct Limb
		{
			std::vector<Vec3> at;
			std::vector<f32> radius;
			int level;
			f32 length;
		};
		struct Spray { Vec3 at; Vec3 along; f32 size; f32 hang; };       // where a card of leaves goes, and the way its twig runs

		struct Skeleton
		{
			std::vector<Limb> limbs;
			std::vector<Spray> sprays;
			f32 height, trunkRadius;
			Vec3 crownCentre; f32 crownRadius;
		};

		Vec3 AnyPerpendicular(const Vec3 &d)
		{
			const Vec3 other = fabsf(d.y) < .9f ? Vec3(0.f, 1.f, 0.f) : Vec3(1.f, 0.f, 0.f);
			return d.cross(other).normalize();
		}
		// `d` turned by `angle` about `axis` (a unit vector).
		Vec3 Turned(const Vec3 &d, const Vec3 &axis, const f32 angle)
		{
			const f32 c = cosf(angle), s = sinf(angle);
			return d * c + axis.cross(d) * s + axis * (axis.dotProduct(d) * (1.f - c));
		}

		void Grow(Skeleton &K, const Species &S, Dice &dice, const Vec3 &from, Vec3 dir, const f32 length, const f32 radius, const int level, const int crownShape)
		{
			if (length < .05f) return;
			const int steps = level == 0 ? 12 : (level == 1 ? 8 : (level == 2 ? 6 : 4));
			Limb limb; limb.level = level; limb.length = length;
			Vec3 p = from;
			const f32 step = length / (f32)steps;
			const f32 tipRadius = level >= S.levels ? radius * .22f : radius * (level == 0 ? .30f : .36f);
			limb.at.push_back(p); limb.radius.push_back(radius);
			for (int i = 1; i <= steps; i++)
			{
				const f32 t = (f32)i / (f32)steps;
				// crooked, and bent by its own habit: to the light, or under its weight
				Vec3 wander = dice.OnSphere() * (S.gnarl * (level == 0 ? .45f : 1.f));
				dir = (dir + wander + Vec3(0.f, S.up[std::min(level, 3)] * (level == 0 ? 1.f : (.6f + t)), 0.f) * .32f).normalize();
				if (level == 0) dir = (dir + Vec3(0.f, .35f, 0.f)).normalize();          // a stem finds the sky again
				p = p + dir * step;
				limb.at.push_back(p);
				// (a stem flares at the ground)
				const f32 flare = level == 0 ? 1.f + .55f * powf(1.f - t, 6.f) : 1.f;
				limb.radius.push_back((radius + (tipRadius - radius) * powf(t, level == 0 ? .85f : 1.f)) * flare);
			}
			if (level == 0) limb.radius[0] = radius * 1.55f;
			const size_t me = K.limbs.size();
			K.limbs.push_back(limb);

			if (level < S.levels)
			{
				const int n = std::max(1, (int)(S.count[level] * dice.In(.85f, 1.15f) + .5f));
				const f32 first = level == 0 ? S.bare : .22f;
				f32 turn = dice.Unit() * 2.f * kPi;
				for (int c = 0; c < n; c++)
				{
					// where along this one, and how far round it (the golden angle: no child above another)
					const f32 t = first + (1.f - first) * std::min(.985f, ((f32)c + dice.In(.15f, .85f)) / (f32)n);
					turn += 2.39996f + dice.In(-.5f, .5f);
					const f32 ft = t * (f32)steps;
					const int i0 = std::min(steps - 1, (int)ft);
					const f32 f = ft - (f32)i0;
					const Limb &L = K.limbs[me];
					const Vec3 at = L.at[i0].Lerp(L.at[i0 + 1], f);
					const Vec3 along = (L.at[i0 + 1] - L.at[i0]).normalize();
					const f32 rHere = L.radius[i0] + (L.radius[i0 + 1] - L.radius[i0]) * f;
					const f32 crownT = level == 0 ? (t - first) / std::max(.001f, 1.f - first) : t;
					f32 a = (S.angle[level] + dice.In(-S.angleVar, S.angleVar)) * kPi / 180.f;
					// (limbs high on a stem reach up; a conifer's low boughs hang)
					if (level == 0) a *= (crownShape == Crown::Cone ? 1.f - .38f * crownT : 1.f - .30f * crownT * crownT);
					const Vec3 side = Turned(AnyPerpendicular(along), along, turn);
					const Vec3 childDir = (along * cosf(a) + side * sinf(a)).normalize();
					const f32 reach = level == 0 ? CrownReach(crownShape, crownT) : (.55f + .45f * sinf(kPi * std::min(1.f, t * 1.05f)));
					const f32 childLength = length * S.length[level] * reach * dice.In(.80f, 1.15f);
					const f32 childRadius = std::min(rHere * .62f, radius * (level == 0 ? .40f : .55f)) * dice.In(.8f, 1.f);
					Grow(K, S, dice, at, childDir, childLength, std::max(childRadius, .006f), level + 1, crownShape);
				}
				// (a stem or limb ends in a twig of its own: no bare stick out of the top of a crown)
				if (level > 0 || crownShape != Crown::Umbrella)
				{
					const Limb &L = K.limbs[me];
					Grow(K, S, dice, L.at.back(), dir, length * S.length[level] * .55f, std::max(L.radius.back() * .9f, .006f), level + 1, crownShape);
				}
			}
			else if (S.sprays > 0)
			{
				// leaves: along the outer part of a twig, and one at its end
				const Limb &L = K.limbs[me];
				const bool boughs = crownShape == Crown::Cone;
				for (int s = 0; s < S.sprays; s++)
				{
					const f32 t = boughs ? .10f + .90f * (f32)s / (f32)(S.sprays - 1) : (s == S.sprays - 1 ? 1.f : dice.In(.30f, .95f));
					const f32 ft = t * (f32)steps;
					const int i0 = std::min(steps - 1, (int)ft);
					const f32 f = ft - (f32)i0;
					Spray sp;
					sp.along = (L.at[i0 + 1] - L.at[i0]).normalize();
					sp.size = boughs ? std::max(.55f, length * .62f * (1.f - .35f * t)) * dice.Around(1.f, S.sprayVar) : dice.Around(S.spray, S.sprayVar);
					sp.at = L.at[i0].Lerp(L.at[i0 + 1], f) + dice.OnSphere() * (sp.size * (boughs ? .05f : .22f));
					sp.hang = S.up[std::min(level, 3)];
					K.sprays.push_back(sp);
				}
			}
		}

		void GrowPalm(Skeleton &K, const Species &S, Dice &dice)
		{
			// a stem that leans and comes back, ringed; fronds are made with the leaves
			Limb stem; stem.level = 0; stem.length = K.height;
			const int steps = 14;
			Vec3 p(0.f, -.6f, 0.f);
			const f32 heading = dice.Unit() * 2.f * kPi;
			const Vec3 leanTo(cosf(heading), 0.f, sinf(heading));
			const f32 lean = S.lean * dice.In(.4f, 1.3f);
			stem.at.push_back(p); stem.radius.push_back(K.trunkRadius * 1.45f);
			for (int i = 1; i <= steps; i++)
			{
				const f32 t = (f32)i / (f32)steps;
				const Vec3 dir = (Vec3(0.f, 1.f, 0.f) + leanTo * (lean * sinf(kPi * t) * 1.6f)).normalize();
				p = p + dir * (K.height / (f32)steps);
				stem.at.push_back(p);
				stem.radius.push_back(K.trunkRadius * (1.f - .32f * t) * (1.f + .55f * powf(1.f - t, 6.f)));
			}
			K.limbs.push_back(stem);
			const int fronds = 17;
			for (int f = 0; f < fronds; f++)
			{
				Spray sp;
				const f32 a = (f32)f * 2.39996f + dice.In(-.25f, .25f);
				const f32 lift = dice.In(-.15f, .95f);                 // from hanging to nearly upright: a head, not a wheel
				sp.along = Vec3(cosf(a) * cosf(lift * 1.2f), sinf(lift * 1.2f), sinf(a) * cosf(lift * 1.2f)).normalize();
				sp.at = p;
				sp.size = K.height * dice.In(.36f, .46f);
				sp.hang = lift;
				K.sprays.push_back(sp);
			}
		}

		void GrowTree(Skeleton &K, const uint32 species, const uint32 seed)
		{
			const Species &S = kSpecies[species];
			Dice dice(seed * 7919u + species * 104729u + 17u);
			K.height = dice.Around(S.height, S.heightVar);
			K.trunkRadius = K.height * S.trunkRadius * dice.In(.85f, 1.2f);
			if (species == TreeSpecies::Palm) GrowPalm(K, S, dice);
			else if (species == TreeSpecies::Bush)
			{
				// several stems out of one root, leaning apart
				const int stems = 5 + (int)(dice.Unit() * 3.f);
				for (int i = 0; i < stems; i++)
				{
					const f32 a = (f32)i * 2.39996f + dice.In(-.4f, .4f), out = dice.In(.25f, .85f);
					const Vec3 dir = Vec3(cosf(a) * out, 1.f, sinf(a) * out).normalize();
					Grow(K, S, dice, Vec3(cosf(a) * .08f, 0.f, sinf(a) * .08f), dir, K.height * dice.In(.7f, 1.05f), K.trunkRadius, 1, S.crown);
				}
			}
			else
			{
				const f32 heading = dice.Unit() * 2.f * kPi;
				const Vec3 dir = Vec3(cosf(heading) * S.lean, 1.f, sinf(heading) * S.lean).normalize();
				// (from under the ground: on a slope its foot must not show)
				Grow(K, S, dice, Vec3(0.f, -.6f, 0.f), dir, K.height * (S.levels > 0 ? .86f : 1.f) + .6f, K.trunkRadius, 0, S.crown);
			}
			// the crown: what the leaves fill (or, bare, the limbs' ends)
			Vec3 lo(1e9f, 1e9f, 1e9f), hi(-1e9f, -1e9f, -1e9f);
			auto take = [&lo, &hi](const Vec3 &p) {
				lo = Vec3(std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z));
				hi = Vec3(std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z));
			};
			if (!K.sprays.empty()) for (size_t i = 0; i < K.sprays.size(); i++) take(K.sprays[i].at);
			else for (size_t i = 0; i < K.limbs.size(); i++) if (K.limbs[i].level > 0) take(K.limbs[i].at.back());
			if (lo.x > hi.x) { lo = Vec3(-1.f, 0.f, -1.f); hi = Vec3(1.f, K.height, 1.f); }
			K.crownCentre = (lo + hi) * .5f;
			K.crownRadius = std::max(.5f, std::max(hi.x - lo.x, hi.z - lo.z) * .5f);
		}

		struct Out
		{
			PrimitiveGeometry* g;
			void Vertex(const Vec3 &p, const Vec3 &n, const Vec2 &uv, const Vec4 &c)
			{
				g->tVertex.push_back(p); g->tNormal.push_back(n); g->tTexcoord.push_back(uv); g->tColor.push_back(c);
			}
			void Triangle(const uint32 a, const uint32 b, const uint32 c) { g->index.push_back(a); g->index.push_back(b); g->index.push_back(c); }
			uint32 Count() const { return (uint32)g->tVertex.size(); }
		};

		// A limb as a tube: rings of `sides`, the bark wound round it and run along it.
		void Tube(Out &o, const Limb &L, const int sides, const f32 barkScale, const f32 shadeAtTip)
		{
			const size_t n = L.at.size();
			if (n < 2 || sides < 3) return;
			Vec3 side = AnyPerpendicular((L.at[1] - L.at[0]).normalize());
			f32 run = 0.f;
			const uint32 base = o.Count();
			for (size_t i = 0; i < n; i++)
			{
				const Vec3 along = (L.at[std::min(i + 1, n - 1)] - L.at[i > 0 ? i - 1 : 0]).normalize();
				// (the ring carried along the limb, not made afresh each step: no twist)
				side = (side - along * along.dotProduct(side)).normalize();
				const Vec3 other = along.cross(side);
				if (i > 0) run += L.at[i].distance(L.at[i - 1]);
				const f32 shade = 1.f - (1.f - shadeAtTip) * ((f32)i / (f32)(n - 1));
				for (int s = 0; s <= sides; s++)
				{
					const f32 a = (f32)s / (f32)sides * 2.f * kPi;
					const Vec3 out = side * cosf(a) + other * sinf(a);
					o.Vertex(L.at[i] + out * L.radius[i], out, Vec2((f32)s / (f32)sides * (L.level == 0 ? 2.f : 1.f), run * barkScale), Vec4(shade, shade, shade, 1.f));
				}
			}
			for (size_t i = 0; i + 1 < n; i++)
				for (int s = 0; s < sides; s++)
				{
					const uint32 a = base + (uint32)(i * (sides + 1) + s), b = a + 1, c = a + (uint32)(sides + 1), d = c + 1;
					o.Triangle(a, c, b); o.Triangle(b, c, d);
				}
		}

		// A card, there twice (see the header): its shading normal is given, not its own.
		void Card(Out &o, const Vec3 corner[4], const Vec2 uv[4], const Vec3 normal[4], const Vec4 &colour)
		{
			const uint32 base = o.Count();
			for (int k = 0; k < 4; k++) o.Vertex(corner[k], normal[k], uv[k], colour);
			o.Triangle(base, base + 1, base + 2); o.Triangle(base + 2, base + 3, base);
			const uint32 back = o.Count();
			for (int k = 0; k < 4; k++) o.Vertex(corner[k], normal[k], uv[k], colour);
			o.Triangle(back + 2, back + 1, back); o.Triangle(back, back + 3, back + 2);
		}

		Vec4 LeafColour(const Species &S, Dice &dice, const Skeleton &K, const Vec3 &at)
		{
			// its own tint, and the dark of the crown: least at the outside and the top
			const f32 v = dice.In(-S.tintVar, S.tintVar);
			const Vec3 t = Vec3(S.tint.x * (1.f + v * .9f), S.tint.y * (1.f + v * .35f), S.tint.z * (1.f - v * .8f));
			const Vec3 rel = at - K.crownCentre;
			const f32 out = std::min(1.f, sqrtf(rel.x * rel.x + rel.z * rel.z) / K.crownRadius);
			const f32 high = std::min(1.f, std::max(0.f, .5f + rel.y / std::max(.5f, K.crownRadius * 1.6f)));
			const f32 open = std::min(1.f, .45f + .55f * std::max(out * out, high));
			// each spray its own light: some catch the sun full and go pale and yellow, some sit in
			// another's shade - a crown is not one green
			const f32 lot = dice.Unit();
			const f32 own = lot > .78f ? 1.24f : (lot < .22f ? .84f : 1.f);
			const f32 shade = (S.shade + (1.f - S.shade) * open) * own;
			const f32 gold = lot > .70f ? .30f : 0.f;
			return Vec4(t.x * shade * (1.f + gold), t.y * shade * (1.f + gold * .5f), t.z * shade * (1.f - gold), 1.f);
		}
		Vec3 LeafNormal(const Species &S, const Skeleton &K, const Vec3 &at)
		{
			Vec3 out = at - K.crownCentre;
			out.y *= .8f;
			if (out.magnitudeSQR() < 1e-6f) out = Vec3(0.f, 1.f, 0.f);
			// (never facing down: the underside of a crown is lit through its leaves, and a card
			// whose normal points at the ground is lit by nothing but what the grass throws back)
			Vec3 n = (out.normalize() * (S.round * .75f) + Vec3(0.f, 1.f, 0.f) * (1.f - S.round * .40f)).normalize();
			if (n.y < .25f) n = Vec3(n.x, .25f, n.z).normalize();
			return n;
		}
	}

	TreeMesh::TreeMesh(const uint32 species_, const uint32 seed, const uint32 part, const uint32 detail, const uint32 sprays_)
		: species(species_ < TreeSpecies::Count ? species_ : (uint32)TreeSpecies::Oak), seed(seed), part(part), detail(std::min<uint32>(detail, 2)),
		height(0.f), crownRadius(0.f), trunkRadius(0.f)
	{
		sprays = std::max<uint32>(1, std::min<uint32>(4, sprays_));
		isFlipped = false;
		isSmooth = false;
		calculateTangentBitangent = false;

		const Species &S = kSpecies[species];
		Skeleton K;
		GrowTree(K, species, seed);
		height = K.height; crownRadius = K.crownRadius; trunkRadius = K.trunkRadius;

		Out o = { geometry };
		Dice dice(seed * 31337u + species * 977u + 5u);
		const uint32 d = this->detail;
		if (part == TreePart::Wood)
		{
			// what of the wood is seen from how far: twigs under the leaves go first
			const int deepest = d == 0 ? 3 : (d == 1 ? (species == TreeSpecies::Dead ? 2 : 1) : (species == TreeSpecies::Dead ? 1 : 0));
			for (size_t i = 0; i < K.limbs.size(); i++)
			{
				const Limb &L = K.limbs[i];
				if (L.level > deepest) continue;
				if (species == TreeSpecies::Bush && d > 0 && L.level > 1) continue;
				int sides = L.level == 0 ? 12 : (L.level == 1 ? 7 : (L.level == 2 ? 5 : 3));
				if (d == 1) sides = L.level == 0 ? 8 : 4;
				if (d == 2) sides = L.level == 0 ? 5 : 3;
				// (wood under a crown is in its shade)
				Tube(o, L, sides, 1.f / std::max(.08f, L.radius[0] * 2.f * kPi * (L.level == 0 ? 1.f : 2.f)), L.level == 0 ? (S.sprays > 0 ? .70f : 1.f) : .82f);
			}
		}
		else if (species == TreeSpecies::Palm)
		{
			// a frond: a strip that leaves the head upward and comes over under its own weight
			const int pieces = d == 0 ? 7 : (d == 1 ? 4 : 2);
			const size_t fronds = d == 2 ? (K.sprays.size() + 1) / 2 : K.sprays.size();
			for (size_t f = 0; f < fronds; f++)
			{
				const Spray &sp = K.sprays[f];
				const f32 len = sp.size, width = len * .30f;
				Vec3 dir = sp.along;
				const Vec3 flat = Vec3(dir.x, 0.f, dir.z).magnitudeSQR() > 1e-5f ? Vec3(dir.x, 0.f, dir.z).normalize() : Vec3(1.f, 0.f, 0.f);
				const Vec3 across = Vec3(0.f, 1.f, 0.f).cross(flat).normalize();
				const Vec4 colour = LeafColour(S, dice, K, sp.at + dir * (len * .5f));
				Vec3 p = sp.at;
				Vec3 lastL, lastR, lastN;
				for (int i = 0; i <= pieces; i++)
				{
					const f32 t = (f32)i / (f32)pieces;
					const f32 w = width * (.35f + .65f * sinf(kPi * std::min(1.f, t * .9f + .1f)));
					const Vec3 n = across.cross(dir).normalize();
					const Vec3 up = (n.y < 0.f ? n.negate() : n);
					// (a shallow V: the two halves of a frond are not one flat blade)
					const Vec3 L0 = p - across * w + up * (w * .28f), R0 = p + across * w + up * (w * .28f);
					if (i > 0)
					{
						// two halves, each a card
						const Vec3 midA = (lastL + lastR) * .5f - lastN * (width * .0f), midB = (L0 + R0) * .5f;
						const f32 v0 = (f32)(i - 1) / (f32)pieces, v1 = t;
						{
							const Vec3 c[4] = { lastL, (lastL + lastR) * .5f - up * (w * .28f), (L0 + R0) * .5f - up * (w * .28f), L0 };
							const Vec2 uv[4] = { Vec2(v0, 0.f), Vec2(v0, .5f), Vec2(v1, .5f), Vec2(v1, 0.f) };
							const Vec3 nn = (up + Vec3(0.f, .6f, 0.f) - across * .3f).normalize();
							const Vec3 ns[4] = { nn, nn, nn, nn };
							Card(o, c, uv, ns, colour);
						}
						{
							const Vec3 c[4] = { (lastL + lastR) * .5f - up * (w * .28f), lastR, R0, (L0 + R0) * .5f - up * (w * .28f) };
							const Vec2 uv[4] = { Vec2(v0, .5f), Vec2(v0, 1.f), Vec2(v1, 1.f), Vec2(v1, .5f) };
							const Vec3 nn = (up + Vec3(0.f, .6f, 0.f) + across * .3f).normalize();
							const Vec3 ns[4] = { nn, nn, nn, nn };
							Card(o, c, uv, ns, colour);
						}
						(void)midA; (void)midB;
					}
					lastL = L0; lastR = R0; lastN = up;
					p = p + dir * (len / (f32)pieces);
					// its weight: the further out, the more it comes down
					dir = (dir + Vec3(0.f, -1.f, 0.f) * ((.10f + .30f * t) * (1.25f - sp.hang * .5f))).normalize();
				}
			}
		}
		else if (S.sprays > 0)
		{
			// fewer sprays further off, each bigger: the crown keeps what it covers
			const f32 keep = d == 0 ? 1.f : (d == 1 ? .55f : .22f);
			const f32 grow = d == 0 ? 1.f : (d == 1 ? 2.0f : 3.9f);
			const bool bough = S.crown == Crown::Cone;               // a conifer's spray lies along its branch
			for (size_t i = 0; i < K.sprays.size(); i++)
			{
				const Spray &sp = K.sprays[i];
				// (the same sprays are kept at every level: the dice are thrown for all of them)
				const f32 lot = dice.Unit();
				// (a far crown is fewer, bigger cards with none of the gaps between sprays that darken a
				// near one: it is given that darkness, or the same wood is paler the further off it is)
				Vec4 colour = LeafColour(S, dice, K, sp.at);
				{ const f32 k = d == 0 ? 1.f : (d == 1 ? .80f : .68f); colour = Vec4(colour.x * k, colour.y * k, colour.z * k, 1.f); }
				const Vec3 normal = LeafNormal(S, K, sp.at);
				const f32 roll = dice.Unit() * 2.f * kPi;
				const Vec3 random = dice.OnSphere();
				if (lot > keep) continue;
				const f32 half = sp.size * grow * .5f;
				const Vec3 ns[4] = { normal, normal, normal, normal };
				// (which spray of the sheet this card shows)
				const f32 cell = 1.f / (f32)sprays;
				const f32 u0 = (f32)(uint32)(dice.Unit() * (f32)sprays) * cell, v0 = (f32)(uint32)(dice.Unit() * (f32)sprays) * cell;
				const Vec2 uv[4] = { Vec2(u0, v0 + cell), Vec2(u0 + cell, v0 + cell), Vec2(u0 + cell, v0), Vec2(u0, v0) };
				if (bough)
				{
					// along the branch and a little down, lying flat; and a second, on edge, hanging under it
					Vec3 along = (sp.along + Vec3(0.f, -.18f, 0.f)).normalize();
					Vec3 across = along.cross(Vec3(0.f, 1.f, 0.f));
					if (across.magnitudeSQR() < 1e-5f) across = Vec3(1.f, 0.f, 0.f);
					across.normalizeSelf();
					const Vec3 start = sp.at - along * (half * .5f);
					const Vec3 a[4] = { start - across * half, start + across * half, start + across * half + along * (half * 2.f), start - across * half + along * (half * 2.f) };
					const Vec2 uva[4] = { Vec2(0.f, 0.f), Vec2(0.f, 1.f), Vec2(1.f, 1.f), Vec2(1.f, 0.f) };
					Card(o, a, uva, ns, colour);
					if (d < 2)
					{
						const Vec3 down = across.cross(along).y > 0.f ? across.cross(along).negate() : across.cross(along);
						const Vec3 b[4] = { start + down * (half * .9f), start - down * (half * .15f), start - down * (half * .15f) + along * (half * 2.f), start + down * (half * .9f) + along * (half * 2.f) };
						Card(o, b, uva, ns, Vec4(colour.x * .86f, colour.y * .86f, colour.z * .86f, 1.f));
					}
					continue;
				}
				// a card facing mostly out of the crown, turned about that by its own roll
				Vec3 facing = (normal * .55f + random * .75f).normalize();
				// (hanging twigs carry hanging leaves: the card stands, its picture's top up)
				if (sp.hang < -.2f) facing = Vec3(facing.x, facing.y * .25f, facing.z).normalize();
				Vec3 right = Turned(AnyPerpendicular(facing), facing, sp.hang < -.2f ? 0.f : roll);
				if (sp.hang < -.2f)
				{
					right = facing.cross(Vec3(0.f, 1.f, 0.f));
					if (right.magnitudeSQR() < 1e-5f) right = Vec3(1.f, 0.f, 0.f);
					right.normalizeSelf();
				}
				const Vec3 upv = right.cross(facing).normalize();
				const Vec3 centre = sp.hang < -.2f ? sp.at - Vec3(0.f, half * .55f, 0.f) : sp.at;
				const Vec3 c[4] = { centre - right * half - upv * half, centre + right * half - upv * half, centre + right * half + upv * half, centre - right * half + upv * half };
				Card(o, c, uv, ns, colour);
				// (near, a second card across the first: a spray has depth from every side)
				if (d == 0)
				{
					const Vec3 r2 = facing, f2 = right;
					const Vec3 c2[4] = { centre - r2 * half - upv * half, centre + r2 * half - upv * half, centre + r2 * half + upv * half, centre - r2 * half + upv * half };
					(void)f2;
					Card(o, c2, uv, ns, Vec4(colour.x * .90f, colour.y * .90f, colour.z * .90f, 1.f));
				}
			}
		}

		// (a renderable with nothing in it has no buffer to make: a dead tree's leaves, asked for)
		if (geometry->tVertex.empty())
		{
			const Vec3 zero(0.f, 0.f, 0.f);
			for (int k = 0; k < 3; k++) o.Vertex(zero, Vec3(0.f, 1.f, 0.f), Vec2(0.f, 0.f), Vec4(1.f, 1.f, 1.f, 1.f));
			o.Triangle(0, 1, 2);
		}

		Build();

		Vec3 lo(1e9f, 1e9f, 1e9f), hi(-1e9f, -1e9f, -1e9f);
		for (size_t i = 0; i < geometry->tVertex.size(); i++)
		{
			const Vec3 &p = geometry->tVertex[i];
			lo = Vec3(std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z));
			hi = Vec3(std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z));
		}
		minBounds = lo; maxBounds = hi;
		BoundingSphereCenter = (lo + hi) * .5f;
		BoundingSphereRadius = lo.distance(BoundingSphereCenter);
	}

	const char* TreeMesh::SpeciesName(const uint32 species)
	{
		return species < TreeSpecies::Count ? kSpecies[species].name : "";
	}
};
