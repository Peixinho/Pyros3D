// The two claims that separate global illumination from ambient light,
// measured rather than looked at.
//
//   1. COLOUR BLEEDS. A red wall tints a white floor that receives no
//      direct light of its own. If this fails there is no indirect
//      light, whatever the screenshot suggests.
//   2. LIGHT DOES NOT LEAK. A probe on the far side of a wall does not
//      contribute through it. This is the defect that makes a naive
//      probe grid unusable, and the Chebyshev visibility test is the
//      only reason it passes.
//
// A Cornell box is used because its answer is known in advance: the
// left wall is red, the right is green, and the white floor between
// them must come out reddish on the left and greenish on the right.
// Nobody has to interpret that.
//
//   c++ -std=c++17 -I include tools/tests/ddgi.cpp -o /tmp/ddgi \
//       -L build_gl -lPyrosEngine -Wl,-rpath,$PWD/build_gl
//   /tmp/ddgi
#include <Pyros3D/Rendering/GI/DDGIVolume.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>

using namespace p3d;
static int failures = 0;

static void check(bool c, const std::string &what, const std::string &extra = "")
{
	printf("%s  %s%s%s\n", c?"PASS":"FAIL", what.c_str(), extra.empty()?"":" - ", extra.c_str());
	if(!c) failures++;
}

static void AddQuad(RayScene &s, const Vec3 &a, const Vec3 &b, const Vec3 &c, const Vec3 &d, uint32 mat)
{
	// Wound so the face points INTO the box, which is what a room is:
	// you see a wall because its front faces you. The obvious winding
	// (a,b,c) gives the opposite - the outside of a solid - and every
	// surface then reads as a backface from inside, which is invisible
	// to shading (ShadeHit flips the normal toward the ray) and fatal
	// to probe classification, which uses exactly that signal to decide
	// whether a probe is buried.
	RayTriangle t1, t2;
	t1.v0=a; t1.v1=c; t1.v2=b; t1.materialIndex=mat;
	t2.v0=a; t2.v1=d; t2.v2=c; t2.materialIndex=mat;
	Vec3 n = (c-a).cross(b-a).normalize();
	t1.n0=t1.n1=t1.n2=n;
	t2.n0=t2.n1=t2.n2=n;
	s.triangles.push_back(t1);
	s.triangles.push_back(t2);
}

// A solid object inside the room: six faces pointing OUT of it, which
// is the opposite of AddQuad's room shell and is what makes the two
// distinguishable to a probe. A probe inside this box sees backfaces
// in every direction; a probe outside sees fronts.
static void AddSolidBox(RayScene &s, const Vec3 &c, const Vec3 &h, uint32 mat)
{
	const Vec3 p000(c.x-h.x, c.y-h.y, c.z-h.z), p100(c.x+h.x, c.y-h.y, c.z-h.z);
	const Vec3 p110(c.x+h.x, c.y+h.y, c.z-h.z), p010(c.x-h.x, c.y+h.y, c.z-h.z);
	const Vec3 p001(c.x-h.x, c.y-h.y, c.z+h.z), p101(c.x+h.x, c.y-h.y, c.z+h.z);
	const Vec3 p111(c.x+h.x, c.y+h.y, c.z+h.z), p011(c.x-h.x, c.y+h.y, c.z+h.z);
	// AddQuad winds inward, so each face is given in the order that
	// makes "inward for that quad" point away from the box centre.
	AddQuad(s, p001, p101, p100, p000, mat);   // -Y
	AddQuad(s, p010, p110, p111, p011, mat);   // +Y
	AddQuad(s, p000, p100, p110, p010, mat);   // -Z
	AddQuad(s, p101, p001, p011, p111, mat);   // +Z
	AddQuad(s, p001, p000, p010, p011, mat);   // -X
	AddQuad(s, p100, p101, p111, p110, mat);   // +X
}

static uint32 AddMaterial(RayScene &s, const Vec3 &albedo)
{
	RayMaterial m; m.albedo = albedo;
	s.materials.push_back(m);
	return (uint32)s.materials.size() - 1;
}

