//============================================================================
// Name        : AxisHelper.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Axis Helper - the viewport's orientation widget: the world's
//               axes as the camera sees them, drawn by ImGui over the
//               viewport's corner. Clicking an axis looks down it; the centre
//               goes back to the perspective view.
//============================================================================

#ifndef AXISHELPER_H
#define	AXISHELPER_H

#include <imgui.h>
#include <Pyros3D/GameObjects/GameObject.h>
using namespace p3d;

namespace AXIS_HELPER_AXIS {
	enum {
		CENTER = 0,
		POSITIVE_X,
		NEGATIVE_X,
		POSITIVE_Y,
		NEGATIVE_Y,
		POSITIVE_Z,
		NEGATIVE_Z
	};
};

class AxisHelper {
public:

	AxisHelper() {}
	virtual ~AxisHelper() {}

	// Draws it in the square at `topLeft`, `size` points a side, as `camera` sees the world
	void Draw(ImDrawList* drawList, const ImVec2 &topLeft, const f32 size, GameObject* camera, const bool isPerspective);
	// What is under the mouse as it was last drawn (AXIS_HELPER_AXIS, -1: nothing)
	int32 MouseClick() const;

private:

	// Where each was last drawn, and how big - AXIS_HELPER_AXIS order
	ImVec2 targets[7];
	f32 radii[7] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
	bool drawn = false;
};

#endif	/* AXISHELPER_H */
