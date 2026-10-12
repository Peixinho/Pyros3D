//============================================================================
// Name        : SceneEditor2D.cpp
// Description : The Scene Editor's 2D physics: the Physics 2D, Joint 2D and
//               Layer 2D sections of Properties, the operations behind them,
//               and the agent commands that reach every control they have.
//
//               Beside SceneEditorSprites.cpp, for the same reason that file
//               exists.
//============================================================================

#include "SceneEditor.h"
#include "SceneCommands.h"
#include <Pyros3D/Physics/Physics2D/Physics2D.h>
#include <Pyros3D/Physics/Physics2D/Joint2D.h>
#include <Pyros3D/Physics/Physics2D/Physics2DWorld.h>
#include <Pyros3D/Rendering/Components/Layer2D/Layer2D.h>
#include <Pyros3D/Rendering/Components/Sprite2D/Sprite2D.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <algorithm>
#include <cmath>
#include <map>

using namespace p3d;
using json = nlohmann::json;

namespace {

	template <typename T>
	T* FindComponent(GameObject* go, const uint32 type)
	{
		if (!go) return NULL;
		const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
		for (size_t i = 0; i < comps.size(); i++)
			if (comps[i] && comps[i]->GetComponentType() == type)
				return static_cast<T*>(comps[i].get());
		return NULL;
	}

	SceneObject* FindObjectByName(SceneObjects* so, const std::string& name)
	{
		const std::map<uint32, SceneObject*>& list = so->GetList();
		for (std::map<uint32, SceneObject*>::const_iterator i = list.begin(); i != list.end(); i++)
			if (i->second && i->second->GetType() == SceneObjectTypes::GAMEOBJECT
				&& i->second->GetName() == name)
				return i->second;
		return NULL;
	}

	std::string Lower(std::string s)
	{
		for (size_t i = 0; i < s.size(); i++) s[i] = (char)tolower((unsigned char)s[i]);
		return s;
	}

	const char* const kJointNames[] = { "hinge", "distance", "weld", "slider", "wheel", "target", "ignore", "pulley", "gear" };
	const char* const kJointLabels[] = { "Hinge", "Distance", "Weld", "Slider", "Wheel", "Target", "Ignore", "Pulley", "Gear" };
	const char* const kShapeNames[] = { "box", "circle", "capsule", "polygon", "chain" };

	const f32 kRadToDeg = 57.29577951f;

}

// --- operations ----------------------------------------------------------------

bool SceneEditor::OpAddJoint2D(uint32 goId, const uint32 jointType, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "not a game object"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();
	if (FindComponent<Joint2D>(go, ComponentType::Joint2D))
	{
		// One to an object: the world reads the first it finds, and a second
		// would be silently ignored. Two joints on one thing are two objects.
		errOut = "this object already has a Joint2D";
		return false;
	}
	const std::string before = SnapshotSubtree(goId);
	// A joint ties a BODY. Without one it would sit there doing nothing and
	// saying nothing, so the body comes with it - unless the object is
	// under something that has one, in which case the joint is a second (or
	// third) on THAT body, which is what a child is for here: an object
	// holds one joint, and a wheel can be hinged and geared.
	bool bodyAbove = false;
	for (GameObject* at = go->GetParent(); at != NULL && !bodyAbove; at = at->GetParent())
		if (FindComponent<Physics2D>(at, ComponentType::Physics2D)) bodyAbove = true;
	if (!FindComponent<Physics2D>(go, ComponentType::Physics2D) && !bodyAbove)
		go->Add(std::static_pointer_cast<IComponent>(std::make_shared<Physics2D>()));
	go->Add(std::static_pointer_cast<IComponent>(std::make_shared<Joint2D>(jointType > Joint2DType::Gear ? 0u : jointType)));
	MarkSceneDirty();
	PushReplaceCommand(goId, before, "Add Joint 2D");
	return true;
}

// --- properties -------------------------------------------------------------------

