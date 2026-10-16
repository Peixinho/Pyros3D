//=============================================================================
// Name        : AnimatorEditor.h
// Description : The .p3dac state machine window: parameters on the left, the
//               graph of states and transitions in the middle, the selected
//               state or transition on the right. While a scene plays, the
//               state the running character is in is lit, and its parameters
//               can be changed from here.
//=============================================================================

#ifndef ANIMATOREDITOR_UI_H
#define ANIMATOREDITOR_UI_H

#include <string>
#include <utility>
#include <vector>

struct AnimatorDocument;
namespace p3d { class Animator; }

namespace AnimatorEditor {

struct FrameRequests {
	bool save = false;
	bool close = false;
};

// `clipFiles`: (label, absolute path) of the project's .p3da / .p3d2d files, for
// the "clips from" picker. `live`: the running Animator using this controller,
// or NULL when nothing plays it.
void DrawWindow(AnimatorDocument& doc, const std::vector<std::pair<std::string, std::string> >& clipFiles,
	p3d::Animator* live, FrameRequests& requests);

} // namespace AnimatorEditor

#endif
