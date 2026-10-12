//=============================================================================
// Name        : SpriteSheetDocument.cpp
// Description : See the header.
//=============================================================================

#include "SpriteSheetDocument.h"
#include <Pyros3D/Assets/TileSet2D/TileSet2D.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <algorithm>
#include <filesystem>
#include <set>

using namespace p3d;

namespace {

	// Undo as a pair of serialized snapshots, through the FILE serializer -
	// the guarantee TileSetDocument takes: an undo can never restore
	// something the format cannot express.
	class SpriteSheetEditCommand : public IUndoableCommand {
	public:
		SpriteSheetEditCommand(SpriteSheetDocument* doc, const std::string& before,
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
			SpriteSheet2D restored;
			std::string err;
			if (!SpriteSheet2DFromString(text, restored, &err)) return;
			// The image size is not in the file - carry the resolved one
			// across rather than blanking every UV.
			restored.SetImageSize(doc_->sheet.GetImageWidth(), doc_->sheet.GetImageHeight());
			doc_->sheet = restored;
			doc_->dirty = true;
			// What was selected may no longer exist.
			std::vector<int32> kept;
			for (size_t i = 0; i < doc_->view.selection.size(); i++)
				if (doc_->sheet.ValidFrame(doc_->view.selection[i])) kept.push_back(doc_->view.selection[i]);
			doc_->view.selection = kept;
			if (doc_->view.clip >= (int)doc_->sheet.clips.size()) doc_->view.clip = -1;
			doc_->view.framesTextFor = -2;
		}

		SpriteSheetDocument* doc_;
		std::string before_, after_, what_;
	};

}

bool SpriteSheetDocument::LoadFromFile(const std::string& absPath, std::string& errOut)
{
	if (!LoadSpriteSheet2D(absPath, sheet, &errOut)) return false;

	absolutePath = absPath;
	displayName = std::filesystem::path(absPath).stem().string();

	imagePath.clear();
	if (!sheet.image.empty())
	{
		std::filesystem::path p = std::filesystem::path(projectRoot) / sheet.image;
		if (std::filesystem::exists(p)) imagePath = p.string();
		else
		{
			// Beside the .p3ds, which is where a sheet and its image are
			// written together.
			std::filesystem::path beside =
				std::filesystem::path(absPath).parent_path() / std::filesystem::path(sheet.image).filename();
			if (std::filesystem::exists(beside)) imagePath = beside.string();
		}
	}
	int32 iw = 0, ih = 0;
	if (!imagePath.empty() && TileSet2DReadImageSize(imagePath, iw, ih))
		sheet.SetImageSize(iw, ih);
	else
		echo("WARNING: sprite sheet image not found: " + sheet.image);

	if (view.prefix.empty()) view.prefix = displayName + "_";
	dirty = false;
	undo.Clear();
	return true;
}

bool SpriteSheetDocument::SaveAs(const std::string& absPath, std::string& errOut)
{
	if (!SaveSpriteSheet2D(absPath, sheet, &errOut)) return false;
	absolutePath = absPath;
	displayName = std::filesystem::path(absPath).stem().string();
	dirty = false;
	return true;
}

void SpriteSheetDocument::PushEdit(const std::string& before, const char* what)
{
	dirty = true;
	view.framesTextFor = -2;
	undo.Push(std::make_unique<SpriteSheetEditCommand>(
		this, before, SpriteSheet2DToString(sheet), what ? what : "Edit Sprite Sheet"));
}

void SpriteSheetDocument::BeginEdit()
{
	pendingBefore = SpriteSheet2DToString(sheet);
}

void SpriteSheetDocument::EndEdit(const char* what)
{
	if (pendingBefore.empty()) return;
	const std::string before = pendingBefore;
	pendingBefore.clear();
	if (before == SpriteSheet2DToString(sheet)) return;
	PushEdit(before, what);
}