void SceneEditor::DrawPhysics2DProperties(GameObject* go, uint32 goId, Physics2D* ph)
{
	ImGui::Text("Physics 2D");

	int bt = (int)ph->GetBodyType();
	const char* bodyTypes[] = { "Static", "Kinematic", "Dynamic" };
	{
		const std::string b = SnapshotSubtree(goId);
		if (ImGui::Combo("Body", &bt, bodyTypes, 3))
		{ ph->SetBodyType((uint32)bt); MarkSceneDirty(); PushReplaceCommand(goId, b, "Set Body Type"); }
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Static never moves (the ground). Kinematic is moved by\nscript and pushes others but ignores forces. Dynamic is\nmoved by the solver.");

	int sh = (int)ph->GetShapeType();
	const char* shapes[] = { "Box", "Circle", "Capsule", "Polygon", "Chain" };
	{
		const std::string b = SnapshotSubtree(goId);
		if (ImGui::Combo("Shape", &sh, shapes, 5))
		{
			// A chain needs points too: the top edge of what it replaces,
			// so it is a floor where the box's top was.
			if (sh == (int)Shape2DType::Chain)
			{
				const Vec2 h = ph->GetSize();
				std::vector<Vec2> pts = ph->GetPolygon();
				if (ph->GetShapeType() != Shape2DType::Polygon || pts.size() < 2)
				{
					pts.clear();
					pts.push_back(Vec2(-h.x, h.y)); pts.push_back(Vec2(h.x, h.y));
				}
				ph->SetChain(pts, ph->GetShapeType() == Shape2DType::Polygon);
			}
			else if (sh == (int)Shape2DType::Polygon && ph->GetShapeType() == Shape2DType::Chain && !ph->SetPolygon(ph->GetPolygon()))
			{
				// A chain that is not a usable outline (two points, or one
				// that crosses itself): start the polygon over as a box.
				const Vec2 h = ph->GetSize();
				std::vector<Vec2> pts;
				pts.push_back(Vec2(-h.x, -h.y)); pts.push_back(Vec2(h.x, -h.y));
				pts.push_back(Vec2(h.x, h.y)); pts.push_back(Vec2(-h.x, h.y));
				ph->SetPolygon(pts);
			}
			// A polygon needs points before it is anything: start it as the
			// box it replaces, so switching never leaves a body with no shape.
			if (sh == (int)Shape2DType::Polygon && ph->GetPolygon().size() < 3)
			{
				const Vec2 h = ph->GetSize();
				std::vector<Vec2> pts;
				pts.push_back(Vec2(-h.x, -h.y)); pts.push_back(Vec2(h.x, -h.y));
				pts.push_back(Vec2(h.x, h.y)); pts.push_back(Vec2(-h.x, h.y));
				ph->SetPolygon(pts);
			}
			ph->SetShapeType((uint32)sh);
			MarkSceneDirty();
			PushReplaceCommand(goId, b, "Set Body Shape");
		}
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Capsule: a box with round ends - what a character wants, since it\ndoes not catch on the seam between two floor pieces.\nPolygon: a solid outline of any shape - one that is not convex is cut\ninto convex pieces for the solver.\nChain: a line of edges with no inside, open or closed - hand-drawn\nground and walls. No mass: for a Static or Kinematic body.");

	if (sceneIsTwoD && !(SelectedSceneObject && SelectedSceneObject->GetType() == SceneObjectTypes::PHYSICS2D_COMPONENT))
	{
		bool editing = collider2DEdit && collider2DEditGo == goId;
		if (ImGui::Checkbox("Edit Collider", &editing))
		{
			collider2DEdit = editing;
			collider2DEditGo = goId;
			collider2DDragKind = C2D_None;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Handles on the shape in the Scene View: an edge resizes it from\nthat side, the centre moves it, a polygon's or a chain's points\nmove one at a time. On those, Shift-click an edge adds a point\n(off the end of an open chain, carries the line on) and Alt-click\na point removes it. Hold Ctrl to snap to a quarter unit. The move\ngizmo is put away while this is on.");
	}

	if (ph->GetShapeType() == Shape2DType::Polygon || ph->GetShapeType() == Shape2DType::Chain)
	{
		const bool isChain = ph->GetShapeType() == Shape2DType::Chain;
		std::vector<Vec2> pts = ph->GetPolygon();
		bool loop = ph->IsChainLoop();
		const size_t fewest = isChain ? (loop ? 3 : 2) : 3;
		if (isChain)
		{
			const std::string b = SnapshotSubtree(goId);
			if (ImGui::Checkbox("Closed Loop", &loop))
			{
				if (ph->SetChain(pts, loop)) { MarkSceneDirty(); PushReplaceCommand(goId, b, "Close Chain"); }
				loop = ph->IsChainLoop();
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Open: a line that stops things from both sides.\nClosed: the skin of something solid - it stops what is outside it.");
			ImGui::TextDisabled("%d points (local space)", (int)pts.size());
		}
		else
		{
			std::vector<std::vector<Vec2> > pieces;
			Physics2D::DecomposePolygon(pts, pieces);
			if (pieces.size() > 1) ImGui::TextDisabled("%d points (local space), %d convex pieces", (int)pts.size(), (int)pieces.size());
			else ImGui::TextDisabled("%d points (local space)", (int)pts.size());
		}
		ImGui::TextDisabled("Edit Collider: drag points; Shift-click an edge to\nadd one, Alt-click a point to remove it.");
		bool changed = false, structural = false;
		// A long outline is edited in the Scene View; listing a hundred
		// rows of numbers here would bury the rest of the panel.
		const size_t shown = pts.size() > 16 ? 16 : pts.size();
		for (size_t k = 0; k < shown; k++)
		{
			ImGui::PushID((int)(2000 + k));
			ImGui::SetNextItemWidth(150);
			BeginComponentUndo(goId);
			if (ImGui::DragFloat2("##pt", (float*)&pts[k], 0.01f)) changed = true;
			EndComponentUndo(goId, "Move Outline Point");
			ImGui::SameLine();
			if (pts.size() > fewest && ImGui::SmallButton("x")) { pts.erase(pts.begin() + (std::ptrdiff_t)k); structural = true; ImGui::PopID(); break; }
			ImGui::PopID();
		}
		if (pts.size() > shown) ImGui::TextDisabled("... and %d more", (int)(pts.size() - shown));
		if (pts.size() < (size_t)Physics2D::MaxOutlinePoints && ImGui::SmallButton("Add Point"))
		{
			if (isChain && !loop)
			{
				// One more step the way the line was already going.
				const Vec2 a = pts[pts.size() - 2], b = pts.back();
				pts.push_back(Vec2(b.x + (b.x - a.x), b.y + (b.y - a.y)));
			}
			else
			{
				// Halfway along the closing edge, nudged outward.
				const Vec2 a = pts.back(), b = pts.front();
				Vec2 mid((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
				const Vec2 out(b.y - a.y, -(b.x - a.x));
				const f32 len = sqrtf(out.x * out.x + out.y * out.y);
				if (len > 1e-5f) mid = Vec2(mid.x + out.x / len * 0.1f, mid.y + out.y / len * 0.1f);
				pts.push_back(mid);
			}
			structural = true;
		}
		bool refused = false;
		if (changed || structural)
		{
			const std::string b = structural ? SnapshotSubtree(goId) : std::string();
			if (isChain ? ph->SetChain(pts, loop) : ph->SetPolygon(pts))
			{
				MarkSceneDirty();
				if (structural) PushReplaceCommand(goId, b, isChain ? "Edit Chain" : "Edit Polygon");
			}
			else refused = true;
		}
		// Refused, the last good outline is kept.
		if (refused)
			ImGui::TextColored(ImVec4(1.f, 0.6f, 0.4f, 1.f), isChain ? "Two points in the same place - not applied."
				: "The outline would cross itself - not applied.");
	}
	else
	{
		Vec2 sz = ph->GetSize();
		BeginComponentUndo(goId);
		if (ImGui::DragFloat2("Half Extents", (float*)&sz, 0.01f, 0.001f, 1000.f))
		{
			ph->SetSize(sz);
			MarkSceneDirty();
		}
		EndComponentUndo(goId, "Set Body Size");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Half-extents, because that is what Box2D takes - a 1x1\nbox is 0.5, 0.5. For a circle only X is used, as the radius.\nFor a capsule X is the radius and Y the half-height, caps included.");
	}

	Vec2 off = ph->GetOffset();
	BeginComponentUndo(goId);
	if (ImGui::DragFloat2("Offset", (float*)&off, 0.01f)) { ph->SetOffset(off); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Body Offset");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Where the shape sits relative to the object's origin. A sprite\npivoted at its feet has its origin on the ground; its collider\nbelongs half its height above that.");

	// The usual reason to touch size and offset at all: make the collider
	// the sprite. One click, then adjusted from there.
	if (Sprite2D* sp = SpriteOf(go))
	{
		if (ImGui::Button("Fit to Sprite"))
		{
			const std::string b = SnapshotSubtree(goId);
			const Vec4 r = sp->GetLocalRect();
			const Vec2 half((r.z - r.x) * 0.5f, (r.w - r.y) * 0.5f);
			if (half.x > 1e-4f && half.y > 1e-4f)
			{
				if (ph->GetShapeType() == Shape2DType::Circle) ph->SetSize(Vec2(std::min(half.x, half.y), std::min(half.x, half.y)));
				else if (ph->GetShapeType() == Shape2DType::Capsule) ph->SetSize(Vec2(std::min(half.x, half.y), half.y));
				else if (ph->GetShapeType() == Shape2DType::Polygon || ph->GetShapeType() == Shape2DType::Chain)
				{
					std::vector<Vec2> pts;
					pts.push_back(Vec2(-half.x, -half.y)); pts.push_back(Vec2(half.x, -half.y));
					pts.push_back(Vec2(half.x, half.y)); pts.push_back(Vec2(-half.x, half.y));
					if (ph->GetShapeType() == Shape2DType::Chain) ph->SetChain(pts, true);
					else ph->SetPolygon(pts);
				}
				else ph->SetSize(half);
				ph->SetOffset(Vec2((r.x + r.z) * 0.5f, (r.y + r.w) * 0.5f));
				MarkSceneDirty();
				PushReplaceCommand(goId, b, "Fit Body to Sprite");
			}
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Sizes and places the shape over the sprite's own rectangle,\npivot included.");
	}

	f32 d = ph->GetDensity();
	BeginComponentUndo(goId);
	if (ImGui::DragFloat("Density", &d, 0.05f, 0.f, 100.f)) { ph->SetDensity(d); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Density");
	f32 fr = ph->GetFriction();
	BeginComponentUndo(goId);
	if (ImGui::DragFloat("Friction", &fr, 0.01f, 0.f, 2.f)) { ph->SetFriction(fr); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Friction");
	f32 re = ph->GetRestitution();
	BeginComponentUndo(goId);
	if (ImGui::DragFloat("Bounciness", &re, 0.01f, 0.f, 1.f)) { ph->SetRestitution(re); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Bounciness");

	f32 gs = ph->GetGravityScale();
	BeginComponentUndo(goId);
	if (ImGui::DragFloat("Gravity Scale", &gs, 0.01f, -10.f, 10.f)) { ph->SetGravityScale(gs); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Gravity Scale");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("How much of the scene's gravity it feels: 0 floats,\n2 falls twice as hard.");
	f32 ld = ph->GetLinearDamping();
	BeginComponentUndo(goId);
	if (ImGui::DragFloat("Linear Damping", &ld, 0.01f, 0.f, 50.f)) { ph->SetLinearDamping(ld); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Linear Damping");
	f32 ad = ph->GetAngularDamping();
	BeginComponentUndo(goId);
	if (ImGui::DragFloat("Angular Damping", &ad, 0.01f, 0.f, 50.f)) { ph->SetAngularDamping(ad); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Angular Damping");

	{
		const std::string b = SnapshotSubtree(goId);
		bool fixed = ph->IsFixedRotation();
		if (ImGui::Checkbox("Fixed Rotation", &fixed))
		{ ph->SetFixedRotation(fixed); MarkSceneDirty(); PushReplaceCommand(goId, b, "Toggle Fixed Rotation"); }
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Stops the body tipping over - what almost every\nplatformer character wants.");

		bool sensor = ph->IsSensor();
		if (ImGui::Checkbox("Sensor (trigger)", &sensor))
		{ ph->SetSensor(sensor); MarkSceneDirty(); PushReplaceCommand(goId, b, "Toggle Sensor"); }
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Collides with nothing and says who is inside it:\nonTriggerEnter / onTriggerExit, on the sensor and on whoever\nwalked in. A coin, a checkpoint, a kill zone.");

		bool oneWay = ph->IsOneWay();
		if (ImGui::Checkbox("One-Way Platform", &oneWay))
		{ ph->SetOneWay(oneWay); MarkSceneDirty(); PushReplaceCommand(goId, b, "Toggle One-Way"); }
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Solid from above, open from below and the sides:\njumped up through and stood on.");

		bool bullet = ph->IsBullet();
		if (ImGui::Checkbox("Bullet (continuous)", &bullet))
		{ ph->SetBullet(bullet); MarkSceneDirty(); PushReplaceCommand(goId, b, "Toggle Bullet"); }
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Continuous collision against other MOVING bodies too, for\nsomething small and fast. Costs more; static geometry is\nalways safe without it.");

		bool cast = ph->CastsShadow();
		if (ImGui::Checkbox("Casts 2D Shadow", &cast))
		{ ph->SetCastsShadow(cast); MarkSceneDirty(); PushReplaceCommand(goId, b, "Toggle Casts Shadow"); }
	}

	if (ImGui::TreeNode("Collision Layers"))
	{
		int layer = (int)ph->GetLayer();
		BeginComponentUndo(goId);
		ImGui::SetNextItemWidth(90);
		if (ImGui::DragInt("On Layer", &layer, 0.1f, 0, 31)) { ph->SetLayer((uint32)std::max(0, std::min(31, layer))); MarkSceneDirty(); }
		EndComponentUndo(goId, "Set Collision Layer");
		ImGui::TextDisabled("Collides with layers:");
		// 16 at a time: the first sixteen cover any game that is not
		// reaching, and all 32 are still one node away.
		uint32 mask = ph->GetMask();
		const std::string b = SnapshotSubtree(goId);
		bool changed = false;
		for (int l = 0; l < 32; l++)
		{
			if (l % 8) ImGui::SameLine();
			ImGui::PushID(3000 + l);
			bool on = (mask & (1u << l)) != 0;
			char label[8];
			snprintf(label, sizeof(label), "%d", l);
			if (ImGui::Checkbox(label, &on))
			{
				if (on) mask |= (1u << l); else mask &= ~(1u << l);
				changed = true;
			}
			ImGui::PopID();
		}
		if (ImGui::SmallButton("All")) { mask = 0xFFFFFFFFu; changed = true; }
		ImGui::SameLine();
		if (ImGui::SmallButton("None")) { mask = 0u; changed = true; }
		if (changed) { ph->SetMask(mask); MarkSceneDirty(); PushReplaceCommand(goId, b, "Set Collision Mask"); }
		ImGui::TextDisabled("Two bodies collide when each has the other's\nlayer ticked.");
		ImGui::TreePop();
	}

	if (playMode && ph->HaveBody())
	{
		const Vec2 v = ph->GetLinearVelocity();
		ImGui::TextDisabled("velocity %.2f, %.2f   %s%s", v.x, v.y,
			ph->IsGrounded() ? "grounded" : "in the air", ph->IsAwake() ? "" : "  (asleep)");
	}
}

void SceneEditor::DrawJoint2DProperties(GameObject* go, uint32 goId, Joint2D* jt)
{
	ImGui::Text("Joint 2D");
	if (!FindComponent<Physics2D>(go, ComponentType::Physics2D))
	{
		GameObject* above = NULL;
		for (GameObject* at = go->GetParent(); at != NULL && !above; at = at->GetParent())
			if (FindComponent<Physics2D>(at, ComponentType::Physics2D)) above = at;
		if (above) ImGui::TextDisabled("Ties the body of '%s' (this object has none).\nAnchors are in that body's space.", above->GetName().c_str());
		else ImGui::TextColored(ImVec4(1.f, 0.6f, 0.4f, 1.f), "No Physics 2D on this object or above it:\nthere is nothing for the joint to tie.");
	}

	int type = (int)jt->GetJointType();
	{
		const std::string b = SnapshotSubtree(goId);
		if (ImGui::Combo("Kind", &type, kJointLabels, 9))
		{ jt->SetJointType((uint32)type); MarkSceneDirty(); PushReplaceCommand(goId, b, "Set Joint Kind"); }
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Hinge: turns about a pin - a door, a pendulum.\nDistance: a rod, a rope (with a maximum) or a spring.\nWeld: glued together as they are.\nSlider: moves along one line - a lift, a piston.\nWheel: a slider that also turns - a wheel on suspension.\nTarget: pulled towards a point a script sets - dragging with the mouse.\nIgnore: the two bodies simply do not collide with each other.\nPulley: a rope over two fixed points; what one side gives the other takes.\nGear: two turning bodies turn together, in opposite directions.");

	// What it is tied to: picked from the objects that actually have a body,
	// because a name that matches nothing is indistinguishable, here, from
	// one that does - the joint is simply never made.
	const std::string con = jt->GetConnected();
	if (ImGui::BeginCombo("Connected To", con.empty() ? "(the world)" : con.c_str()))
	{
		const std::string b = SnapshotSubtree(goId);
		if (ImGui::Selectable("(the world)", con.empty()))
		{ jt->SetConnected(std::string()); MarkSceneDirty(); PushReplaceCommand(goId, b, "Set Joint Connection"); }
		const std::map<uint32, SceneObject*>& list = sceneObjects->GetList();
		for (std::map<uint32, SceneObject*>::const_iterator i = list.begin(); i != list.end(); ++i)
		{
			if (!i->second || i->second->GetType() != SceneObjectTypes::GAMEOBJECT) continue;
			GameObject* other = (GameObject*)i->second->GetPTR();
			if (other == go || !FindComponent<Physics2D>(other, ComponentType::Physics2D)) continue;
			const std::string n = i->second->GetName();
			if (ImGui::Selectable(n.c_str(), n == con))
			{ jt->SetConnected(n); MarkSceneDirty(); PushReplaceCommand(goId, b, "Set Joint Connection"); }
		}
		ImGui::EndCombo();
	}
	if (!con.empty() && !FindObjectByName(sceneObjects, con))
		ImGui::TextColored(ImVec4(1.f, 0.6f, 0.4f, 1.f), "No object is named '%s' - the joint will not be made.", con.c_str());

	Vec2 an = jt->GetAnchor();
	BeginComponentUndo(goId);
	if (ImGui::DragFloat2("Anchor", (float*)&an, 0.01f)) { jt->SetAnchor(an); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Joint Anchor");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Where on THIS object the joint acts, in its local space:\nthe hinge pin, this end of the rope.");

	bool ownOther = jt->HasConnectedAnchor();
	{
		const std::string b = SnapshotSubtree(goId);
		if (ImGui::Checkbox("Own anchor on the other", &ownOther))
		{
			if (ownOther) jt->SetConnectedAnchor(jt->GetConnectedAnchor()); else jt->ClearConnectedAnchor();
			MarkSceneDirty();
			PushReplaceCommand(goId, b, "Set Joint Anchor");
		}
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Off, the other end is wherever this object's anchor is when\nPlay starts - right for a hinge or a weld. On, it is a point\nof its own: the far end of a rope. In the other body's local\nspace, or world space when tied to the world.");
	if (jt->HasConnectedAnchor())
	{
		Vec2 ca = jt->GetConnectedAnchor();
		BeginComponentUndo(goId);
		if (ImGui::DragFloat2("Other Anchor", (float*)&ca, 0.01f)) { jt->SetConnectedAnchor(ca); MarkSceneDirty(); }
		EndComponentUndo(goId, "Set Joint Anchor");
	}

	const uint32 t = jt->GetJointType();
	if (t == Joint2DType::Target)
	{
		Vec2 tg = jt->GetTarget();
		BeginComponentUndo(goId);
		if (ImGui::DragFloat2("Target", (float*)&tg, 0.01f)) { jt->SetTarget(tg); MarkSceneDirty(); }
		EndComponentUndo(goId, "Set Joint Target");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("The world point it is pulled to when tied to the world - usually\nset by a script every frame (joint:setTarget). Until one is set it\nholds the body where it starts. Spring is how eagerly it follows;\nMax Force is the most it may pull with.");
		f32 force = jt->GetMaxMotorForce();
		BeginComponentUndo(goId);
		if (ImGui::DragFloat("Max Force", &force, 1.f, 0.f, 1000000.f)) { jt->SetMotor(false, 0.f, std::max(0.f, force)); MarkSceneDirty(); }
		EndComponentUndo(goId, "Set Joint Force");
	}
	if (t == Joint2DType::Pulley)
	{
		Vec2 ga = jt->GetGroundAnchor(), gb = jt->GetConnectedGroundAnchor();
		BeginComponentUndo(goId);
		if (ImGui::DragFloat2("Wheel (this)", (float*)&ga, 0.01f)) { jt->SetGroundAnchors(ga, gb); MarkSceneDirty(); }
		EndComponentUndo(goId, "Set Pulley Wheel");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("The fixed point in the WORLD this object hangs from.");
		BeginComponentUndo(goId);
		if (ImGui::DragFloat2("Wheel (other)", (float*)&gb, 0.01f)) { jt->SetGroundAnchors(ga, gb); MarkSceneDirty(); }
		EndComponentUndo(goId, "Set Pulley Wheel");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("The fixed point in the WORLD the other object hangs from.");
		if (jt->GetConnected().empty())
			ImGui::TextColored(ImVec4(1.f, 0.6f, 0.4f, 1.f), "A pulley needs another body: pick Connected To.");
	}
	if (t == Joint2DType::Pulley || t == Joint2DType::Gear)
	{
		f32 ratio = jt->GetRatio();
		BeginComponentUndo(goId);
		if (ImGui::DragFloat("Ratio", &ratio, 0.01f, -100.f, 100.f, "%.2f") && fabsf(ratio) > 1e-4f) { jt->SetRatio(ratio); MarkSceneDirty(); }
		EndComponentUndo(goId, "Set Joint Ratio");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(t == Joint2DType::Pulley
				? "How much rope the other side counts for: at 2, this side moves\ntwice as far as the other and lifts twice the weight."
				: "How far this turns for each turn of the other: at 2 the other\nturns half as fast. They turn opposite ways; negative, the same way.");
		if (t == Joint2DType::Gear)
		{
			ImGui::TextDisabled("Each of the two usually has a Hinge to the world too.");
			if (jt->GetConnected().empty())
				ImGui::TextColored(ImVec4(1.f, 0.6f, 0.4f, 1.f), "A gear needs another body: pick Connected To.");
		}
	}
	// The rest is the solver's own joints' settings.
	const bool plain = t <= Joint2DType::Wheel;
	if (t == Joint2DType::Slider || t == Joint2DType::Wheel)
	{
		Vec2 ax = jt->GetAxis();
		BeginComponentUndo(goId);
		if (ImGui::DragFloat2("Axis", (float*)&ax, 0.01f, -1.f, 1.f)) { jt->SetAxis(ax); MarkSceneDirty(); }
		EndComponentUndo(goId, "Set Joint Axis");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("The line it moves along, in the world: (1, 0) slides\nsideways, (0, 1) up and down.");
	}
	if (t == Joint2DType::Distance)
	{
		f32 len = jt->GetLength();
		BeginComponentUndo(goId);
		if (ImGui::DragFloat("Length", &len, 0.01f, 0.f, 10000.f, len > 0.f ? "%.2f" : "as placed"))
		{ jt->SetLength(std::max(0.f, len)); MarkSceneDirty(); }
		EndComponentUndo(goId, "Set Joint Length");
	}

	if (plain && t != Joint2DType::Weld)
	{
		// A hinge's limits are angles, shown in degrees because that is what
		// anyone thinks a door's swing in.
		const bool angular = (t == Joint2DType::Hinge);
		const f32 k = angular ? kRadToDeg : 1.f;
		bool en = jt->IsLimitEnabled();
		f32 lim[2] = { jt->GetLowerLimit() * k, jt->GetUpperLimit() * k };
		const std::string b = SnapshotSubtree(goId);
		if (ImGui::Checkbox(t == Joint2DType::Distance ? "Limit length (rope)" : "Limits", &en))
		{ jt->SetLimits(en, jt->GetLowerLimit(), jt->GetUpperLimit()); MarkSceneDirty(); PushReplaceCommand(goId, b, "Toggle Joint Limits"); }
		if (jt->IsLimitEnabled())
		{
			BeginComponentUndo(goId);
			if (ImGui::DragFloat2(angular ? "Min / Max (deg)" : "Min / Max", lim, angular ? 0.5f : 0.01f))
			{ jt->SetLimits(true, lim[0] / k, lim[1] / k); MarkSceneDirty(); }
			EndComponentUndo(goId, "Set Joint Limits");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(angular ? "Measured from how the two bodies stand when Play starts."
					: (t == Joint2DType::Distance ? "The shortest and longest the joint may be. A rope is 0 and its length."
						: "Distance along the axis, from where it is when Play starts."));
		}
	}

	if (plain && t != Joint2DType::Weld)
	{
		bool en = jt->IsMotorEnabled();
		const std::string b = SnapshotSubtree(goId);
		if (ImGui::Checkbox("Motor", &en))
		{ jt->SetMotor(en, jt->GetMotorSpeed(), jt->GetMaxMotorForce()); MarkSceneDirty(); PushReplaceCommand(goId, b, "Toggle Joint Motor"); }
		if (jt->IsMotorEnabled())
		{
			const bool angular = (t == Joint2DType::Hinge || t == Joint2DType::Wheel);
			const f32 k = angular ? kRadToDeg : 1.f;
			f32 speed = jt->GetMotorSpeed() * k, force = jt->GetMaxMotorForce();
			BeginComponentUndo(goId);
			if (ImGui::DragFloat(angular ? "Speed (deg/s)" : "Speed", &speed, angular ? 1.f : 0.05f))
			{ jt->SetMotor(true, speed / k, force); MarkSceneDirty(); }
			EndComponentUndo(goId, "Set Motor Speed");
			BeginComponentUndo(goId);
			if (ImGui::DragFloat(angular ? "Max Torque" : "Max Force", &force, 1.f, 0.f, 1000000.f))
			{ jt->SetMotor(true, jt->GetMotorSpeed(), std::max(0.f, force)); MarkSceneDirty(); }
			EndComponentUndo(goId, "Set Motor Force");
		}
	}

	if (plain || t == Joint2DType::Target)
	{
		bool en = jt->IsSpringEnabled();
		const std::string b = SnapshotSubtree(goId);
		if (ImGui::Checkbox("Spring", &en))
		{ jt->SetSpring(en, jt->GetSpringHertz(), jt->GetSpringDamping()); MarkSceneDirty(); PushReplaceCommand(goId, b, "Toggle Joint Spring"); }
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Pulls the joint towards rest rather than holding it rigid.");
		if (jt->IsSpringEnabled())
		{
			f32 hz = jt->GetSpringHertz(), dr = jt->GetSpringDamping();
			BeginComponentUndo(goId);
			if (ImGui::DragFloat("Stiffness (Hz)", &hz, 0.05f, 0.f, 60.f)) { jt->SetSpring(true, std::max(0.f, hz), dr); MarkSceneDirty(); }
			EndComponentUndo(goId, "Set Spring Stiffness");
			BeginComponentUndo(goId);
			if (ImGui::DragFloat("Damping", &dr, 0.01f, 0.f, 10.f)) { jt->SetSpring(true, hz, std::max(0.f, dr)); MarkSceneDirty(); }
			EndComponentUndo(goId, "Set Spring Damping");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("1 settles without bouncing; less oscillates, more is sluggish.");
		}
	}

	{
		bool cc = jt->GetCollideConnected();
		const std::string b = SnapshotSubtree(goId);
		if (ImGui::Checkbox("Joined bodies collide", &cc))
		{ jt->SetCollideConnected(cc); MarkSceneDirty(); PushReplaceCommand(goId, b, "Toggle Collide Connected"); }
	}

	if (!FindComponent<Physics2D>(go, ComponentType::Physics2D))
		ImGui::TextColored(ImVec4(1.f, 0.6f, 0.4f, 1.f), "This object has no Physics 2D - a joint ties a body.");
	if (playMode) ImGui::TextDisabled(jt->HaveJoint() ? "built" : "NOT built - see the warnings above");
}

void SceneEditor::DrawLayer2DProperties(uint32 goId, Layer2D* l)
{
	ImGui::Text("Layer 2D");

	Vec2 par = l->GetParallax();
	BeginComponentUndo(goId);
	if (ImGui::DragFloat2("Parallax", (float*)&par, 0.01f, 0.f, 4.f))
	{
		l->SetParallax(par);
		MarkSceneDirty();
	}
	EndComponentUndo(goId, "Set Parallax");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("How fast the layer crosses the screen as the view moves:\n1 = with the level, 0.5 = half speed (further away),\n0 = pinned to the screen, above 1 = foreground.\nThe two axes are independent, so a sky can scroll sideways\nand stay put vertically.");

	{
		const std::string b = SnapshotSubtree(goId);
		bool automatic = l->IsAutomatic();
		if (ImGui::Checkbox("Automatic", &automatic))
		{
			l->SetAutomatic(automatic);
			// The first automatic layer turns the scene's switch on with it:
			// ticking this and seeing nothing scroll would be a second place
			// to look for no reason.
			if (automatic && !view2D.autoParallax) view2D.autoParallax = true;
			MarkSceneDirty();
			PushReplaceCommand(goId, b, "Toggle Automatic Parallax");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("The engine scrolls this layer as the view moves - no script.\nLeave off for a layer a script of yours already moves.");

		bool vis = l->IsVisible();
		if (ImGui::Checkbox("Visible", &vis))
		{
			l->SetVisible(vis);
			MarkSceneDirty();
			PushReplaceCommand(goId, b, "Toggle Layer Visible");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Hides every mesh under this layer without unparenting\nor deleting anything.");
	}

	ImGui::TextDisabled("Draw order is this object's Z.");
}

// --- agent ---------------------------------------------------------------------------

bool SceneEditor::AgentApplyPhysics2DExtras(Physics2D* ph, const json& p, std::string& errOut)
{
	try {
		if (p.contains("offset"))
		{
			if (!p["offset"].is_array() || p["offset"].size() < 2) { errOut = "offset must be [x, y]"; return false; }
			ph->SetOffset(Vec2(p["offset"][0].get<f32>(), p["offset"][1].get<f32>()));
		}
		if (p.contains("polygon"))
		{
			std::vector<Vec2> pts;
			const json &a = p["polygon"];
			// [[x, y], ...] or a flat [x, y, x, y, ...].
			if (a.is_array() && !a.empty() && a[0].is_array())
				for (size_t i = 0; i < a.size(); i++) pts.push_back(Vec2(a[i][0].get<f32>(), a[i][1].get<f32>()));
			else if (a.is_array())
				for (size_t i = 0; i + 1 < a.size(); i += 2) pts.push_back(Vec2(a[i].get<f32>(), a[i + 1].get<f32>()));
			// The same list is a chain's points when the shape is (or is
			// being made) a chain.
			std::string want = p.contains("shape") && p["shape"].is_string() ? p["shape"].get<std::string>() : std::string();
			for (size_t i = 0; i < want.size(); i++) want[i] = (char)tolower((unsigned char)want[i]);
			const bool chain = want == "chain" || (want.empty() && ph->GetShapeType() == Shape2DType::Chain);
			if (chain)
			{
				if (!ph->SetChain(pts, p.value("loop", ph->IsChainLoop())))
				{ errOut = "a chain needs 2 or more points (3 for a loop), no two in a row the same"; return false; }
				ph->SetShapeType(Shape2DType::Chain);
			}
			else
			{
				if (!ph->SetPolygon(pts)) { errOut = "polygon must be 3 or more points making an outline that does not cross itself"; return false; }
				ph->SetShapeType(Shape2DType::Polygon);
			}
		}
		else if (p.contains("loop") && ph->GetShapeType() == Shape2DType::Chain)
		{
			if (!ph->SetChain(ph->GetPolygon(), p["loop"].get<bool>())) { errOut = "a loop needs 3 or more points"; return false; }
		}
		if (p.contains("sensor")) ph->SetSensor(p["sensor"].get<bool>());
		if (p.contains("oneWay")) ph->SetOneWay(p["oneWay"].get<bool>());
		if (p.contains("layer"))
		{
			const int l = p["layer"].get<int>();
			if (l < 0 || l > 31) { errOut = "layer must be 0 to 31"; return false; }
			ph->SetLayer((uint32)l);
		}
		if (p.contains("mask")) ph->SetMask(p["mask"].get<uint32>());
		// The mask a layer at a time, which is how anyone thinks of it.
		if (p.contains("collidesWith") && p["collidesWith"].is_array())
		{
			uint32 m = 0;
			for (size_t i = 0; i < p["collidesWith"].size(); i++)
			{
				const int l = p["collidesWith"][i].get<int>();
				if (l < 0 || l > 31) { errOut = "collidesWith holds layers 0 to 31"; return false; }
				m |= (1u << l);
			}
			ph->SetMask(m);
		}
		if (p.contains("ignores") && p["ignores"].is_array())
			for (size_t i = 0; i < p["ignores"].size(); i++)
			{
				const int l = p["ignores"][i].get<int>();
				if (l < 0 || l > 31) { errOut = "ignores holds layers 0 to 31"; return false; }
				ph->SetCollidesWith((uint32)l, false);
			}
		if (p.contains("gravityScale")) ph->SetGravityScale(p["gravityScale"].get<f32>());
		if (p.contains("linearDamping")) ph->SetLinearDamping(p["linearDamping"].get<f32>());
		if (p.contains("angularDamping")) ph->SetAngularDamping(p["angularDamping"].get<f32>());
		if (p.contains("bullet")) ph->SetBullet(p["bullet"].get<bool>());
		if (p.contains("castsShadow")) ph->SetCastsShadow(p["castsShadow"].get<bool>());
		if (p.value("fitToSprite", false))
		{
			Sprite2D* sp = SpriteOf(ph->GetOwner());
			if (!sp) { errOut = "fitToSprite: this object has no Sprite2D"; return false; }
			const Vec4 r = sp->GetLocalRect();
			const Vec2 half((r.z - r.x) * 0.5f, (r.w - r.y) * 0.5f);
			if (half.x <= 1e-4f || half.y <= 1e-4f) { errOut = "fitToSprite: the sprite has no size (no image yet?)"; return false; }
			if (ph->GetShapeType() == Shape2DType::Circle) ph->SetSize(Vec2(std::min(half.x, half.y), std::min(half.x, half.y)));
			else if (ph->GetShapeType() == Shape2DType::Capsule) ph->SetSize(Vec2(std::min(half.x, half.y), half.y));
			else ph->SetSize(half);
			ph->SetOffset(Vec2((r.x + r.z) * 0.5f, (r.y + r.w) * 0.5f));
		}
	}
	catch (const std::exception &e) {
		errOut = std::string("a value has the wrong type (") + e.what() + ")";
		return false;
	}
	return true;
}

bool SceneEditor::AgentApplyJoint2D(Joint2D* jt, const json& p, std::string& errOut)
{
	static const char* known[] = { "object", "name", "joint", "connected", "anchor", "connectedAnchor", "axis",
		"collideConnected", "limit", "lower", "upper", "lowerDegrees", "upperDegrees", "motor", "motorSpeed",
		"motorSpeedDegrees", "maxMotorForce", "spring", "hertz", "damping", "length",
		"target", "groundAnchor", "connectedGroundAnchor", "ratio", NULL };
	for (json::const_iterator it = p.begin(); it != p.end(); ++it)
	{
		bool ok = false;
		for (int k = 0; known[k] && !ok; k++) ok = (it.key() == known[k]);
		if (!ok) { errOut = "unknown key '" + it.key() + "'"; return false; }
	}
	try {
		if (p.contains("joint"))
		{
			const std::string k = Lower(p["joint"].get<std::string>());
			int found = -1;
			for (int i = 0; i < 9; i++) if (k == kJointNames[i]) found = i;
			if (found < 0) { errOut = "joint must be hinge, distance, weld, slider, wheel, target, ignore, pulley or gear"; return false; }
			jt->SetJointType((uint32)found);
		}
		if (p.contains("connected"))
		{
			const std::string c = p["connected"].get<std::string>();
			if (!c.empty())
			{
				SceneObject* other = FindObjectByName(sceneObjects, c);
				if (!other) { errOut = "connected: no object named '" + c + "'"; return false; }
				if (!FindComponent<Physics2D>((GameObject*)other->GetPTR(), ComponentType::Physics2D))
				{ errOut = "connected: '" + c + "' has no Physics2D to tie to"; return false; }
			}
			jt->SetConnected(c);
		}
		auto v2 = [&p, &errOut](const char* key, Vec2 &out) {
			if (!p[key].is_array() || p[key].size() < 2) { errOut = std::string(key) + " must be [x, y]"; return false; }
			out = Vec2(p[key][0].get<f32>(), p[key][1].get<f32>());
			return true;
		};
		Vec2 v;
		if (p.contains("anchor")) { if (!v2("anchor", v)) return false; jt->SetAnchor(v); }
		if (p.contains("connectedAnchor"))
		{
			if (p["connectedAnchor"].is_null()) jt->ClearConnectedAnchor();
			else { if (!v2("connectedAnchor", v)) return false; jt->SetConnectedAnchor(v); }
		}
		if (p.contains("axis")) { if (!v2("axis", v)) return false; jt->SetAxis(v); }
		if (p.contains("target")) { if (!v2("target", v)) return false; jt->SetTarget(v); }
		if (p.contains("groundAnchor") || p.contains("connectedGroundAnchor"))
		{
			Vec2 ga = jt->GetGroundAnchor(), gb = jt->GetConnectedGroundAnchor();
			if (p.contains("groundAnchor") && !v2("groundAnchor", ga)) return false;
			if (p.contains("connectedGroundAnchor") && !v2("connectedGroundAnchor", gb)) return false;
			jt->SetGroundAnchors(ga, gb);
		}
		if (p.contains("ratio"))
		{
			const f32 ratio = p["ratio"].get<f32>();
			if (fabsf(ratio) < 1e-4f) { errOut = "ratio cannot be 0"; return false; }
			jt->SetRatio(ratio);
		}
		if (p.contains("collideConnected")) jt->SetCollideConnected(p["collideConnected"].get<bool>());

		// Limits in the joint's own units, or - for a hinge - in degrees.
		bool limit = jt->IsLimitEnabled();
		f32 lower = jt->GetLowerLimit(), upper = jt->GetUpperLimit();
		const bool anyLimit = p.contains("limit") || p.contains("lower") || p.contains("upper")
			|| p.contains("lowerDegrees") || p.contains("upperDegrees");
		if (p.contains("limit")) limit = p["limit"].get<bool>();
		if (p.contains("lower")) lower = p["lower"].get<f32>();
		if (p.contains("upper")) upper = p["upper"].get<f32>();
		if (p.contains("lowerDegrees")) lower = p["lowerDegrees"].get<f32>() / kRadToDeg;
		if (p.contains("upperDegrees")) upper = p["upperDegrees"].get<f32>() / kRadToDeg;
		// Giving a range without saying "limit" means it: nobody types a
		// lower and an upper for limits they want off.
		if (!p.contains("limit") && anyLimit) limit = true;
		if (anyLimit) jt->SetLimits(limit, lower, upper);

		bool motor = jt->IsMotorEnabled();
		f32 speed = jt->GetMotorSpeed(), force = jt->GetMaxMotorForce();
		const bool anyMotor = p.contains("motor") || p.contains("motorSpeed") || p.contains("motorSpeedDegrees") || p.contains("maxMotorForce");
		if (p.contains("motor")) motor = p["motor"].get<bool>();
		if (p.contains("motorSpeed")) speed = p["motorSpeed"].get<f32>();
		if (p.contains("motorSpeedDegrees")) speed = p["motorSpeedDegrees"].get<f32>() / kRadToDeg;
		if (p.contains("maxMotorForce")) force = p["maxMotorForce"].get<f32>();
		if (!p.contains("motor") && anyMotor) motor = true;
		if (anyMotor) jt->SetMotor(motor, speed, force);

		bool spring = jt->IsSpringEnabled();
		f32 hz = jt->GetSpringHertz(), dr = jt->GetSpringDamping();
		const bool anySpring = p.contains("spring") || p.contains("hertz") || p.contains("damping");
		if (p.contains("spring")) spring = p["spring"].get<bool>();
		if (p.contains("hertz")) hz = p["hertz"].get<f32>();
		if (p.contains("damping")) dr = p["damping"].get<f32>();
		if (!p.contains("spring") && anySpring) spring = true;
		if (anySpring) jt->SetSpring(spring, hz, dr);

		if (p.contains("length")) jt->SetLength(std::max(0.f, p["length"].get<f32>()));
	}
	catch (const std::exception &e) {
		errOut = std::string("a value has the wrong type (") + e.what() + ")";
		return false;
	}
	return true;
}

bool SceneEditor::AgentAddJoint2D(const std::string& name, const json& p, std::string& errOut)
{
	if (playMode) { errOut = "editor is in play mode"; return false; }
	SceneObject* obj = FindObjectByName(sceneObjects, name);
	if (!obj) { errOut = "object '" + name + "' not found"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();
	const uint32 goId = obj->GetID();
	if (FindComponent<Joint2D>(go, ComponentType::Joint2D)) { errOut = "'" + name + "' already has a Joint2D - set_joint2d changes it"; return false; }

	const std::string before = SnapshotSubtree(goId);
	std::shared_ptr<Joint2D> jt = std::make_shared<Joint2D>();
	if (p.is_object() && !AgentApplyJoint2D(jt.get(), p, errOut)) { errOut = "add_joint2d: " + errOut; return false; }
	// (the same rule as OpAddJoint2D: under something that has a body, the
	// joint is a second one on THAT body, and the object gets none of its own)
	bool bodyAbove = false;
	for (GameObject* at = go->GetParent(); at != NULL && !bodyAbove; at = at->GetParent())
		if (FindComponent<Physics2D>(at, ComponentType::Physics2D)) bodyAbove = true;
	if (!FindComponent<Physics2D>(go, ComponentType::Physics2D) && !bodyAbove)
		go->Add(std::static_pointer_cast<IComponent>(std::make_shared<Physics2D>()));
	go->Add(std::static_pointer_cast<IComponent>(jt));
	MarkSceneDirty();
	PushReplaceCommand(goId, before, "Add Joint 2D");
	return true;
}

void SceneEditor::SetGravity2D(const Vec2& g)
{
	gravity2D = g;
	if (physics2D) physics2D->SetGravity(g);
	MarkSceneDirty();
}

bool SceneEditor::AgentAddPhysics2DWith(const std::string& name, const json& p, std::string& errOut)
{
	if (playMode) { errOut = "editor is in play mode"; return false; }
	SceneObject* obj = FindObjectByName(sceneObjects, name);
	if (!obj) { errOut = "object '" + name + "' not found"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();
	const uint32 goId = obj->GetID();
	if (FindComponent<Physics2D>(go, ComponentType::Physics2D)) { errOut = "this object already has a Physics2D - set_physics2d changes it"; return false; }

	const std::string bts = Lower(p.value("bodyType", std::string("dynamic")));
	const uint32 bt = bts == "static" ? Body2DType::Static : (bts == "kinematic" ? Body2DType::Kinematic : Body2DType::Dynamic);
	uint32 shape = Shape2DType::Box;
	if (p.contains("shape"))
	{
		const std::string sh = Lower(p.value("shape", std::string()));
		int found = -1;
		for (int i = 0; i < 5; i++) if (sh == kShapeNames[i]) found = i;
		if (found < 0) { errOut = "shape must be box, circle, capsule, polygon or chain"; return false; }
		shape = (uint32)found;
	}
	Vec2 size(0.5f, 0.5f);
	// "size" is what this command was first given; "halfExtents" is what
	// set_physics2d calls the same thing.
	const char* sizeKey = p.contains("halfExtents") ? "halfExtents" : (p.contains("size") ? "size" : NULL);
	if (sizeKey)
	{
		if (!p[sizeKey].is_array() || p[sizeKey].size() < 1) { errOut = std::string(sizeKey) + " must be [halfWidth, halfHeight]"; return false; }
		size.x = p[sizeKey][0].get<f32>();
		size.y = p[sizeKey].size() > 1 ? p[sizeKey][1].get<f32>() : size.x;
	}

	const std::string before = SnapshotSubtree(goId);
	std::shared_ptr<Physics2D> ph = std::make_shared<Physics2D>(bt, shape, size);
	// Default true for a DYNAMIC body: a character or a crate that spins
	// free tips over on the first step it walks into, and "my sprite is
	// lying on its side" reads as a rendering bug. Anything that really
	// wants to tumble passes fixedRotation:false explicitly.
	ph->SetFixedRotation(p.contains("fixedRotation") ? p["fixedRotation"].get<bool>() : (bt == Body2DType::Dynamic));
	if (p.contains("density")) ph->SetDensity(p["density"].get<f32>());
	if (p.contains("friction")) ph->SetFriction(p["friction"].get<f32>());
	if (p.contains("bounciness")) ph->SetRestitution(p["bounciness"].get<f32>());
	// Owned before the extras run: fitToSprite looks at the owner's sprite.
	go->Add(std::static_pointer_cast<IComponent>(ph));
	if (!AgentApplyPhysics2DExtras(ph.get(), p, errOut))
	{
		go->Remove(std::static_pointer_cast<IComponent>(ph));
		errOut = "add_physics2d: " + errOut;
		return false;
	}
	MarkSceneDirty();
	PushReplaceCommand(goId, before, "Add Physics 2D");
	return true;
}

json SceneEditor::AgentPhysics2DInfo(const std::string& name, std::string& errOut)
{
	json r;
	SceneObject* obj = FindObjectByName(sceneObjects, name);
	if (!obj) { errOut = "object '" + name + "' not found"; return r; }
	GameObject* go = (GameObject*)obj->GetPTR();
	Physics2D* ph = FindComponent<Physics2D>(go, ComponentType::Physics2D);
	Joint2D* jt = FindComponent<Joint2D>(go, ComponentType::Joint2D);
	if (!ph && !jt) { errOut = "'" + name + "' has no Physics2D"; return r; }
	if (ph)
	{
		static const char* const bodies[] = { "static", "kinematic", "dynamic" };
		json b;
		b["bodyType"] = bodies[ph->GetBodyType() <= 2 ? ph->GetBodyType() : 2];
		b["shape"] = kShapeNames[ph->GetShapeType() <= 4 ? ph->GetShapeType() : 0];
		b["halfExtents"] = { ph->GetSize().x, ph->GetSize().y };
		b["offset"] = { ph->GetOffset().x, ph->GetOffset().y };
		if (ph->GetShapeType() == Shape2DType::Polygon || ph->GetShapeType() == Shape2DType::Chain)
		{
			json pts = json::array();
			for (size_t i = 0; i < ph->GetPolygon().size(); i++) pts.push_back({ ph->GetPolygon()[i].x, ph->GetPolygon()[i].y });
			b["polygon"] = pts;
			if (ph->GetShapeType() == Shape2DType::Chain) b["loop"] = ph->IsChainLoop();
			else
			{
				// How many pieces the solver was given: 1 for an outline it
				// takes whole.
				std::vector<std::vector<Vec2> > pieces;
				Physics2D::DecomposePolygon(ph->GetPolygon(), pieces);
				b["convexPieces"] = pieces.size();
			}
		}
		b["density"] = ph->GetDensity();
		b["friction"] = ph->GetFriction();
		b["bounciness"] = ph->GetRestitution();
		b["fixedRotation"] = ph->IsFixedRotation();
		b["sensor"] = ph->IsSensor();
		b["oneWay"] = ph->IsOneWay();
		b["layer"] = ph->GetLayer();
		b["mask"] = ph->GetMask();
		b["gravityScale"] = ph->GetGravityScale();
		b["linearDamping"] = ph->GetLinearDamping();
		b["angularDamping"] = ph->GetAngularDamping();
		b["bullet"] = ph->IsBullet();
		b["castsShadow"] = ph->CastsShadow();
		b["built"] = ph->HaveBody();
		if (ph->HaveBody())
		{
			// What the solver has, which in Play is where it has got to.
			b["position"] = { ph->GetPosition().x, ph->GetPosition().y };
			b["angle"] = ph->GetAngle();
			b["velocity"] = { ph->GetLinearVelocity().x, ph->GetLinearVelocity().y };
			b["angularVelocity"] = ph->GetAngularVelocity();
			b["mass"] = ph->GetMass();
			b["awake"] = ph->IsAwake();
			b["grounded"] = ph->IsGrounded();
			b["contacts"] = ph->GetContactCount();
		}
		r["body"] = b;
	}
	if (jt)
	{
		json j;
		j["joint"] = kJointNames[jt->GetJointType() <= 8 ? jt->GetJointType() : 0];
		if (jt->GetJointType() == Joint2DType::Target) j["target"] = { jt->GetTarget().x, jt->GetTarget().y };
		if (jt->GetJointType() == Joint2DType::Pulley)
		{
			j["groundAnchor"] = { jt->GetGroundAnchor().x, jt->GetGroundAnchor().y };
			j["connectedGroundAnchor"] = { jt->GetConnectedGroundAnchor().x, jt->GetConnectedGroundAnchor().y };
		}
		if (jt->IsSoft()) j["ratio"] = jt->GetRatio();
		j["connected"] = jt->GetConnected();
		j["anchor"] = { jt->GetAnchor().x, jt->GetAnchor().y };
		if (jt->HasConnectedAnchor()) j["connectedAnchor"] = { jt->GetConnectedAnchor().x, jt->GetConnectedAnchor().y };
		j["axis"] = { jt->GetAxis().x, jt->GetAxis().y };
		j["collideConnected"] = jt->GetCollideConnected();
		j["limit"] = jt->IsLimitEnabled();
		j["lower"] = jt->GetLowerLimit();
		j["upper"] = jt->GetUpperLimit();
		j["motor"] = jt->IsMotorEnabled();
		j["motorSpeed"] = jt->GetMotorSpeed();
		j["maxMotorForce"] = jt->GetMaxMotorForce();
		j["spring"] = jt->IsSpringEnabled();
		j["hertz"] = jt->GetSpringHertz();
		j["damping"] = jt->GetSpringDamping();
		j["length"] = jt->GetLength();
		j["built"] = jt->IsBuilt();
		r["joint"] = j;
	}
	if (physics2D)
	{
		r["world"] = { { "gravity", { gravity2D.x, gravity2D.y } }, { "bodies", physics2D->BodyCount() },
			{ "joints", physics2D->JointCount() } };
	}
	return r;
}


// --- Editing a 2D collider in the Scene View --------------------------------

namespace {
	Vec2 Rotate2D(const Vec2& v, const f32 a)
	{
		const f32 c = cosf(a), s = sinf(a);
		return Vec2(v.x * c - v.y * s, v.x * s + v.y * c);
	}
	const f32 kMinCollider2D = 0.02f;
}

Physics2D* SceneEditor::EditingCollider2D(uint32* goIdOut)
{
	// The body's own row selected in the Scene Tree: that is the collider
	// being edited, with nothing to tick - the way a tile map's row is its
	// brushes.
	if (SelectedSceneObject != NULL && SelectedSceneObject->GetType() == SceneObjectTypes::PHYSICS2D_COMPONENT
		&& sceneIsTwoD && !playMode)
	{
		Physics2D* ph = (Physics2D*)SelectedSceneObject->GetPTR();
		if (ph && ph->GetOwner())
		{
			if (goIdOut) *goIdOut = SelectedSceneObject->GetParentID();
			return ph;
		}
	}
	if (!collider2DEdit) return NULL;
	if (playMode || !sceneIsTwoD || SelectedSceneObject == NULL
		|| SelectedSceneObject->GetType() != SceneObjectTypes::GAMEOBJECT
		|| SelectedSceneObject->GetID() != collider2DEditGo)
	{
		collider2DEdit = false;
		collider2DDragKind = C2D_None;
		collider2DHoverKind = C2D_None;
		return NULL;
	}
	GameObject* go = (GameObject*)SelectedSceneObject->GetPTR();
	const std::vector<std::shared_ptr<IComponent> >& cs = go->GetComponents();
	for (size_t i = 0; i < cs.size(); i++)
		if (cs[i] && cs[i]->GetComponentType() == ComponentType::Physics2D)
		{
			if (goIdOut) *goIdOut = collider2DEditGo;
			return static_cast<Physics2D*>(cs[i].get());
		}
	// The body was removed while its collider was being edited.
	collider2DEdit = false;
	return NULL;
}

void SceneEditor::Collider2DGrips(Physics2D* ph, std::vector<Collider2DGrip>& out)
{
	out.clear();
	const Vec2 o = ph->GetOffset();
	const Vec2 h = ph->GetSize();
	Collider2DGrip g;
	g.index = 0;
	g.local = o; g.kind = C2D_Center; out.push_back(g);
	switch (ph->GetShapeType())
	{
	case Shape2DType::Circle:
		g.local = Vec2(o.x + h.x, o.y); g.kind = C2D_Radius; out.push_back(g);
		break;
	case Shape2DType::Polygon:
	case Shape2DType::Chain:
	{
		const std::vector<Vec2>& pts = ph->GetPolygon();
		for (size_t i = 0; i < pts.size(); i++)
		{
			g.local = Vec2(o.x + pts[i].x, o.y + pts[i].y); g.kind = C2D_Point; g.index = (int)i;
			out.push_back(g);
		}
		break;
	}
	default:     // Box, Capsule: the same four sides
		g.local = Vec2(o.x + h.x, o.y); g.kind = C2D_Right;  out.push_back(g);
		g.local = Vec2(o.x - h.x, o.y); g.kind = C2D_Left;   out.push_back(g);
		g.local = Vec2(o.x, o.y + h.y); g.kind = C2D_Top;    out.push_back(g);
		g.local = Vec2(o.x, o.y - h.y); g.kind = C2D_Bottom; out.push_back(g);
		break;
	}
}

bool SceneEditor::ApplyCollider2DHandle(Physics2D* ph, const int kind, const int index, const Vec2& target)
{
	Vec2 o = ph->GetOffset();
	Vec2 h = ph->GetSize();
	const Vec2 o0 = o, h0 = h;
	switch (kind)
	{
	case C2D_Center:
		o = target;
		break;
	case C2D_Radius:
	{
		const f32 dx = target.x - o.x, dy = target.y - o.y;
		h.x = std::max(kMinCollider2D, sqrtf(dx * dx + dy * dy));
		break;
	}
	// A side moves and the one across from it stays where it is: that is
	// what dragging a side means, and it is why the offset moves too.
	case C2D_Right:  { const f32 l = o.x - h.x; const f32 r = std::max(target.x, l + 2.f * kMinCollider2D); h.x = (r - l) * 0.5f; o.x = (r + l) * 0.5f; break; }
	case C2D_Left:   { const f32 r = o.x + h.x; const f32 l = std::min(target.x, r - 2.f * kMinCollider2D); h.x = (r - l) * 0.5f; o.x = (r + l) * 0.5f; break; }
	case C2D_Top:    { const f32 b = o.y - h.y; const f32 t = std::max(target.y, b + 2.f * kMinCollider2D); h.y = (t - b) * 0.5f; o.y = (t + b) * 0.5f; break; }
	case C2D_Bottom: { const f32 t = o.y + h.y; const f32 b = std::min(target.y, t - 2.f * kMinCollider2D); h.y = (t - b) * 0.5f; o.y = (t + b) * 0.5f; break; }
	case C2D_Point:
	{
		std::vector<Vec2> pts = ph->GetPolygon();
		if (index < 0 || index >= (int)pts.size()) return false;
		const Vec2 np(target.x - o.x, target.y - o.y);
		if (np.x == pts[index].x && np.y == pts[index].y) return false;
		pts[index] = np;
		// Refused when the outline would cross itself (or two of a chain's
		// points would meet): the point stays where it can still be used.
		return ph->GetShapeType() == Shape2DType::Chain ? ph->SetChain(pts, ph->IsChainLoop()) : ph->SetPolygon(pts);
	}
	default:
		return false;
	}
	bool changed = false;
	if (h.x != h0.x || h.y != h0.y) { ph->SetSize(h); changed = true; }
	if (o.x != o0.x || o.y != o0.y) { ph->SetOffset(o); changed = true; }
	return changed;
}

void SceneEditor::DrawCollider2DHandles()
{
	Physics2D* ph = EditingCollider2D();
	if (!ph || !debugRenderer) return;

	const Vec2 P = ph->GetPosition();
	const f32 a = ph->GetAngle();
	const Vec2 o = ph->GetOffset();
	const Vec2 h = ph->GetSize();
	const Vec4 line(0.35f, 1.f, 0.55f, 1.f);
	struct L {
		static Vec3 W(const Vec2& P, const f32 a, const Vec2& local)
		{
			const Vec2 w = Rotate2D(local, a);
			return Vec3(P.x + w.x, P.y + w.y, 0.f);
		}
	};

	// The outline itself, whether or not the physics overlay is on: this is
	// what is being edited.
	std::vector<Vec2> outline;
	switch (ph->GetShapeType())
	{
	case Shape2DType::Circle:
		for (int i = 0; i < 32; i++)
		{
			const f32 t = (f32)i / 32.f * 6.2831853f;
			outline.push_back(Vec2(o.x + cosf(t) * h.x, o.y + sinf(t) * h.x));
		}
		break;
	case Shape2DType::Capsule:
	{
		const f32 r = std::min(h.x, h.y), half = std::max(0.f, h.y - r);
		for (int i = 0; i <= 16; i++) { const f32 t = (f32)i / 16.f * 3.14159265f; outline.push_back(Vec2(o.x + cosf(t) * r, o.y + half + sinf(t) * r)); }
		for (int i = 0; i <= 16; i++) { const f32 t = 3.14159265f + (f32)i / 16.f * 3.14159265f; outline.push_back(Vec2(o.x + cosf(t) * r, o.y - half + sinf(t) * r)); }
		break;
	}
	case Shape2DType::Polygon:
	case Shape2DType::Chain:
		for (size_t i = 0; i < ph->GetPolygon().size(); i++)
			outline.push_back(Vec2(o.x + ph->GetPolygon()[i].x, o.y + ph->GetPolygon()[i].y));
		break;
	default:
		outline.push_back(Vec2(o.x - h.x, o.y - h.y)); outline.push_back(Vec2(o.x + h.x, o.y - h.y));
		outline.push_back(Vec2(o.x + h.x, o.y + h.y)); outline.push_back(Vec2(o.x - h.x, o.y + h.y));
		break;
	}
	// (an open chain has no edge from its last point back to its first)
	const bool open = ph->GetShapeType() == Shape2DType::Chain && !ph->IsChainLoop();
	for (size_t i = 0; i + (open ? 1 : 0) < outline.size(); i++)
		debugRenderer->drawLine(L::W(P, a, outline[i]), L::W(P, a, outline[(i + 1) % outline.size()]), line);
	// The pieces a concave outline was cut into, faintly: what the solver
	// actually collides, and the place to look when a shape behaves oddly.
	if (ph->GetShapeType() == Shape2DType::Polygon)
	{
		std::vector<std::vector<Vec2> > pieces;
		Physics2D::DecomposePolygon(ph->GetPolygon(), pieces);
		if (pieces.size() > 1)
			for (size_t q = 0; q < pieces.size(); q++)
				for (size_t i = 0; i < pieces[q].size(); i++)
				{
					const Vec2 u(o.x + pieces[q][i].x, o.y + pieces[q][i].y);
					const Vec2 &w = pieces[q][(i + 1) % pieces[q].size()];
					debugRenderer->drawLine(L::W(P, a, u), L::W(P, a, Vec2(o.x + w.x, o.y + w.y)), Vec4(0.35f, 1.f, 0.55f, 0.3f));
				}
	}

	// Handles a fixed size on screen, so they are as easy to hit zoomed out
	// as zoomed in.
	const f32 px = ViewportWorldPerPixel();
	std::vector<Collider2DGrip> grips;
	Collider2DGrips(ph, grips);
	for (size_t i = 0; i < grips.size(); i++)
	{
		const bool hot = (collider2DDragKind != C2D_None)
			? (grips[i].kind == collider2DDragKind && grips[i].index == collider2DDragIndex)
			: (grips[i].kind == collider2DHoverKind && grips[i].index == collider2DHoverIndex);
		const f32 s = px * (hot ? 6.f : 4.5f);
		const Vec4 col = hot ? Vec4(1.f, 1.f, 1.f, 1.f)
			: (grips[i].kind == C2D_Center ? Vec4(1.f, 0.85f, 0.35f, 1.f) : line);
		const Vec3 c = L::W(P, a, grips[i].local);
		if (grips[i].kind == C2D_Center)
		{
			// The centre is a cross: it moves, where the squares resize.
			debugRenderer->drawLine(Vec3(c.x - s * 1.4f, c.y, 0.f), Vec3(c.x + s * 1.4f, c.y, 0.f), col);
			debugRenderer->drawLine(Vec3(c.x, c.y - s * 1.4f, 0.f), Vec3(c.x, c.y + s * 1.4f, 0.f), col);
			continue;
		}
		debugRenderer->drawLine(Vec3(c.x - s, c.y - s, 0.f), Vec3(c.x + s, c.y - s, 0.f), col);
		debugRenderer->drawLine(Vec3(c.x + s, c.y - s, 0.f), Vec3(c.x + s, c.y + s, 0.f), col);
		debugRenderer->drawLine(Vec3(c.x + s, c.y + s, 0.f), Vec3(c.x - s, c.y + s, 0.f), col);
		debugRenderer->drawLine(Vec3(c.x - s, c.y + s, 0.f), Vec3(c.x - s, c.y - s, 0.f), col);
	}
}

bool SceneEditor::HandleCollider2DInput()
{
	uint32 goId = 0;
	Physics2D* ph = EditingCollider2D(&goId);
	if (!ph) return false;
	if (editorDisabled) return true;

	const Vec2 P = ph->GetPosition();
	const f32 a = ph->GetAngle();

	// Let go anywhere - inside the image or not - and the drag is one undo
	// step. Checked before the cursor is, so a release off the viewport is
	// not a drag that never ends.
	if (collider2DDragKind != C2D_None && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		if (collider2DChanged && !collider2DBefore.empty())
			PushReplaceCommand(goId, collider2DBefore, "Edit Collider");
		collider2DBefore.clear();
		collider2DChanged = false;
		collider2DDragKind = C2D_None;
		return true;
	}

	Vec2 m;
	if (!ViewportToWorld2D(m)) return true;
	const Vec2 ml = Rotate2D(Vec2(m.x - P.x, m.y - P.y), -a);

	if (collider2DDragKind != C2D_None)
	{
		Vec2 target(ml.x + collider2DGrab.x, ml.y + collider2DGrab.y);
		if (ImGui::GetIO().KeyCtrl)
		{
			target.x = floorf(target.x / 0.25f + 0.5f) * 0.25f;
			target.y = floorf(target.y / 0.25f + 0.5f) * 0.25f;
		}
		if (ApplyCollider2DHandle(ph, collider2DDragKind, collider2DDragIndex, target))
		{
			collider2DChanged = true;
			MarkSceneDirty();
		}
		return true;
	}

	// Nearest handle within reach of the pointer, measured on screen.
	const ImVec2 mp = ImGui::GetIO().MousePos;
	const bool hovered = (mp.x >= viewportImgMin.x && mp.x < viewportImgMin.x + viewportImgSize.x
		&& mp.y >= viewportImgMin.y && mp.y < viewportImgMin.y + viewportImgSize.y);
	std::vector<Collider2DGrip> grips;
	Collider2DGrips(ph, grips);
	const f32 reach = ViewportWorldPerPixel() * 9.f;
	int best = -1;
	f32 bestD = reach * reach;
	if (hovered)
		for (size_t i = 0; i < grips.size(); i++)
		{
			const f32 dx = grips[i].local.x - ml.x, dy = grips[i].local.y - ml.y;
			const f32 d = dx * dx + dy * dy;
			// A side or a point before the centre when both are in reach: on
			// a small shape they overlap, and the centre can still be had by
			// zooming in, where a side under the centre could not.
			if (d < bestD || (d <= reach * reach && best >= 0 && grips[best].kind == C2D_Center && grips[i].kind != C2D_Center))
			{ bestD = d; best = (int)i; }
		}
	collider2DHoverKind = best >= 0 ? grips[best].kind : (int)C2D_None;
	collider2DHoverIndex = best >= 0 ? grips[best].index : 0;

	if (!hovered) return true;
	if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
	{
		ViewportPickAtMouse(/*selectComponent=*/true);
		return true;
	}
	const bool outlineShape = ph->GetShapeType() == Shape2DType::Polygon || ph->GetShapeType() == Shape2DType::Chain;
	// Alt-click a point: it is removed. Shift-click near an edge: a point is
	// added there. An outline is drawn in the view, not typed.
	if (outlineShape && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && (ImGui::GetIO().KeyAlt || ImGui::GetIO().KeyShift))
	{
		const bool isChain = ph->GetShapeType() == Shape2DType::Chain;
		const Vec2 off = ph->GetOffset();
		std::vector<Vec2> pts = ph->GetPolygon();
		bool edited = false;
		if (ImGui::GetIO().KeyAlt)
		{
			const size_t fewest = isChain ? (ph->IsChainLoop() ? 3 : 2) : 3;
			if (best >= 0 && grips[best].kind == C2D_Point && pts.size() > fewest)
			{ pts.erase(pts.begin() + grips[best].index); edited = true; }
		}
		else if (pts.size() < (size_t)Physics2D::MaxOutlinePoints)
		{
			// The edge nearest the cursor, if it is within reach.
			const Vec2 m2(ml.x - off.x, ml.y - off.y);
			const size_t edges = (isChain && !ph->IsChainLoop()) ? pts.size() - 1 : pts.size();
			int at = -1;
			f32 nearest = reach * reach * 4.f;
			Vec2 where;
			for (size_t i = 0; i < edges; i++)
			{
				const Vec2 &u = pts[i], &w = pts[(i + 1) % pts.size()];
				const f32 ex = w.x - u.x, ey = w.y - u.y, len2 = ex * ex + ey * ey;
				if (len2 < 1e-10f) continue;
				f32 t = ((m2.x - u.x) * ex + (m2.y - u.y) * ey) / len2;
				t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
				const Vec2 c(u.x + ex * t, u.y + ey * t);
				const f32 d = (c.x - m2.x) * (c.x - m2.x) + (c.y - m2.y) * (c.y - m2.y);
				if (d < nearest) { nearest = d; at = (int)i; where = m2; }
			}
			if (at >= 0) { pts.insert(pts.begin() + at + 1, where); edited = true; }
			// Off the end of an open chain: the line is carried on to there.
			else if (isChain && !ph->IsChainLoop())
			{
				const f32 d0 = (pts.front().x - m2.x) * (pts.front().x - m2.x) + (pts.front().y - m2.y) * (pts.front().y - m2.y);
				const f32 d1 = (pts.back().x - m2.x) * (pts.back().x - m2.x) + (pts.back().y - m2.y) * (pts.back().y - m2.y);
				if (d0 < d1) pts.insert(pts.begin(), m2); else pts.push_back(m2);
				edited = true;
			}
		}
		if (edited)
		{
			const std::string before = SnapshotSubtree(goId);
			if (isChain ? ph->SetChain(pts, ph->IsChainLoop()) : ph->SetPolygon(pts))
			{ MarkSceneDirty(); PushReplaceCommand(goId, before, ImGui::GetIO().KeyAlt ? "Remove Outline Point" : "Add Outline Point"); }
		}
		return true;
	}
	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		if (best >= 0)
		{
			collider2DDragKind = grips[best].kind;
			collider2DDragIndex = grips[best].index;
			collider2DGrab = Vec2(grips[best].local.x - ml.x, grips[best].local.y - ml.y);
			collider2DBefore = SnapshotSubtree(goId);
			collider2DChanged = false;
		}
		else
			// Not a handle: an ordinary click. Picking another object ends
			// the mode by itself; picking this one again leaves it on.
			ViewportPickAtMouse();
	}
	return true;
}

bool SceneEditor::AgentEditCollider2D(const std::string& name, const bool on, std::string& errOut)
{
	if (playMode) { errOut = "editor is in play mode"; return false; }
	if (!sceneIsTwoD) { errOut = "collider handles are for 2D scenes"; return false; }
	if (!on)
	{
		collider2DEdit = false;
		collider2DDragKind = C2D_None;
		return true;
	}
	SceneObject* obj = FindObjectByName(sceneObjects, name);
	if (!obj) { errOut = "object '" + name + "' not found"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();
	bool has = false;
	for (size_t i = 0; i < go->GetComponents().size(); i++)
		if (go->GetComponents()[i] && go->GetComponents()[i]->GetComponentType() == ComponentType::Physics2D) has = true;
	if (!has) { errOut = "'" + name + "' has no Physics2D"; return false; }
	SelectSceneObject(obj);
	collider2DEdit = true;
	collider2DEditGo = obj->GetID();
	collider2DDragKind = C2D_None;
	return true;
}

bool SceneEditor::AgentDragCollider2D(const std::string& name, const json& p, std::string& errOut)
{
	if (playMode) { errOut = "editor is in play mode"; return false; }
	SceneObject* obj = FindObjectByName(sceneObjects, name);
	if (!obj) { errOut = "object '" + name + "' not found"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();
	Physics2D* ph = NULL;
	for (size_t i = 0; i < go->GetComponents().size(); i++)
		if (go->GetComponents()[i] && go->GetComponents()[i]->GetComponentType() == ComponentType::Physics2D)
			ph = static_cast<Physics2D*>(go->GetComponents()[i].get());
	if (!ph) { errOut = "'" + name + "' has no Physics2D"; return false; }
	if (!p.contains("to") || !p["to"].is_array() || p["to"].size() < 2)
	{ errOut = "needs to: [x, y] in world units"; return false; }

	const std::string hname = p.value("handle", std::string("center"));
	int kind = C2D_None;
	if (hname == "center") kind = C2D_Center;
	else if (hname == "right") kind = C2D_Right;
	else if (hname == "left") kind = C2D_Left;
	else if (hname == "top") kind = C2D_Top;
	else if (hname == "bottom") kind = C2D_Bottom;
	else if (hname == "radius") kind = C2D_Radius;
	else if (hname == "point") kind = C2D_Point;
	else { errOut = "handle is one of center, right, left, top, bottom, radius, point"; return false; }

	// Only the handles the shape has: "radius" on a box would set a number
	// nothing reads.
	std::vector<Collider2DGrip> grips;
	Collider2DGrips(ph, grips);
	const int index = p.value("index", 0);
	bool exists = false;
	for (size_t i = 0; i < grips.size(); i++)
		if (grips[i].kind == kind && (kind != C2D_Point || grips[i].index == index)) exists = true;
	if (!exists) { errOut = "this shape has no '" + hname + "' handle" + (kind == C2D_Point ? " at that index" : ""); return false; }

	const Vec2 P = ph->GetPosition();
	const Vec2 world(p["to"][0].get<f32>(), p["to"][1].get<f32>());
	const Vec2 target = Rotate2D(Vec2(world.x - P.x, world.y - P.y), -ph->GetAngle());
	const std::string before = SnapshotSubtree(obj->GetID());
	if (!ApplyCollider2DHandle(ph, kind, index, target))
	{
		if (kind == C2D_Point) { errOut = "refused: the outline would not be convex (or the point did not move)"; return false; }
		return true;     // already there
	}
	MarkSceneDirty();
	PushReplaceCommand(obj->GetID(), before, "Edit Collider");
	return true;
}
