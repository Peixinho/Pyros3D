// The canvas layout solve, on its own: anchors, nesting, scale modes, the
// canvas-space -> world y flip, and hit testing.
//
// Headless on purpose. UIRect/UICanvas are pure math over a GameObject tree,
// so none of this needs a render device - which means the part of the UI
// system most likely to be quietly wrong is also the part that can be
// checked without a window.
//
//   c++ -std=c++17 -I include -I $(pkg-config --variable=includedir freetype2)/freetype2 \
//       tools/tests/ui_layout.cpp -o /tmp/ui_layout \
//       -L build_vulkan -lPyrosEngine -Wl,-rpath,$PWD/build_vulkan
//   /tmp/ui_layout
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Rendering/Components/UI/UICanvas.h>
#include <Pyros3D/Rendering/Components/UI/UIRect.h>

#include <cstdio>
#include <cmath>
#include <memory>

using namespace p3d;

static int failures = 0;

static void check(bool cond, const char* what)
{
	printf("%s  %s\n", cond ? "PASS" : "FAIL", what);
	if (!cond) failures++;
}

static bool near(const f32 a, const f32 b) { return fabsf(a - b) < 0.01f; }

static void checkRect(const UIRectValue &r, const f32 x, const f32 y, const f32 w, const f32 h, const char* what)
{
	const bool ok = near(r.x, x) && near(r.y, y) && near(r.width, w) && near(r.height, h);
	printf("%s  %s  (got %.1f,%.1f %.1fx%.1f)\n", ok ? "PASS" : "FAIL", what, r.x, r.y, r.width, r.height);
	if (!ok) failures++;
}

int main()
{
	SceneGraph scene;

	std::shared_ptr<GameObject> canvasGO = std::make_shared<GameObject>();
	canvasGO->SetName("Canvas");
	std::shared_ptr<UICanvas> canvas = std::make_shared<UICanvas>(1920.f, 1080.f);
	canvas->SetScaleMode(UIScaleMode::Stretch);
	canvasGO->Add(std::static_pointer_cast<IComponent>(canvas));
	scene.Add(canvasGO);

	// -------- pinned to the top-left, explicit size --------
	std::shared_ptr<GameObject> panelGO = std::make_shared<GameObject>();
	panelGO->SetName("Panel");
	std::shared_ptr<UIRect> panel = std::make_shared<UIRect>();
	panel->SetAnchoredPosition(Vec2(0.f, 0.f), Vec2(40.f, 20.f), Vec2(400.f, 200.f));
	panel->SetPivot(Vec2(0.f, 0.f));
	panelGO->Add(std::static_pointer_cast<IComponent>(panel));
	canvasGO->Add(panelGO);

	scene.Update(0.0);
	canvas->Solve(1920, 1080);

	checkRect(panel->GetRect(), 40.f, 20.f, 400.f, 200.f, "pinned rect is at its anchored position");

	// Canvas y is down, world y is up, and the pivot is the top-left corner.
	check(near(panelGO->GetPosition().x, 40.f) && near(panelGO->GetPosition().y, -20.f),
		"owner position is the pivot point, y negated");

	// -------- stretched to fill, with insets --------
	std::shared_ptr<GameObject> barGO = std::make_shared<GameObject>();
	std::shared_ptr<UIRect> bar = std::make_shared<UIRect>();
	bar->SetAnchors(Vec2(0.f, 0.f), Vec2(1.f, 0.f));
	bar->SetOffsets(Vec2(16.f, 8.f), Vec2(-16.f, 64.f));
	barGO->Add(std::static_pointer_cast<IComponent>(bar));
	canvasGO->Add(barGO);

	scene.Update(0.0);
	canvas->Solve(1920, 1080);
	checkRect(bar->GetRect(), 16.f, 8.f, 1888.f, 56.f, "stretched rect insets from both edges");

	// -------- a child solves against its parent, not the canvas --------
	std::shared_ptr<GameObject> childGO = std::make_shared<GameObject>();
	std::shared_ptr<UIRect> child = std::make_shared<UIRect>();
	child->SetAnchors(Vec2(1.f, 1.f), Vec2(1.f, 1.f));
	child->SetOffsets(Vec2(-100.f, -50.f), Vec2(-20.f, -10.f));
	child->SetPivot(Vec2(0.f, 0.f));
	childGO->Add(std::static_pointer_cast<IComponent>(child));
	panelGO->Add(childGO);

	scene.Update(0.0);
	canvas->Solve(1920, 1080);
	// Parent is (40,20 400x200), so its bottom-right corner is (440,220).
	checkRect(child->GetRect(), 340.f, 170.f, 80.f, 40.f, "child anchors to its parent's corner");
	// And its position is LOCAL - a delta from the parent's own pivot
	// point (40,20), or the parent's transform would be counted twice.
	check(near(childGO->GetPosition().x, 300.f) && near(childGO->GetPosition().y, -150.f),
		"child position is local to its parent");

	// -------- scale modes --------
	canvas->SetScaleMode(UIScaleMode::MatchWidth);
	canvas->Solve(1280, 720);
	checkRect(canvas->GetCanvasRect(), 0.f, 0.f, 1920.f, 1080.f, "MatchWidth at 16:9 gives the reference size");
	canvas->Solve(1280, 960);
	checkRect(canvas->GetCanvasRect(), 0.f, 0.f, 1920.f, 1440.f, "MatchWidth at 4:3 keeps width, grows height");

	canvas->SetScaleMode(UIScaleMode::MatchHeight);
	canvas->Solve(1280, 960);
	checkRect(canvas->GetCanvasRect(), 0.f, 0.f, 1440.f, 1080.f, "MatchHeight keeps height, shrinks width");

	canvas->SetScaleMode(UIScaleMode::ConstantPixel);
	canvas->Solve(800, 600);
	checkRect(canvas->GetCanvasRect(), 0.f, 0.f, 800.f, 600.f, "ConstantPixel is the viewport itself");

	// -------- a stretched element actually follows the canvas --------
	canvas->SetScaleMode(UIScaleMode::MatchWidth);
	canvas->Solve(1280, 720);
	const f32 wideBar = bar->GetRect().width;
	canvas->Solve(640, 720);
	check(near(bar->GetRect().width, wideBar), "a width-stretched bar is resolution independent under MatchWidth");

	// -------- hit testing --------
	canvas->SetScaleMode(UIScaleMode::Stretch);
	canvas->Solve(1920, 1080);
	// Inside the child, which sits on top of the panel: topmost wins.
	check(canvas->HitTest(Vec2(360.f, 180.f)) == childGO.get(), "hit test returns the topmost element");
	check(canvas->HitTest(Vec2(100.f, 100.f)) == panelGO.get(), "hit test falls through to the panel");
	check(canvas->HitTest(Vec2(1900.f, 1000.f)) == NULL, "hit test misses empty canvas");

	// -------- and the registry only reports live canvases --------
	check(UICanvas::GetCanvasesOnScene(&scene).size() == 1, "the canvas is registered with its scene");
	scene.Remove(canvasGO);
	check(UICanvas::GetCanvasesOnScene(&scene).empty(), "removing the object unregisters the canvas");

	printf("\n%s (%d failure(s))\n", failures ? "FAILED" : "ALL PASSED", failures);
	return failures ? 1 : 0;
}
