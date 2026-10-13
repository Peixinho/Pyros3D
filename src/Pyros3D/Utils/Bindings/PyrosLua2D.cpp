//============================================================================
// Name        : PyrosLua2D.cpp
// Description : The 2D toolkit as scripts see it: sprites, sprite batches,
//               and constructors for the 2D components that could only be
//               authored in a scene file before.
//============================================================================

#ifdef LUA_BINDINGS

#include <Pyros3D/Utils/Bindings/PyrosLuaBindings.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaHelpers.h>
#include <Pyros3D/Rendering/Components/Sprite2D/Sprite2D.h>
#include <Pyros3D/Rendering/Components/Sprite2D/SpriteBatch2D.h>
#include <Pyros3D/Rendering/Components/Sprite2D/SpriteMask2D.h>
#include <Pyros3D/Rendering/Components/Sprite2D/Line2D.h>
#include <Pyros3D/Rendering/Components/Sprite2D/Text2D.h>
#include <Pyros3D/Rendering/Components/Extrusion2D/Extrusion2D.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <Pyros3D/Rendering/Components/Layer2D/Layer2D.h>
#include <Pyros3D/Rendering/Components/Occluder2D/Occluder2D.h>
#include <Pyros3D/Physics/Physics2D/Physics2D.h>
#include <Pyros3D/Physics/Physics2D/Joint2D.h>
#include <Pyros3D/Physics/Physics2D/Physics2DWorld.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaView2D.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <fstream>

namespace p3d {

	namespace {

		bool FileExists(const std::string &p)
		{
			std::ifstream f(p.c_str());
			return f.good();
		}

		// Turns what a script wrote into something loadable. A script names
		// an asset the way a scene file does ("assets/textures/hero.p3ds"),
		// or relative to the assets folder ("textures/hero.p3ds"), or with
		// ASSETS_PATH already on the front - all three are in use across the
		// example projects, and guessing wrong loads nothing and says so
		// only in the log.
		std::string ResolveForScript(sol::state_view lua, const std::string &path)
		{
			if (path.empty() || FileExists(path)) return path;
			const sol::object ap = lua["ASSETS_PATH"];
			if (!ap.is<std::string>()) return path;
			const std::string assets = ap.as<std::string>();
			std::string rel = path;
			if (rel.compare(0, 7, "assets/") == 0) rel = rel.substr(7);
			if (FileExists(assets + rel)) return assets + rel;
			return path;
		}

		sol::table StringList(sol::state_view lua, const std::vector<std::string> &v)
		{
			sol::table t = lua.create_table();
			for (size_t i = 0; i < v.size(); i++) t[i + 1] = v[i];
			return t;
		}

	}

