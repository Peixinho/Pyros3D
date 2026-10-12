//=============================================================================
// Name        : SpriteSheetDocument.h
// Description : A .p3ds open for editing - a dockable peer window to Scene
//               View, the Tile Set editor and the rest.
//
//               The document IS the file: a SpriteSheet2D edited in place and
//               written straight back through SaveSpriteSheet2D. What you
//               edit is what a Sprite2D loads.
//
//               A sprite is scene content; the sheet it draws from is an
//               asset. Where its frames are cut, where each one's pivot is,
//               which runs of frames are clips - all of that is shared by
//               every sprite on the sheet in every scene, which is why it is
//               edited here and not on an object.
//
//               Pure data + file I/O + edit operations. Nothing here touches
//               ImGui; the panels live in UI/SpriteSheetEditor.
//=============================================================================

#ifndef SPRITESHEETDOCUMENT_H
#define SPRITESHEETDOCUMENT_H

#include "UndoStack.h"
#include <Pyros3D/Assets/SpriteSheet2D/SpriteSheet2D.h>
#include <cstdint>
#include <string>
#include <vector>

struct SpriteSheetDocument {

	uint32_t id = 0;
	std::string absolutePath;
	std::string displayName = "SpriteSheet";
	bool dirty = false;

	// The file's contents.
	p3d::SpriteSheet2D sheet;
	// Project root, so the image can be resolved for display and its path can
	// stay project-relative in the file.
	std::string projectRoot;
	// Absolute path of the image, resolved once on load.
	std::string imagePath;

	UndoStack undo;

	// What the panel is looking at. On the document rather than in the
	// panel's statics so two sheets open side by side each keep their own.
	struct View {
		std::vector<p3d::int32> selection;   // frames, in the order picked
		int anchor = -1;
		int clip = -1;
		float zoom = 2.f;
		bool fitPending = true;
		int tool = 0;                        // 0 select, 1 draw a frame
		bool dragging = false;
		float dragX0 = 0.f, dragY0 = 0.f;
		// A handle of the selected frame held by the mouse: 0-8 are the
		// corners and sides (row * 3 + column, top-left first), 4 the frame
		// itself (moves it), 9 its pivot. -1 for none.
		int grab = -1;
		float grabX = 0.f, grabY = 0.f;      // image pixel the drag started at
		int grabRect[4] = { 0, 0, 0, 0 };    // the frame as it was then
		// Slice settings, remembered between presses of the button.
		int sliceMode = 0;                   // 0 by cell size, 1 by count, 2 automatic
		int cellW = 32, cellH = 32, margin = 0, spacing = 0;
		int columns = 4, rows = 1;
		int alphaThreshold = 8, mergeDistance = 1, minSize = 2;
		std::string prefix;
		std::string newClipName = "clip";
		std::string atlasPath;
		std::string framesText;              // the selected clip's frames, being typed
		int framesTextFor = -2;
		// Preview playback of the selected clip.
		bool playing = true;
		float clipTime = 0.f;
		double lastTick = 0.0;
	} view;

	bool LoadFromFile(const std::string& absPath, std::string& errOut);
	bool SaveAs(const std::string& absPath, std::string& errOut);

	// --- edits, each one undo entry ---------------------------------------

	void SetPixelsPerUnit(float ppu);
	void SetFilter(int filter);
	// Project-relative, or empty for none.
	void SetNormalImage(const std::string& path);

	// Re-cutting replaces every frame and drops the clips, whose indices
	// meant the old ones. Returns how many frames the cut made; 0 leaves the
	// sheet as it was.
	int SliceGrid(int cellW, int cellH, int margin, int spacing, const std::string& prefix);
	int SliceCount(int columns, int rows, const std::string& prefix);
	// Round the opaque islands of the image. False when the image cannot be read.
	bool AutoSlice(int alphaThreshold, int merge, int minSize, const std::string& prefix, int& made);
	// An Aseprite / TexturePacker JSON, already read into `text`.
	bool ImportAtlas(const std::string& text, std::string& errOut);

	// Returns the new frame's index, or -1 for a rectangle with no area or
	// one that leaves the image.
	int AddFrame(int x, int y, int w, int h, const std::string& name);
	// Clips keep the frames that remain, renumbered.
	void RemoveFrames(const std::vector<p3d::int32>& indices);
	// Replaces one frame's description wholesale (name made unique).
	bool SetFrame(p3d::int32 index, const p3d::SpriteFrame2D& f);
	void SetPivot(const std::vector<p3d::int32>& indices, const p3d::Math::Vec2& pivot);
	void SetBorder(const std::vector<p3d::int32>& indices, const p3d::Math::Vec4& border);

	int AddClip(const std::string& name, const std::vector<p3d::int32>& frames, float fps, bool loop);
	void RemoveClip(int index);
	bool SetClip(int index, const p3d::SpriteClip2D& clip);

	// For edits made live by a dragged control: take the snapshot when the
	// drag starts, push one entry when it ends.
	void BeginEdit();
	void EndEdit(const char* what);

	// A name no other frame (or clip) has, derived from `wanted`.
	std::string UniqueFrameName(const std::string& wanted, p3d::int32 except = -1) const;
	std::string UniqueClipName(const std::string& wanted, int except = -1) const;

private:
	void PushEdit(const std::string& before, const char* what);
	std::string pendingBefore;
};

#endif /* SPRITESHEETDOCUMENT_H */
