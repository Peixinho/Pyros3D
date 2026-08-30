// Canvas events reaching real Lua functions: the half of the UI that only
// exists once something is scripted.
//
// The engine reports that a slider changed; shared/UIDispatch.h decides
// which named handler that calls and what it is passed. Both the player and
// the editor's play mode go through it, so this is the one place to check
// that a screen authored in the editor does anything at all when played.
//
//   c++ -std=c++17 -DLUA_BINDINGS -I include -I shared -I /opt/homebrew/include/lua \
//       -I $(pkg-config --variable=includedir freetype2)/freetype2 \
//       tools/tests/ui_dispatch.cpp -o /tmp/ui_dispatch \
//       -L build_vulkan -lPyrosEngine -llua -Wl,-rpath,$PWD/build_vulkan
//   /tmp/ui_dispatch
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Rendering/Components/UI/UICanvas.h>
#include <Pyros3D/Rendering/Components/UI/UIRect.h>
#include <Pyros3D/Rendering/Components/UI/UIButton.h>
#include <Pyros3D/Rendering/Components/UI/UIToggle.h>
#include <Pyros3D/Rendering/Components/UI/UISlider.h>
#include <Pyros3D/Rendering/Components/UI/UIInput.h>
#include <Pyros3D/Rendering/Components/UI/UIList.h>
#include <Pyros3D/Rendering/Components/UI/UIPopup.h>
#include "UIDispatch.h"

#include <cstdio>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

using namespace p3d;

static int failures = 0;
static std::vector<std::string> complaints;

static void check(bool cond, const char* what, const std::string &extra = std::string())
{
	printf("%s  %s %s\n", cond ? "PASS" : "FAIL", what, extra.c_str());
	if (!cond) failures++;
}

static void collect(const std::string &message, void*) { complaints.push_back(message); }

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

