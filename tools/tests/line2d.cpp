//============================================================================
// Name        : line2d.cpp
// Description : Line2D's ribbon, and SpriteMask2D's shape, without a window.
//
//   c++ -std=c++17 -I include tools/tests/line2d.cpp -o /tmp/line2d \
//       -L build_vulkan -lPyrosEngine -Wl,-rpath,$PWD/build_vulkan && /tmp/line2d
//============================================================================

#include <Pyros3D/Rendering/Components/Sprite2D/Line2D.h>
#include <Pyros3D/Rendering/Components/Sprite2D/SpriteMask2D.h>
#include <Pyros3D/Assets/SpriteSheet2D/SpriteSheet2D.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace p3d;

static int failures = 0;
static void check(bool cond, const std::string &what)
{
	printf("%s  %s\n", cond ? "PASS" : "FAIL", what.c_str());
	if (!cond) failures++;
}
static void nearly(const f32 got, const f32 want, const std::string &what, const f32 tol = 1e-3f)
{
	const bool ok = fabsf(got - want) <= tol;
	printf("%s  %s", ok ? "PASS" : "FAIL", what.c_str());
	if (!ok) printf(" (got %.4f, want %.4f)", got, want);
	printf("\n");
	if (!ok) failures++;
}

int main()
{
	const Vec4 white(1.f, 1.f, 1.f, 1.f);

	// --- the ribbon ---------------------------------------------------------
	{
		std::vector<Vec2> two;
		two.push_back(Vec2(0.f, 0.f)); two.push_back(Vec2(4.f, 0.f));
		SpriteMesh2D m;
		Line2D::BuildMesh(two, false, 1.f, 1.f, white, white, NULL, m);
		check(m.QuadCount() == 1 && m.vertex.size() == 4, "two points are one quad");
		nearly(m.minBounds.y, -0.5f, "half its width below the line");
		nearly(m.maxBounds.y, 0.5f, "and half above");
		nearly(m.maxBounds.x, 4.f, "ending where the line ends");

		std::vector<Vec2> one;
		one.push_back(Vec2(1.f, 1.f));
		SpriteMesh2D none;
		Line2D::BuildMesh(one, false, 1.f, 1.f, white, white, NULL, none);
		check(none.vertex.empty(), "one point is no line");
		std::vector<Vec2> same;
		same.push_back(Vec2(1.f, 1.f)); same.push_back(Vec2(1.f, 1.f));
		Line2D::BuildMesh(same, false, 1.f, 1.f, white, white, NULL, none);
		check(none.vertex.empty(), "nor are two points in the same place");

		// A right angle: the corner is mitred, so the ribbon keeps its width.
		std::vector<Vec2> bend;
		bend.push_back(Vec2(0.f, 0.f)); bend.push_back(Vec2(4.f, 0.f)); bend.push_back(Vec2(4.f, 4.f));
		SpriteMesh2D b;
		Line2D::BuildMesh(bend, false, 1.f, 1.f, white, white, NULL, b);
		check(b.QuadCount() == 2, "three points are two quads");
		nearly(b.maxBounds.x, 4.5f, "the outside of the corner reaches half a width past it");
		nearly(b.minBounds.y, -0.5f, "both ways");
		// The two quads share the corner's two vertices exactly: no gap, no overlap.
		check(fabsf(b.vertex[1].x - b.vertex[4].x) < 1e-5f && fabsf(b.vertex[1].y - b.vertex[4].y) < 1e-5f
			&& fabsf(b.vertex[2].x - b.vertex[7].x) < 1e-5f && fabsf(b.vertex[2].y - b.vertex[7].y) < 1e-5f,
			"and the two quads meet at the corner");

		// A hairpin does not grow a spike to the horizon.
		std::vector<Vec2> pin;
		pin.push_back(Vec2(0.f, 0.f)); pin.push_back(Vec2(4.f, 0.f)); pin.push_back(Vec2(0.f, 0.1f));
		SpriteMesh2D h;
		Line2D::BuildMesh(pin, false, 1.f, 1.f, white, white, NULL, h);
		check(h.maxBounds.x < 6.f, "a hairpin's corner is held to three half-widths");

		// Closed: the last point is joined to the first.
		std::vector<Vec2> square;
		square.push_back(Vec2(0.f, 0.f)); square.push_back(Vec2(2.f, 0.f)); square.push_back(Vec2(2.f, 2.f)); square.push_back(Vec2(0.f, 2.f));
		SpriteMesh2D open, closed;
		Line2D::BuildMesh(square, false, 0.2f, 0.2f, white, white, NULL, open);
		Line2D::BuildMesh(square, true, 0.2f, 0.2f, white, white, NULL, closed);
		check(open.QuadCount() == 3 && closed.QuadCount() == 4, "closed, a square has its fourth side");
		nearly(closed.minBounds.x, -0.1f, "with every corner mitred");

		// Taper and gradient.
		std::vector<Vec2> three;
		three.push_back(Vec2(0.f, 0.f)); three.push_back(Vec2(1.f, 0.f)); three.push_back(Vec2(4.f, 0.f));
		SpriteMesh2D t;
		Line2D::BuildMesh(three, false, 2.f, 0.f, Vec4(1.f, 0.f, 0.f, 1.f), Vec4(0.f, 0.f, 1.f, 0.f), NULL, t);
		nearly(t.vertex[3].y - t.vertex[0].y, 2.f, "full width at the start");
		nearly(t.vertex[6].y - t.vertex[5].y, 0.f, "none at the end");
		nearly(t.vertex[2].y - t.vertex[1].y, 1.5f, "and a quarter of the way along, three quarters of it - by length, not by point");
		nearly(t.color[0].x, 1.f, "the start's colour at the start");
		nearly(t.color[5].z, 1.f, "the end's at the end");
		nearly(t.color[1].w, 0.75f, "blended by length in between");
		nearly(t.texcoord[1].x, 0.25f, "u runs along the length");

		// An image along it: stretched once, or repeated by distance.
		SpriteMesh2D once, tiled;
		Line2D::BuildMesh(three, false, 1.f, 1.f, white, white, NULL, once);
		Line2D::BuildMesh(three, false, 1.f, 1.f, white, white, NULL, tiled, 0.5f);
		nearly(once.texcoord[5].x, 1.f, "an image stretched once ends at u = 1");
		nearly(tiled.texcoord[5].x, 8.f, "repeated every half unit, a line 4 long has eight copies");
		nearly(tiled.texcoord[1].x, 2.f, "two of them in its first unit");
		nearly(tiled.texcoord[3].y - tiled.texcoord[0].y, 1.f, "and v still runs once across the width");

		// Ages fade a trail.
		std::vector<f32> ages;
		ages.push_back(0.f); ages.push_back(0.5f); ages.push_back(1.f);
		SpriteMesh2D f;
		Line2D::BuildMesh(three, false, 1.f, 1.f, white, white, &ages, f);
		nearly(f.color[0].w, 1.f, "a trail's newest point is solid");
		nearly(f.color[1].w, 0.5f, "half way through its life, half there");
		nearly(f.color[5].w, 0.f, "and at the end of it, gone");
	}

	// --- a trail lays points behind a moving object ---------------------------
	{
		SceneGraph scene;
		std::shared_ptr<GameObject> go = std::make_shared<GameObject>();
		go->SetName("Comet");
		std::shared_ptr<Line2D> line = std::make_shared<Line2D>();
		line->SetTrail(true);
		line->SetTrailLifetime(0.5f);
		line->SetTrailMinDistance(0.25f);
		go->Add(std::static_pointer_cast<IComponent>(line));
		scene.Add(go);
		f64 time = 0.0;
		for (int i = 0; i < 30; i++)
		{
			time += 1.0 / 60.0;
			go->SetPosition(Vec3((f32)i * 0.1f, 0.f, 0.f));        // 6 units a second
			scene.Update(time);
		}
		// Half a second at 6 units a second is 3 units, a point each 0.25.
		check(line->TrailPointCount() >= 10 && line->TrailPointCount() <= 14, "a moving object lays a point each minimum distance");
		const uint32 moving = line->TrailPointCount();
		for (int i = 0; i < 15; i++) { time += 1.0 / 60.0; scene.Update(time); }
		check(line->TrailPointCount() < moving && line->TrailPointCount() > 0, "standing still, the trail shortens from its old end");
		for (int i = 0; i < 30; i++) { time += 1.0 / 60.0; scene.Update(time); }
		check(line->TrailPointCount() == 0, "and is gone after its lifetime");
		for (int i = 0; i < 10; i++) { time += 1.0 / 60.0; go->SetPosition(Vec3(10.f + (f32)i, 0.f, 0.f)); scene.Update(time); }
		check(line->TrailPointCount() > 0, "moving again, it starts again");
		line->SetEmitting(false);
		const uint32 atStop = line->TrailPointCount();
		for (int i = 0; i < 6; i++) { time += 1.0 / 60.0; go->SetPosition(Vec3(30.f + (f32)i, 0.f, 0.f)); scene.Update(time); }
		check(line->TrailPointCount() <= atStop, "told to stop emitting, it lays no more");
		line->ClearTrail();
		check(line->TrailPointCount() == 0, "and can be cleared outright");
	}

	// --- a mask's outline, and which mask a sprite means ----------------------
	{
		SceneGraph scene;
		std::shared_ptr<GameObject> window = std::make_shared<GameObject>();
		window->SetName("Window");
		window->SetPosition(Vec3(10.f, 5.f, 0.f));
		std::shared_ptr<SpriteMask2D> mask = std::make_shared<SpriteMask2D>();
		mask->SetSize(Vec2(4.f, 2.f));
		window->Add(std::static_pointer_cast<IComponent>(mask));
		std::shared_ptr<GameObject> child = std::make_shared<GameObject>();
		child->SetName("Inside");
		window->Add(child);
		std::shared_ptr<GameObject> elsewhere = std::make_shared<GameObject>();
		elsewhere->SetName("Elsewhere");
		scene.Add(window);
		scene.Add(elsewhere);
		scene.Update(0.0);

		std::vector<Vec2> o;
		mask->WorldOutline(o);
		check(o.size() == 4, "a rectangle mask has four corners");
		f32 minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
		for (size_t i = 0; i < o.size(); i++) { minX = fminf(minX, o[i].x); maxX = fmaxf(maxX, o[i].x); minY = fminf(minY, o[i].y); maxY = fmaxf(maxY, o[i].y); }
		nearly(minX, 8.f, "placed in the world by its object"); nearly(maxX, 12.f, "its full width");
		nearly(minY, 4.f, "and height"); nearly(maxY, 6.f, "about its centre");

		check(SpriteMask2D::Find(child.get(), "") == mask.get(), "with no name, a sprite means the mask above it");
		check(SpriteMask2D::Find(elsewhere.get(), "") == NULL, "one with no mask above it means none");
		check(SpriteMask2D::Find(elsewhere.get(), "Window") == mask.get(), "by name, a mask anywhere in the scene");
		check(SpriteMask2D::Find(elsewhere.get(), "Nope") == NULL, "a name that is not there is none");
		const uint32 rev = mask->GetRevision();
		mask->SetEnabled(false);
		check(SpriteMask2D::Find(child.get(), "") == NULL && mask->GetRevision() != rev, "a disabled mask is none, and says it changed");
		mask->SetEnabled(true);

		mask->SetShape(SpriteMaskShape2D::Circle);
		mask->WorldOutline(o);
		check(o.size() == 32, "a round mask is a many-sided outline");
		nearly(o[0].x, 12.f, "of its size's x across");

		std::vector<Vec2> tri;
		tri.push_back(Vec2(0.f, 0.f)); tri.push_back(Vec2(0.f, 2.f)); tri.push_back(Vec2(2.f, 0.f));      // clockwise
		check(mask->SetPolygon(tri), "a polygon mask takes a convex outline, either way round");
		mask->SetShape(SpriteMaskShape2D::Polygon);
		mask->WorldOutline(o);
		f32 area = 0.f;
		for (size_t i = 0; i < o.size(); i++) { const Vec2 &a = o[i], &b = o[(i + 1) % o.size()]; area += a.x * b.y - b.x * a.y; }
		check(o.size() == 3 && area > 0.f, "and hands it out counter-clockwise");
		std::vector<Vec2> dart;
		dart.push_back(Vec2(0.f, 0.f)); dart.push_back(Vec2(2.f, 0.f)); dart.push_back(Vec2(0.5f, 0.5f)); dart.push_back(Vec2(0.f, 2.f));
		check(!mask->SetPolygon(dart) && mask->GetPolygon().size() == 3, "a concave one is refused and the old one kept");
	}

	printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
