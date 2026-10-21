//============================================================================
// Name        : LoadingScreen.cpp
// Author      : Duarte Peixinho
// Description : See LoadingScreen.h.
//============================================================================

#include <Pyros3D/Rendering/Renderer/SpecialRenderers/UIRenderer/LoadingScreen.h>
#include <Pyros3D/Rendering/Renderer/SpecialRenderers/UIRenderer/UIRenderer.h>
#include <Pyros3D/Rendering/Components/UI/UICanvas.h>
#include <Pyros3D/Rendering/Components/UI/UIRect.h>
#include <Pyros3D/Rendering/Components/UI/UIText.h>
#include <Pyros3D/Rendering/Components/UI/UIImage.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Assets/Font/Font.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Utils/LoadProgress/LoadProgress.h>
#include <Pyros3D/Utils/Json/json.hpp>
#include <cmath>
#include <cstdio>

namespace p3d {

	using json = nlohmann::json;

	namespace {
		json V4(const Vec4 &v) { return json::array({ v.x, v.y, v.z, v.w }); }
		json LsRect(f32 ax0, f32 ay0, f32 ax1, f32 ay1, f32 ox0, f32 oy0, f32 ox1, f32 oy1)
		{
			return { { "type", "UIRect" }, { "anchorMin", { ax0, ay0 } }, { "anchorMax", { ax1, ay1 } },
					 { "offsetMin", { ox0, oy0 } }, { "offsetMax", { ox1, oy1 } }, { "pivot", { 0.5f, 0.5f } } };
		}
		json LsNode(const std::string &name, const json &components, const json &children = json::array())
		{
			return { { "name", name }, { "position", { 0, 0, 0 } }, { "rotation", { 0, 0, 0 } }, { "scale", { 1, 1, 1 } },
					 { "static", false }, { "tags", json::array() }, { "components", components }, { "children", children } };
		}
		json LsImage(const Vec4 &tint, const std::string &texture = std::string())
		{
			json j = { { "type", "UIImage" }, { "tint", V4(tint) }, { "border", { 0, 0, 0, 0 } } };
			if (!texture.empty()) j["texture"] = texture;
			return j;
		}
		json LsText(const std::string &text, const f32 size, const Vec4 &color, const std::string &align)
		{
			return { { "type", "UIText" }, { "text", text }, { "font", Font::BuiltinDefault() }, { "fontSDF", true }, { "fontSize", 48.f },
					 { "size", size }, { "color", V4(color) }, { "align", align }, { "verticalAlign", "Middle" } };
		}
		template <class T> T* Component(GameObject* go, const uint32 type)
		{
			if (!go) return NULL;
			const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
			for (size_t i = 0; i < cs.size(); i++)
				if (cs[i] && cs[i]->GetComponentType() == type) return static_cast<T*>(cs[i].get());
			return NULL;
		}
		GameObject* FindIn(GameObject* go, const std::string &name)
		{
			if (!go) return NULL;
			if (go->GetName() == name) return go;
			const std::vector<std::shared_ptr<GameObject> > &kids = go->GetChildren();
			for (size_t i = 0; i < kids.size(); i++)
				if (GameObject* f = FindIn(kids[i].get(), name)) return f;
			return NULL;
		}
	}

	std::string LoadingScreen::DefaultSceneJson(const Look &look)
	{
		// Laid out on a 1920 x 1080 canvas that fits the window: the title a little above the
		// middle, the bar under it across the middle third, the percentage and the stage under that.
		const Vec4 dim(look.text.x, look.text.y, look.text.z, 0.55f);
		const Vec4 track(look.text.x, look.text.y, look.text.z, 0.12f);
		json bar = LsNode("Loading_BarTrack", { LsRect(0.5f, 0.5f, 0.5f, 0.5f, -480.f, 70.f, 480.f, 78.f), LsImage(track) },
			json::array({ LsNode("Loading_Bar", { LsRect(0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f), LsImage(look.accent) }) }));
		json kids = json::array();
		kids.push_back(LsNode("Loading_Background", { LsRect(0.f, 0.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0.f), LsImage(look.background) }));
		if (!look.image.empty())
			kids.push_back(LsNode("Loading_Image", { LsRect(0.f, 0.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0.f), LsImage(Vec4(1.f, 1.f, 1.f, 1.f), look.image) }));
		kids.push_back(LsNode("Loading_Title", { LsRect(0.5f, 0.5f, 0.5f, 0.5f, -900.f, -90.f, 900.f, 30.f),
			LsText(look.title.empty() ? std::string("Loading") : look.title, 84.f, look.text, "Center") }));
		kids.push_back(bar);
		kids.push_back(LsNode("Loading_Stage", { LsRect(0.5f, 0.5f, 0.5f, 0.5f, -480.f, 92.f, 200.f, 132.f), LsText("", 30.f, dim, "Left") }));
		kids.push_back(LsNode("Loading_Percent", { LsRect(0.5f, 0.5f, 0.5f, 0.5f, 200.f, 92.f, 480.f, 132.f), LsText(look.showPercent ? "0%" : "", 30.f, look.text, "Right") }));
		json canvas = LsNode("LoadingScreen", { { { "type", "UICanvas" }, { "referenceWidth", 1920.f }, { "referenceHeight", 1080.f },
			{ "scaleMode", "Fit" }, { "sortOrder", 0 } } }, kids);
		json scene = { { "version", 1 }, { "materials", json::array() }, { "roots", json::array({ canvas }) } };
		return scene.dump();
	}

