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
// Only what is the engine's, process-wide. What is the window's - the
// frame-rate limit - each applies in its own way.

#ifndef PYROS_SHARED_QUALITYSETTINGS_H
#define PYROS_SHARED_QUALITYSETTINGS_H

#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Utils/Json/json.hpp>
#include <algorithm>

namespace quality {

	// q: the "quality" object of game.json / project.json (anything else does nothing)
	inline void ApplyToEngine(const nlohmann::json &q)
	{
		if (!q.is_object()) return;
		// seconds a change of level of detail dissolves over; 0 switches at once
		if (q.contains("lodCrossfade") && q["lodCrossfade"].is_number() && q["lodCrossfade"].get<float>() >= 0.f)
			p3d::RenderingComponent::SetLODCrossfade(q["lodCrossfade"].get<float>());
	}
}

#endif
