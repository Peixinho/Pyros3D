//============================================================================
// Name        : UIStyleResolver.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : .uistyle and .palette files, resolved into UI component
//               property bags.
//
//               Deliberately outside the engine, shared by the editor and
//               the player, for the same reason prefab references are: the
//               engine renders a canvas, it does not need a notion of a
//               style asset, a theme, or a file on disk. UIRect carries an
//               opaque styleRef string and nothing in the engine ever looks
//               inside it. Everything that gives that string meaning is
//               here.
//
//               A style carries LOOK - tints, borders, text colour and
//               size, button state colours. It never carries content or
//               layout: the text a label says and the rect it occupies
//               belong to the element, so applying a style can never move
//               or rewrite a UI, only re-skin it. That split is what makes
//               re-applying a style safe enough to do automatically on
//               load.
//============================================================================

#ifndef UISTYLERESOLVER_H
#define UISTYLERESOLVER_H

#include <Pyros3D/Utils/Json/json.hpp>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Rendering/Components/UI/UICanvas.h>
#include <Pyros3D/Rendering/Components/UI/UIRect.h>
#include <Pyros3D/Rendering/Components/UI/UIImage.h>
#include <Pyros3D/Rendering/Components/UI/UIText.h>
#include <Pyros3D/Rendering/Components/UI/UIButton.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>

namespace uistyle {

	using json = nlohmann::json;

	// ------------------------------------------------------------------
	// Files
	// ------------------------------------------------------------------

	inline bool ReadJsonFile(const std::string &path, json &out)
	{
		std::ifstream in(path.c_str());
		if (!in) return false;
		try { in >> out; }
		catch (const std::exception&) { return false; }
		return out.is_object();
	}

	inline bool WriteJsonFile(const std::string &path, const json &j)
	{
		std::ofstream out(path.c_str());
		if (!out) return false;
		out << j.dump(2);
		return true;
	}

	// ------------------------------------------------------------------
	// Palette
	//
	// A theme is a name -> colour map and nothing else. Styles name
	// "@accent" rather than a hex value, so re-skinning, a high-contrast
	// mode, or a per-faction tint is one file swap instead of a hunt
	// through every style.
	// ------------------------------------------------------------------

	typedef std::map<std::string, json> Palette;

	inline Palette LoadPalette(const std::string &path)
	{
		Palette p;
		json j;
		if (!ReadJsonFile(path, j)) return p;
		if (j.find("colors") == j.end() || !j["colors"].is_object()) return p;
		for (json::const_iterator it = j["colors"].begin(); it != j["colors"].end(); ++it)
			if (it.value().is_array() && it.value().size() == 4)
				p[it.key()] = it.value();
		return p;
	}

	// "@name" resolves through the palette; anything else passes straight
	// through. An unresolved name is left as-is rather than defaulted to a
	// colour, so it surfaces as an error at apply time instead of as a
	// silently wrong shade.
	inline bool ResolveColor(const json &v, const Palette &palette, json &out, std::string &errOut)
	{
		if (v.is_array() && v.size() == 4) { out = v; return true; }
		if (v.is_string())
		{
			const std::string s = v.get<std::string>();
			if (!s.empty() && s[0] == '@')
			{
				Palette::const_iterator it = palette.find(s.substr(1));
				if (it == palette.end()) { errOut = "no palette entry named '" + s + "'"; return false; }
				out = it->second;
				return true;
			}
		}
		errOut = "expected [r, g, b, a] or a \"@paletteName\"";
		return false;
	}

	// ------------------------------------------------------------------
	// Style -> property bag
	//
	// The output is exactly the flat bag the editor's set_ui and
	// ApplyProperties below already speak, so a style is not a second way
	// of describing an element - it is a saved set of the same edits.
	// ------------------------------------------------------------------