	void RegisterLua2D(sol::state* lua)
	{
		lua->create_named_table("SpriteDrawMode",
			"Simple", (int)SpriteDraw2D::Simple,
			"Sliced", (int)SpriteDraw2D::Sliced,
			"Tiled", (int)SpriteDraw2D::Tiled);
		lua->create_named_table("SpriteMask",
			"None", (int)Sprite2D::MaskNone,
			"Inside", (int)Sprite2D::MaskInside,
			"Outside", (int)Sprite2D::MaskOutside);
		lua->create_named_table("SpriteMaskShape",
			"Rect", (int)SpriteMaskShape2D::Rect,
			"Circle", (int)SpriteMaskShape2D::Circle,
			"Polygon", (int)SpriteMaskShape2D::Polygon,
			"Image", (int)SpriteMaskShape2D::Image);

		// A mask: a shape sprites are shown only inside (or outside) of.
		//   local m = SpriteMask2D.new()
		//   m:setSize(4, 2)                      -- a rectangle 4 wide, 2 tall
		//   window:addComponent(m)
		//   sprite:setMaskInteraction(SpriteMask.Inside)
		lua->new_usertype<SpriteMask2D>("SpriteMask2D",
			sol::factories([]() { return std::make_shared<SpriteMask2D>(); }),
			"setShape", &SpriteMask2D::SetShape,
			"getShape", &SpriteMask2D::GetShape,
			"setSize", sol::overload(
				[](SpriteMask2D &m, const Vec2 &s) { m.SetSize(s); },
				[](SpriteMask2D &m, const f32 w, const f32 h) { m.SetSize(Vec2(w, h)); }),
			"getSize", &SpriteMask2D::GetSize,
			"setOffset", sol::overload(
				[](SpriteMask2D &m, const Vec2 &o) { m.SetOffset(o); },
				[](SpriteMask2D &m, const f32 x, const f32 y) { m.SetOffset(Vec2(x, y)); }),
			"getOffset", &SpriteMask2D::GetOffset,
			// The width of the soft edge, in from the outline; 0 is hard.
			"setFeather", &SpriteMask2D::SetFeather,
			"getFeather", &SpriteMask2D::GetFeather,
			// A table of Vec2, 3 to 16, convex. False if it is not.
			"setPolygon", [](SpriteMask2D &m, sol::table pts) {
				std::vector<Vec2> v;
				for (size_t i = 1; i <= pts.size(); i++) v.push_back(pts.get<Vec2>(i));
				const bool ok = m.SetPolygon(v);
				if (ok) m.SetShape(SpriteMaskShape2D::Polygon);
				return ok;
			},
			// The silhouette of an image, laid over the mask's size. False if
			// the image cannot be read.
			"setImage", [](SpriteMask2D &m, const std::string &path, sol::this_state ts) {
				sol::state_view lv(ts);
				const bool ok = m.SetImage(ResolveForScript(lv, path), path);
				if (ok) m.SetShape(SpriteMaskShape2D::Image);
				return ok;
			},
			"setEnabled", &SpriteMask2D::SetEnabled,
			"isEnabled", &SpriteMask2D::IsEnabled,
			sol::base_classes, sol::bases<IComponent>()
			);

		// A line with width through a list of points - or a trail.
		//   local l = Line2D.new()
		//   l:setPoints({ Vec2.new(0, 0), Vec2.new(2, 1), Vec2.new(4, 0) })
		//   l:setWidth(0.2, 0.05)
		//   rope:addComponent(l)
		//
		//   local t = Line2D.new()               -- behind a moving thing
		//   t:setTrail(true)  t:setTrailLifetime(0.4)  t:setWidth(0.3, 0)
		//   bullet:addComponent(t)
		// Words in the world: Text2D.new("assets/fonts/x.ttf", "+100", 0.6),
		// added to an object like a sprite. The size is the em's height in
		// world units; the object's origin is the block's middle unless
		// setAnchor says otherwise ((0, 0) is its bottom left).
		// An outline given depth, with its collider (see Extrusion2D.h):
		//   local ground = go:getComponent("Extrusion2D")
		//   local y = ground:heightAt(x)                 -- an open outline's height at x, in its own space
		//   ground:setPoint(3, x, y) ground:apply()      -- the shape, and the collider with it, changed while playing
		lua->new_usertype<Extrusion2D>("Extrusion2D",
			sol::factories([]() { return std::make_shared<Extrusion2D>(); }),
			"setPoints", [](Extrusion2D &e, sol::table pts) {
				std::vector<Vec2> v;
				for (size_t i = 1; i <= pts.size(); i++)
				{
					sol::object o = pts[i];
					if (o.is<Vec2>()) v.push_back(o.as<Vec2>());
					else if (o.is<sol::table>()) { sol::table t = o; v.push_back(Vec2(t.get_or(1, 0.f), t.get_or(2, 0.f))); }
				}
				e.SetPoints(v);
			},
			"getPointCount", [](Extrusion2D &e) { return (int)e.GetPoints().size(); },
			"getPoint", [](Extrusion2D &e, const int i) {
				if (i < 1 || (size_t)i > e.GetPoints().size()) return std::make_tuple(0.f, 0.f);
				return std::make_tuple(e.GetPoints()[(size_t)i - 1].x, e.GetPoints()[(size_t)i - 1].y);
			},
			"setPoint", [](Extrusion2D &e, const int i, const f32 x, const f32 y) { if (i >= 1) e.SetPoint((uint32)(i - 1), Vec2(x, y)); },
			"addPoint", [](Extrusion2D &e, const f32 x, const f32 y) { e.AddPoint(Vec2(x, y)); },
			"setClosed", &Extrusion2D::SetClosed, "isClosed", &Extrusion2D::IsClosed,
			"setSmooth", &Extrusion2D::SetSmooth, "getSmooth", &Extrusion2D::GetSmooth,
			"setDepth", &Extrusion2D::SetDepth, "getDepth", &Extrusion2D::GetDepth,
			"setFloor", &Extrusion2D::SetFloor, "getFloor", &Extrusion2D::GetFloor,
			"setTile", &Extrusion2D::SetTile,
			"setCollider", &Extrusion2D::SetCollider,
			"setSkinAcross", &Extrusion2D::SetSkinAcross,
			"heightAt", &Extrusion2D::HeightAt,
			// the curve the points make: a table of {x, y}
			"getPath", [](Extrusion2D &e, sol::this_state ts) {
				sol::state_view lv(ts);
				sol::table out = lv.create_table();
				const std::vector<Vec2> &p = e.GetPath();
				for (size_t i = 0; i < p.size(); i++) out[i + 1] = lv.create_table_with(1, p[i].x, 2, p[i].y);
				return out;
			},
			"apply", &Extrusion2D::Apply,
			sol::base_classes, sol::bases<IComponent>());

		lua->new_usertype<Text2D>("Text2D",
			sol::factories(
				[](const std::string &fontPath, const std::string &text, const f32 size, sol::this_state ts) {
					sol::state_view lv(ts);
					std::shared_ptr<Font> font = std::make_shared<Font>(ResolveForScript(lv, fontPath), 64.f, true);
					std::shared_ptr<Text2D> t = std::make_shared<Text2D>(font, text, size);
					t->SetFontPath(fontPath);
					return t;
				}),
			"setText", &Text2D::SetText,
			"getText", &Text2D::GetText,
			"setColor", sol::overload(
				[](Text2D &t, const Vec4 &c) { t.SetColor(c); },
				[](Text2D &t, const f32 r, const f32 g, const f32 b, const f32 a) { t.SetColor(Vec4(r, g, b, a)); }),
			"getColor", &Text2D::GetColor,
			"setAlpha", &Text2D::SetAlpha,
			"setSize", &Text2D::SetSize,
			"getSize", &Text2D::GetSize,
			"setAnchor", sol::overload(
				[](Text2D &t, const Vec2 &a) { t.SetAnchor(a); },
				[](Text2D &t, const f32 x, const f32 y) { t.SetAnchor(Vec2(x, y)); }),
			"getBlockSize", [](Text2D &t) { const Vec2 s = t.GetBlockSize(); return std::make_tuple(s.x, s.y); },
			sol::base_classes, sol::bases<IComponent>()
			);

		lua->new_usertype<Line2D>("Line2D",
			sol::factories([]() { return std::make_shared<Line2D>(); }),
			"setPoints", [](Line2D &l, sol::table pts) {
				std::vector<Vec2> v;
				for (size_t i = 1; i <= pts.size(); i++) v.push_back(pts.get<Vec2>(i));
				l.SetPoints(v);
			},
			"getPoints", [](Line2D &l) { return sol::as_table(l.GetPoints()); },
			"addPoint", sol::overload(
				[](Line2D &l, const Vec2 &p) { l.AddPoint(p); },
				[](Line2D &l, const f32 x, const f32 y) { l.AddPoint(Vec2(x, y)); }),
			// 1-based, like every list in a script.
			"setPoint", sol::overload(
				[](Line2D &l, const int i, const Vec2 &p) { if (i >= 1) l.SetPoint((uint32)(i - 1), p); },
				[](Line2D &l, const int i, const f32 x, const f32 y) { if (i >= 1) l.SetPoint((uint32)(i - 1), Vec2(x, y)); }),
			"clearPoints", &Line2D::ClearPoints,
			"setClosed", &Line2D::SetClosed,
			"isClosed", &Line2D::IsClosed,
			"setWidth", sol::overload(
				[](Line2D &l, const f32 w) { l.SetWidth(w, w); },
				[](Line2D &l, const f32 a, const f32 b) { l.SetWidth(a, b); }),
			"setColor", sol::overload(
				[](Line2D &l, const f32 r, const f32 g, const f32 b, const f32 a) { l.SetColors(Vec4(r, g, b, a), Vec4(r, g, b, a)); },
				[](Line2D &l, const Vec4 &c) { l.SetColors(c, c); }),
			"setColors", [](Line2D &l, const Vec4 &a, const Vec4 &b) { l.SetColors(a, b); },
			"setAdditive", &Line2D::SetAdditive,
			// An image along the line ("" for none), and how much length one
			// copy of it covers (0: stretched once, end to end).
			"setTexture", [](Line2D &l, const std::string &path, sol::this_state ts) {
				sol::state_view lv(ts);
				l.SetTexture(path.empty() ? std::string() : ResolveForScript(lv, path), path);
			},
			"setTextureRepeat", &Line2D::SetTextureRepeat,
			"setTextureNearest", &Line2D::SetTextureNearest,
			"setSortOrder", &Line2D::SetSortOrder,
			"setTrail", &Line2D::SetTrail,
			"isTrail", &Line2D::IsTrail,
			"setTrailLifetime", &Line2D::SetTrailLifetime,
			"setTrailMinDistance", &Line2D::SetTrailMinDistance,
			"setEmitting", &Line2D::SetEmitting,
			"isEmitting", &Line2D::IsEmitting,
			"clearTrail", &Line2D::ClearTrail,
			sol::base_classes, sol::bases<IComponent>()
			);

		lua->create_named_table("SpriteLighting",
			"Unlit", (int)SpriteLighting2D::Unlit,
			"Lit2D", (int)SpriteLighting2D::Lit2D,
			"Lit3D", (int)SpriteLighting2D::Lit3D);
		lua->create_named_table("SpriteBlend",
			"Alpha", (int)SpriteBlend2D::Alpha,
			"Additive", (int)SpriteBlend2D::Additive,
			"Premultiplied", (int)SpriteBlend2D::Premultiplied);

		// A sprite. Everything a 2D game does to one, per frame, is here:
		// which clip, which way it faces, what colour. None of it touches a
		// material, so none of it costs anything shared.
		//
		//   local s = Sprite2D.new("assets/textures/hero.p3ds")
		//   go:addComponent(s)
		//   s:play("run")
		//   s:setFlipX(vx < 0)
		//
		// Sprite2D.new(path) loads the sheet (or a bare image) there and
		// then; a sprite made with no path draws nothing until load() is
		// called on it.
		lua->new_usertype<Sprite2D>("Sprite2D",
			sol::factories(
				[]() { return std::make_shared<Sprite2D>(); },
				[](const std::string &path, sol::this_state ts) {
					sol::state_view lv(ts);
					std::shared_ptr<Sprite2D> s = std::make_shared<Sprite2D>();
					std::string err;
					if (!s->LoadSource(ResolveForScript(lv, path),
						[lv](const std::string &p) { return ResolveForScript(lv, p); }, &err))
						echo("WARNING: Sprite2D.new: " + err);
					s->SetSourcePath(path);
					return s;
				}),
			// Returns true, or false and the reason.
			"load", [](Sprite2D &s, const std::string &path, sol::this_state ts) {
				sol::state_view lv(ts);
				std::string err;
				const bool ok = s.LoadSource(ResolveForScript(lv, path),
					[lv](const std::string &p) { return ResolveForScript(lv, p); }, &err);
				if (ok) s.SetSourcePath(path);
				return std::make_tuple(ok, err);
			},
			"getSource", &Sprite2D::GetSourcePath,

			"setFrame", sol::overload(
				[](Sprite2D &s, const int32 i) { s.SetFrame(i); return true; },
				[](Sprite2D &s, const std::string &n) { return s.SetFrameByName(n); }),
			"getFrame", &Sprite2D::GetFrame,
			"getFrameName", &Sprite2D::GetFrameName,
			"frameCount", &Sprite2D::FrameCount,

			"setFlipX", &Sprite2D::SetFlipX,
			"setFlipY", &Sprite2D::SetFlipY,
			"getFlipX", &Sprite2D::GetFlipX,
			"getFlipY", &Sprite2D::GetFlipY,
			// A Vec4, or four numbers (alpha optional) - the second because a
			// script fading something writes s:setColor(1, 1, 1, a) and should
			// not have to build a vector to do it.
			"setColor", sol::overload(
				[](Sprite2D &s, const Vec4 &c) { s.SetColor(c); },
				[](Sprite2D &s, const f32 r, const f32 g, const f32 b) { s.SetColor(Vec4(r, g, b, s.GetColor().w)); },
				[](Sprite2D &s, const f32 r, const f32 g, const f32 b, const f32 a) { s.SetColor(Vec4(r, g, b, a)); }),
			"getColor", &Sprite2D::GetColor,
			"setAlpha", &Sprite2D::SetAlpha,
			"setDrawMode", &Sprite2D::SetDrawMode,
			"getDrawMode", &Sprite2D::GetDrawMode,
			"setSize", sol::overload(
				[](Sprite2D &s, const Vec2 &v) { s.SetSize(v); },
				[](Sprite2D &s, const f32 w, const f32 h) { s.SetSize(Vec2(w, h)); }),
			"getSize", &Sprite2D::GetSize,
			"setPixelsPerUnit", &Sprite2D::SetPixelsPerUnit,
			"getPixelsPerUnit", &Sprite2D::EffectivePixelsPerUnit,
			"setPivot", sol::overload(
				[](Sprite2D &s, const Vec2 &v) { s.SetPivot(v); },
				[](Sprite2D &s, const f32 x, const f32 y) { s.SetPivot(Vec2(x, y)); }),
			"clearPivot", &Sprite2D::ClearPivot,
			"getPivot", &Sprite2D::GetPivot,
			"setLit", &Sprite2D::SetLit,
			"setLighting", &Sprite2D::SetLighting,
			// A .mat in place of the sprite's own material; "" (or
			// clearMaterial) puts its own back. Returns whether it loaded.
			"setMaterial", [](Sprite2D &s, const std::string &path, sol::this_state ts) {
				if (path.empty()) { s.ClearCustomMaterial(); return true; }
				sol::state_view lv(ts);
				std::shared_ptr<IMaterial> m = SceneSerializer::LoadMaterialFile(ResolveForScript(lv, path), path);
				if (!m) return false;
				s.SetCustomMaterial(m, path);
				return true;
			},
			"clearMaterial", &Sprite2D::ClearCustomMaterial,
			"getMaterial", [](Sprite2D &s) { return s.GetMaterialPath(); },
			"getLighting", &Sprite2D::GetLighting,
			"isLit", &Sprite2D::IsLit,
			"setBlend", &Sprite2D::SetBlend,
			"getBlend", &Sprite2D::GetBlend,
			"setSortOrder", &Sprite2D::SetSortOrder,
			// Hidden, it draws nothing (alone or in a batch) and its clip
			// stands still.
			"setVisible", &Sprite2D::SetVisible,
			"isVisible", &Sprite2D::IsVisible,
			// Shown only inside a SpriteMask2D (SpriteMask.Inside) or only
			// outside one; the name is the mask's object's, or leave it for
			// the nearest mask above the sprite.
			"setMaskInteraction", &Sprite2D::SetMaskInteraction,
			"getMaskInteraction", &Sprite2D::GetMaskInteraction,
			"setMaskName", &Sprite2D::SetMaskName,
			"getMaskName", &Sprite2D::GetMaskName,
			"getSortOrder", &Sprite2D::GetSortOrder,
			// minX, minY, maxX, maxY in the owner's local space.
			"getLocalRect", [](Sprite2D &s) {
				const Vec4 r = s.GetLocalRect();
				return std::make_tuple(r.x, r.y, r.z, r.w);
			},

			// play(name) keeps a clip that is already playing going;
			// play(name, true) restarts it.
			"play", sol::overload(
				[](Sprite2D &s, const std::string &n) { return s.Play(n, false); },
				[](Sprite2D &s, const std::string &n, const bool restart) { return s.Play(n, restart); }),
			"stop", &Sprite2D::Stop,
			"pause", sol::overload(
				[](Sprite2D &s) { s.Pause(true); },
				[](Sprite2D &s, const bool p) { s.Pause(p); }),
			"resume", [](Sprite2D &s) { s.Pause(false); },
			"isPlaying", &Sprite2D::IsPlaying,
			"isFinished", &Sprite2D::IsFinished,
			"getClip", &Sprite2D::GetClip,
			"hasClip", &Sprite2D::HasClip,
			"getClips", [](Sprite2D &s, sol::this_state ts) { return StringList(sol::state_view(ts), s.ClipNames()); },
			"setSpeed", &Sprite2D::SetSpeed,
			"getSpeed", &Sprite2D::GetSpeed,
			"getClipTime", &Sprite2D::GetClipTime,
			"setClipTime", &Sprite2D::SetClipTime,
			"setAutoPlay", &Sprite2D::SetAutoPlay,
			// Assign a function: onEvent(name) for a clip's named events,
			// onFinished(clipName) when a one-shot clip ends.
			"onEvent", &Sprite2D::OnEvent,
			"onFinished", &Sprite2D::OnFinished,
			sol::base_classes, sol::bases<IComponent>()
			);

		// --- 2D physics: constructors and what could not be reached ---------
		lua->create_named_table("Shape2D",
			"Box", (int)Shape2DType::Box,
			"Circle", (int)Shape2DType::Circle,
			"Capsule", (int)Shape2DType::Capsule,
			"Polygon", (int)Shape2DType::Polygon,
			"Chain", (int)Shape2DType::Chain);
		lua->create_named_table("JointType2D",
			"Hinge", (int)Joint2DType::Hinge,
			"Distance", (int)Joint2DType::Distance,
			"Weld", (int)Joint2DType::Weld,
			"Slider", (int)Joint2DType::Slider,
			"Wheel", (int)Joint2DType::Wheel,
			"Target", (int)Joint2DType::Target,
			"Ignore", (int)Joint2DType::Ignore,
			"Pulley", (int)Joint2DType::Pulley,
			"Gear", (int)Joint2DType::Gear);

		// A joint. Made in a script the same way one is authored in the
		// editor - on the object that moves, naming the other:
		//
		//   local j = Joint2D.new(JointType2D.Hinge)
		//   j:setConnected("Post")          -- or nothing, for the world
		//   j:setAnchor(-1, 0)
		//   j:setLimits(true, -0.5, 0.5)
		//   door:addComponent(j)
		lua->new_usertype<Joint2D>("Joint2D",
			sol::factories(
				[]() { return std::make_shared<Joint2D>(); },
				[](const int type) { return std::make_shared<Joint2D>((uint32)type); }),
			"getType", &Joint2D::GetJointType,
			"setType", &Joint2D::SetJointType,
			"getConnected", &Joint2D::GetConnected,
			"setConnected", &Joint2D::SetConnected,
			"getAnchor", &Joint2D::GetAnchor,
			"setAnchor", sol::overload(
				[](Joint2D &j, const Vec2 &v) { j.SetAnchor(v); },
				[](Joint2D &j, const f32 x, const f32 y) { j.SetAnchor(Vec2(x, y)); }),
			"setConnectedAnchor", sol::overload(
				[](Joint2D &j, const Vec2 &v) { j.SetConnectedAnchor(v); },
				[](Joint2D &j, const f32 x, const f32 y) { j.SetConnectedAnchor(Vec2(x, y)); }),
			"clearConnectedAnchor", &Joint2D::ClearConnectedAnchor,
			"setAxis", sol::overload(
				[](Joint2D &j, const Vec2 &v) { j.SetAxis(v); },
				[](Joint2D &j, const f32 x, const f32 y) { j.SetAxis(Vec2(x, y)); }),
			"getAxis", &Joint2D::GetAxis,
			"setCollideConnected", &Joint2D::SetCollideConnected,
			"setLimits", &Joint2D::SetLimits,
			"getLowerLimit", &Joint2D::GetLowerLimit,
			"getUpperLimit", &Joint2D::GetUpperLimit,
			"setMotor", &Joint2D::SetMotor,
			// Every frame if you like: applied to the running joint.
			"setMotorSpeed", &Joint2D::SetMotorSpeed,
			// A joint that gives way under more than this; joint.onBreak =
			// function() ... end hears it go, joint:mend() makes it again.
			"setBreakForce", &Joint2D::SetBreakForce,
			"getBreakForce", &Joint2D::GetBreakForce,
			"isBroken", &Joint2D::IsBroken,
			"mend", &Joint2D::Mend,
			"onBreak", &Joint2D::OnBreak,
			// Target: where it pulls to, in the world. Cheap - set it every
			// frame (the mouse, a marker).
			"setTarget", sol::overload(
				[](Joint2D &j, const Vec2 &t) { j.SetTarget(t); },
				[](Joint2D &j, const f32 x, const f32 y) { j.SetTarget(Vec2(x, y)); }),
			"getTarget", &Joint2D::GetTarget,
			// Pulley: the two fixed points its rope runs over, in the world
			// (this body's, then the other's). Pulley and Gear: the ratio.
			"setGroundAnchors", sol::overload(
				[](Joint2D &j, const Vec2 &a, const Vec2 &b) { j.SetGroundAnchors(a, b); },
				[](Joint2D &j, const f32 ax, const f32 ay, const f32 bx, const f32 by) { j.SetGroundAnchors(Vec2(ax, ay), Vec2(bx, by)); }),
			"setRatio", &Joint2D::SetRatio,
			"getRatio", &Joint2D::GetRatio,
			"isBuilt", &Joint2D::IsBuilt,
			"getMotorSpeed", &Joint2D::GetMotorSpeed,
			"setSpring", &Joint2D::SetSpring,
			"setLength", &Joint2D::SetLength,
			"getLength", &Joint2D::GetLength,
			// Takes where the bodies are NOW as the joint's zero.
			"rezero", &Joint2D::ResetReference,
			"isBuilt", &Joint2D::HaveJoint,
			sol::base_classes, sol::bases<IComponent>()
			);

		lua->new_usertype<Occluder2D>("Occluder2D",
			sol::factories(
				[]() { return std::make_shared<Occluder2D>(); },
				[](const int shape, const f32 hw, const f32 hh) { return std::make_shared<Occluder2D>((uint32)shape, Vec2(hw, hh)); }),
			"getSize", &Occluder2D::GetSize,
			"setSize", sol::overload(
				[](Occluder2D &o, const Vec2 &v) { o.SetSize(v); },
				[](Occluder2D &o, const f32 x, const f32 y) { o.SetSize(Vec2(x, y)); }),
			"setShapeType", &Occluder2D::SetShapeType,
			"getShapeType", &Occluder2D::GetShapeType,
			"setEnabled", &Occluder2D::SetEnabled,
			"isEnabled", &Occluder2D::IsEnabled,
			sol::base_classes, sol::bases<IComponent>()
			);

		lua->new_usertype<SpriteBatch2D>("SpriteBatch2D",
			sol::factories([]() { return std::make_shared<SpriteBatch2D>(); }),
			"setEnabled", &SpriteBatch2D::SetEnabled,
			"isEnabled", &SpriteBatch2D::IsEnabled,
			"setBatchLit", &SpriteBatch2D::SetBatchLit,
			"batchesLit", &SpriteBatch2D::BatchesLit,
			"setLightCellSize", &SpriteBatch2D::SetLightCellSize,
			"getLightCellSize", &SpriteBatch2D::GetLightCellSize,
			"spriteCount", &SpriteBatch2D::SpriteCount,
			"drawCalls", &SpriteBatch2D::DrawCalls,
			"rebuilds", &SpriteBatch2D::Rebuilds,
			sol::base_classes, sol::bases<IComponent>()
			);
	}