std::string SpriteSheetDocument::UniqueFrameName(const std::string& wanted, const int32 except) const
{
	std::string base = wanted.empty() ? std::string("frame") : wanted;
	std::string name = base;
	for (int n = 2; ; n++)
	{
		bool taken = false;
		for (size_t i = 0; i < sheet.frames.size() && !taken; i++)
			taken = ((int32)i != except && sheet.frames[i].name == name);
		if (!taken) return name;
		name = base + "_" + std::to_string(n);
	}
}

std::string SpriteSheetDocument::UniqueClipName(const std::string& wanted, const int except) const
{
	std::string base = wanted.empty() ? std::string("clip") : wanted;
	std::string name = base;
	for (int n = 2; ; n++)
	{
		bool taken = false;
		for (size_t i = 0; i < sheet.clips.size() && !taken; i++)
			taken = ((int)i != except && sheet.clips[i].name == name);
		if (!taken) return name;
		name = base + "_" + std::to_string(n);
	}
}

void SpriteSheetDocument::SetPixelsPerUnit(const float ppu)
{
	if (!(ppu > 0.f) || ppu == sheet.pixelsPerUnit) return;
	const std::string before = SpriteSheet2DToString(sheet);
	sheet.pixelsPerUnit = ppu;
	PushEdit(before, "Set Pixels Per Unit");
}

void SpriteSheetDocument::SetFilter(const int filter)
{
	const int f = filter == SpriteFilter2D::Nearest ? SpriteFilter2D::Nearest : SpriteFilter2D::Linear;
	if (f == sheet.filter) return;
	const std::string before = SpriteSheet2DToString(sheet);
	sheet.filter = f;
	PushEdit(before, "Set Sprite Filter");
}

void SpriteSheetDocument::SetNormalImage(const std::string& path)
{
	if (path == sheet.normalImage) return;
	const std::string before = SpriteSheet2DToString(sheet);
	sheet.normalImage = path;
	PushEdit(before, "Set Normal Map");
}

int SpriteSheetDocument::SliceGrid(const int cellW, const int cellH, const int margin,
	const int spacing, const std::string& prefix)
{
	const std::string before = SpriteSheet2DToString(sheet);
	SpriteSheet2D cut = sheet;
	const int n = cut.SliceGrid(cellW, cellH, margin, spacing, prefix.empty() ? std::string("frame") : prefix);
	if (n < 1) return 0;
	sheet = cut;
	view.selection.clear();
	view.clip = -1;
	PushEdit(before, "Slice Sprite Sheet");
	return n;
}

int SpriteSheetDocument::SliceCount(const int columns, const int rows, const std::string& prefix)
{
	if (columns < 1 || rows < 1 || !sheet.HaveImageSize()) return 0;
	return SliceGrid(sheet.GetImageWidth() / columns, sheet.GetImageHeight() / rows, 0, 0, prefix);
}

bool SpriteSheetDocument::AutoSlice(const int alphaThreshold, const int merge, const int minSize,
	const std::string& prefix, int& made)
{
	made = 0;
	std::vector<SpriteRect2D> rects;
	if (imagePath.empty() || !SpriteSheet2DAutoSliceFile(imagePath, rects, alphaThreshold, merge, minSize))
		return false;
	if (rects.empty()) return true;

	const std::string before = SpriteSheet2DToString(sheet);
	sheet.frames.clear();
	sheet.clips.clear();
	const std::string pre = prefix.empty() ? std::string("frame") : prefix;
	for (size_t i = 0; i < rects.size(); i++)
	{
		SpriteFrame2D f;
		f.name = pre + std::to_string(i);
		f.x = rects[i].x; f.y = rects[i].y; f.w = rects[i].w; f.h = rects[i].h;
		sheet.frames.push_back(f);
	}
	made = (int)rects.size();
	view.selection.clear();
	view.clip = -1;
	PushEdit(before, "Auto Slice Sprite Sheet");
	return true;
}