	inline bool Resolve(const json &style, const Palette &palette, json &bag, std::string &errOut)
	{
		bag = json::object();
		if (!style.is_object()) { errOut = "not a style object"; return false; }

		if (style.find("image") != style.end() && style["image"].is_object())
		{
			const json &im = style["image"];
			if (im.find("tint") != im.end())
			{
				json c;
				if (!ResolveColor(im["tint"], palette, c, errOut)) { errOut = "image.tint: " + errOut; return false; }
				bag["tint"] = c;
			}
			if (im.find("border") != im.end()) bag["border"] = im["border"];
			if (im.find("texture") != im.end()) bag["texture"] = im["texture"];
		}

		if (style.find("text") != style.end() && style["text"].is_object())
		{
			const json &tx = style["text"];
			if (tx.find("color") != tx.end())
			{
				json c;
				if (!ResolveColor(tx["color"], palette, c, errOut)) { errOut = "text.color: " + errOut; return false; }
				bag["color"] = c;
			}
			if (tx.find("size") != tx.end()) bag["size"] = tx["size"];
			if (tx.find("align") != tx.end()) bag["align"] = tx["align"];
			if (tx.find("verticalAlign") != tx.end()) bag["verticalAlign"] = tx["verticalAlign"];
		}

		if (style.find("button") != style.end() && style["button"].is_object())
		{
			const json &bt = style["button"];
			if (bt.find("transition") != bt.end()) bag["transition"] = bt["transition"];
			const char* states[3] = { "hover", "pressed", "disabled" };
			for (int i = 0; i < 3; i++)
			{
				if (bt.find(states[i]) == bt.end() || !bt[states[i]].is_object()) continue;
				const json &st = bt[states[i]];
				if (st.find("tint") != st.end())
				{
					json c;
					if (!ResolveColor(st["tint"], palette, c, errOut)) { errOut = std::string("button.") + states[i] + ".tint: " + errOut; return false; }
					bag[std::string(states[i]) + "Tint"] = c;
				}
				if (st.find("textColor") != st.end())
				{
					json c;
					if (!ResolveColor(st["textColor"], palette, c, errOut)) { errOut = std::string("button.") + states[i] + ".textColor: " + errOut; return false; }
					bag[std::string(states[i]) + "TextColor"] = c;
				}
				if (st.find("offset") != st.end() && std::string(states[i]) == "pressed")
					bag["pressedOffset"] = st["offset"];
			}
		}
		return true;
	}

	// ------------------------------------------------------------------
	// Applying a bag to an element
	//
	// One implementation, shared by the editor (which also wraps it in
	// undo) and the player (which does not) - a style that resolved
	// differently in the two would be worse than no styles at all. Keys the
	// element has no component for are skipped rather than rejected: a
	// button style applied to a plain image should set the image half and
	// say nothing about the states it cannot use.
	// ------------------------------------------------------------------