	// --- the scene's view ------------------------------------------------------

	namespace {
		GameObject* FindByNameIn(GameObject* go, const std::string &name)
		{
			if (go->GetName() == name) return go;
			const std::vector<std::shared_ptr<GameObject> > &kids = go->GetChildren();
			for (size_t i = 0; i < kids.size(); i++)
				if (kids[i])
					if (GameObject* hit = FindByNameIn(kids[i].get(), name)) return hit;
			return NULL;
		}

		void ForEachLayer(GameObject* go, const std::function<void(GameObject*, Layer2D*)> &f)
		{
			const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
			for (size_t c = 0; c < comps.size(); c++)
				if (comps[c] && comps[c]->GetComponentType() == ComponentType::Layer2D)
					f(go, static_cast<Layer2D*>(comps[c].get()));
			const std::vector<std::shared_ptr<GameObject> > &kids = go->GetChildren();
			for (size_t i = 0; i < kids.size(); i++)
				if (kids[i]) ForEachLayer(kids[i].get(), f);
		}
	}

	bool UpdateSceneView2D(SceneMeta::View2D &view, SceneGraph* scene, const f32 dt, const f32 aspect)
	{
		if (!view.enabled) return false;

		// What it follows, by name, ANYWHERE in the scene. The roots alone
		// used to be searched, so a character placed under a layer - where a
		// character belongs - was never found and the view sat still.
		GameObject* target = NULL;
		if (!view.follow.empty() && scene != NULL)
		{
			std::vector<std::shared_ptr<GameObject> > &roots = scene->GetAllGameObjectList();
			for (size_t i = 0; i < roots.size() && target == NULL; i++)
				if (roots[i]) target = FindByNameIn(roots[i].get(), view.follow);
		}
		if (target != NULL)
		{
			const Vec3 at = target->GetWorldPosition();
			view.Update(dt, aspect, &at);
		}
		else view.Update(dt, aspect, NULL);

		if (view.autoParallax && scene != NULL && Layer2D::LiveCount() > 0)
		{
			// From the centre the view is DRAWN at, so a shake moves the
			// layers with it and does not smear them against each other.
			const Vec2 c = view.RenderCenter();
			std::vector<std::shared_ptr<GameObject> > &roots = scene->GetAllGameObjectList();
			for (size_t i = 0; i < roots.size(); i++)
				if (roots[i])
					ForEachLayer(roots[i].get(), [&c](GameObject*, Layer2D* l) {
						if (l->IsAutomatic()) l->ScrollWithView(c);
					});
		}
		return true;
	}

