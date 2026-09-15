//=============================================================================
// Name        : TileSetEditor.h
// Description : ImGui panels for a TileSetDocument - the sheet, which cells
//               collide, and what they are called.
//
//               Free functions rather than a class, matching MaterialEditor,
//               AnimationEditor and Character2DEditor: the state lives on the
//               document.
//=============================================================================

#ifndef TILESETEDITOR_UI_H
#define TILESETEDITOR_UI_H

struct TileSetDocument;

namespace TileSetEditor {

// What the window is asking the host to do after this frame. The host owns
// document lifetime and the project, so the panel only requests.
struct FrameRequests {
	bool save = false;
	bool close = false;
};

// Draws the whole document window (already inside Begin/End - the host owns
// the window so it can drive docking and the unsaved marker).
// `atlasTexId` is the sheet uploaded for ImGui, 0 when it could not be loaded.
void DrawWindow(TileSetDocument& doc, void* atlasTexId, FrameRequests& requests);

} // namespace TileSetEditor

#endif /* TILESETEDITOR_UI_H */