	inline bool ApplyProperties(p3d::GameObject* go, const json &bag,
		const std::string &textureRoot, std::string &errOut)
	{
		using namespace p3d;
		if (!go || !bag.is_object()) { errOut = "nothing to apply"; return false; }

		UIRect* rect = NULL; UIImage* image = NULL; UIText* text = NULL; UIButton* button = NULL;
		const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
		for (size_t i = 0; i < cs.size(); i++)
		{
			if (!cs[i]) continue;
			switch (cs[i]->GetComponentType())
			{
			case ComponentType::UIRect:   rect   = static_cast<UIRect*>(cs[i].get());   break;
			case ComponentType::UIImage:  image  = static_cast<UIImage*>(cs[i].get());  break;
			case ComponentType::UIText:   text   = static_cast<UIText*>(cs[i].get());   break;
			case ComponentType::UIButton: button = static_cast<UIButton*>(cs[i].get()); break;
			default: break;
			}
		}
		(void)rect;

		auto v4 = [](const json &v, Vec4 &out) -> bool {
			if (!v.is_array() || v.size() != 4) return false;
			out = Vec4(v[0].get<f32>(), v[1].get<f32>(), v[2].get<f32>(), v[3].get<f32>());
			return true;
		};

		Vec4 c;
		if (image)
		{
			if (bag.find("tint") != bag.end() && v4(bag["tint"], c)) image->SetTint(c);
			if (bag.find("border") != bag.end() && v4(bag["border"], c)) image->SetBorder(c);
			if (bag.find("texture") != bag.end() && bag["texture"].is_string())
			{
				const std::string rel = bag["texture"].get<std::string>();
				if (rel.empty()) image->SetTexture(std::shared_ptr<Texture>());
				else
				{
					std::shared_ptr<Texture> t = std::make_shared<Texture>();
					const std::string abs = textureRoot.empty() ? rel : (textureRoot + "/" + rel);
					if (t->LoadTexture(abs, TextureType::Texture))
					{
						t->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
						t->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
						image->SetTexture(t);
					}
					else { errOut = "could not load " + abs; return false; }
				}
			}
		}
		if (text)
		{
			if (bag.find("color") != bag.end() && v4(bag["color"], c)) text->SetColor(c);
			if (bag.find("size") != bag.end() && bag["size"].is_number()) text->SetSize(bag["size"].get<f32>());
			uint32 h = text->GetHorizontalAlignment(), v = text->GetVerticalAlignment();
			if (bag.find("align") != bag.end() && bag["align"].is_string())
			{
				const std::string n = bag["align"].get<std::string>();
				h = (n == "Center") ? UIAlign::Center : (n == "Right") ? UIAlign::Right : UIAlign::Left;
			}
			if (bag.find("verticalAlign") != bag.end() && bag["verticalAlign"].is_string())
			{
				const std::string n = bag["verticalAlign"].get<std::string>();
				v = (n == "Middle") ? UIVerticalAlign::Middle : (n == "Bottom") ? UIVerticalAlign::Bottom : UIVerticalAlign::Top;
			}
			text->SetAlignment(h, v);
		}
		if (button)
		{
			if (bag.find("transition") != bag.end() && bag["transition"].is_number())
				button->SetTransition(bag["transition"].get<f32>());
			const char* names[3] = { "hover", "pressed", "disabled" };
			const uint32 ids[3] = { UIState::Hover, UIState::Pressed, UIState::Disabled };
			for (int i = 0; i < 3; i++)
			{
				const std::string tk = std::string(names[i]) + "Tint";
				const std::string ck = std::string(names[i]) + "TextColor";
				if (bag.find(tk) != bag.end() && v4(bag[tk], c)) { button->State(ids[i]).hasTint = true; button->State(ids[i]).tint = c; }
				if (bag.find(ck) != bag.end() && v4(bag[ck], c)) { button->State(ids[i]).hasTextColor = true; button->State(ids[i]).textColor = c; }
			}
			if (bag.find("pressedOffset") != bag.end() && bag["pressedOffset"].is_array() && bag["pressedOffset"].size() == 2)
				button->State(UIState::Pressed).offset = Vec2(bag["pressedOffset"][0].get<f32>(), bag["pressedOffset"][1].get<f32>());
		}
		return true;
	}

	// ------------------------------------------------------------------
	// Whole-scene re-skin
	//
	// Called after a scene loads, by both the editor and the player. Every
	// element that names a style gets it re-applied, so editing a style
	// file - or swapping the palette - changes every element using it
	// without touching a single scene. This is why a style must not carry
	// layout or content: doing this automatically would otherwise move
	// things and rewrite labels.
	// ------------------------------------------------------------------

	inline void ApplyToSubtree(p3d::GameObject* node, const std::string &assetRoot,
		const Palette &palette, std::map<std::string, json> &styleCache, int &applied, std::string &firstError)
	{
		using namespace p3d;
		if (!node) return;
		const std::vector<std::shared_ptr<IComponent> > &cs = node->GetComponents();
		for (size_t i = 0; i < cs.size(); i++)
		{
			if (!cs[i] || cs[i]->GetComponentType() != ComponentType::UIRect) continue;
			const std::string ref = static_cast<UIRect*>(cs[i].get())->GetStyleRef();
			if (ref.empty()) break;

			std::map<std::string, json>::iterator cached = styleCache.find(ref);
			if (cached == styleCache.end())
			{
				json st;
				const std::string abs = assetRoot.empty() ? ref : (assetRoot + "/" + ref);
				if (!ReadJsonFile(abs, st))
				{
					if (firstError.empty()) firstError = "could not read style " + ref;
					// Cached as an empty object so a missing file is read
					// once, not once per element using it.
					styleCache[ref] = json::object();
					break;
				}
				styleCache[ref] = st;
				cached = styleCache.find(ref);
			}
			if (cached->second.empty()) break;

			json bag; std::string err;
			if (!Resolve(cached->second, palette, bag, err))
			{
				if (firstError.empty()) firstError = ref + ": " + err;
				break;
			}
			if (ApplyProperties(node, bag, assetRoot, err)) applied++;
			else if (firstError.empty()) firstError = ref + ": " + err;
			break;
		}
		const std::vector<std::shared_ptr<GameObject> > &kids = node->GetChildren();
		for (size_t i = 0; i < kids.size(); i++) ApplyToSubtree(kids[i].get(), assetRoot, palette, styleCache, applied, firstError);
	}