	void ResetAutomaticLayers(SceneGraph* scene)
	{
		if (scene == NULL || Layer2D::LiveCount() == 0) return;
		std::vector<std::shared_ptr<GameObject> > &roots = scene->GetAllGameObjectList();
		for (size_t i = 0; i < roots.size(); i++)
			if (roots[i])
				ForEachLayer(roots[i].get(), [](GameObject*, Layer2D* l) {
					if (l->IsAutomatic()) l->ResetScroll();
				});
	}

	void LuaBindView2D(sol::state &lua, const LuaView2DHost &hostIn)
	{
		// By value: the lambdas below outlive whoever called this.
		const LuaView2DHost host = hostIn;
		auto V = [host]() -> SceneMeta::View2D* { return host.view ? host.view() : NULL; };
		auto touched = [host]() { if (host.changed) host.changed(); };
		auto size = [host]() -> Vec2 { return host.viewportSize ? host.viewportSize() : Vec2(1.f, 1.f); };

		// A table of plain functions rather than a usertype: the view is a
		// handful of numbers on the scene, not an object with a lifetime, and
		// a script wanting "move the camera left" should not have to find out
		// what owns it.
		//
		// Writing a position, a zoom, a follow or bounds turns the view on. A
		// script that frames the scene has said, unambiguously, that the
		// scene is framed by it.
		sol::table v = lua.create_table();
		v["setCenter"] = [V, touched](f32 x, f32 y) {
			if (SceneMeta::View2D* w = V()) { w->enabled = true; w->center = Vec2(x, y); touched(); }
		};
		v["center"] = [V]() {
			SceneMeta::View2D* w = V();
			return w ? std::make_tuple(w->center.x, w->center.y) : std::make_tuple(0.f, 0.f);
		};
		v["setZoom"] = [V, touched](f32 halfHeight) {
			SceneMeta::View2D* w = V();
			if (!w) return;
			w->enabled = true;
			if (halfHeight > 0.0001f) { w->halfHeight = halfHeight; w->zoomTarget = 0.f; }
			touched();
		};
		v["zoom"] = [V]() { SceneMeta::View2D* w = V(); return w ? w->halfHeight : 0.f; };
		// Eased rather than cut: zoomTo(4, 0.5) closes in over half a second.
		v["zoomTo"] = [V, touched](f32 halfHeight, sol::optional<f32> seconds) {
			if (SceneMeta::View2D* w = V()) { w->enabled = true; w->ZoomTo(halfHeight, seconds ? *seconds : 0.4f); touched(); }
		};
		// By NAME, matching how the editor stores it. An empty name is a
		// fixed view, which is how you stop following something.
		v["follow"] = [V, touched](const std::string &name, sol::optional<f32> lag) {
			SceneMeta::View2D* w = V();
			if (!w) return;
			w->enabled = true;
			w->follow = name;
			if (lag) w->followLag = *lag;
			touched();
		};
		v["setFollowOffset"] = [V, touched](f32 x, f32 y) {
			if (SceneMeta::View2D* w = V()) { w->followOffset = Vec2(x, y); touched(); }
		};
		// Which axes the follow moves. followAxes(true, false) is the
		// side-scroller default: track the character across the level, leave
		// the horizon where it is.
		v["followAxes"] = [V, touched](bool x, bool y) {
			if (SceneMeta::View2D* w = V()) { w->followX = x; w->followY = y; touched(); }
		};
		v["setBounds"] = [V, touched](f32 minX, f32 minY, f32 maxX, f32 maxY) {
			SceneMeta::View2D* w = V();
			if (!w) return;
			w->enabled = true;
			w->clamp = true;
			w->clampMin = Vec2(minX, minY);
			w->clampMax = Vec2(maxX, maxY);
			touched();
		};
		v["clearBounds"] = [V, touched]() { if (SceneMeta::View2D* w = V()) { w->clamp = false; touched(); } };
		// Half-extents of the box the followed object can move in without
		// the view moving. (0, 0) turns it off.
		v["setDeadZone"] = [V, touched](f32 halfW, f32 halfH) {
			if (SceneMeta::View2D* w = V()) { w->deadZone = Vec2(halfW > 0.f ? halfW : 0.f, halfH > 0.f ? halfH : 0.f); touched(); }
		};
		// How far the view leads what it follows, per axis, and how long the
		// lead takes to swing round.
		v["setLookAhead"] = [V, touched](f32 x, sol::optional<f32> y, sol::optional<f32> lag) {
			SceneMeta::View2D* w = V();
			if (!w) return;
			w->lookAhead = Vec2(x, y ? *y : 0.f);
			if (lag) w->lookAheadLag = *lag;
			touched();
		};
		// shake(0.3, 0.4) - a third of a unit, dying away over 0.4 seconds.
		v["shake"] = [V](f32 amplitude, f32 duration, sol::optional<f32> frequency) {
			if (SceneMeta::View2D* w = V()) w->Shake(amplitude, duration, frequency ? *frequency : 28.f);
		};
		// Pixels of the art per world unit; 0 turns snapping off.
		v["setPixelSnap"] = [V, touched](f32 pixelsPerUnit) {
			if (SceneMeta::View2D* w = V()) { w->pixelSnap = pixelsPerUnit > 0.f ? pixelsPerUnit : 0.f; touched(); }
		};
		// Sets the zoom so that one pixel of the art is exactly `scale`
		// pixels of the screen - the only zooms at which pixel art is not
		// resampled - and snaps the view to that grid.
		v["setPixelPerfect"] = [V, touched, size](f32 pixelsPerUnit, sol::optional<f32> scale) {
			SceneMeta::View2D* w = V();
			if (!w || !(pixelsPerUnit > 0.f)) return;
			const f32 k = (scale && *scale >= 1.f) ? floorf(*scale) : 1.f;
			w->enabled = true;
			w->halfHeight = size().y / (2.f * pixelsPerUnit * k);
			w->zoomTarget = 0.f;
			w->pixelSnap = pixelsPerUnit * k;
			touched();
		};
		v["setAutoParallax"] = [V, touched](bool on) { if (SceneMeta::View2D* w = V()) { w->autoParallax = on; touched(); } };
		// minX, minY, maxX, maxY of the world on screen: what a spawner
		// checks before putting an enemy "just off screen".
		v["visibleRect"] = [V, size]() {
			SceneMeta::View2D* w = V();
			const Vec2 s = size();
			const Vec4 r = w ? w->VisibleRect(s.y > 0.f ? s.x / s.y : 1.f) : Vec4(0.f, 0.f, 0.f, 0.f);
			return std::make_tuple(r.x, r.y, r.z, r.w);
		};
		// A pixel of the game's viewport (from its top-left) to the world,
		// and back.
		v["screenToWorld"] = [V, size](f32 px, f32 py) {
			SceneMeta::View2D* w = V();
			const Vec2 p = w ? w->ScreenToWorld(Vec2(px, py), size()) : Vec2(0.f, 0.f);
			return std::make_tuple(p.x, p.y);
		};
		v["worldToScreen"] = [V, size](f32 x, f32 y) {
			SceneMeta::View2D* w = V();
			const Vec2 p = w ? w->WorldToScreen(Vec2(x, y), size()) : Vec2(0.f, 0.f);
			return std::make_tuple(p.x, p.y);
		};
		// Where the mouse is in the world. The one-liner an aiming reticle,
		// a drag or a click-to-move wants - and the host knows where its own
		// viewport is, which a script running in the editor's preview cannot.
		v["mouseWorld"] = [V, size, host]() {
			SceneMeta::View2D* w = V();
			const Vec2 m = host.mouseInViewport ? host.mouseInViewport() : Vec2(0.f, 0.f);
			const Vec2 p = w ? w->ScreenToWorld(m, size()) : Vec2(0.f, 0.f);
			return std::make_tuple(p.x, p.y);
		};
		lua["view"] = v;
	}

}

#endif