int main()
{
	// ---- a Cornell box, 10 units on a side, centred on the origin -----
	RayScene scene;
	const uint32 white = AddMaterial(scene, Vec3(0.75f, 0.75f, 0.75f));
	const uint32 red   = AddMaterial(scene, Vec3(0.75f, 0.06f, 0.06f));
	const uint32 green = AddMaterial(scene, Vec3(0.06f, 0.75f, 0.06f));
	const f32 H = 5.f;

	// floor, ceiling, back wall - white
	AddQuad(scene, Vec3(-H,-H,-H), Vec3( H,-H,-H), Vec3( H,-H, H), Vec3(-H,-H, H), white);
	AddQuad(scene, Vec3(-H, H, H), Vec3( H, H, H), Vec3( H, H,-H), Vec3(-H, H,-H), white);
	AddQuad(scene, Vec3(-H,-H, H), Vec3( H,-H, H), Vec3( H, H, H), Vec3(-H, H, H), white);
	// left wall red, right wall green
	AddQuad(scene, Vec3(-H,-H,-H), Vec3(-H,-H, H), Vec3(-H, H, H), Vec3(-H, H,-H), red);
	AddQuad(scene, Vec3( H,-H, H), Vec3( H,-H,-H), Vec3( H, H,-H), Vec3( H, H, H), green);
	scene.Build(4);
	check(scene.TriangleCount() == 10, "the box has 10 triangles",
		std::to_string(scene.TriangleCount()));

	// A point light INSIDE the box, near the ceiling.
	//
	// Not a directional light: the box is sealed, so a light outside it
	// illuminates nothing at all - every shadow ray from the floor hits
	// the ceiling. That is physically right and it is what the first
	// version of this test got wrong, measuring a correct zero.
	//
	// White, so the light itself contributes no colour. Any red or
	// green reaching the floor has bounced off a wall.
	std::vector<RayLight> lights(1);
	lights[0].isPoint = 1.f;
	lights[0].positionOrDirection = Vec3(0.f, 4.f, 0.f);
	lights[0].color = Vec3(6.f, 6.f, 6.f);
	lights[0].range = 40.f;

	DDGIVolume volume;
	check(volume.Allocate(Vec3(-4.f,-4.f,-4.f), Vec3(2.f,2.f,2.f), 5, 5, 5, 8, 16),
		"a 5x5x5 volume allocates");
	volume.SetSkyColor(Vec3(0.f,0.f,0.f)); // closed box - nothing outside

	// hysteresis 0: converge immediately, so the test measures the
	// estimator and not the blend schedule.
	for (uint32 f = 0; f < 8; f++)
		volume.Update(scene, lights, 256, f, 0.f);

	// ---- 1. colour bleeding ---------------------------------------------
	//
	// Sample the floor near the red wall and near the green wall, facing
	// up. Direct light is white and identical at both, so any difference
	// between them is indirect.
	const Vec3 up(0.f, 1.f, 0.f);
	const Vec3 nearRed   = volume.SampleIrradiance(Vec3(-3.5f, -4.4f, 0.f), up);
	const Vec3 nearGreen = volume.SampleIrradiance(Vec3( 3.5f, -4.4f, 0.f), up);
	const Vec3 middle    = volume.SampleIrradiance(Vec3( 0.0f, -4.4f, 0.f), up);

	printf("      floor near red wall   R=%.4f G=%.4f B=%.4f\n", nearRed.x, nearRed.y, nearRed.z);
	printf("      floor centre          R=%.4f G=%.4f B=%.4f\n", middle.x, middle.y, middle.z);
	printf("      floor near green wall R=%.4f G=%.4f B=%.4f\n", nearGreen.x, nearGreen.y, nearGreen.z);

	check(nearRed.x > 1e-4f, "the floor receives indirect light at all",
		"R = " + std::to_string(nearRed.x));
	check(nearRed.x > nearRed.y * 1.25f, "next to the red wall the floor is REDDER than it is green",
		"R/G = " + std::to_string(nearRed.x / std::max(1e-6f, nearRed.y)));
	check(nearGreen.y > nearGreen.x * 1.25f, "next to the green wall the floor is GREENER than it is red",
		"G/R = " + std::to_string(nearGreen.y / std::max(1e-6f, nearGreen.x)));
	// And the effect is local - the centre should be far more neutral
	// than either edge, or the "bleed" is just a global tint.
	{
		const f32 edgeSkew = nearRed.x / std::max(1e-6f, nearRed.y);
		const f32 midSkew  = middle.x / std::max(1e-6f, middle.y);
		check(midSkew < edgeSkew * 0.85f, "the bleed is local, not a uniform tint over the whole floor",
			"centre R/G = " + std::to_string(midSkew) + " vs edge " + std::to_string(edgeSkew));
	}

	// ---- 2. no leaking through a wall ------------------------------------
	//
	// A sealed box with a bright interior, and a sample point just
	// OUTSIDE it. Probes inside the box are metres away and full of
	// light; without the Chebyshev test they interpolate straight
	// through the wall and light the outside.
	{
		RayScene sealed;
		const uint32 bright = AddMaterial(sealed, Vec3(0.9f, 0.9f, 0.9f));
		const f32 B = 3.f;
		// A fully closed cube.
		AddQuad(sealed, Vec3(-B,-B,-B), Vec3( B,-B,-B), Vec3( B,-B, B), Vec3(-B,-B, B), bright);
		AddQuad(sealed, Vec3(-B, B, B), Vec3( B, B, B), Vec3( B, B,-B), Vec3(-B, B,-B), bright);
		AddQuad(sealed, Vec3(-B,-B, B), Vec3( B,-B, B), Vec3( B, B, B), Vec3(-B, B, B), bright);
		AddQuad(sealed, Vec3( B,-B,-B), Vec3(-B,-B,-B), Vec3(-B, B,-B), Vec3( B, B,-B), bright);
		AddQuad(sealed, Vec3(-B,-B,-B), Vec3(-B,-B, B), Vec3(-B, B, B), Vec3(-B, B,-B), bright);
		AddQuad(sealed, Vec3( B,-B, B), Vec3( B,-B,-B), Vec3( B, B,-B), Vec3( B, B, B), bright);
		sealed.Build(4);

		std::vector<RayLight> inner(1);
		inner[0].isPoint = 1.f;
		inner[0].positionOrDirection = Vec3(0.f, 0.f, 0.f);
		inner[0].color = Vec3(8.f, 8.f, 8.f);
		inner[0].range = 20.f;

		DDGIVolume v2;
		// The volume spans well beyond the box, so some probes are
		// inside it and some outside - exactly the configuration that
		// leaks.
		v2.Allocate(Vec3(-6.f,-6.f,-6.f), Vec3(2.f,2.f,2.f), 7, 7, 7, 8, 16);
		v2.SetSkyColor(Vec3(0.f,0.f,0.f));
		for (uint32 f = 0; f < 8; f++)
			v2.Update(sealed, inner, 256, f, 0.f);

		// Inside: should be brightly lit.
		const Vec3 insideL = v2.SampleIrradiance(Vec3(0.f, 0.f, 0.f), up);
		// Outside, just past the +X wall, facing away from the box.
		const Vec3 outsideL = v2.SampleIrradiance(Vec3(4.2f, 0.f, 0.f), Vec3(1.f,0.f,0.f));

		printf("      inside the sealed box  %.4f\n", insideL.x);
		printf("      outside, facing away   %.4f\n", outsideL.x);

		check(insideL.x > 0.05f, "the sealed box is lit inside",
			"L = " + std::to_string(insideL.x));
		check(outsideL.x < insideL.x * 0.10f,
			"and that light does NOT leak through the wall to the outside",
			"outside/inside = " + std::to_string(outsideL.x / std::max(1e-6f, insideL.x)));
	}

	// ---- 3. an unlit scene stays dark -------------------------------------
	//
	// Guards against the whole thing returning a constant, which would
	// pass a surprising number of the checks above.
	{
		DDGIVolume dark;
		dark.Allocate(Vec3(-4.f,-4.f,-4.f), Vec3(2.f,2.f,2.f), 3, 3, 3, 8, 16);
		dark.SetSkyColor(Vec3(0.f,0.f,0.f));
		std::vector<RayLight> none;
		for (uint32 f = 0; f < 4; f++)
			dark.Update(scene, none, 128, f, 0.f);
		const Vec3 l = dark.SampleIrradiance(Vec3(0.f,-4.4f,0.f), up);
		check(l.x < 1e-3f && l.y < 1e-3f && l.z < 1e-3f, "a scene with no lights produces no indirect light",
			"L = " + std::to_string(l.x));
	}

	// ---- multi-bounce ----------------------------------------------------
	//
	// A probe ray reports the light on whatever it hits. With one
	// bounce that is the DIRECT light there and nothing else, so a
	// room lit indirectly - a lamp aimed at the ceiling - comes out
	// nearly black however many rays are traced. Feeding the volume's
	// own irradiance back at each hit adds a bounce per update, for no
	// extra rays.
	//
	// The two things that can go wrong are opposite and both are
	// checked: it does nothing, or it never stops.
	{
		RayScene box;
		const uint32 grey = AddMaterial(box, Vec3(0.75f, 0.75f, 0.75f));
		const f32 B = 5.f;
		AddQuad(box, Vec3(-B,-B,-B), Vec3( B,-B,-B), Vec3( B,-B, B), Vec3(-B,-B, B), grey);
		AddQuad(box, Vec3(-B, B, B), Vec3( B, B, B), Vec3( B, B,-B), Vec3(-B, B,-B), grey);
		AddQuad(box, Vec3(-B,-B, B), Vec3( B,-B, B), Vec3( B, B, B), Vec3(-B, B, B), grey);
		AddQuad(box, Vec3( B,-B,-B), Vec3(-B,-B,-B), Vec3(-B, B,-B), Vec3( B, B,-B), grey);
		AddQuad(box, Vec3(-B,-B,-B), Vec3(-B,-B, B), Vec3(-B, B, B), Vec3(-B, B,-B), grey);
		AddQuad(box, Vec3( B,-B, B), Vec3( B,-B,-B), Vec3( B, B,-B), Vec3( B, B, B), grey);
		box.Build(4);

		std::vector<RayLight> lamp(1);
		lamp[0].isPoint = 1.f;
		lamp[0].positionOrDirection = Vec3(0.f, 3.f, 0.f);
		lamp[0].color = Vec3(3.f, 3.f, 3.f);
		lamp[0].range = 25.f;

		const Vec3 probePoint(0.f, -4.f, 0.f);

		DDGIVolume single, multi;
		single.Allocate(Vec3(-4,-4,-4), Vec3(2,2,2), 5,5,5, 8, 16);
		multi .Allocate(Vec3(-4,-4,-4), Vec3(2,2,2), 5,5,5, 8, 16);
		single.SetMultiBounce(0.f);

		f32 prev = 0.f, growth = 0.f;
		for (uint32 f = 0; f < 40; f++)
		{
			single.Update(box, lamp, 128, f, 0.f, 0);
			multi.Update(box, lamp, 128, f, 0.f, 0);
			const f32 e = multi.SampleIrradiance(probePoint, up).x;
			// The growth over the LAST few updates, once it should
			// have settled.
			if (f >= 36) growth = fmaxf(growth, e - prev);
			prev = e;
		}
		const f32 one = single.SampleIrradiance(probePoint, up).x;
		const f32 many = multi.SampleIrradiance(probePoint, up).x;
		printf("      one bounce %.4f, converged multi-bounce %.4f (x%.2f)\n",
			one, many, many / fmaxf(one, 1e-6f));

		check(many > one * 1.2f, "multi-bounce puts more light in the room than one bounce",
			"x" + std::to_string(many / fmaxf(one, 1e-6f)));

		// The series is sum of albedo^n, so a grey room of albedo 0.75
		// lands near 1/(1-0.75) = 4x. Bounded on both sides: too low
		// means the feedback is being swallowed, too high means it is
		// being counted more than once per update.
		check(many < one * 6.f, "and not more than the geometric series allows",
			"x" + std::to_string(many / fmaxf(one, 1e-6f)));

		// The failure that matters. Feedback is a loop; if its gain is
		// wrong it does not look wrong for a few frames, it grows
		// without bound and the scene whites out minutes later.
		check(growth < many * 0.01f, "and it converges rather than running away",
			"still growing by " + std::to_string(growth) + " per update at update 40");

		// Leak-through-a-wall is the other thing feedback amplifies -
		// whatever crosses a wall is re-injected every update - and it
		// is the sealed-box check above that covers it, now that
		// multi-bounce is on by default there. Not repeated here: this
		// volume lies entirely INSIDE its box, so a sample beyond the
		// floor is clamped back into the lit room and would measure a
		// leak of exactly 1.0 whatever the code did.
	}

	// ---- probe relocation -------------------------------------------------
	//
	// The last structural weakness of a fixed grid: a probe that lands
	// inside geometry. It sees the inside of that surface in every
	// direction, so its distance moments are near zero, and a point on
	// the far side of the wall passes the Chebyshev test against
	// them - while the backface weight's 0.2 floor lets a fifth of a
	// fully lit probe through. The wall stops the light and the probe
	// carries it across anyway.
	//
	// A room divided by a SOLID slab, with a probe plane landing inside
	// the slab. Solid, not a single quad: a probe exactly coplanar with
	// a zero-thickness quad never intersects it at all, so no statistic
	// sees anything wrong and there is nothing for relocation to act
	// on. Real walls have thickness, and that is the case worth fixing.
	{
		RayScene room;
		const uint32 grey = AddMaterial(room, Vec3(0.8f, 0.8f, 0.8f));
		const f32 B = 6.f;
		AddQuad(room, Vec3(-B,-B,-B), Vec3( B,-B,-B), Vec3( B,-B, B), Vec3(-B,-B, B), grey);
		AddQuad(room, Vec3(-B, B, B), Vec3( B, B, B), Vec3( B, B,-B), Vec3(-B, B,-B), grey);
		AddQuad(room, Vec3(-B,-B, B), Vec3( B,-B, B), Vec3( B, B, B), Vec3(-B, B, B), grey);
		AddQuad(room, Vec3( B,-B,-B), Vec3(-B,-B,-B), Vec3(-B, B,-B), Vec3( B, B,-B), grey);
		AddQuad(room, Vec3(-B,-B,-B), Vec3(-B,-B, B), Vec3(-B, B, B), Vec3(-B, B,-B), grey);
		AddQuad(room, Vec3( B,-B, B), Vec3( B,-B,-B), Vec3( B, B,-B), Vec3( B, B, B), grey);
		// The divider, straddling x = 0 and sealing the room in two.
		// Deliberately taller and deeper than the room, so it pokes
		// through the walls instead of meeting them. A slab that ends
		// exactly at a wall puts two triangles in the same plane -
		// one facing in, one facing out - and a ray landing there can
		// legitimately return either, so whether it counts as a
		// backface becomes a coin toss decided by traversal order.
		// The CPU and GPU walk the BVH differently and disagreed on
		// 18 of 125 probes because of it, which looked like a
		// relocation bug and was a modelling one.
		AddSolidBox(room, Vec3(0.f, 0.f, 0.f), Vec3(0.8f, B + 1.f, B + 1.f), grey);
		room.Build(4);

		// Lit on the +X side only. The -X half has no light of its own,
		// so anything measured there arrived through the slab.
		std::vector<RayLight> lamp(1);
		lamp[0].isPoint = 1.f;
		lamp[0].positionOrDirection = Vec3(3.f, 0.f, 0.f);
		lamp[0].color = Vec3(6.f, 6.f, 6.f);
		lamp[0].range = 30.f;

		// Spacing 3 from -6 puts a probe plane at exactly x = 0, inside
		// the slab.
		const Vec3 darkSide(-1.5f, 0.f, 0.f);       // just behind the slab
		const Vec3 litSide( 1.5f, 0.f, 0.f);
		const Vec3 towardSlab(-1.f, 0.f, 0.f);
		const Vec3 awayFromSlab(1.f, 0.f, 0.f);

		f32 darkOff = 0.f, darkOn = 0.f, litOff = 0.f, litOn = 0.f, maxOffset = 0.f;
		uint32 moved = 0, switched = 0;

		for (uint32 pass = 0; pass < 2; pass++)
		{
			DDGIVolume v;
			v.Allocate(Vec3(-6,-6,-6), Vec3(3,3,3), 5,5,5, 8, 16);
			v.SetProbeRelocation(pass == 1);
			for (uint32 f = 0; f < 12; f++)
				v.Update(room, lamp, 128, f, 0.f, 0);

			// Facing the slab from the dark side: the worst case, since
			// the backface term favours exactly the probes inside it.
			const f32 dark = v.SampleIrradiance(darkSide, awayFromSlab).x;
			const f32 lit = v.SampleIrradiance(litSide, towardSlab).x;
			if (pass == 0) { darkOff = dark; litOff = lit; }
			else
			{
				darkOn = dark; litOn = lit;
				const Vec3 bound = v.GetMaxProbeOffset();
				bool inCell = true;
				for (uint32 p = 0; p < v.ProbeCount(); p++)
				{
					const Vec4 &d = v.GetProbeData()[p];
					const f32 m = fmaxf(fabsf(d.x), fmaxf(fabsf(d.y), fabsf(d.z)));
					if (m > 1e-4f) moved++;
					if (d.w <= 0.5f) switched++;
					maxOffset = fmaxf(maxOffset, m);
					if (fabsf(d.x) > bound.x + 1e-4f || fabsf(d.y) > bound.y + 1e-4f ||
						fabsf(d.z) > bound.z + 1e-4f)
						inCell = false;
				}
				// A probe that wanders out of its own cell is being
				// weighted by a trilinear interpolation computed for
				// where it is not - a worse artefact than the one
				// relocation set out to fix.
				check(inCell, "every probe offset stays inside its own cell",
					"largest " + std::to_string(maxOffset) + " against bound "
					+ std::to_string(bound.x));
			}
		}

		printf("      buried probes: %u/%u relocated, %u switched off, largest offset %.3f\n",
			moved, 125u, switched, maxOffset);
		printf("      behind the slab: %.4f -> %.4f   (lit side %.4f -> %.4f)\n",
			darkOff, darkOn, litOff, litOn);

		check(moved + switched > 0, "probes inside the slab are noticed at all",
			std::to_string(moved) + " moved, " + std::to_string(switched) + " off");
		// The residue, not the whole leak: Chebyshev already rejects
		// most of what a buried probe would otherwise carry across, so
		// what is left here is small in absolute terms - a few tenths
		// of a percent of the lit side. It is still light arriving
		// through a sealed wall from nowhere, it is exactly what makes
		// a probe grid look subtly wrong rather than obviously broken,
		// and it is what relocation is for.
		check(darkOff > 1e-3f,
			"without relocation, light crosses a solid wall",
			"dark = " + std::to_string(darkOff) + " against lit "
			+ std::to_string(litOff) + " (" + std::to_string(darkOff/fmaxf(litOff,1e-6f)) + ")");
		check(darkOn < darkOff * 0.35f, "and relocation removes most of what is left",
			std::to_string(darkOff) + " -> " + std::to_string(darkOn));
		// Fixing a leak by putting the lights out would pass the check
		// above and be worthless.
		// It gets BRIGHTER, in fact: the probes inside the slab were
		// being averaged into the lit side's gather as near-black, and
		// moving them out stops that.
		check(litOn > litOff * 0.5f, "without darkening the side that is lit",
			"lit " + std::to_string(litOff) + " -> " + std::to_string(litOn));
	}

	// ---- content whose winding cannot be trusted ---------------------------
	//
	// Classification reads "was this surface hit from behind", which
	// assumes triangles are wound with their fronts toward the probes.
	// A scene wound the other way makes every probe look buried, and
	// switching them all off would delete the lighting of a scene that
	// merely has its normals backwards. The volume must notice and stop
	// classifying instead.
	{
		RayScene inverted;
		const uint32 grey = AddMaterial(inverted, Vec3(0.8f, 0.8f, 0.8f));
		const f32 B = 4.f;
		// AddQuad winds inward, so passing the corners in reverse winds
		// these outward - a room whose walls all face away from it.
		AddQuad(inverted, Vec3(-B,-B, B), Vec3( B,-B, B), Vec3( B,-B,-B), Vec3(-B,-B,-B), grey);
		AddQuad(inverted, Vec3(-B, B,-B), Vec3( B, B,-B), Vec3( B, B, B), Vec3(-B, B, B), grey);
		AddQuad(inverted, Vec3(-B, B, B), Vec3( B, B, B), Vec3( B,-B, B), Vec3(-B,-B, B), grey);
		AddQuad(inverted, Vec3(-B, B,-B), Vec3(-B,-B,-B), Vec3( B,-B,-B), Vec3( B, B,-B), grey);
		AddQuad(inverted, Vec3(-B, B,-B), Vec3(-B, B, B), Vec3(-B,-B, B), Vec3(-B,-B,-B), grey);
		AddQuad(inverted, Vec3( B, B, B), Vec3( B, B,-B), Vec3( B,-B,-B), Vec3( B,-B, B), grey);
		inverted.Build(4);

		std::vector<RayLight> lamp(1);
		lamp[0].isPoint = 1.f;
		lamp[0].positionOrDirection = Vec3(0.f, 2.f, 0.f);
		lamp[0].color = Vec3(5.f, 5.f, 5.f);
		lamp[0].range = 20.f;

		DDGIVolume v;
		v.Allocate(Vec3(-3,-3,-3), Vec3(1.5f,1.5f,1.5f), 5,5,5, 8, 16);
		for (uint32 f = 0; f < 6; f++)
			v.Update(inverted, lamp, 128, f, 0.f, 0);

		uint32 off = 0;
		for (uint32 p = 0; p < v.ProbeCount(); p++)
			if (v.GetProbeData()[p].w <= 0.5f) off++;
		const f32 lit = v.SampleIrradiance(Vec3(0.f,-2.f,0.f), up).x;
		printf("      inverted winding: %u/%u probes off, irradiance %.4f\n", off, v.ProbeCount(), lit);
		check(off == 0, "a scene with inverted winding disables classification rather than itself",
			std::to_string(off) + " probes switched off");
		check(lit > 0.05f, "and still lights the room", "E = " + std::to_string(lit));
	}

	// ---- the CPU path has to fit in a frame ------------------------------
	//
	// Every target without compute traces probes on the CPU: WebGL2 has
	// no compute stage, and the GLES3 profile the web, Android and
	// Raspberry Pi builds share is ES 3.0, where it does not exist
	// either. Those are the slowest machines the engine runs on and
	// they are the ones doing this work.
	//
	// A probe costs about 0.7 ms at 128 rays on a fast desktop, so the
	// fixed budget of 12 probes the editor and player used to pass was
	// 8.6 ms - over half a 60Hz frame here, and the entire frame on a
	// phone. A count cannot be right for both; a ceiling in
	// milliseconds can.
	{
		RayScene box;
		const uint32 grey = AddMaterial(box, Vec3(0.75f, 0.75f, 0.75f));
		const f32 B = 5.f;
		AddQuad(box, Vec3(-B,-B,-B), Vec3( B,-B,-B), Vec3( B,-B, B), Vec3(-B,-B, B), grey);
		AddQuad(box, Vec3(-B, B, B), Vec3( B, B, B), Vec3( B, B,-B), Vec3(-B, B,-B), grey);
		AddQuad(box, Vec3(-B,-B, B), Vec3( B,-B, B), Vec3( B, B, B), Vec3(-B, B, B), grey);
		AddQuad(box, Vec3( B,-B,-B), Vec3(-B,-B,-B), Vec3(-B, B,-B), Vec3( B, B,-B), grey);
		AddQuad(box, Vec3(-B,-B,-B), Vec3(-B,-B, B), Vec3(-B, B, B), Vec3(-B, B,-B), grey);
		AddQuad(box, Vec3( B,-B, B), Vec3( B,-B,-B), Vec3( B, B,-B), Vec3( B, B, B), grey);
		box.Build(4);

		std::vector<RayLight> lamp(1);
		lamp[0].isPoint = 1.f;
		lamp[0].positionOrDirection = Vec3(0.f, 3.f, 0.f);
		lamp[0].color = Vec3(4.f, 4.f, 4.f);
		lamp[0].range = 25.f;

		DDGIVolume v;
		v.Allocate(Vec3(-4,-4,-4), Vec3(2,2,2), 7,6,7, 8, 16);
		v.Update(box, lamp, 128, 0, 0.f, 4);        // warm the caches

		check(v.GetUpdateTimeBudget() == 0.f, "no ceiling by default");

		// Unbounded probe count, bounded time. The budget is generous
		// enough that a slow or loaded CI machine still passes, and
		// small enough that "it ignored the budget" cannot.
		const f32 budgetMs = 3.f;
		v.SetUpdateTimeBudget(budgetMs);
		f32 worstMs = 0.f;
		uint32 fewest = 0xFFFFFFFFu, most = 0;
		for (uint32 f = 0; f < 8; f++)
		{
			const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
			v.Update(box, lamp, 128, f, 0.9f, 0);   // 0 = every probe, if it could
			const f32 ms = (f32)std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - t0).count();
			worstMs = fmaxf(worstMs, ms);
			fewest = std::min(fewest, v.GetLastUpdatedProbeCount());
			most = std::max(most, v.GetLastUpdatedProbeCount());
		}
		printf("      %u-probe volume, %.0f ms ceiling: %u-%u probes per update, worst %.2f ms\n",
			v.ProbeCount(), budgetMs, fewest, most, worstMs);

		// The volume has 294 probes and asking for all of them would be
		// ~200 ms. Overshooting by one probe is expected - the budget
		// is checked between probes, not inside one - so the allowance
		// is the budget plus a probe's worth.
		check(worstMs < budgetMs * 3.f,
			"an update stops when its time is spent, whatever it was asked for",
			"worst " + std::to_string(worstMs) + " ms against a " + std::to_string(budgetMs) + " ms ceiling");
		check(most < v.ProbeCount(), "so it does not trace the whole volume",
			std::to_string(most) + " of " + std::to_string(v.ProbeCount()));
		// And it must still make progress: a ceiling that traced
		// nothing would pass the check above and never converge.
		check(fewest >= 1, "and always traces at least one probe",
			std::to_string(fewest));

		// A ceiling too small for even one probe still has to advance,
		// or a slow machine's volume stays black forever.
		v.SetUpdateTimeBudget(0.0001f);
		v.Update(box, lamp, 128, 9, 0.9f, 0);
		check(v.GetLastUpdatedProbeCount() == 1,
			"a ceiling smaller than one probe still traces one",
			std::to_string(v.GetLastUpdatedProbeCount()));
	}

	printf("\n%s  ddgi: %d failure(s)\n", failures?"FAIL":"PASS", failures);
	return failures ? 1 : 0;
}
