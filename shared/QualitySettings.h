//============================================================================
// Name        : QualitySettings.h
// Author      : Duarte Peixinho
// Description : A project's "quality" block, applied to the engine - by the
//               player when a game starts and by the editor when Play is
//               pressed, through this one function.
//============================================================================
//
// The editor's Play applied three of these by hand and the player all of
// them: every setting added since (LOD crossfade, the shadow budget) made the
// game look and run differently from Play in the editor. Both call this now;
// a new setting is added here once.
//
// Only what is the engine's, process-wide. What is the window's - the render
// scale, the upscaler, the frame-rate limit - each applies in its own way
// (the player to its window, the editor to its viewport's preview), and
// "ssaoTemporal", which depends on the anti-aliasing in use, the player decides
// per frame.

#ifndef PYROS_SHARED_QUALITYSETTINGS_H
#define PYROS_SHARED_QUALITYSETTINGS_H

#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/Rendering/Renderer/DeferredRenderer/DeferredRenderer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Utils/Json/json.hpp>
#include <algorithm>

namespace quality {

	// q: the "quality" object of game.json / project.json (anything else does nothing)
	inline void ApplyToEngine(const nlohmann::json &q)
	{
		if (!q.is_object()) return;
		// pixels under which a thing is not drawn
		if (q.contains("smallObjectCull") && q["smallObjectCull"].is_number())
			p3d::IRenderer::SetSmallObjectCull(std::max(0.f, q["smallObjectCull"].get<float>()));
		// the sun's shadow drawn every this many frames
		if (q.contains("shadowUpdateInterval") && q["shadowUpdateInterval"].is_number())
			p3d::IRenderer::SetShadowUpdateInterval((p3d::uint32)std::max(1, q["shadowUpdateInterval"].get<int>()));
		if (q.contains("ssaoHalfResolution") && q["ssaoHalfResolution"].is_boolean())
			p3d::DeferredRenderer::SetSSAOHalfResolution(q["ssaoHalfResolution"].get<bool>());
		// seconds a change of level of detail dissolves over; 0 switches at once
		if (q.contains("lodCrossfade") && q["lodCrossfade"].is_number() && q["lodCrossfade"].get<float>() >= 0.f)
			p3d::RenderingComponent::SetLODCrossfade(q["lodCrossfade"].get<float>());
		// how many point / spot lights may cast shadows in a frame (0: no limit)
		if (q.contains("pointShadowBudget") || q.contains("spotShadowBudget"))
		{
			const int point = q.value("pointShadowBudget", -1), spot = q.value("spotShadowBudget", -1);
			p3d::IRenderer::SetShadowBudget(point >= 0 ? (p3d::uint32)point : p3d::IRenderer::GetPointShadowBudget(),
				spot >= 0 ? (p3d::uint32)spot : p3d::IRenderer::GetSpotShadowBudget());
		}
	}
}

#endif
