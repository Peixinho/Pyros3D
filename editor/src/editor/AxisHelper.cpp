//============================================================================
// Name        : AxisHelper.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Axis Helper
//============================================================================

#include "AxisHelper.h"
#include <algorithm>

using namespace p3d;

void AxisHelper::Draw(ImDrawList* drawList, const ImVec2 &topLeft, const f32 size, GameObject* camera, const bool isPerspective)
{
	drawn = false;
	if (!drawList || !camera) return;
	// The world's axes in the camera's space: its view matrix's rotation
	const Matrix view = camera->GetWorldTransformation().Inverse();
	const ImVec2 centre(topLeft.x + size * 0.5f, topLeft.y + size * 0.5f);
	const f32 reach = size * 0.36f, dot = size * 0.1f;
	const Vec3 axes[3] = { Vec3(1.f, 0.f, 0.f), Vec3(0.f, 1.f, 0.f), Vec3(0.f, 0.f, 1.f) };
	const ImU32 colours[3] = { IM_COL32(230, 70, 80, 255), IM_COL32(120, 200, 60, 255), IM_COL32(70, 130, 240, 255) };
	const char* names[3] = { "X", "Y", "Z" };
	const ImVec2 mouse = ImGui::GetMousePos();

	struct Item { int axis; int which; f32 depth; ImVec2 at; bool positive; };
	std::vector<Item> items;
	for (int a = 0; a < 3; a++)
	{
		// (rotation only: the axis's direction as seen; y up on the screen)
		const Vec3 d = view * axes[a] - view * Vec3(0.f, 0.f, 0.f);
		for (int sign = 0; sign < 2; sign++)
		{
			const f32 k = sign == 0 ? 1.f : -1.f;
			Item it;
			it.axis = a; it.positive = sign == 0;
			it.which = 1 + a * 2 + sign;
			it.depth = d.z * k;
			it.at = ImVec2(centre.x + d.x * k * reach, centre.y - d.y * k * reach);
			items.push_back(it);
		}
	}
	// (the far ones first: the near ones are drawn over them)
	std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) { return a.depth < b.depth; });

	drawList->AddCircleFilled(centre, size * 0.5f, IM_COL32(20, 20, 24, 90), 32);
	int hovered = -1;
	for (int i = (int)items.size() - 1; i >= 0 && hovered < 0; i--)
	{
		const ImVec2 dlt(mouse.x - items[i].at.x, mouse.y - items[i].at.y);
		if (dlt.x * dlt.x + dlt.y * dlt.y <= dot * dot) hovered = items[i].which;
	}
	{
		const ImVec2 dlt(mouse.x - centre.x, mouse.y - centre.y);
		if (hovered < 0 && dlt.x * dlt.x + dlt.y * dlt.y <= (dot * 0.8f) * (dot * 0.8f)) hovered = AXIS_HELPER_AXIS::CENTER;
	}
	for (const Item &it : items)
	{
		const ImU32 c = colours[it.axis];
		if (it.positive) drawList->AddLine(centre, it.at, c, 2.f);
		const bool hot = hovered == it.which;
		if (it.positive)
		{
			drawList->AddCircleFilled(it.at, dot * (hot ? 1.15f : 1.f), hot ? IM_COL32(255, 255, 255, 255) : c, 20);
			const ImVec2 ts = ImGui::CalcTextSize(names[it.axis]);
			drawList->AddText(ImVec2(it.at.x - ts.x * 0.5f, it.at.y - ts.y * 0.5f), IM_COL32(15, 15, 18, 255), names[it.axis]);
		}
		else
		{
			drawList->AddCircleFilled(it.at, dot * 0.8f, (c & 0x00FFFFFF) | ((ImU32)(hot ? 200 : 90) << 24), 20);
			drawList->AddCircle(it.at, dot * 0.8f, c, 20, 1.5f);
		}
		targets[it.which] = it.at;
		radii[it.which] = it.positive ? dot : dot * 0.8f;
	}
	drawList->AddCircleFilled(centre, dot * 0.55f, hovered == AXIS_HELPER_AXIS::CENTER ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 200, 205, 200), 16);
	targets[AXIS_HELPER_AXIS::CENTER] = centre;
	radii[AXIS_HELPER_AXIS::CENTER] = dot * 0.8f;
	// (which projection the view is drawn with, under it)
	const char* label = isPerspective ? "Persp" : "Ortho";
	const ImVec2 ls = ImGui::CalcTextSize(label);
	drawList->AddText(ImVec2(centre.x - ls.x * 0.5f, topLeft.y + size + 2.f), IM_COL32(220, 220, 225, 200), label);
	drawn = true;
}

int32 AxisHelper::MouseClick() const
{
	if (!drawn) return -1;
	const ImVec2 mouse = ImGui::GetMousePos();
	int32 best = -1;
	f32 bestD = 1e30f;
	for (int i = 0; i < 7; i++)
	{
		const f32 dx = mouse.x - targets[i].x, dy = mouse.y - targets[i].y, d = dx * dx + dy * dy;
		if (d <= radii[i] * radii[i] && d < bestD) { best = i; bestD = d; }
	}
	return best;
}
