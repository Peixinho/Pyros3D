// The widget set's behaviour, on its own: what a checkbox, a radio group, a
// slider and a text field do with the input the canvas hands them.
//
// Headless on purpose, like ui_layout.cpp: none of this needs a render
// device, because the part most likely to be quietly wrong - which widget
// gets the click, where a drag lands on the track, what the caret does - is
// pure logic over a GameObject tree.
//
//   c++ -std=c++17 -I include -I $(pkg-config --variable=includedir freetype2)/freetype2 \
//       tools/tests/ui_widgets.cpp -o /tmp/ui_widgets \
//       -L build_vulkan -lPyrosEngine -Wl,-rpath,$PWD/build_vulkan
//   /tmp/ui_widgets
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Rendering/Components/UI/UICanvas.h>
#include <Pyros3D/Rendering/Components/UI/UIRect.h>
#include <Pyros3D/Rendering/Components/UI/UIToggle.h>
#include <Pyros3D/Rendering/Components/UI/UISlider.h>
#include <Pyros3D/Rendering/Components/UI/UIInput.h>

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

static bool near(const f32 a, const f32 b) { return fabsf(a - b) < 0.001f; }

// A canvas element at an explicit place in canvas units, which is all any of
// these tests need to aim a pointer at.
static std::shared_ptr<GameObject> Element(const std::shared_ptr<GameObject> &parent,
	const char* name, const f32 x, const f32 y, const f32 w, const f32 h)
{
	std::shared_ptr<GameObject> go = std::make_shared<GameObject>();
	go->SetName(name);
	std::shared_ptr<UIRect> r = std::make_shared<UIRect>();
	r->SetAnchors(Vec2(0.f, 0.f), Vec2(0.f, 0.f));
	r->SetPivot(Vec2(0.f, 0.f));
	r->SetOffsets(Vec2(x, y), Vec2(x + w, y + h));
	go->Add(std::static_pointer_cast<IComponent>(r));
	parent->Add(go);
	return go;
}

static UIRect* RectOn(GameObject* go)
{
	const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
	for (size_t i = 0; i < cs.size(); i++)
		if (cs[i]->GetComponentType() == ComponentType::UIRect) return static_cast<UIRect*>(cs[i].get());
	return NULL;
}

// A released frame first, every time. Widgets arm on the up-to-down
// transition and start life believing the pointer is already held, so that a
// pointer down when a canvas appears cannot click whatever it happens to be
// over - which means a test that never releases never presses either.
static void Click(SceneGraph &scene, const std::shared_ptr<UICanvas> &canvas, const Vec2 &at)
{
	canvas->UpdateInput(at, false);
	canvas->UpdateInput(at, true);
	canvas->UpdateInput(at, false);
	scene.Update(0.0);
}

// Press and hold, from a released frame - for drags, which have to observe
// what happens between the press and the release.
static void Press(SceneGraph &scene, const std::shared_ptr<UICanvas> &canvas, const Vec2 &at)
{
	canvas->UpdateInput(at, false);
	canvas->UpdateInput(at, true);
	scene.Update(0.0);
}

