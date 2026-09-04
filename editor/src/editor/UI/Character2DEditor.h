//=============================================================================
// Name        : Character2DEditor.h
// Description : ImGui panels for a Character2DDocument - the three stages of
//               building a 2D character, over its own viewport.
//
//               Free functions rather than a class, matching MaterialEditor
//               and AnimationEditor: the state all lives on the document and
//               its Character2DPreview.
//
//               The Animate stage is not a second timeline. It is
//               AnimationEditor's dope sheet, transport and keying, pointed at
//               this character's rig (see AnimationEditorDocument::externalRig)
//               - one implementation of "key a bone", not two.
//=============================================================================

#ifndef CHARACTER2DEDITOR_UI_H
#define CHARACTER2DEDITOR_UI_H

#include <string>
#include <vector>

struct Character2DDocument;

namespace Character2DEditor {

// What the window is asking the host to do after this frame. The host owns
// file dialogs, document lifetime and the project, so the panel only requests.
struct FrameRequests {
	bool save = false;
	bool saveAs = false;
	bool close = false;
	// Set when the user asked to pick artwork for a sprite; the host opens the
	// file dialog and calls back in with the result.
	bool pickTexture = false;
	std::string pickTextureForSprite;
};

// Every texture in the project the sprite picker can offer: display label and
// project-relative path.
struct TextureChoice {
	std::string label;
	std::string relativePath;
};

// Draws the whole document window (already inside Begin/End - the host owns
// the window itself so it can drive docking and the tab's unsaved marker).
// `dt` advances playback.
void DrawWindow(Character2DDocument& doc, const std::vector<TextureChoice>& textures,
	float dt, FrameRequests& requests);

} // namespace Character2DEditor

#endif /* CHARACTER2DEDITOR_UI_H */
