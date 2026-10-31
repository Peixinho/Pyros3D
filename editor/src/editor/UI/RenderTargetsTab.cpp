//============================================================================
// Name        : RenderTargetsTab.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See RenderTargetsTab.h
//============================================================================

#include "RenderTargetsTab.h"
#include <Pyros3D/Rendering/Renderer/ImGuiRenderer.h>
#include <imgui.h>

RenderTargetsTab::RenderTargetsTab(const std::string &name, bool* open) : Name(name), Open(open), thumbSize(200.f) {}

void RenderTargetsTab::Show()
{
	if (!*Open) return;
	ImGui::SetNextWindowSize(ImVec2(560, 620), ImGuiCond_FirstUseEver);
	if (ImGui::Begin(Name.c_str(), Open))
	{
		p3d::SceneRenderer* renderer = source ? source() : nullptr;
		if (!renderer || !renderer->HasDrawn())
			ImGui::TextDisabled("Nothing drawn yet - the Scene View's renderer shows its targets here.");
		else
		{
			ImGui::SetNextItemWidth(200.f);
			ImGui::SliderFloat("Size", &thumbSize, 64.f, 512.f, "%.0f px");
			const std::vector<p3d::SceneRenderer::DebugTarget> targets = renderer->DebugTargets();
			for (size_t k = 0; k < targets.size(); k++)
			{
				const p3d::SceneRenderer::DebugTarget &t = targets[k];
				ImGui::Text("%s  %ux%u", t.name, t.width, t.height);
				const float aspect = t.height ? (float)t.width / (float)t.height : 1.f;
				ImGui::Image((ImTextureID)p3d::ImGuiRenderer::Id(t.texture), ImVec2(thumbSize * aspect, thumbSize));
			}
		}
	}
	ImGui::End();
}
