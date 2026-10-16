//=============================================================================
// Name        : AnimatorEditor.cpp
// Description : See the header.
//=============================================================================

#include "AnimatorEditor.h"
#include "../AnimatorDocument.h"
#include "../AnimatorPreview.h"
#include "../AnimationPreview.h"
#include "../AnimationEditorDocument.h"
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/AnimationManager/AnimatorController.h>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace p3d;

namespace AnimatorEditor {

namespace {

const float kNodeW = 150.f, kNodeH = 42.f;
const char* kTypeLabels[] = { "float", "bool", "trigger" };
const char* kOpLabels[] = { ">", "<", "==", "!=", "is true", "is false" };

ImVec2 NodeCenter(AnimatorDocument& doc, const std::string& state, const ImVec2& origin)
{
	if (state == "*") return ImVec2(origin.x + (doc.anyX + doc.panX) * doc.zoom, origin.y + (doc.anyY + doc.panY) * doc.zoom);
	const int i = doc.L().FindState(state);
	if (i < 0) return origin;
	const Vec2& p = doc.L().states[i].position;
	return ImVec2(origin.x + (p.x + doc.panX) * doc.zoom, origin.y + (p.y + doc.panY) * doc.zoom);
}

// Where a line from `from` to `to` leaves a node's box, so arrows meet edges.
ImVec2 BoxEdge(const ImVec2& c, const ImVec2& toward, float hw, float hh)
{
	const float dx = toward.x - c.x, dy = toward.y - c.y;
	if (std::fabs(dx) < 1e-3f && std::fabs(dy) < 1e-3f) return c;
	const float sx = std::fabs(dx) > 1e-6f ? hw / std::fabs(dx) : 1e9f;
	const float sy = std::fabs(dy) > 1e-6f ? hh / std::fabs(dy) : 1e9f;
	const float s = std::min(sx, sy);
	return ImVec2(c.x + dx * s, c.y + dy * s);
}

float DistToSegment(const ImVec2& p, const ImVec2& a, const ImVec2& b)
{
	const float abx = b.x - a.x, aby = b.y - a.y;
	const float l2 = abx * abx + aby * aby;
	float t = l2 > 0.f ? ((p.x - a.x) * abx + (p.y - a.y) * aby) / l2 : 0.f;
	t = std::max(0.f, std::min(1.f, t));
	const float dx = a.x + abx * t - p.x, dy = a.y + aby * t - p.y;
	return std::sqrt(dx * dx + dy * dy);
}

void DrawParams(AnimatorDocument& doc, Animator* live)
{
	ImGui::TextUnformatted("Parameters");
	ImGui::SameLine();
	if (ImGui::SmallButton("+")) ImGui::OpenPopup("##addparam");
	if (ImGui::BeginPopup("##addparam"))
	{
		for (int t = 0; t < 3; t++)
			if (ImGui::MenuItem(kTypeLabels[t])) doc.AddParam(t == 2 ? "fire" : (t == 1 ? "flag" : "speed"), t);
		ImGui::EndPopup();
	}
	ImGui::TextDisabled(live ? "Playing: edit to drive it live." : "What scripts set (setAnimatorFloat...).");
	ImGui::Separator();
	std::string remove;
	for (size_t i = 0; i < doc.controller.params.size(); i++)
	{
		AnimatorParam& p = doc.controller.params[i];
		ImGui::PushID((int)i);
		std::string name = p.name;
		ImGui::SetNextItemWidth(95.f);
		if (ImGui::InputText("##n", &name, ImGuiInputTextFlags_EnterReturnsTrue)) doc.RenameParam(p.name, name);
		ImGui::SameLine();
		ImGui::TextDisabled("%s", kTypeLabels[std::max(0, std::min(2, p.type))]);
		ImGui::SameLine();
		if (live)
		{
			// The running character's value, not the default.
			if (p.type == ANIMATOR_FLOAT)
			{
				float v = live->GetFloat(p.name);
				ImGui::SetNextItemWidth(60.f);
				if (ImGui::DragFloat("##v", &v, 0.05f)) live->SetFloat(p.name, v);
			}
			else if (p.type == ANIMATOR_BOOL)
			{
				bool v = live->GetBool(p.name);
				if (ImGui::Checkbox("##v", &v)) live->SetBool(p.name, v);
			}
			else if (ImGui::SmallButton("fire")) live->SetTrigger(p.name);
		}
		else if (p.type == ANIMATOR_FLOAT)
		{
			ImGui::SetNextItemWidth(60.f);
			ImGui::DragFloat("##d", &p.defaultValue, 0.05f);
			if (ImGui::IsItemActivated()) doc.BeginEdit();
			if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndEdit("Parameter default");
		}
		else if (p.type == ANIMATOR_BOOL)
		{
			bool v = p.defaultValue > 0.5f;
			if (ImGui::Checkbox("##d", &v))
			{
				const std::string before = doc.Snapshot();
				p.defaultValue = v ? 1.f : 0.f;
				doc.PushEdit(before, "Parameter default");
			}
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("x")) remove = p.name;
		ImGui::PopID();
	}
	if (!remove.empty()) doc.RemoveParam(remove);
}

void DrawInspector(AnimatorDocument& doc, const std::vector<std::pair<std::string, std::string> >& clipFiles)
{
	// Where state clips are picked from.
	ImGui::TextUnformatted("Clips from");
	ImGui::SetNextItemWidth(-1.f);
	std::string shown = doc.clipSourcePath.empty() ? std::string("(choose a .p3da)") : doc.clipSourcePath;
	for (size_t i = 0; i < clipFiles.size(); i++) if (clipFiles[i].second == doc.clipSourcePath) shown = clipFiles[i].first;
	if (ImGui::BeginCombo("##clipsrc", shown.c_str()))
	{
		for (size_t i = 0; i < clipFiles.size(); i++)
			if (ImGui::Selectable(clipFiles[i].first.c_str(), clipFiles[i].second == doc.clipSourcePath))
				doc.LoadClipNames(clipFiles[i].second);
		ImGui::EndCombo();
	}
	ImGui::Separator();

	const int si = doc.L().FindState(doc.selectedState);
	if (si >= 0)
	{
		AnimatorState& s = doc.L().states[si];
		ImGui::Text("State");
		std::string name = s.name;
		ImGui::SetNextItemWidth(-1.f);
		if (ImGui::InputText("##sname", &name, ImGuiInputTextFlags_EnterReturnsTrue)) doc.RenameState(s.name, name);
		ImGui::TextUnformatted("Clip");
		ImGui::SetNextItemWidth(-1.f);
		if (ImGui::BeginCombo("##sclip", s.clip.empty() ? "(none)" : s.clip.c_str()))
		{
			for (size_t i = 0; i < doc.clipNames.size(); i++)
				if (ImGui::Selectable(doc.clipNames[i].c_str(), doc.clipNames[i] == s.clip))
				{
					const std::string before = doc.Snapshot();
					s.clip = doc.clipNames[i];
					doc.PushEdit(before, "State clip");
				}
			if (doc.clipNames.empty()) ImGui::TextDisabled("Pick 'Clips from' above.");
			ImGui::EndCombo();
		}
		std::string typed = s.clip;
		ImGui::SetNextItemWidth(-1.f);
		if (ImGui::InputTextWithHint("##sclipname", "or type a clip name", &typed, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			const std::string before = doc.Snapshot();
			s.clip = typed;
			doc.PushEdit(before, "State clip");
		}
		ImGui::SetNextItemWidth(100.f);
		ImGui::DragFloat("Speed", &s.speed, 0.01f, -4.f, 4.f, "%.2fx");
		if (ImGui::IsItemActivated()) doc.BeginEdit();
		if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndEdit("State speed");
		bool loop = s.loop;
		if (ImGui::Checkbox("Loop", &loop)) { const std::string before = doc.Snapshot(); s.loop = loop; doc.PushEdit(before, "State loop"); }

		// Blend: several clips mixed by a parameter, instead of one clip.
		ImGui::Separator();
		bool blend = s.IsBlend();
		if (ImGui::Checkbox("Blend by a parameter", &blend))
		{
			const std::string before = doc.Snapshot();
			if (blend)
			{
				AnimatorBlendClip a; a.clip = s.clip; a.at = 0.f;
				AnimatorBlendClip b; b.at = 1.f;
				s.blend.push_back(a); s.blend.push_back(b);
				for (size_t p = 0; p < doc.controller.params.size() && s.blendParam.empty(); p++)
					if (doc.controller.params[p].type == ANIMATOR_FLOAT) s.blendParam = doc.controller.params[p].name;
			}
			else { s.blend.clear(); s.blendParam.clear(); }
			doc.PushEdit(before, blend ? "Make blend state" : "Make plain state");
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("One state that is always the right mix of several clips - walk at speed 1,\nrun at 4 - instead of a state per clip. The clips are kept in step.");
		if (s.IsBlend())
		{
			ImGui::SetNextItemWidth(-1.f);
			if (ImGui::BeginCombo("##bparam", s.blendParam.empty() ? "(parameter)" : s.blendParam.c_str()))
			{
				for (size_t p = 0; p < doc.controller.params.size(); p++)
					if (doc.controller.params[p].type == ANIMATOR_FLOAT && ImGui::Selectable(doc.controller.params[p].name.c_str(), doc.controller.params[p].name == s.blendParam))
					{
						const std::string before = doc.Snapshot();
						s.blendParam = doc.controller.params[p].name;
						doc.PushEdit(before, "Blend parameter");
					}
				ImGui::EndCombo();
			}
			int removeClip = -1;
			for (size_t k = 0; k < s.blend.size(); k++)
			{
				ImGui::PushID(1000 + (int)k);
				ImGui::SetNextItemWidth(110.f);
				if (ImGui::BeginCombo("##bc", s.blend[k].clip.empty() ? "(clip)" : s.blend[k].clip.c_str()))
				{
					for (size_t c = 0; c < doc.clipNames.size(); c++)
						if (ImGui::Selectable(doc.clipNames[c].c_str(), doc.clipNames[c] == s.blend[k].clip))
						{
							const std::string before = doc.Snapshot();
							s.blend[k].clip = doc.clipNames[c];
							doc.PushEdit(before, "Blend clip");
						}
					if (doc.clipNames.empty()) ImGui::TextDisabled("Pick 'Clips from' above.");
					ImGui::EndCombo();
				}
				ImGui::SameLine();
				ImGui::SetNextItemWidth(55.f);
				ImGui::DragFloat("##at", &s.blend[k].at, 0.05f, -100.f, 100.f, "at %.2f");
				if (ImGui::IsItemActivated()) doc.BeginEdit();
				if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndEdit("Blend position");
				ImGui::SameLine();
				if (ImGui::SmallButton("x")) removeClip = (int)k;
				ImGui::PopID();
			}
			if (removeClip >= 0 && s.blend.size() > 1)
			{
				const std::string before = doc.Snapshot();
				s.blend.erase(s.blend.begin() + removeClip);
				doc.PushEdit(before, "Blend clip");
			}
			if (ImGui::SmallButton("+ Blend clip"))
			{
				const std::string before = doc.Snapshot();
				AnimatorBlendClip b;
				b.at = s.blend.empty() ? 0.f : s.blend.back().at + 1.f;
				s.blend.push_back(b);
				doc.PushEdit(before, "Blend clip");
			}
		}
		if (doc.L().defaultState != s.name && ImGui::Button("Make default"))
		{
			const std::string before = doc.Snapshot();
			doc.L().defaultState = s.name;
			doc.PushEdit(before, "Default state");
		}
		if (doc.L().defaultState == s.name) ImGui::TextDisabled("Default - where it starts.");
		if (ImGui::Button("Transition from here...")) doc.linkingFrom = s.name;
		if (ImGui::Button("Delete state")) doc.RemoveState(s.name);
		return;
	}

	if (doc.selectedTransition >= 0 && doc.selectedTransition < (int)doc.L().transitions.size())
	{
		AnimatorTransition& t = doc.L().transitions[doc.selectedTransition];
		ImGui::Text("Transition");
		ImGui::TextDisabled("%s  ->  %s", t.from == "*" ? "Any State" : t.from.c_str(), t.to.c_str());
		ImGui::SetNextItemWidth(100.f);
		ImGui::DragFloat("Fade (s)", &t.duration, 0.01f, 0.f, 5.f, "%.2f");
		if (ImGui::IsItemActivated()) doc.BeginEdit();
		if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndEdit("Transition fade");
		bool hasExit = t.exitTime >= 0.f;
		if (ImGui::Checkbox("Exit time", &hasExit))
		{
			const std::string before = doc.Snapshot();
			t.exitTime = hasExit ? 1.f : -1.f;
			doc.PushEdit(before, "Transition exit time");
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Wait until the clip has played this far (0..1 of a loop) before leaving.\nWith no conditions: leave when the clip is done.");
		if (hasExit)
		{
			ImGui::SameLine();
			ImGui::SetNextItemWidth(80.f);
			ImGui::DragFloat("##exit", &t.exitTime, 0.01f, 0.f, 1.f, "%.2f");
			if (ImGui::IsItemActivated()) doc.BeginEdit();
			if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndEdit("Transition exit time");
		}
		ImGui::Separator();
		ImGui::TextUnformatted("When (all of)");
		int removeCond = -1;
		for (size_t k = 0; k < t.conditions.size(); k++)
		{
			AnimatorCondition& c = t.conditions[k];
			ImGui::PushID((int)k);
			ImGui::SetNextItemWidth(90.f);
			if (ImGui::BeginCombo("##p", c.param.empty() ? "(param)" : c.param.c_str()))
			{
				for (size_t p = 0; p < doc.controller.params.size(); p++)
					if (ImGui::Selectable(doc.controller.params[p].name.c_str(), doc.controller.params[p].name == c.param))
					{
						const std::string before = doc.Snapshot();
						c.param = doc.controller.params[p].name;
						// A bool or trigger wants "is true", a number a comparison.
						if (doc.controller.params[p].type != ANIMATOR_FLOAT) c.op = ANIMATOR_TRUE;
						doc.PushEdit(before, "Condition");
					}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			ImGui::SetNextItemWidth(70.f);
			int op = c.op;
			if (ImGui::Combo("##o", &op, kOpLabels, 6)) { const std::string before = doc.Snapshot(); c.op = op; doc.PushEdit(before, "Condition"); }
			if (c.op < ANIMATOR_TRUE)
			{
				ImGui::SameLine();
				ImGui::SetNextItemWidth(55.f);
				ImGui::DragFloat("##v", &c.value, 0.05f);
				if (ImGui::IsItemActivated()) doc.BeginEdit();
				if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndEdit("Condition");
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("x")) removeCond = (int)k;
			ImGui::PopID();
		}
		if (removeCond >= 0) { const std::string before = doc.Snapshot(); t.conditions.erase(t.conditions.begin() + removeCond); doc.PushEdit(before, "Condition"); }
		if (ImGui::SmallButton("+ Condition"))
		{
			const std::string before = doc.Snapshot();
			AnimatorCondition c;
			if (!doc.controller.params.empty())
			{
				c.param = doc.controller.params[0].name;
				if (doc.controller.params[0].type != ANIMATOR_FLOAT) c.op = ANIMATOR_TRUE;
			}
			t.conditions.push_back(c);
			doc.PushEdit(before, "Condition");
		}
		if (t.conditions.empty() && t.exitTime < 0.f)
			ImGui::TextColored(ImVec4(1.f, 0.6f, 0.4f, 1.f), "No condition and no exit time: never fires.");
		ImGui::Separator();
		if (ImGui::Button("Delete transition")) doc.RemoveTransition(doc.selectedTransition);
		return;
	}
	ImGui::TextDisabled("Select a state or a transition.\nRight-click the graph to add a state.");
}

void DrawGraph(AnimatorDocument& doc, Animator* live)
{
	ImGui::BeginChild("##graph", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImVec2 p0 = ImGui::GetCursorScreenPos();
	const ImVec2 size = ImGui::GetContentRegionAvail();
	ImGui::InvisibleButton("##canvas", ImVec2(std::max(size.x, 10.f), std::max(size.y, 10.f)),
		ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
	const bool hovered = ImGui::IsItemHovered();
	const ImVec2 mouse = ImGui::GetIO().MousePos;
	const ImVec2 origin(p0.x + size.x * 0.5f, p0.y + size.y * 0.5f);
	// Fit everything (the states and Any State) into the view: on first sight,
	// and on request.
	if (doc.frameRequested && size.x > 20.f && size.y > 20.f)
	{
		doc.frameRequested = false;
		float lo[2] = { doc.anyX, doc.anyY }, hi[2] = { doc.anyX, doc.anyY };
		for (size_t i = 0; i < doc.L().states.size(); i++)
		{
			const Vec2& q = doc.L().states[i].position;
			lo[0] = std::min(lo[0], q.x); lo[1] = std::min(lo[1], q.y);
			hi[0] = std::max(hi[0], q.x); hi[1] = std::max(hi[1], q.y);
		}
		const float w = hi[0] - lo[0] + kNodeW * 1.6f, h = hi[1] - lo[1] + kNodeH * 3.f;
		doc.zoom = std::max(0.12f, std::min(1.5f, std::min(size.x / w, size.y / h)));
		doc.panX = -(lo[0] + hi[0]) * 0.5f;
		doc.panY = -(lo[1] + hi[1]) * 0.5f;
	}
	dl->AddRectFilled(p0, ImVec2(p0.x + size.x, p0.y + size.y), IM_COL32(30, 32, 38, 255));
	// Grid.
	const float step = 40.f * doc.zoom;
	if (step > 6.f)
	{
		const float ox = std::fmod(origin.x + doc.panX * doc.zoom - p0.x, step), oy = std::fmod(origin.y + doc.panY * doc.zoom - p0.y, step);
		for (float x = p0.x + ox; x < p0.x + size.x; x += step) dl->AddLine(ImVec2(x, p0.y), ImVec2(x, p0.y + size.y), IM_COL32(40, 43, 50, 255));
		for (float y = p0.y + oy; y < p0.y + size.y; y += step) dl->AddLine(ImVec2(p0.x, y), ImVec2(p0.x + size.x, y), IM_COL32(40, 43, 50, 255));
	}
	dl->PushClipRect(p0, ImVec2(p0.x + size.x, p0.y + size.y), true);

	const float hw = kNodeW * 0.5f * doc.zoom, hh = kNodeH * 0.5f * doc.zoom;
	const std::string liveLayer = doc.LayerName();
	const std::string liveState = live ? live->GetState(liveLayer) : std::string();
	const int liveTransition = live ? live->GetLastTransition(liveLayer) : -1;

	// Transitions. A pair of opposite ones is drawn side by side, not on top.
	int hotTransition = -1;
	for (size_t i = 0; i < doc.L().transitions.size(); i++)
	{
		const AnimatorTransition& t = doc.L().transitions[i];
		ImVec2 a = NodeCenter(doc, t.from, origin), b = NodeCenter(doc, t.to, origin);
		const float dx = b.x - a.x, dy = b.y - a.y, l = std::sqrt(dx * dx + dy * dy);
		if (l < 1.f) continue;
		const ImVec2 n(-dy / l * 6.f * doc.zoom, dx / l * 6.f * doc.zoom);
		a = ImVec2(a.x + n.x, a.y + n.y); b = ImVec2(b.x + n.x, b.y + n.y);
		const ImVec2 ea = BoxEdge(a, b, hw, hh), eb = BoxEdge(b, a, hw, hh);
		const bool sel = doc.selectedTransition == (int)i;
		const bool hot = hovered && DistToSegment(mouse, ea, eb) < 6.f;
		if (hot) hotTransition = (int)i;
		ImU32 col = sel ? IM_COL32(255, 200, 60, 255) : hot ? IM_COL32(230, 230, 240, 255) : IM_COL32(150, 160, 180, 220);
		if ((int)i == liveTransition && live) col = IM_COL32(120, 240, 140, 255);
		dl->AddLine(ea, eb, col, sel ? 2.5f : 1.7f);
		// Arrow head, in the middle so it is never hidden under a node.
		const ImVec2 m((ea.x + eb.x) * 0.5f, (ea.y + eb.y) * 0.5f);
		const ImVec2 u(dx / l, dy / l);
		const float ah = 8.f * doc.zoom;
		dl->AddTriangleFilled(ImVec2(m.x + u.x * ah, m.y + u.y * ah),
			ImVec2(m.x - u.x * ah * 0.6f + u.y * ah * 0.6f, m.y - u.y * ah * 0.6f - u.x * ah * 0.6f),
			ImVec2(m.x - u.x * ah * 0.6f - u.y * ah * 0.6f, m.y - u.y * ah * 0.6f + u.x * ah * 0.6f), col);
	}

	// The linking line, while choosing where a new transition goes.
	if (!doc.linkingFrom.empty())
		dl->AddLine(NodeCenter(doc, doc.linkingFrom, origin), mouse, IM_COL32(255, 200, 60, 200), 2.f);

	// Nodes: Any State, then the states.
	int hotState = -2;   // -1 = Any State
	{
		const ImVec2 c = NodeCenter(doc, "*", origin);
		const ImVec2 a(c.x - hw, c.y - hh), b(c.x + hw, c.y + hh);
		const bool hot = hovered && mouse.x >= a.x && mouse.x <= b.x && mouse.y >= a.y && mouse.y <= b.y;
		if (hot) hotState = -1;
		dl->AddRectFilled(a, b, IM_COL32(70, 60, 95, 255), 6.f);
		dl->AddRect(a, b, hot ? IM_COL32(240, 240, 250, 255) : IM_COL32(140, 120, 190, 255), 6.f, 0, 1.5f);
		const float fs = ImGui::GetFontSize() * std::max(0.55f, std::min(1.2f, doc.zoom));
		dl->PushClipRect(a, b, true);
		dl->AddText(NULL, fs, ImVec2(a.x + 8.f * doc.zoom, c.y - fs * 0.5f), IM_COL32(230, 225, 245, 255), "Any State");
		dl->PopClipRect();
	}
	for (size_t i = 0; i < doc.L().states.size(); i++)
	{
		const AnimatorState& s = doc.L().states[i];
		const ImVec2 c = NodeCenter(doc, s.name, origin);
		const ImVec2 a(c.x - hw, c.y - hh), b(c.x + hw, c.y + hh);
		const bool hot = hovered && mouse.x >= a.x && mouse.x <= b.x && mouse.y >= a.y && mouse.y <= b.y;
		if (hot) hotState = (int)i;
		const bool sel = doc.selectedState == s.name;
		const bool isDefault = doc.L().defaultState == s.name;
		const bool isLive = !liveState.empty() && liveState == s.name;
		ImU32 fill = isDefault ? IM_COL32(150, 95, 40, 255) : IM_COL32(60, 66, 80, 255);
		if (isLive) fill = IM_COL32(50, 140, 70, 255);
		dl->AddRectFilled(a, b, fill, 6.f);
		dl->AddRect(a, b, sel ? IM_COL32(255, 200, 60, 255) : hot ? IM_COL32(240, 240, 250, 255) : IM_COL32(110, 118, 135, 255), 6.f, 0, sel ? 2.5f : 1.5f);
		// Text follows the zoom, and stays inside its box.
		const float fs = ImGui::GetFontSize() * std::max(0.55f, std::min(1.2f, doc.zoom));
		dl->PushClipRect(a, b, true);
		dl->AddText(NULL, fs, ImVec2(a.x + 8.f * doc.zoom, a.y + 4.f * doc.zoom), IM_COL32(240, 240, 248, 255), s.name.c_str());
		std::string sub = s.clip.empty() ? std::string("(no clip)") : s.clip;
		if (s.IsBlend())
		{
			// A blend: its clips, with the running character's weights.
			sub = "blend " + (s.blendParam.empty() ? std::string("?") : s.blendParam) + ":";
			const std::vector<f32> w = (isLive && live) ? live->GetBlendWeights(liveLayer) : std::vector<f32>();
			for (size_t k = 0; k < s.blend.size(); k++)
			{
				char buf[64];
				if (k < w.size()) snprintf(buf, sizeof(buf), " %s %.0f%%", s.blend[k].clip.c_str(), w[k] * 100.f);
				else snprintf(buf, sizeof(buf), " %s", s.blend[k].clip.c_str());
				sub += buf;
			}
		}
		dl->AddText(NULL, fs * 0.9f, ImVec2(a.x + 8.f * doc.zoom, a.y + 4.f * doc.zoom + fs * 1.1f), IM_COL32(180, 186, 200, 255), sub.c_str());
		dl->PopClipRect();
		// The live one shows how far through its clip it is.
		if (isLive && live)
			dl->AddRectFilled(ImVec2(a.x + 4.f, b.y - 4.f), ImVec2(a.x + 4.f + (b.x - a.x - 8.f) * live->GetStateProgress(liveLayer), b.y - 2.f),
				IM_COL32(170, 255, 180, 255));
	}
	dl->PopClipRect();

	// ---- interaction ----
	static std::string dragging;
	static bool draggingAny = false;
	if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		if (!doc.linkingFrom.empty())
		{
			if (hotState >= 0)
			{
				const int t = doc.AddTransition(doc.linkingFrom, doc.L().states[hotState].name);
				if (t >= 0) { doc.selectedTransition = t; doc.selectedState.clear(); }
			}
			doc.linkingFrom.clear();
		}
		else if (hotState >= 0)
		{
			doc.selectedState = doc.L().states[hotState].name;
			doc.selectedTransition = -1;
			dragging = doc.selectedState;
			doc.BeginEdit();
		}
		else if (hotState == -1) { draggingAny = true; doc.selectedState.clear(); doc.selectedTransition = -1; }
		else if (hotTransition >= 0) { doc.selectedTransition = hotTransition; doc.selectedState.clear(); }
		else { doc.selectedState.clear(); doc.selectedTransition = -1; }
	}
	if (!dragging.empty())
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			const int i = doc.L().FindState(dragging);
			if (i >= 0)
			{
				doc.L().states[i].position.x += ImGui::GetIO().MouseDelta.x / doc.zoom;
				doc.L().states[i].position.y += ImGui::GetIO().MouseDelta.y / doc.zoom;
				doc.dirty = true;
			}
		}
		else { dragging.clear(); doc.EndEdit("Move state"); }
	}
	if (draggingAny)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) { doc.anyX += ImGui::GetIO().MouseDelta.x / doc.zoom; doc.anyY += ImGui::GetIO().MouseDelta.y / doc.zoom; }
		else draggingAny = false;
	}
	if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
	{
		doc.panX += ImGui::GetIO().MouseDelta.x / doc.zoom;
		doc.panY += ImGui::GetIO().MouseDelta.y / doc.zoom;
	}
	if (hovered && ImGui::GetIO().MouseWheel != 0.f)
		doc.zoom = std::max(0.12f, std::min(2.5f, doc.zoom * std::pow(1.1f, ImGui::GetIO().MouseWheel)));
	if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		ImGui::GetStateStorage()->SetFloat(ImGui::GetID("##ctxx"), (mouse.x - origin.x) / doc.zoom - doc.panX);
		ImGui::GetStateStorage()->SetFloat(ImGui::GetID("##ctxy"), (mouse.y - origin.y) / doc.zoom - doc.panY);
		ImGui::GetStateStorage()->SetInt(ImGui::GetID("##ctxs"), hotState);
		ImGui::OpenPopup("##graphmenu");
	}
	if (ImGui::BeginPopup("##graphmenu"))
	{
		const int hs = ImGui::GetStateStorage()->GetInt(ImGui::GetID("##ctxs"), -2);
		if (hs >= 0 && hs < (int)doc.L().states.size())
		{
			const std::string name = doc.L().states[hs].name;
			if (ImGui::MenuItem("Transition from here")) doc.linkingFrom = name;
			if (ImGui::MenuItem("Make default"))
			{
				const std::string before = doc.Snapshot();
				doc.L().defaultState = name;
				doc.PushEdit(before, "Default state");
			}
			if (ImGui::MenuItem("Delete")) doc.RemoveState(name);
		}
		else if (hs == -1)
		{
			if (ImGui::MenuItem("Transition from Any State")) doc.linkingFrom = "*";
		}
		else if (ImGui::MenuItem("Add state here"))
		{
			doc.selectedState = doc.AddState("State", std::string(),
				ImGui::GetStateStorage()->GetFloat(ImGui::GetID("##ctxx"), 0.f),
				ImGui::GetStateStorage()->GetFloat(ImGui::GetID("##ctxy"), 0.f));
			doc.selectedTransition = -1;
		}
		ImGui::EndPopup();
	}
	if (!doc.linkingFrom.empty() && (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
		doc.linkingFrom.clear();
	if (hovered && ImGui::IsKeyPressed(ImGuiKey_Delete) && !ImGui::GetIO().WantTextInput)
	{
		if (!doc.selectedState.empty()) doc.RemoveState(doc.selectedState);
		else if (doc.selectedTransition >= 0) doc.RemoveTransition(doc.selectedTransition);
	}
	ImGui::EndChild();
}

// The preview: rig and clip pickers, transport, and the viewport.
void DrawPreview(AnimatorDocument& doc, const std::vector<std::pair<std::string, std::string> >& clipFiles,
	const std::vector<std::pair<std::string, std::string> >& meshes, float dt, float height)
{
	if (!doc.preview) doc.preview.reset(new AnimatorPreview());
	AnimatorPreview& pv = *doc.preview;
	ImGui::BeginChild("##apreview", ImVec2(0, height), true);
	std::string rigLabel = pv.meshPath.empty() ? std::string("(rig)") : pv.meshPath;
	for (size_t i = 0; i < meshes.size(); i++) if (meshes[i].second == pv.meshPath) rigLabel = meshes[i].first;
	ImGui::SetNextItemWidth(std::max(80.f, ImGui::GetContentRegionAvail().x * 0.5f));
	bool reload = false;
	if (ImGui::BeginCombo("##prig", rigLabel.c_str()))
	{
		for (size_t i = 0; i < meshes.size(); i++)
			if (ImGui::Selectable(meshes[i].first.c_str(), meshes[i].second == pv.meshPath)) { pv.meshPath = meshes[i].second; reload = true; }
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	const std::string clipsLabel = pv.clipFiles.empty() ? std::string("(clips)") : std::to_string(pv.clipFiles.size()) + " clip file(s)";
	ImGui::SetNextItemWidth(-1.f);
	if (ImGui::BeginCombo("##pclips", clipsLabel.c_str()))
	{
		for (size_t i = 0; i < clipFiles.size(); i++)
		{
			if (clipFiles[i].second.size() > 6 && clipFiles[i].second.compare(clipFiles[i].second.size() - 6, 6, ".p3d2d") == 0) continue;
			std::vector<std::string>::iterator it = std::find(pv.clipFiles.begin(), pv.clipFiles.end(), clipFiles[i].second);
			bool on = it != pv.clipFiles.end();
			if (ImGui::Checkbox(clipFiles[i].first.c_str(), &on))
			{
				if (on) pv.clipFiles.push_back(clipFiles[i].second); else pv.clipFiles.erase(it);
				reload = true;
			}
		}
		ImGui::EndCombo();
	}
	if (reload) pv.Load();
	if (ImGui::SmallButton(pv.playing ? "Pause" : "Play")) pv.playing = !pv.playing;
	ImGui::SameLine();
	if (ImGui::SmallButton("Restart")) { pv.boundText.clear(); pv.clock = 0.f; }
	ImGui::SameLine();
	if (pv.view && ImGui::SmallButton("Frame")) pv.view->FrameCamera();
	if (pv.view && pv.view->instance) pv.Step(doc.controller, dt);
	if (!pv.error.empty()) ImGui::TextColored(ImVec4(1.f, 0.6f, 0.4f, 1.f), "%s", pv.error.c_str());
	if (pv.view && pv.view->instance)
	{
		const ImVec2 avail = ImGui::GetContentRegionAvail();
		pv.view->width = std::max(64, (int)avail.x);
		pv.view->height = std::max(64, (int)avail.y);
		// The viewport draws and takes the mouse (orbit, zoom) itself; it is
		// told nothing is selected, so no gizmo or bone highlight comes up.
		pv.blank->selectedBone = -1;
		pv.view->DrawAndUpdate(*pv.blank);
		pv.blank->selectedBone = -1;
	}
	else if (pv.error.empty()) ImGui::TextDisabled("Pick a rig and the clip files its states use.");
	ImGui::EndChild();
}

} // namespace

void DrawWindow(AnimatorDocument& doc, const std::vector<std::pair<std::string, std::string> >& clipFiles,
	const std::vector<std::pair<std::string, std::string> >& meshes, Animator* gameLive, float dt,
	FrameRequests& requests)
{
	// What the graph lights and the parameters drive: a character playing in
	// the scene if there is one, else the window's own preview.
	Animator* live = gameLive;
	if (!live && doc.showPreview && doc.preview && doc.preview->Ready()) live = &doc.preview->animator;
	if (ImGui::Button("Save")) requests.save = true;
	ImGui::SameLine();
	if (ImGui::Button("Frame")) doc.frameRequested = true;
	ImGui::SameLine();
	if (ImGui::Button(doc.showPreview ? "Hide Preview" : "Preview")) doc.showPreview = !doc.showPreview;
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Run the machine on a rig here, without playing a scene:\nthe parameters on the left drive it.");
	ImGui::SameLine();
	if (ImGui::Button("+ State"))
	{
		doc.selectedState = doc.AddState("State", std::string(), -doc.panX, -doc.panY);
		doc.selectedTransition = -1;
	}
	ImGui::SameLine();
	ImGui::TextDisabled("|  %d states, %d transitions, %d parameters", (int)doc.L().states.size(),
		(int)doc.L().transitions.size(), (int)doc.controller.params.size());
	ImGui::SameLine();
	if (live) ImGui::TextColored(ImVec4(0.5f, 1.f, 0.6f, 1.f), "|  %s: %s", gameLive ? "live" : "preview", live->GetState(doc.LayerName()).c_str());
	else ImGui::TextDisabled("|  (press Play to watch a character run it)");
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Right-click the graph to add a state; right-click a state to start a transition, then click\n"
			"where it goes. Drag states to arrange, middle-drag to pan, wheel to zoom. Delete removes the selection.\n"
			"Orange = default state, green = the state a playing character is in.");
	ImGui::Separator();

	// The graph is the point: the side panels give way to it in a small
	// window, and in a narrow one the inspector moves under the graph.
	const float total = ImGui::GetContentRegionAvail().x;
	const bool narrow = total < 650.f;
	const float side = std::max(120.f, std::min(250.f, total * 0.2f));
	const float inspectH = narrow ? std::min(320.f, ImGui::GetContentRegionAvail().y * 0.45f) : 0.f;
	ImGui::BeginChild("##aparams", ImVec2(side, narrow ? -inspectH - 4.f : 0.f), true);
	DrawParams(doc, live);
	ImGui::EndChild();
	ImGui::SameLine();
	const float avail = ImGui::GetContentRegionAvail().x;
	ImGui::BeginChild("##agraph", ImVec2(narrow ? 0.f : std::max(100.f, avail - side - 8.f), narrow ? -inspectH - 4.f : 0.f), false);
	// Layers: the base machine and the ones on part of the skeleton.
	{
		const int count = (int)doc.controller.layers.size();
		for (int l = 0; l <= count; l++)
		{
			if (l) ImGui::SameLine();
			const std::string label = l == 0 ? std::string("Base") : doc.controller.layers[l - 1].name;
			if (ImGui::RadioButton((label + "##layer" + std::to_string(l)).c_str(), doc.activeLayer == l))
			{
				doc.activeLayer = l;
				doc.selectedState.clear();
				doc.selectedTransition = -1;
				doc.linkingFrom.clear();
				doc.frameRequested = true;
			}
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("+ Layer")) doc.AddLayer(std::string(), std::string());
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("A second machine on part of the skeleton - the bones under one root bone -\nover the base: the upper body aims while the legs walk.");
		if (doc.activeLayer > 0 && doc.activeLayer <= count)
		{
			AnimatorLayer& layer = doc.controller.layers[doc.activeLayer - 1];
			ImGui::SameLine();
			ImGui::TextDisabled("|");
			ImGui::SameLine();
			std::string name = layer.name;
			ImGui::SetNextItemWidth(90.f);
			if (ImGui::InputText("##lname", &name, ImGuiInputTextFlags_EnterReturnsTrue) && !name.empty() && doc.controller.FindLayer(name) < 0)
			{
				const std::string before = doc.Snapshot();
				layer.name = name;
				doc.PushEdit(before, "Rename layer");
			}
			ImGui::SameLine();
			std::string root = layer.rootBone;
			ImGui::SetNextItemWidth(120.f);
			if (ImGui::InputTextWithHint("##lroot", "root bone", &root, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				const std::string before = doc.Snapshot();
				layer.rootBone = root;
				doc.PushEdit(before, "Layer root bone");
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("This bone and everything under it is the layer's (e.g. Spine1 for the upper body).");
			ImGui::SameLine();
			if (ImGui::SmallButton("Delete layer")) doc.RemoveLayer(doc.activeLayer);
		}
	}
	DrawGraph(doc, live);
	ImGui::EndChild();
	if (!narrow) ImGui::SameLine();
	ImGui::BeginGroup();
	if (doc.showPreview)
		DrawPreview(doc, clipFiles, meshes, dt, std::max(180.f, ImGui::GetContentRegionAvail().y * 0.5f));
	ImGui::BeginChild("##ainspect", ImVec2(0, 0), true);
	DrawInspector(doc, clipFiles);
	ImGui::EndChild();
	ImGui::EndGroup();
}

} // namespace AnimatorEditor
