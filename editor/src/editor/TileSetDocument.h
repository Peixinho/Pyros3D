//=============================================================================
// Name        : TileSetDocument.h
// Description : A .p3dt open for editing - a dockable peer window to Scene
//               View, the Material Editor, the Animation Editor and the 2D
//               Character editor.
//
//               The document IS the file: a TileSet2D edited in place and
//               written straight back through SaveTileSet2D. What you edit is
//               what the runtime loads.
//
//               A tile MAP is scene content - it is painted in the scene, in
//               the level it belongs to. A tile SET is an asset: it exists on
//               its own, several maps and several scenes share one, and
//               changing which cells are solid changes every level built on
//               it. That is why this is a document and painting is not, and it
//               is the same split Character2DAsset draws between building a
//               character and placing one.
//
//               Pure data + file I/O + edit operations. Nothing here touches
//               ImGui; the panels live in UI/TileSetEditor.
//=============================================================================

#ifndef TILESETDOCUMENT_H
#define TILESETDOCUMENT_H

#include "UndoStack.h"
#include <Pyros3D/Assets/TileSet2D/TileSet2D.h>
#include <cstdint>
#include <string>
#include <vector>

struct TileSetDocument {

	uint32_t id = 0;
	// Absolute path of the .p3dt. Never empty in practice - a tileset is
	// always created from an image by "Create Tile Set", so it has a home
	// before it is ever opened.
	std::string absolutePath;
	std::string displayName = "TileSet";
	bool dirty = false;

	// The file's contents.
	p3d::TileSet2D set;
	// Project root, so the atlas can be resolved for display and the image
	// path can stay project-relative in the file.
	std::string projectRoot;
	// Absolute path of the atlas, resolved once on load.
	std::string atlasPath;

	UndoStack undo;

	bool LoadFromFile(const std::string& absPath, std::string& errOut);
	bool SaveAs(const std::string& absPath, std::string& errOut);

	// --- edits (each pushes one undo entry) -------------------------------
	// Whether a cell collides. The one thing a tileset says that a scene
	// cannot: marking a cell solid changes the colliders of every map built
	// on this set, everywhere.
	void SetSolid(p3d::int32 index, bool solid);
	// Flips the whole selection at once, so marking a floor row is one entry
	// rather than twenty.
	void SetSolidRange(const std::vector<p3d::int32>& indices, bool solid);
	void SetTag(p3d::int32 index, const std::string& tag, bool on);
	// Cell size / margin / spacing. Re-cutting the sheet, which changes what
	// every index means - hence one undo entry for the lot.
	void SetGrid(p3d::int32 tileW, p3d::int32 tileH, p3d::int32 margin,
		p3d::int32 spacing, p3d::int32 columns);

	// Every tag used anywhere in the set, for the editor's tag row.
	std::vector<std::string> AllTags() const;
	int SolidCount() const;

private:
	// Undo for a tileset is a whole-set snapshot through the FILE serializer,
	// the same guarantee Character2DDocument takes: an undo can never restore
	// something the format cannot express. A tileset is a few hundred bytes,
	// so the cost of that guarantee is nothing.
	void PushEdit(const std::string& before, const char* what);
};

#endif /* TILESETDOCUMENT_H */