int main()
{
	sol::state lua;
	lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table);

	// The handlers a scene would name on its elements. Each records what it
	// was called with, so the test can check the arguments as well as the
	// fact of the call - a handler that fires with the wrong value is worse
	// than one that does not fire.
	lua.script(
		"log = {}\n"
		"function onPress(name) log[#log+1] = 'press:'..name end\n"
		"function onVolume(name, value) log[#log+1] = 'volume:'..name..'='..string.format('%.0f', value) end\n"
		"function onToggle(name, value) log[#log+1] = 'toggle:'..name..'='..tostring(value) end\n"
		"function onType(name, text) log[#log+1] = 'type:'..name..'='..text end\n"
		"function onSubmit(name, text) log[#log+1] = 'submit:'..name..'='..text end\n"
		"function onPick(name, item, index) log[#log+1] = 'pick:'..name..'='..item..'@'..index end\n"
		"function onClose(name) log[#log+1] = 'close:'..name end\n"
		"function onBroken(name) error('handler blew up') end\n");

	SceneGraph scene;
	std::shared_ptr<GameObject> canvasGO = std::make_shared<GameObject>();
	std::shared_ptr<UICanvas> canvas = std::make_shared<UICanvas>(400.f, 300.f);
	canvas->SetScaleMode(UIScaleMode::ConstantPixel);
	canvasGO->Add(std::static_pointer_cast<IComponent>(canvas));
	scene.Add(canvasGO);

	std::shared_ptr<GameObject> buttonGO = Element(canvasGO, "Play", 10.f, 10.f, 100.f, 30.f);
	std::shared_ptr<UIButton> button = std::make_shared<UIButton>();
	button->SetOnClick("onPress");
	buttonGO->Add(std::static_pointer_cast<IComponent>(button));

	std::shared_ptr<GameObject> sliderGO = Element(canvasGO, "Volume", 10.f, 50.f, 200.f, 20.f);
	std::shared_ptr<UISlider> slider = std::make_shared<UISlider>();
	slider->SetRange(0.f, 100.f);
	slider->SetOnChange("onVolume");
	sliderGO->Add(std::static_pointer_cast<IComponent>(slider));

	std::shared_ptr<GameObject> toggleGO = Element(canvasGO, "Music", 10.f, 80.f, 30.f, 30.f);
	std::shared_ptr<UIToggle> toggle = std::make_shared<UIToggle>();
	toggle->SetOnChange("onToggle");
	toggleGO->Add(std::static_pointer_cast<IComponent>(toggle));

	std::shared_ptr<GameObject> fieldGO = Element(canvasGO, "Name", 10.f, 120.f, 200.f, 30.f);
	std::shared_ptr<UIInput> field = std::make_shared<UIInput>();
	field->SetOnChange("onType");
	field->SetOnSubmit("onSubmit");
	fieldGO->Add(std::static_pointer_cast<IComponent>(field));

	std::shared_ptr<GameObject> listGO = Element(canvasGO, "Levels", 220.f, 10.f, 160.f, 90.f);
	std::shared_ptr<UIList> list = std::make_shared<UIList>();
	list->SetItemHeight(30.f);
	list->SetOnChange("onPick");
	listGO->Add(std::static_pointer_cast<IComponent>(list));
	for (int i = 0; i < 3; i++)
	{
		char row[16];
		snprintf(row, sizeof(row), "Row%d", i);
		Element(listGO, row, 0.f, 0.f, 160.f, 30.f);
	}
	{
		std::vector<std::string> items;
		items.push_back("Ashfall");
		items.push_back("Blue Harbour");
		items.push_back("Cinder");
		list->SetItems(items);
	}

	scene.Update(0.0);
	canvas->Solve(400.f, 300.f);
	scene.Update(0.0);

	// One frame of input, then dispatch - exactly what the player does.
	auto frameOfInput = [&](const Vec2 &at, const bool down) {
		canvas->UpdateInput(at, down);
		uidispatch::Dispatch(canvas.get(), lua, &collect, NULL);
		scene.Update(0.0);
	};
	auto click = [&](const Vec2 &at) {
		frameOfInput(at, false);
		frameOfInput(at, true);
		frameOfInput(at, false);
	};
	auto logLine = [&](const int index) -> std::string {
		sol::table log = lua["log"];
		sol::optional<std::string> v = log[index];
		return v ? *v : std::string("<none>");
	};
	auto logSize = [&]() -> int { sol::table log = lua["log"]; return (int)log.size(); };

	// ---- a button ----
	click(Vec2(60.f, 25.f));
	check(logSize() == 1 && logLine(1) == "press:Play", "a click calls the button's handler", logLine(1));

	// ---- a slider ----
	frameOfInput(Vec2(110.f, 60.f), false);
	frameOfInput(Vec2(110.f, 60.f), true);
	check(logLine(logSize()) == "volume:Volume=50", "a drag calls the change handler, with the value", logLine(logSize()));
	frameOfInput(Vec2(110.f, 60.f), false);

	// ---- a checkbox ----
	click(Vec2(25.f, 95.f));
	check(logLine(logSize()) == "toggle:Music=true", "a checkbox reports what it was set to", logLine(logSize()));

	// ---- a text field ----
	click(Vec2(60.f, 135.f));
	canvas->UpdateText("Nova");
	uidispatch::Dispatch(canvas.get(), lua, &collect, NULL);
	check(logLine(logSize()) == "type:Name=Nova", "typing calls the change handler", logLine(logSize()));

	canvas->UpdateKey(UIKey::Enter);
	uidispatch::Dispatch(canvas.get(), lua, &collect, NULL);
	check(logLine(logSize()) == "submit:Name=Nova", "enter calls the submit handler instead", logLine(logSize()));

	// ---- a list ----
	click(Vec2(300.f, 45.f));
	check(logLine(logSize()) == "pick:Levels=Blue Harbour@1", "picking a row passes the item and its index", logLine(logSize()));

	// ---- a dialog ----
	// Its close handler runs on the press that dismissed it, and nothing
	// underneath hears that press at all.
	{
		std::shared_ptr<GameObject> popupGO = Element(canvasGO, "Confirm", 0.f, 0.f, 400.f, 300.f);
		std::shared_ptr<UIPopup> popup = std::make_shared<UIPopup>();
		popup->SetOnClose("onClose");
		popupGO->Add(std::static_pointer_cast<IComponent>(popup));
		Element(popupGO, "Dialog", 120.f, 100.f, 160.f, 100.f);
		popup->Open();
		scene.Update(0.0);
		canvas->Solve(400.f, 300.f);

		const int before = logSize();
		frameOfInput(Vec2(20.f, 280.f), false);
		frameOfInput(Vec2(20.f, 280.f), true);
		check(logLine(logSize()) == "close:Confirm", "dismissing a dialog calls its close handler", logLine(logSize()));
		check(logSize() == before + 1, "and nothing under it hears that press");
		frameOfInput(Vec2(20.f, 280.f), false);
		popupGO->Remove(std::static_pointer_cast<IComponent>(popup));
		canvasGO->Remove(popupGO);
		scene.Update(0.0);
		canvas->Solve(400.f, 300.f);
	}

	// ---- handlers that are not there, and handlers that break ----
	// Neither is allowed to take the frame down with it: a typo in a scene
	// file is a warning, not a crash.
	complaints.clear();
	button->SetOnClick("noSuchFunction");
	click(Vec2(60.f, 25.f));
	check(complaints.size() == 1 && complaints[0].find("not a global function") != std::string::npos,
		"a missing handler is a warning", complaints.empty() ? "" : complaints[0]);

	complaints.clear();
	button->SetOnClick("onBroken");
	click(Vec2(60.f, 25.f));
	check(complaints.size() == 1 && complaints[0].find("handler blew up") != std::string::npos,
		"a handler that errors is reported, not fatal", complaints.empty() ? "" : complaints[0]);

	// ---- an element with no handler ----
	complaints.clear();
	button->SetOnClick("");
	const int quiet = logSize();
	click(Vec2(60.f, 25.f));
	check(logSize() == quiet && complaints.empty(), "an element with no handler says nothing");

	printf("\n%s (%d failure(s))\n", failures ? "FAILED" : "ALL PASSED", failures);
	return failures ? 1 : 0;
}