bool SpriteSheetDocument::ImportAtlas(const std::string& text, std::string& errOut)
{
	SpriteSheet2D in;
	in.pixelsPerUnit = sheet.pixelsPerUnit;
	in.filter = sheet.filter;
	if (!ImportSpriteAtlasJson(text, in, &errOut)) return false;
	// The atlas names its image by file name only; this sheet already knows
	// which image it is cut from, and where.
	in.image = sheet.image;
	in.SetImageSize(sheet.GetImageWidth(), sheet.GetImageHeight());
	for (size_t i = 0; i < in.frames.size(); i++)
	{
		const SpriteFrame2D &f = in.frames[i];
		if (f.x + f.w > sheet.GetImageWidth() || f.y + f.h > sheet.GetImageHeight())
		{
			errOut = "frame '" + f.name + "' lies outside this image - is the atlas for another one?";
			return false;
		}
	}
	const std::string before = SpriteSheet2DToString(sheet);
	sheet = in;
	view.selection.clear();
	view.clip = -1;
	PushEdit(before, "Import Atlas");
	return true;
}

int SpriteSheetDocument::AddFrame(const int x, const int y, const int w, const int h, const std::string& name)
{
	if (w < 1 || h < 1 || x < 0 || y < 0) return -1;
	if (sheet.HaveImageSize() && (x + w > sheet.GetImageWidth() || y + h > sheet.GetImageHeight())) return -1;
	const std::string before = SpriteSheet2DToString(sheet);
	SpriteFrame2D f;
	f.name = UniqueFrameName(name.empty() ? view.prefix + std::to_string(sheet.frames.size()) : name);
	f.x = x; f.y = y; f.w = w; f.h = h;
	sheet.frames.push_back(f);
	PushEdit(before, "Add Frame");
	return (int)sheet.frames.size() - 1;
}

void SpriteSheetDocument::RemoveFrames(const std::vector<int32>& indices)
{
	std::set<int32> gone;
	for (size_t i = 0; i < indices.size(); i++)
		if (sheet.ValidFrame(indices[i])) gone.insert(indices[i]);
	if (gone.empty()) return;

	const std::string before = SpriteSheet2DToString(sheet);
	// Where each surviving frame ends up.
	std::vector<int32> remap(sheet.frames.size(), -1);
	std::vector<SpriteFrame2D> kept;
	for (size_t i = 0; i < sheet.frames.size(); i++)
	{
		if (gone.count((int32)i)) continue;
		remap[i] = (int32)kept.size();
		kept.push_back(sheet.frames[i]);
	}
	sheet.frames = kept;
	for (size_t c = 0; c < sheet.clips.size(); c++)
	{
		SpriteClip2D &clip = sheet.clips[c];
		// Events are addressed by position in the clip, so they move with
		// the positions that survive.
		std::vector<int32> frames, stepMap(clip.frames.size(), -1);
		for (size_t k = 0; k < clip.frames.size(); k++)
		{
			const int32 to = (clip.frames[k] >= 0 && clip.frames[k] < (int32)remap.size()) ? remap[(size_t)clip.frames[k]] : -1;
			if (to < 0) continue;
			stepMap[k] = (int32)frames.size();
			frames.push_back(to);
		}
		std::vector<SpriteEvent2D> events;
		for (size_t e = 0; e < clip.events.size(); e++)
		{
			const int32 at = clip.events[e].at;
			if (at < 0 || at >= (int32)stepMap.size() || stepMap[(size_t)at] < 0) continue;
			SpriteEvent2D ev = clip.events[e];
			ev.at = stepMap[(size_t)at];
			events.push_back(ev);
		}
		clip.frames = frames;
		clip.events = events;
	}
	view.selection.clear();
	PushEdit(before, gone.size() == 1 ? "Remove Frame" : "Remove Frames");
}

bool SpriteSheetDocument::SetFrame(const int32 index, const SpriteFrame2D& f)
{
	if (!sheet.ValidFrame(index) || f.w < 1 || f.h < 1 || f.x < 0 || f.y < 0) return false;
	if (sheet.HaveImageSize() && (f.x + f.w > sheet.GetImageWidth() || f.y + f.h > sheet.GetImageHeight())) return false;
	const std::string before = SpriteSheet2DToString(sheet);
	SpriteFrame2D next = f;
	next.name = UniqueFrameName(f.name, index);
	sheet.frames[(size_t)index] = next;
	if (before == SpriteSheet2DToString(sheet)) return true;
	PushEdit(before, "Edit Frame");
	return true;
}

