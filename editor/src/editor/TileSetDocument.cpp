//=============================================================================
// Name        : TileSetDocument.cpp
// Description : See the header.
//=============================================================================

#include "TileSetDocument.h"
#include <Pyros3D/Core/Logs/Log.h>
#include <algorithm>
#include <filesystem>
#include <set>

using namespace p3d;

namespace {

	// Undo as a pair of serialized snapshots. See PushEdit's comment.
	class TileSetEditCommand : public IUndoableCommand {
	public:
		TileSetEditCommand(TileSetDocument* doc, const std::string& before,
			const std::string& after, const std::string& what)
			: doc_(doc), before_(before), after_(after), what_(what) {}

		void Undo() override { Apply(before_); }
		void Redo() override { Apply(after_); }
		std::string Description() const override { return what_; }
		size_t MemoryCost() const override
		{
			return sizeof(*this) + before_.capacity() + after_.capacity() + what_.capacity();
		}

	private:
		void Apply(const std::string& text)
		{
			TileSet2D restored;
			std::string err;
			if (!TileSet2DFromString(text, restored, &err)) return;
			// The image size is not in the file (the PNG is the truth about
			// its own dimensions), so carry the resolved one across rather
			// than dropping back to "unknown" and blanking every UV.
			const int32 w = doc_->set.GetImageWidth();
			const int32 h = doc_->set.GetImageHeight();
			doc_->set = restored;
			doc_->set.SetImageSize(w, h);
			doc_->dirty = true;
		}

		TileSetDocument* doc_;
		std::string before_, after_, what_;
	};

}

bool TileSetDocument::LoadFromFile(const std::string& absPath, std::string& errOut)
{
	if (!LoadTileSet2D(absPath, set, &errOut)) return false;

	absolutePath = absPath;
	displayName = std::filesystem::path(absPath).stem().string();

	// Resolve the atlas and read its real pixel size. Without it every UV in
	// the set is a zero rect and the grid below draws nothing - which looks
	// like a broken editor rather than a missing image.
	atlasPath.clear();
	if (!set.image.empty())
	{
		std::filesystem::path p = std::filesystem::path(projectRoot) / set.image;
		if (std::filesystem::exists(p)) atlasPath = p.string();
		else
		{
			// Fall back to beside the .p3dt, which is where "Create Tile Set"
			// puts the pair.
			std::filesystem::path beside =
				std::filesystem::path(absPath).parent_path() / std::filesystem::path(set.image).filename();
			if (std::filesystem::exists(beside)) atlasPath = beside.string();
		}
	}
	int32 iw = 0, ih = 0;
	if (!atlasPath.empty() && TileSet2DReadImageSize(atlasPath, iw, ih))
		set.SetImageSize(iw, ih);
	else
		echo("WARNING: tileset atlas not found: " + set.image);

	dirty = false;
	undo.Clear();
	return true;
}

bool TileSetDocument::SaveAs(const std::string& absPath, std::string& errOut)
{
	if (!SaveTileSet2D(absPath, set, &errOut)) return false;
	absolutePath = absPath;
	displayName = std::filesystem::path(absPath).stem().string();
	dirty = false;
	return true;
}

void TileSetDocument::PushEdit(const std::string& before, const char* what)
{
	dirty = true;
	undo.Push(std::make_unique<TileSetEditCommand>(
		this, before, TileSet2DToString(set), what ? what : "Edit Tile Set"));
}

void TileSetDocument::SetSolid(const int32 index, const bool solid)
{
	if (index < 0) return;
	if (set.IsSolid(index) == solid) return;
	const std::string before = TileSet2DToString(set);
	set.tiles[index].solid = solid;
	PushEdit(before, solid ? "Mark Tile Solid" : "Mark Tile Passable");
}

void TileSetDocument::SetSolidRange(const std::vector<int32>& indices, const bool solid)
{
	bool any = false;
	for (size_t i = 0; i < indices.size(); i++)
		if (indices[i] >= 0 && set.IsSolid(indices[i]) != solid) { any = true; break; }
	if (!any) return;

	const std::string before = TileSet2DToString(set);
	for (size_t i = 0; i < indices.size(); i++)
		if (indices[i] >= 0) set.tiles[indices[i]].solid = solid;
	PushEdit(before, solid ? "Mark Tiles Solid" : "Mark Tiles Passable");
}

void TileSetDocument::SetShapeRange(const std::vector<int32>& indices, const int32 shape)
{
	bool any = false;
	for (size_t i = 0; i < indices.size(); i++)
		if (indices[i] >= 0
			&& (set.Shape(indices[i]) != shape
				|| (shape != TileShape2D::Box && !set.IsSolid(indices[i]))))
		{ any = true; break; }
	if (!any) return;

	const std::string before = TileSet2DToString(set);
	for (size_t i = 0; i < indices.size(); i++)
	{
		if (indices[i] < 0) continue;
		set.tiles[indices[i]].shape = shape;
		// A slope that is not solid is a slope nothing can stand on. Picking
		// the shape is the author saying "this collides like this", so the
		// solid flag follows rather than being a second thing to remember.
		if (shape != TileShape2D::Box) set.tiles[indices[i]].solid = true;
	}
	PushEdit(before, shape == TileShape2D::Box ? "Set Tiles Square" : "Set Tile Slope");
}

void TileSetDocument::SetTag(const int32 index, const std::string& tag, const bool on)
{
	if (index < 0 || tag.empty()) return;
	std::vector<std::string>& tags = set.tiles[index].tags;
	const bool has = std::find(tags.begin(), tags.end(), tag) != tags.end();
	if (has == on) return;

	const std::string before = TileSet2DToString(set);
	if (on) tags.push_back(tag);
	else tags.erase(std::remove(tags.begin(), tags.end(), tag), tags.end());
	PushEdit(before, on ? "Tag Tile" : "Untag Tile");
}

void TileSetDocument::SetGrid(const int32 tileW, const int32 tileH, const int32 margin,
	const int32 spacing, const int32 columns)
{
	if (tileW < 1 || tileH < 1 || margin < 0 || spacing < 0 || columns < 0) return;
	if (set.tileW == tileW && set.tileH == tileH && set.margin == margin
		&& set.spacing == spacing && set.columns == columns) return;

	const std::string before = TileSet2DToString(set);
	set.tileW = tileW;
	set.tileH = tileH;
	set.margin = margin;
	set.spacing = spacing;
	set.columns = columns;
	PushEdit(before, "Re-cut Tile Set");
}

std::vector<std::string> TileSetDocument::AllTags() const
{
	std::set<std::string> seen;
	for (std::map<int32, TileInfo2D>::const_iterator i = set.tiles.begin(); i != set.tiles.end(); ++i)
		for (size_t k = 0; k < i->second.tags.size(); k++) seen.insert(i->second.tags[k]);
	return std::vector<std::string>(seen.begin(), seen.end());
}

int TileSetDocument::SolidCount() const
{
	int n = 0;
	for (int32 i = 0; i < set.TileCount(); i++) if (set.IsSolid(i)) n++;
	return n;
}
