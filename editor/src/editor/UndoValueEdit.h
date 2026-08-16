#pragma once
#include <imgui.h>
#include <functional>

// Wraps one ImGui scalar/vector/color widget with undo commit boundaries,
// mirroring the gizmo-drag baseline/commit pattern already used for
// transform gizmos (the gizmoBaselinePos/Rot/Scale capture in
// HandleViewportGizmoInput / ApplyGizmoTransformToObject). Does NOT change
// how the widget's backing
// storage is read/written each frame - callers keep doing that themselves
// (e.g. SceneEditor::Update() pushing _translation onto the live
// GameObject every frame); this only observes the widget's activate/
// deactivate edges around it and fires `onCommit` exactly once per whole
// drag/type gesture - never per intermediate frame.
//
// Call immediately after the widget, using the widget's current value:
//   static Vec3 baseline; // one per call site, persists across frames
//   ImGui::DragFloat3("Position", (float*)&live, 0.1f);
//   UndoValueEdit(baseline, live, [](const Vec3& before, const Vec3& after){
//       PushSomeCommand(before, after);
//   });
//
// On the activation frame, `baselineInOut` is set to `liveValueNow` - which
// is correct as long as the widget hasn't already applied this same
// frame's edit to its backing storage before this call runs (true for the
// DragFloat/SliderFloat/ColorEdit/Checkbox family this is meant for, since
// those only mutate on drag/click frames, never on the focus-gain frame
// itself unless also dragged that frame, in which case the "gesture" and
// its baseline are the same moment anyway).
template<typename T>
inline void UndoValueEdit(T& baselineInOut, const T& liveValueNow,
	const std::function<void(const T& before, const T& after)>& onCommit)
{
	if (ImGui::IsItemActivated())
		baselineInOut = liveValueNow;
	if (ImGui::IsItemDeactivatedAfterEdit())
		onCommit(baselineInOut, liveValueNow);
}
