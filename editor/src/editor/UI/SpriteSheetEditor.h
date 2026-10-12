//=============================================================================
// Name        : SpriteSheetEditor.h
// Description : ImGui panels for a SpriteSheetDocument - the image, the
//               frames cut from it, their pivots and nine-slice borders, and
//               the clips.
//
//               Free functions rather than a class, matching TileSetEditor
//               and the rest: the state lives on the document.
//=============================================================================

#ifndef SPRITESHEETEDITOR_UI_H
#define SPRITESHEETEDITOR_UI_H

struct SpriteSheetDocument;

namespace SpriteSheetEditor {

// What the window is asking the host to do after this frame.
struct FrameRequests {
	bool save = false;
	bool close = false;
};

// Draws the whole document window (already inside Begin/End - the host owns
// the window). `imageTexId` is the sheet's image uploaded for ImGui, 0 when
// it could not be loaded.
void DrawWindow(SpriteSheetDocument& doc, void* imageTexId, FrameRequests& requests);

} // namespace SpriteSheetEditor

#endif /* SPRITESHEETEDITOR_UI_H */