int main()
{
	SceneGraph scene;
	std::shared_ptr<GameObject> canvasGO = std::make_shared<GameObject>();
	std::shared_ptr<UICanvas> canvas = std::make_shared<UICanvas>(400.f, 300.f);
	canvas->SetScaleMode(UIScaleMode::ConstantPixel);
	canvasGO->Add(std::static_pointer_cast<IComponent>(canvas));
	scene.Add(canvasGO);

	// ---- checkbox ----
	std::shared_ptr<GameObject> boxGO = Element(canvasGO, "Box", 10.f, 10.f, 40.f, 40.f);
	std::shared_ptr<UIToggle> box = std::make_shared<UIToggle>();
	boxGO->Add(std::static_pointer_cast<IComponent>(box));
	// The tick mark it shows and hides, as a child named the way the
	// toggle's default expects. Its visibility is the toggle's output.
	std::shared_ptr<GameObject> tickGO = Element(boxGO, "Check", 0.f, 0.f, 40.f, 40.f);
	UIRect* tick = RectOn(tickGO.get());

	scene.Update(0.0);
	canvas->Solve(400.f, 300.f);
	scene.Update(0.0);

	check(!box->GetValue(), "a checkbox starts off");
	check(!tick->IsVisible(), "and its tick is hidden");

	Click(scene, canvas, Vec2(30.f, 30.f));
	check(box->GetValue(), "clicking it turns it on");
	check(tick->IsVisible(), "and shows the tick");

	Click(scene, canvas, Vec2(30.f, 30.f));
	check(!box->GetValue(), "clicking again turns it off");
	check(!tick->IsVisible(), "and hides the tick");

	// Clicking elsewhere is not a click on it - the same rule buttons have.
	Click(scene, canvas, Vec2(300.f, 250.f));
	check(!box->GetValue(), "clicking away from it changes nothing");

	box->SetInteractable(false);
	Click(scene, canvas, Vec2(30.f, 30.f));
	check(!box->GetValue(), "a disabled checkbox cannot be clicked");
	box->SetInteractable(true);

	// ---- radio group ----
	std::shared_ptr<GameObject> groupGO = Element(canvasGO, "Group", 10.f, 100.f, 200.f, 40.f);
	std::shared_ptr<UIToggle> radios[3];
	for (int i = 0; i < 3; i++)
	{
		char name[16];
		snprintf(name, sizeof(name), "Radio%d", i);
		std::shared_ptr<GameObject> go = Element(groupGO, name, (f32)(i * 60), 0.f, 50.f, 40.f);
		radios[i] = std::make_shared<UIToggle>();
		radios[i]->SetGroup("quality");
		go->Add(std::static_pointer_cast<IComponent>(radios[i]));
	}
	radios[0]->SetValue(true);
	canvas->Solve(400.f, 300.f);
	scene.Update(0.0);

	// Second radio: its element starts at x=70 in canvas units (10 + 60).
	Click(scene, canvas, Vec2(95.f, 120.f));
	check(radios[1]->GetValue(), "picking one in a group turns it on");
	check(!radios[0]->GetValue(), "and turns the previous one off");
	check(!radios[2]->GetValue(), "leaving the rest alone");

	// A toggle outside the group is untouched by it.
	check(!box->GetValue(), "a checkbox outside the group is not part of it");

	// ---- slider ----
	std::shared_ptr<GameObject> sliderGO = Element(canvasGO, "Slider", 100.f, 200.f, 200.f, 20.f);
	std::shared_ptr<UISlider> slider = std::make_shared<UISlider>();
	slider->SetRange(0.f, 100.f);
	sliderGO->Add(std::static_pointer_cast<IComponent>(slider));
	std::shared_ptr<GameObject> fillGO = Element(sliderGO, "Fill", 0.f, 0.f, 0.f, 0.f);
	UIRect* fillRect = RectOn(fillGO.get());
	// Stretched across the track, so the slider only has to move one anchor.
	fillRect->SetAnchors(Vec2(0.f, 0.f), Vec2(1.f, 1.f));
	fillRect->SetOffsets(Vec2(0.f, 0.f), Vec2(0.f, 0.f));

	canvas->Solve(400.f, 300.f);
	scene.Update(0.0);
	check(near(slider->GetValue(), 0.f), "a slider starts at its minimum");

	// Press at the middle of the track and hold: the value follows.
	Press(scene, canvas, Vec2(200.f, 210.f));
	check(near(slider->GetValue(), 50.f), "pressing halfway along sets half the range");
	check(near(fillRect->GetAnchorMax().x, 0.5f), "and the fill follows the value");

	// Still dragging, now past the right-hand end: pinned, not lost.
	canvas->UpdateInput(Vec2(900.f, 900.f), true);
	scene.Update(0.0);
	check(near(slider->GetValue(), 100.f), "dragging past the end pins it to the maximum");
	canvas->UpdateInput(Vec2(900.f, 900.f), false);

	// A press that started off the track must not grab it.
	Press(scene, canvas, Vec2(900.f, 900.f));
	canvas->UpdateInput(Vec2(150.f, 210.f), true);
	scene.Update(0.0);
	check(near(slider->GetValue(), 100.f), "a drag that started elsewhere does not grab the track");
	canvas->UpdateInput(Vec2(150.f, 210.f), false);

	// Steps snap.
	slider->SetStep(25.f);
	Press(scene, canvas, Vec2(160.f, 210.f));        // 30% along -> 30, snaps to 25
	check(near(slider->GetValue(), 25.f), "a stepped slider snaps to its step");
	canvas->UpdateInput(Vec2(160.f, 210.f), false);

	// Arrow keys move it by one step, and stay inside it: a slider that let
	// Left walk the focus away at zero would be unusable with a pad.
	canvas->SetFocus(sliderGO.get());
	check(canvas->UpdateKey(UIKey::Right), "an arrow along the slider is claimed by it");
	scene.Update(0.0);
	check(near(slider->GetValue(), 50.f), "and moves it by one step");
	check(!canvas->UpdateKey(UIKey::Up), "an arrow across it is left for navigation");

	slider->SetValue(0.f);
	canvas->UpdateKey(UIKey::Left);
	check(near(slider->GetValue(), 0.f), "it does not go below its minimum");

	// ---- events ----
	// A host needs to know what changed, not just that something was
	// clicked: a slider dragged reports Changed and no click at all.
	slider->SetValue(50.f);
	canvas->UpdateInput(Vec2(300.f, 210.f), false);
	canvas->UpdateInput(Vec2(300.f, 210.f), true);
	const std::vector<UICanvas::WidgetEvent> &evs = canvas->GetEvents();
	bool sliderChanged = false;
	for (size_t i = 0; i < evs.size(); i++)
		if (evs[i].node == sliderGO.get() && (evs[i].flags & UIEventFlag::Changed)) sliderChanged = true;
	check(sliderChanged, "a drag reports a change against the element that changed");
	canvas->UpdateInput(Vec2(300.f, 210.f), false);


	// ---- text field ----
	// No render device here, so the field has no label to drive: what is
	// under test is the value, the caret and which keys it claims, all of
	// which are the parts that would be wrong.
	std::shared_ptr<GameObject> fieldGO = Element(canvasGO, "Field", 10.f, 250.f, 200.f, 30.f);
	std::shared_ptr<UIInput> field = std::make_shared<UIInput>();
	fieldGO->Add(std::static_pointer_cast<IComponent>(field));
	canvas->Solve(400.f, 300.f);
	scene.Update(0.0);

	// Typing goes to whatever has focus, and nothing else.
	canvas->UpdateText("hello");
	check(field->GetText().empty(), "an unfocused field ignores typing");

	Press(scene, canvas, Vec2(60.f, 260.f));
	canvas->UpdateInput(Vec2(60.f, 260.f), false);
	check(canvas->GetFocused() == fieldGO.get(), "clicking a field focuses it");

	canvas->UpdateText("hello");
	check(field->GetText() == "hello", "and then it takes what is typed");
	check(field->GetCaret() == 5, "with the caret after it");

	canvas->UpdateKey(UIKey::Left);
	canvas->UpdateKey(UIKey::Left);
	canvas->UpdateText("XY");
	check(field->GetText() == "helXYlo", "typing inserts at the caret");

	canvas->UpdateKey(UIKey::Backspace);
	check(field->GetText() == "helXlo", "backspace deletes before the caret");
	canvas->UpdateKey(UIKey::Delete);
	check(field->GetText() == "helXo", "delete takes the one after it");

	canvas->UpdateKey(UIKey::Home);
	check(field->GetCaret() == 0, "home goes to the start");
	check(canvas->UpdateKey(UIKey::Backspace), "backspace at the start is still the field's");
	check(field->GetText() == "helXo", "and does nothing");
	canvas->UpdateKey(UIKey::End);
	check(field->GetCaret() == 5, "end goes to the end");

	// Arrows belong to the field while it has focus, or editing the middle
	// of a value would walk the focus away instead.
	check(canvas->UpdateKey(UIKey::Left), "a field claims the arrow keys");
	check(!canvas->UpdateKey(UIKey::Up), "but not the ones it has no use for");

	// Enter submits without clearing.
	canvas->UpdateKey(UIKey::Enter);
	bool submitted = false;
	for (size_t i = 0; i < canvas->GetEvents().size(); i++)
		if (canvas->GetEvents()[i].flags & UIEventFlag::Submitted) submitted = true;
	check(submitted, "enter submits");
	check(field->GetText() == "helXo", "and leaves the value alone");

	// Escape puts back what it held when it was focused - and after a
	// submit, that is what was submitted.
	canvas->UpdateText("!!");
	canvas->UpdateKey(UIKey::Escape);
	check(field->GetText() == "helXo", "escape reverts to what was there");

	// A filter rejects as you type rather than after.
	field->SetText("");
	field->SetFilter("0123456789");
	canvas->UpdateText("1a2b3");
	check(field->GetText() == "123", "a filter drops what it does not allow");

	// A limit refuses the whole insert: half of what you typed arriving is
	// worse than none of it.
	field->SetText("12");
	field->SetMaxLength(4);
	canvas->UpdateText("345");
	check(field->GetText() == "12", "typing past the limit inserts nothing");
	canvas->UpdateText("34");
	check(field->GetText() == "1234", "what does fit still goes in");

	field->SetFilter("");
	field->SetMaxLength(0);
	field->SetReadOnly(true);
	field->SetText("locked");
	canvas->UpdateText("x");
	canvas->UpdateKey(UIKey::Backspace);
	check(field->GetText() == "locked", "a read-only field cannot be typed into");
	field->SetReadOnly(false);

	// Password is a display concern only - the value is the value.
	field->SetPassword(true);
	field->SetText("secret");
	check(field->GetText() == "secret", "masking does not change the value");

	printf("\n%s (%d failure(s))\n", failures ? "FAILED" : "ALL PASSED", failures);
	return failures ? 1 : 0;
}
