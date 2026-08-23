#pragma once
//============================================================================
// Name        : EasingPreview.h
// Description : Read-only thumbnail of an easing curve, plus the combo that
//               picks one. Both the animation key popup and the particle
//               inspector draw these, and both evaluate p3d::Ease() to do it -
//               so the picture cannot drift away from what actually plays.
//               That is the whole reason this is shared rather than two
//               local helpers: a preview redrawn from a second copy of the
//               formulas is worse than no preview at all.
//============================================================================

#include <Pyros3D/Core/Math/Easing.h>
#include <imgui.h>
#include <string>

namespace EasingUI {

	// Curve thumbnail. x is the normalised span parameter, y the value it
	// maps to - so the straight diagonal IS linear, and any bow away from it
	// is the easing. The faint diagonal stays drawn underneath as the
	// reference to read that bow against.
	inline void Curve(const unsigned char mode, const float outTangent = 1.f,
		const float inTangent = 1.f, const ImVec2 &size = ImVec2(120.f, 52.f))
	{
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec2 p0 = ImGui::GetCursorScreenPos();
		const ImVec2 p1(p0.x + size.x, p0.y + size.y);

		dl->AddRectFilled(p0, p1, IM_COL32(22, 24, 30, 255), 3.f);
		dl->AddRect(p0, p1, IM_COL32(70, 75, 90, 255), 3.f);

		const float pad = 6.f;
		const float w = size.x - pad * 2.f, h = size.y - pad * 2.f;
		const ImVec2 a(p0.x + pad, p1.y - pad);          // (t=0, v=0)
		const ImVec2 b(p1.x - pad, p0.y + pad);          // (t=1, v=1)
		dl->AddLine(a, b, IM_COL32(70, 75, 90, 160), 1.f);

		// Sampled rather than drawn analytically: Ease() is the authority,
		// and sampling it is what makes that true of the picture too.
		const int kSamples = 48;
		ImVec2 prev(a.x, a.y);
		for (int i = 1; i <= kSamples; i++)
		{
			const float t = (float)i / (float)kSamples;
			float v = p3d::Ease(t, mode, outTangent, inTangent);
			// Bezier tangents can push the curve outside [0,1]; clamp only
			// the DRAWING so an overshoot stays inside the box instead of
			// scribbling over the panel.
			if (v < -0.25f) v = -0.25f;
			if (v > 1.25f) v = 1.25f;
			const ImVec2 cur(p0.x + pad + t * w, p1.y - pad - v * h);
			dl->AddLine(prev, cur, IM_COL32(120, 200, 255, 255), 1.8f);
			prev = cur;
		}

		// Step holds at zero and then jumps, which as a flat line alone is
		// indistinguishable from a bug - the riser says "this is deliberate".
		if (mode == p3d::INTERP_STEP)
			dl->AddLine(ImVec2(b.x, a.y), b, IM_COL32(120, 200, 255, 120), 1.5f);

		ImGui::Dummy(size);
	}

	// Mode combo + thumbnail. Returns true on the frame the mode changes;
	// `outMode` is updated in place. `allowBezier` is false for ramps with
	// only two ends (particles), where there is no next key to take an
	// incoming tangent from.
	inline bool Picker(const char* label, unsigned char &outMode, const bool allowBezier = true)
	{
		const unsigned count = allowBezier
			? p3d::kInterpolationModeCount : p3d::kInterpolationModeCount - 1;
		bool changed = false;
		ImGui::SetNextItemWidth(140.f);
		if (ImGui::BeginCombo(label, p3d::InterpolationModeName(outMode)))
		{
			for (unsigned m = 0; m < count; m++)
			{
				const bool sel = (outMode == (unsigned char)m);
				if (ImGui::Selectable(p3d::InterpolationModeName((unsigned char)m), sel))
				{
					outMode = (unsigned char)m;
					changed = true;
				}
				if (sel) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		return changed;
	}

}