void SpriteSheetDocument::SetPivot(const std::vector<int32>& indices, const Vec2& pivot)
{
	const std::string before = SpriteSheet2DToString(sheet);
	for (size_t i = 0; i < indices.size(); i++)
		if (sheet.ValidFrame(indices[i])) sheet.frames[(size_t)indices[i]].pivot = pivot;
	if (before == SpriteSheet2DToString(sheet)) return;
	PushEdit(before, "Set Pivot");
}

void SpriteSheetDocument::SetBorder(const std::vector<int32>& indices, const Vec4& border)
{
	const std::string before = SpriteSheet2DToString(sheet);
	for (size_t i = 0; i < indices.size(); i++)
	{
		if (!sheet.ValidFrame(indices[i])) continue;
		SpriteFrame2D &f = sheet.frames[(size_t)indices[i]];
		// Clamped so the two sides of an axis cannot cross: a border wider
		// than the frame leaves no middle to stretch.
		Vec4 b(std::max(0.f, border.x), std::max(0.f, border.y), std::max(0.f, border.z), std::max(0.f, border.w));
		if (b.x + b.z > (f32)f.w) { const f32 k = (f32)f.w / (b.x + b.z); b.x *= k; b.z *= k; }
		if (b.y + b.w > (f32)f.h) { const f32 k = (f32)f.h / (b.y + b.w); b.y *= k; b.w *= k; }
		f.border = b;
	}
	if (before == SpriteSheet2DToString(sheet)) return;
	PushEdit(before, "Set Nine-Slice Borders");
}

int SpriteSheetDocument::AddClip(const std::string& name, const std::vector<int32>& frames,
	const float fps, const bool loop)
{
	if (frames.empty()) return -1;
	for (size_t i = 0; i < frames.size(); i++)
		if (!sheet.ValidFrame(frames[i])) return -1;
	const std::string before = SpriteSheet2DToString(sheet);
	SpriteClip2D c;
	c.name = UniqueClipName(name);
	c.frames = frames;
	c.fps = fps > 0.f ? fps : 12.f;
	c.loop = loop;
	sheet.clips.push_back(c);
	PushEdit(before, "Add Clip");
	return (int)sheet.clips.size() - 1;
}

void SpriteSheetDocument::RemoveClip(const int index)
{
	if (index < 0 || index >= (int)sheet.clips.size()) return;
	const std::string before = SpriteSheet2DToString(sheet);
	sheet.clips.erase(sheet.clips.begin() + index);
	if (view.clip >= (int)sheet.clips.size()) view.clip = (int)sheet.clips.size() - 1;
	PushEdit(before, "Remove Clip");
}

bool SpriteSheetDocument::SetClip(const int index, const SpriteClip2D& clip)
{
	if (index < 0 || index >= (int)sheet.clips.size()) return false;
	for (size_t i = 0; i < clip.frames.size(); i++)
		if (!sheet.ValidFrame(clip.frames[i])) return false;
	const std::string before = SpriteSheet2DToString(sheet);
	SpriteClip2D next = clip;
	next.name = UniqueClipName(clip.name, index);
	if (!(next.fps > 0.f)) next.fps = 12.f;
	// An event past the end of the clip can never fire.
	std::vector<SpriteEvent2D> events;
	for (size_t e = 0; e < next.events.size(); e++)
		if (next.events[e].at >= 0 && next.events[e].at < (int32)next.frames.size()) events.push_back(next.events[e]);
	next.events = events;
	sheet.clips[(size_t)index] = next;
	if (before == SpriteSheet2DToString(sheet)) return true;
	PushEdit(before, "Edit Clip");
	return true;
}