	LoadingScreen::LoadingScreen() : scene(NULL), ui(NULL), width(0), height(0), fraction(0.f), showPercent(true) {}
	LoadingScreen::~LoadingScreen() { Close(); }

	bool LoadingScreen::Open(const std::string &sceneJson, const std::string &scenePath, const Look &look, const uint32 w, const uint32 h,
		const std::function<void()> &pumpEvents)
	{
		Close();
		width = w; height = h; pump = pumpEvents;
		background = look.background; showPercent = look.showPercent;
		fraction = 0.f; stage.clear();
		scene = new SceneGraph();
		if (!SceneSerializer::LoadSceneFromText(scene, sceneJson, scenePath, NULL, NULL, &assets, NULL))
		{
			echo("WARNING: LoadingScreen - its scene would not load; loading without one");
			SceneSerializer::UnloadScene(scene, assets);
			delete scene; scene = NULL;
			return false;
		}
		// (the canvases and texts are registered on an update, not as they are added)
		scene->Update(0.0);
		Apply(look);
		ui = new UIRenderer(width, height);
		lastDraw = std::chrono::steady_clock::time_point();
		Draw();
		return true;
	}

	void LoadingScreen::Apply(const Look &look)
	{
		// (a game's own scene keeps its own look; what the game.json gave is filled in where there is a place for it)
		if (!look.title.empty())
			if (UIText* t = Component<UIText>(Find("Loading_Title"), ComponentType::UIText)) t->SetText(look.title);
		if (!showPercent)
			if (UIText* t = Component<UIText>(Find("Loading_Percent"), ComponentType::UIText)) t->SetText("");
	}

	GameObject* LoadingScreen::Find(const std::string &name)
	{
		if (!scene) return NULL;
		std::vector<std::shared_ptr<GameObject> > &all = scene->GetAllGameObjectList();
		for (size_t i = 0; i < all.size(); i++)
			if (GameObject* f = FindIn(all[i].get(), name)) return f;
		return NULL;
	}

	void LoadingScreen::Listen()
	{
		LoadProgress::SetHandler([this](const float f, const std::string &s) { SetProgress(f, s); });
	}

	void LoadingScreen::SetProgress(const f32 f, const std::string &s)
	{
		if (!scene) return;
		const int before = (int)std::floor(fraction * 100.f);
		fraction = f; stage = s;
		// Drawn when a whole percent has gone by, or the stage changed, and not more than ~30 times
		// a second: a load reports far more often than that, and every frame drawn is load time.
		const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		const f64 since = std::chrono::duration<f64>(now - lastDraw).count();
		if (since < 1.0 / 30.0) return;
		if ((int)std::floor(f * 100.f) == before && since < 0.25) return;
		Draw();
	}

	void LoadingScreen::Draw()
	{
		if (!scene || !ui) return;
		if (pump) pump();
		if (GameObject* bar = Find("Loading_Bar"))
			if (UIRect* r = Component<UIRect>(bar, ComponentType::UIRect))
				r->SetAnchors(r->GetAnchorMin(), Vec2(fraction < 0.f ? 0.f : (fraction > 1.f ? 1.f : fraction), r->GetAnchorMax().y));
		if (showPercent)
			if (UIText* t = Component<UIText>(Find("Loading_Percent"), ComponentType::UIText))
			{
				char buf[16]; snprintf(buf, sizeof(buf), "%d%%", (int)std::floor(fraction * 100.f + 1e-4f));
				if (t->GetText() != buf) t->SetText(buf);
			}
		if (UIText* t = Component<UIText>(Find("Loading_Stage"), ComponentType::UIText))
			if (t->GetText() != stage) t->SetText(stage);
		scene->Update(0.0);

		IRenderDevice &dev = GetActiveRenderDevice();
		dev.WaitForFrameRoom();
		GameObject::TakeDrawTransforms();
		ui->Resize(width, height);
		// (it draws a whole frame of its own - the UI renderer opens and presents one when nobody has -
		// cleared to the screen's colour first; the clear colour is the device's, so it is put back)
		const Vec4 was = dev.GetClearColor();
		ui->SetBackground(background);
		ui->ClearBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth);
		ui->RenderUI(scene);
		dev.SetClearColor(was);
		if (present) present();
		lastDraw = std::chrono::steady_clock::now();
	}

	void LoadingScreen::Resize(const uint32 w, const uint32 h) { width = w; height = h; }

	void LoadingScreen::Close()
	{
		if (LoadProgress::HasHandler()) LoadProgress::ClearHandler();
		if (scene)
		{
			SceneSerializer::UnloadScene(scene, assets);
			delete scene; scene = NULL;
		}
		delete ui; ui = NULL;
		pump = std::function<void()>();
		present = std::function<void()>();
	}

}