	inline int ApplyToScene(p3d::SceneGraph* scene, const std::string &assetRoot,
		const std::string &palettePath, std::string &firstError)
	{
		using namespace p3d;
		if (!scene) return 0;
		const Palette palette = palettePath.empty() ? Palette() : LoadPalette(palettePath);
		std::map<std::string, json> styleCache;
		int applied = 0;
		// Every root, not UICanvas::GetCanvasesOnScene(): a canvas only
		// enters that registry when the scene traversal first reaches it, so
		// immediately after a load - which is exactly when this runs - the
		// registry is still empty and this silently styled nothing.
		std::vector<std::shared_ptr<GameObject> > &roots = scene->GetAllGameObjectList();
		for (size_t i = 0; i < roots.size(); i++)
			ApplyToSubtree(roots[i].get(), assetRoot, palette, styleCache, applied, firstError);
		return applied;
	}

	// ------------------------------------------------------------------
	// Element -> style
	//
	// "Author one button by hand, promote it, apply it to the rest" is the
	// workflow that makes styles worth having, and it needs this direction
	// as much as the other. Colours are written back as palette names where
	// one matches exactly, so extracting from a themed element does not
	// quietly bake the theme into the style.
	// ------------------------------------------------------------------

	inline json ExtractFromElement(p3d::GameObject* go, const Palette &palette)
	{
		using namespace p3d;
		json out;
		out["version"] = 1;
		if (!go) return out;

		auto nameFor = [&palette](const Vec4 &c) -> json {
			for (Palette::const_iterator it = palette.begin(); it != palette.end(); ++it)
			{
				const json &v = it->second;
				if (fabsf(v[0].get<f32>() - c.x) < 0.002f && fabsf(v[1].get<f32>() - c.y) < 0.002f
					&& fabsf(v[2].get<f32>() - c.z) < 0.002f && fabsf(v[3].get<f32>() - c.w) < 0.002f)
					return json("@" + it->first);
			}
			return json::array({ c.x, c.y, c.z, c.w });
		};

		const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
		for (size_t i = 0; i < cs.size(); i++)
		{
			if (!cs[i]) continue;
			if (cs[i]->GetComponentType() == ComponentType::UIImage)
			{
				UIImage* img = static_cast<UIImage*>(cs[i].get());
				json im;
				im["tint"] = nameFor(img->GetTint());
				im["border"] = json::array({ img->GetBorder().x, img->GetBorder().y, img->GetBorder().z, img->GetBorder().w });
				if (img->GetTexture() && !img->GetTexture()->GetFilename().empty())
					im["texture"] = img->GetTexture()->GetFilename();
				out["image"] = im;
			}
			else if (cs[i]->GetComponentType() == ComponentType::UIText)
			{
				UIText* t = static_cast<UIText*>(cs[i].get());
				json tx;
				tx["color"] = nameFor(t->GetColor());
				tx["size"] = t->GetSize();
				tx["align"] = (t->GetHorizontalAlignment() == UIAlign::Center) ? "Center"
					: (t->GetHorizontalAlignment() == UIAlign::Right) ? "Right" : "Left";
				tx["verticalAlign"] = (t->GetVerticalAlignment() == UIVerticalAlign::Middle) ? "Middle"
					: (t->GetVerticalAlignment() == UIVerticalAlign::Bottom) ? "Bottom" : "Top";
				out["text"] = tx;
			}
			else if (cs[i]->GetComponentType() == ComponentType::UIButton)
			{
				UIButton* b = static_cast<UIButton*>(cs[i].get());
				json bt;
				bt["transition"] = b->GetTransition();
				const char* names[3] = { "hover", "pressed", "disabled" };
				const uint32 ids[3] = { UIState::Hover, UIState::Pressed, UIState::Disabled };
				for (int k = 0; k < 3; k++)
				{
					const UIStateStyle &ss = b->GetState(ids[k]);
					json st;
					if (ss.hasTint) st["tint"] = nameFor(ss.tint);
					if (ss.hasTextColor) st["textColor"] = nameFor(ss.textColor);
					if (ids[k] == UIState::Pressed && (ss.offset.x != 0.f || ss.offset.y != 0.f))
						st["offset"] = json::array({ ss.offset.x, ss.offset.y });
					if (!st.empty()) bt[names[k]] = st;
				}
				out["button"] = bt;
			}
		}
		return out;
	}

}

#endif /* UISTYLERESOLVER_H */
