//=============================================================================
// Name        : EditorSpriteSheets.cpp
// Description : The editor host's side of sprite sheets (.p3ds): opening,
//               saving and drawing their document windows, making one from
//               an image, and the agent commands that reach everything the
//               Sprite Sheet editor's panels do.
//
//               Its own file rather than more of Editor.cpp, which is ten
//               thousand lines; these are Editor members all the same.
//=============================================================================

#include "Editor.h"
#include "editor/UI/SpriteSheetEditor.h"
#include <Pyros3D/Assets/SpriteSheet2D/SpriteSheet2D.h>
#include <Pyros3D/Assets/TileSet2D/TileSet2D.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace p3d;

namespace {

	std::string LowerOf(std::string s)
	{
		for (size_t i = 0; i < s.size(); i++) s[i] = (char)tolower((unsigned char)s[i]);
		return s;
	}

	// The .p3ds an image's sheet lives in: beside it, named after it.
	std::string SheetPathFor(const std::string& imageRel)
	{
		const size_t dot = imageRel.find_last_of('.');
		const size_t slash = imageRel.find_last_of("/\\");
		const std::string stem = (dot != std::string::npos && (slash == std::string::npos || dot > slash))
			? imageRel.substr(0, dot) : imageRel;
		return stem + ".p3ds";
	}

	int32 FrameArg(const SpriteSheet2D& sheet, const nlohmann::json& v)
	{
		if (v.is_string()) return sheet.FindFrame(v.get<std::string>());
		if (v.is_number_integer()) return sheet.ValidFrame(v.get<int32>()) ? v.get<int32>() : -1;
		return -1;
	}

	bool FrameListArg(const SpriteSheet2D& sheet, const nlohmann::json& c, std::vector<int32>& out, std::string& err)
	{
		out.clear();
		if (c.contains("frames") && c["frames"].is_array())
		{
			for (size_t i = 0; i < c["frames"].size(); i++)
			{
				const int32 f = FrameArg(sheet, c["frames"][i]);
				if (f < 0) { err = "clip frame " + c["frames"][i].dump() + " is not in the sheet"; return false; }
				out.push_back(f);
			}
		}
		else if (c.contains("from") && c.contains("to"))
		{
			const int32 from = FrameArg(sheet, c["from"]), to = FrameArg(sheet, c["to"]);
			if (from < 0 || to < 0) { err = "clip range " + c["from"].dump() + ".." + c["to"].dump() + " is not in the sheet"; return false; }
			const int32 dir = to >= from ? 1 : -1;
			for (int32 f = from; f != to + dir; f += dir) out.push_back(f);
		}
		else { err = "a clip needs 'frames' or 'from' and 'to'"; return false; }
		if (out.empty()) { err = "a clip needs at least one frame"; return false; }
		return true;
	}

	bool ReadClipFields(const SpriteSheet2D& sheet, const nlohmann::json& c, SpriteClip2D& clip, std::string& err)
	{
		if (c.contains("frames") || (c.contains("from") && c.contains("to")))
			if (!FrameListArg(sheet, c, clip.frames, err)) return false;
		if (c.contains("fps")) clip.fps = c["fps"].get<f32>();
		if (c.contains("loop")) clip.loop = c["loop"].get<bool>();
		if (c.contains("pingPong")) clip.pingPong = c["pingPong"].get<bool>();
		if (c.contains("rename")) clip.name = c["rename"].get<std::string>();
		if (c.contains("events") && c["events"].is_array())
		{
			clip.events.clear();
			for (size_t i = 0; i < c["events"].size(); i++)
			{
				const nlohmann::json& e = c["events"][i];
				if (!e.is_object() || !e.contains("name")) { err = "an event needs 'at' and 'name'"; return false; }
				SpriteEvent2D ev;
				ev.at = e.value("at", 0);
				ev.name = e["name"].get<std::string>();
				if (ev.at < 0 || ev.at >= (int32)clip.frames.size())
				{ err = "event '" + ev.name + "' is at step " + std::to_string(ev.at) + ", and the clip has " + std::to_string(clip.frames.size()); return false; }
				clip.events.push_back(ev);
			}
		}
		return true;
	}

	// Everything create_spritesheet and edit_spritesheet can say about a
	// sheet, applied through the document's own (undoable) operations - the
	// same calls its panels make.
	bool ApplySheetEdits(SpriteSheetDocument& d, const nlohmann::json& a, std::string& err)
	{
		static const char* known[] = { "image", "path", "overwrite", "pixelsPerUnit", "filter", "normalImage", "prefix",
			"grid", "count", "auto", "atlas", "pivot", "border", "frame", "addFrame", "removeFrames",
			"clips", "clip", "addClip", "removeClip", NULL };
		for (nlohmann::json::const_iterator it = a.begin(); it != a.end(); ++it)
		{
			bool ok = false;
			for (int k = 0; known[k] && !ok; k++) ok = (it.key() == known[k]);
			if (!ok) { err = "unknown key '" + it.key() + "'"; return false; }
		}
		try {
			if (a.contains("pixelsPerUnit"))
			{
				const f32 ppu = a["pixelsPerUnit"].get<f32>();
				if (!(ppu > 0.f)) { err = "pixelsPerUnit must be greater than 0"; return false; }
				d.SetPixelsPerUnit(ppu);
			}
			if (a.contains("filter"))
			{
				const std::string f = LowerOf(a["filter"].get<std::string>());
				if (f == "nearest") d.SetFilter(SpriteFilter2D::Nearest);
				else if (f == "linear") d.SetFilter(SpriteFilter2D::Linear);
				else { err = "filter must be linear or nearest"; return false; }
			}
			if (a.contains("normalImage"))
			{
				if (!a["normalImage"].is_string()) { err = "normalImage is a project-relative path, or \"\" for none"; return false; }
				d.SetNormalImage(a["normalImage"].get<std::string>());
			}
			const std::string prefix = a.contains("prefix") ? a["prefix"].get<std::string>() : d.view.prefix;

			if (a.contains("grid"))
			{
				const nlohmann::json& g = a["grid"];
				const int n = d.SliceGrid(g.value("cellWidth", 0), g.value("cellHeight", 0),
					g.value("margin", 0), g.value("spacing", 0), prefix);
				if (n < 1) { err = "that grid cuts no frames from a " + std::to_string(d.sheet.GetImageWidth()) + "x" + std::to_string(d.sheet.GetImageHeight()) + " image"; return false; }
			}
			if (a.contains("count"))
			{
				const nlohmann::json& g = a["count"];
				if (d.SliceCount(g.value("columns", 0), g.value("rows", 0), prefix) < 1)
				{ err = "count needs columns and rows of at least 1"; return false; }
			}
			if (a.contains("auto"))
			{
				const nlohmann::json& g = a["auto"].is_object() ? a["auto"] : nlohmann::json::object();
				int made = 0;
				if (!d.AutoSlice(g.value("alphaThreshold", 8), g.value("merge", 1), g.value("minSize", 2), prefix, made))
				{ err = "the image could not be read for automatic slicing"; return false; }
				if (made < 1) { err = "automatic slicing found nothing opaque in the image"; return false; }
			}
			if (a.contains("atlas"))
			{
				const std::string rel = a["atlas"].get<std::string>();
				std::string path = (std::filesystem::path(d.projectRoot) / rel).string();
				std::ifstream in(path.c_str());
				if (!in.is_open()) { in.clear(); in.open(rel.c_str()); }
				if (!in.is_open()) { err = "could not open atlas " + rel; return false; }
				std::stringstream ss;
				ss << in.rdbuf();
				if (!d.ImportAtlas(ss.str(), err)) return false;
			}

			// Top-level pivot/border: every frame, which is what a freshly
			// cut sheet wants ("all of these stand on their feet").
			std::vector<int32> all;
			for (int32 i = 0; i < (int32)d.sheet.frames.size(); i++) all.push_back(i);
			if (a.contains("pivot"))
			{
				if (!a["pivot"].is_array() || a["pivot"].size() < 2) { err = "pivot must be [x, y]"; return false; }
				d.SetPivot(all, Vec2(a["pivot"][0].get<f32>(), a["pivot"][1].get<f32>()));
			}
			if (a.contains("border"))
			{
				if (!a["border"].is_array() || a["border"].size() < 4) { err = "border must be [left, bottom, right, top]"; return false; }
				d.SetBorder(all, Vec4(a["border"][0].get<f32>(), a["border"][1].get<f32>(), a["border"][2].get<f32>(), a["border"][3].get<f32>()));
			}

			if (a.contains("addFrame"))
			{
				const nlohmann::json& f = a["addFrame"];
				if (!f.is_object() || !f.contains("rect") || !f["rect"].is_array() || f["rect"].size() != 4)
				{ err = "addFrame needs a 'rect' of [x, y, w, h]"; return false; }
				if (d.AddFrame(f["rect"][0].get<int>(), f["rect"][1].get<int>(), f["rect"][2].get<int>(), f["rect"][3].get<int>(),
					f.value("name", std::string())) < 0)
				{ err = "that rectangle has no area or leaves the image"; return false; }
			}
			if (a.contains("frame"))
			{
				// One frame, or a list of them.
				const nlohmann::json list = a["frame"].is_array() ? a["frame"] : nlohmann::json::array({ a["frame"] });
				for (size_t i = 0; i < list.size(); i++)
				{
					const nlohmann::json& f = list[i];
					if (!f.is_object() || !(f.contains("index") || f.contains("name")))
					{ err = "frame needs an 'index' or a 'name' to say which"; return false; }
					const int32 index = f.contains("index") ? FrameArg(d.sheet, f["index"]) : d.sheet.FindFrame(f["name"].get<std::string>());
					if (index < 0) { err = "no such frame: " + (f.contains("index") ? f["index"].dump() : f["name"].dump()); return false; }
					SpriteFrame2D next = d.sheet.frames[(size_t)index];
					if (f.contains("rename")) next.name = f["rename"].get<std::string>();
					if (f.contains("rect") && f["rect"].is_array() && f["rect"].size() == 4)
					{ next.x = f["rect"][0].get<int>(); next.y = f["rect"][1].get<int>(); next.w = f["rect"][2].get<int>(); next.h = f["rect"][3].get<int>(); }
					if (f.contains("pivot") && f["pivot"].is_array() && f["pivot"].size() >= 2)
						next.pivot = Vec2(f["pivot"][0].get<f32>(), f["pivot"][1].get<f32>());
					if (f.contains("border") && f["border"].is_array() && f["border"].size() >= 4)
						next.border = Vec4(f["border"][0].get<f32>(), f["border"][1].get<f32>(), f["border"][2].get<f32>(), f["border"][3].get<f32>());
					if (f.contains("duration")) next.duration = std::max(0.f, f["duration"].get<f32>());
					if (!d.SetFrame(index, next)) { err = "frame '" + next.name + "': that rectangle has no area or leaves the image"; return false; }
				}
			}
			if (a.contains("removeFrames"))
			{
				std::vector<int32> gone;
				for (size_t i = 0; i < a["removeFrames"].size(); i++)
				{
					const int32 f = FrameArg(d.sheet, a["removeFrames"][i]);
					if (f < 0) { err = "no such frame: " + a["removeFrames"][i].dump(); return false; }
					gone.push_back(f);
				}
				d.RemoveFrames(gone);
			}

			// "clips" replaces the whole list; "addClip" appends one; "clip"
			// edits one by name.
			if (a.contains("clips"))
			{
				if (!a["clips"].is_array()) { err = "clips must be a list"; return false; }
				while (!d.sheet.clips.empty()) d.RemoveClip(0);
				for (size_t i = 0; i < a["clips"].size(); i++)
				{
					const nlohmann::json& c = a["clips"][i];
					SpriteClip2D clip;
					clip.name = c.value("name", std::string("clip") + std::to_string(i));
					if (!c.contains("frames") && !(c.contains("from") && c.contains("to")))
					{ err = "clip '" + clip.name + "' needs 'frames' or 'from' and 'to'"; return false; }
					if (!ReadClipFields(d.sheet, c, clip, err)) { err = "clip '" + clip.name + "': " + err; return false; }
					const int made = d.AddClip(clip.name, clip.frames, clip.fps, clip.loop);
					if (made < 0) { err = "clip '" + clip.name + "' could not be added"; return false; }
					clip.name = d.sheet.clips[(size_t)made].name;
					d.SetClip(made, clip);
				}
			}
			if (a.contains("addClip"))
			{
				const nlohmann::json& c = a["addClip"];
				SpriteClip2D clip;
				clip.name = c.value("name", std::string("clip"));
				if (!c.contains("frames") && !(c.contains("from") && c.contains("to")))
				{ err = "addClip needs 'frames' or 'from' and 'to'"; return false; }
				if (!ReadClipFields(d.sheet, c, clip, err)) return false;
				const int made = d.AddClip(clip.name, clip.frames, clip.fps, clip.loop);
				if (made < 0) { err = "the clip could not be added"; return false; }
				clip.name = d.sheet.clips[(size_t)made].name;
				d.SetClip(made, clip);
			}
			if (a.contains("clip"))
			{
				const nlohmann::json& c = a["clip"];
				const int index = c.is_object() ? d.sheet.FindClip(c.value("name", std::string())) : -1;
				if (index < 0) { err = "no clip named '" + (c.is_object() ? c.value("name", std::string()) : std::string()) + "'"; return false; }
				SpriteClip2D clip = d.sheet.clips[(size_t)index];
				if (!ReadClipFields(d.sheet, c, clip, err)) return false;
				if (!d.SetClip(index, clip)) { err = "the clip could not be changed"; return false; }
			}
			if (a.contains("removeClip"))
			{
				const int index = d.sheet.FindClip(a["removeClip"].get<std::string>());
				if (index < 0) { err = "no clip named '" + a["removeClip"].get<std::string>() + "'"; return false; }
				d.RemoveClip(index);
			}
		}
		catch (const std::exception& e) {
			err = std::string("a value has the wrong type (") + e.what() + ")";
			return false;
		}
		return true;
	}

	nlohmann::json SheetInfo(const SpriteSheetDocument& d, const std::string& relPath)
	{
		nlohmann::json r;
		const SpriteSheet2D& s = d.sheet;
		r["path"] = relPath;
		r["name"] = d.displayName;
		r["image"] = s.image;
		if (!s.normalImage.empty()) r["normalImage"] = s.normalImage;
		r["imageSize"] = { s.GetImageWidth(), s.GetImageHeight() };
		r["pixelsPerUnit"] = s.pixelsPerUnit;
		r["filter"] = s.filter == SpriteFilter2D::Nearest ? "nearest" : "linear";
		r["dirty"] = d.dirty;
		r["frameCount"] = s.frames.size();
		nlohmann::json frames = nlohmann::json::array();
		for (size_t i = 0; i < s.frames.size(); i++)
		{
			const SpriteFrame2D& f = s.frames[i];
			nlohmann::json e;
			e["index"] = i;
			e["name"] = f.name;
			e["rect"] = { f.x, f.y, f.w, f.h };
			e["pivot"] = { f.pivot.x, f.pivot.y };
			if (f.HasBorder()) e["border"] = { f.border.x, f.border.y, f.border.z, f.border.w };
			if (f.duration > 0.f) e["duration"] = f.duration;
			frames.push_back(e);
		}
		r["frames"] = frames;
		nlohmann::json clips = nlohmann::json::array();
		for (size_t i = 0; i < s.clips.size(); i++)
		{
			const SpriteClip2D& c = s.clips[i];
			nlohmann::json e;
			e["name"] = c.name;
			e["frames"] = c.frames;
			e["fps"] = c.fps;
			e["loop"] = c.loop;
			e["pingPong"] = c.pingPong;
			e["length"] = SpriteClip2DLength(s, c);
			nlohmann::json ev = nlohmann::json::array();
			for (size_t k = 0; k < c.events.size(); k++)
				ev.push_back({ { "at", c.events[k].at }, { "name", c.events[k].name } });
			e["events"] = ev;
			clips.push_back(e);
		}
		r["clips"] = clips;
		return r;
	}

}

// --- documents -------------------------------------------------------------------

SpriteSheetDocument* Editor::FindSpriteSheetDocumentByPath(const std::string& absPath)
{
	for (size_t i = 0; i < spriteSheetDocs.size(); ++i)
		if (spriteSheetDocs[i] && spriteSheetDocs[i]->absolutePath == absPath) return spriteSheetDocs[i];
	return nullptr;
}

bool Editor::OpenSpriteSheetDocument(const std::string& absPath)
{
	if (absPath.empty()) return false;

	if (SpriteSheetDocument* existing = FindSpriteSheetDocumentByPath(absPath))
	{
		pendingSelectSpriteSheetDocId = existing->id;
		activeSpriteSheetDoc = existing;
		lastFocusedDocKind = FocusedDocKind::SpriteSheet;
		return true;
	}

	SpriteSheetDocument* doc = new SpriteSheetDocument();
	doc->id = nextSpriteSheetDocId++;
	// Set BEFORE loading: the load resolves the image against this.
	doc->projectRoot = project.IsOpen() ? project.GetProjectPath() : std::string();

	std::string err;
	if (!doc->LoadFromFile(absPath, err))
	{
		echo("ERROR: " + err);
		delete doc;
		return false;
	}

	// The image, for the canvas. Nearest and no mipmaps whatever the sheet's
	// own filter says: this is where frames are cut to the pixel, and a
	// smoothed picture hides exactly the edges being placed.
	if (!doc->imagePath.empty())
	{
		std::shared_ptr<p3d::Texture> tex = std::make_shared<p3d::Texture>();
		if (tex->LoadTexture(doc->imagePath, p3d::TextureType::Texture, false))
		{
			tex->SetMinMagFilter(p3d::TextureFilter::Nearest, p3d::TextureFilter::Nearest);
			spriteSheetTex[doc->id] = tex;
		}
	}

	// Route Ctrl+Z here while this document is the one being edited.
	doc->undo.onPush = [this, doc]() {
		lastFocusedDocKind = FocusedDocKind::SpriteSheet;
		activeSpriteSheetDoc = doc;
	};

	spriteSheetDocs.push_back(doc);
	pendingSelectSpriteSheetDocId = doc->id;
	activeSpriteSheetDoc = doc;
	lastFocusedDocKind = FocusedDocKind::SpriteSheet;
	echo("Opened sprite sheet " + project.DisplayPath(absPath) + " ("
		+ std::to_string(doc->sheet.frames.size()) + " frames, "
		+ std::to_string(doc->sheet.clips.size()) + " clips)");
	return true;
}

bool Editor::SaveSpriteSheetDocument(SpriteSheetDocument* doc)
{
	if (!doc || doc->absolutePath.empty()) return false;
	std::string err;
	if (!doc->SaveAs(doc->absolutePath, err))
	{
		echo("ERROR: " + err);
		return false;
	}
	// Every open scene that draws from this sheet sees the change now -
	// otherwise the sprites keep the copy they loaded, and the edit looks
	// like it did nothing until the scene is reopened.
	int reloaded = 0;
	for (size_t i = 0; i < sceneDocs.size(); i++)
		if (sceneDocs[i]) reloaded += sceneDocs[i]->ReloadSpritesUsing(doc->absolutePath);
	echo("SUCCESS: Saved " + project.DisplayPath(doc->absolutePath)
		+ (reloaded > 0 ? " (" + std::to_string(reloaded) + " sprite" + (reloaded == 1 ? "" : "s") + " updated)" : std::string()));
	return true;
}

void Editor::DrawSpriteSheetEditorWindows()
{
	if (spriteSheetDocs.empty()) return;

	if (ImGuiWindow* sv = ImGui::FindWindowByName("Scene View"))
		if (sv->DockId != 0) dockCenterId = sv->DockId;

	std::vector<uint32> closeIds;
	for (size_t i = 0; i < spriteSheetDocs.size(); ++i)
	{
		SpriteSheetDocument* doc = spriteSheetDocs[i];
		if (!doc) continue;

		char title[512];
		snprintf(title, sizeof(title), u8" %s###spritesheet_win_%u", doc->displayName.c_str(), doc->id);

		// Only a real dock target counts as having placed the window - see
		// DrawTileSetEditorWindows for what happens otherwise.
		const bool canDock = (dockCenterId != 0);
		const bool forceDock = canDock && (pendingSelectSpriteSheetDocId == doc->id);
		if (canDock)
			ImGui::SetNextWindowDockID(dockCenterId, forceDock ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
		if (forceDock) ImGui::SetNextWindowFocus();
		ImGui::SetNextWindowSize(ImVec2(1000, 680), ImGuiCond_FirstUseEver);

		bool open = true;
		ImGuiWindowFlags wflags = ImGuiWindowFlags_None;
		if (doc->dirty) wflags |= ImGuiWindowFlags_UnsavedDocument;

		if (!ImGui::Begin(title, &open, wflags))
		{
			ImGui::End();
			if (!open) closeIds.push_back(doc->id);
			continue;
		}
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
		{
			activeSpriteSheetDoc = doc;
			lastFocusedDocKind = FocusedDocKind::SpriteSheet;
		}
		if (canDock && pendingSelectSpriteSheetDocId == doc->id) pendingSelectSpriteSheetDocId = 0;

		void* texId = nullptr;
		std::map<uint32, std::shared_ptr<p3d::Texture> >::iterator t = spriteSheetTex.find(doc->id);
		if (t != spriteSheetTex.end() && t->second)
			texId = (void*)GetActiveRenderDevice().GetImGuiTextureID(
				t->second->GetBindID(), t->second->GetTextureType());

		SpriteSheetEditor::FrameRequests req;
		SpriteSheetEditor::DrawWindow(*doc, texId, req);
		ImGui::End();

		if (req.save) SaveSpriteSheetDocument(doc);
		if (req.close || !open) closeIds.push_back(doc->id);
	}

	for (size_t k = 0; k < closeIds.size(); ++k)
	{
		for (size_t i = 0; i < spriteSheetDocs.size(); ++i)
		{
			if (!spriteSheetDocs[i] || spriteSheetDocs[i]->id != closeIds[k]) continue;
			// Closing with unsaved edits saves them. A sheet is small, the
			// edit is undoable for as long as it is open, and the
			// alternative - throwing the cut away without a word - is what a
			// closed tab did before.
			if (spriteSheetDocs[i]->dirty) SaveSpriteSheetDocument(spriteSheetDocs[i]);
			if (activeSpriteSheetDoc == spriteSheetDocs[i]) activeSpriteSheetDoc = nullptr;
			spriteSheetTex.erase(closeIds[k]);
			delete spriteSheetDocs[i];
			spriteSheetDocs.erase(spriteSheetDocs.begin() + i);
			break;
		}
	}
}

// Makes <image>.p3ds beside an image - one frame covering it, to be cut in
// the editor that opens - or opens the one already there.
bool Editor::CreateSpriteSheetFor(const std::string& imageRel, std::string& sheetRelOut, std::string& err)
{
	if (!project.IsOpen()) { err = "open a project first"; return false; }
	const std::string absImage = project.AbsolutePath(imageRel);
	int32 iw = 0, ih = 0;
	if (!TileSet2DReadImageSize(absImage, iw, ih)) { err = "could not read " + imageRel; return false; }
	sheetRelOut = SheetPathFor(project.DisplayPath(absImage));
	const std::string absSheet = project.AbsolutePath(sheetRelOut);
	if (!std::filesystem::exists(absSheet))
	{
		SpriteSheet2D s;
		s.image = project.DisplayPath(absImage);
		s.SetImageSize(iw, ih);
		SpriteFrame2D f;
		f.name = std::filesystem::path(absImage).stem().string();
		f.w = iw;
		f.h = ih;
		s.frames.push_back(f);
		if (!SaveSpriteSheet2D(absSheet, s, &err)) return false;
	}
	return true;
}

namespace {
	// hero_2 before hero_10: the order an animation's files are meant in.
	bool NaturalLess(const std::string& a, const std::string& b)
	{
		size_t i = 0, j = 0;
		while (i < a.size() && j < b.size())
		{
			const bool da = a[i] >= '0' && a[i] <= '9', db = b[j] >= '0' && b[j] <= '9';
			if (da && db)
			{
				size_t ie = i, je = j;
				while (ie < a.size() && a[ie] >= '0' && a[ie] <= '9') ie++;
				while (je < b.size() && b[je] >= '0' && b[je] <= '9') je++;
				const unsigned long long na = strtoull(a.substr(i, ie - i).c_str(), NULL, 10);
				const unsigned long long nb = strtoull(b.substr(j, je - j).c_str(), NULL, 10);
				if (na != nb) return na < nb;
				i = ie; j = je;
				continue;
			}
			const char ca = (char)tolower((unsigned char)a[i]), cb = (char)tolower((unsigned char)b[j]);
			if (ca != cb) return ca < cb;
			i++; j++;
		}
		return a.size() - i < b.size() - j;
	}
}

bool Editor::PackSpriteAtlas(const nlohmann::json& a, std::string& sheetRelOut, nlohmann::json* infoOut, std::string& err)
{
	namespace fs = std::filesystem;
	if (!project.IsOpen()) { err = "open a project first"; return false; }
	static const char* known[] = { "images", "folder", "output", "overwrite", "padding", "extrude", "trim",
		"alphaThreshold", "powerOfTwo", "maxSize", "pixelsPerUnit", "filter", "pivot", "clipFps", NULL };
	if (a.is_object())
		for (nlohmann::json::const_iterator it = a.begin(); it != a.end(); ++it)
		{
			bool ok = false;
			for (int k = 0; known[k] && !ok; k++) ok = (it.key() == known[k]);
			if (!ok) { err = "unknown key '" + it.key() + "'"; return false; }
		}
	try {
		std::vector<std::string> rels;
		std::string outStem = a.value("output", std::string());
		if (a.contains("images") && a["images"].is_array())
			for (size_t i = 0; i < a["images"].size(); i++) rels.push_back(a["images"][i].get<std::string>());
		const std::string folder = a.value("folder", std::string());
		if (!folder.empty())
		{
			const std::string absFolder = project.AbsolutePath(folder);
			std::error_code ec;
			if (!fs::is_directory(absFolder, ec)) { err = "'" + folder + "' is not a folder"; return false; }
			std::vector<std::string> found;
			for (fs::directory_iterator it(absFolder, ec), end; !ec && it != end; it.increment(ec))
			{
				if (!it->is_regular_file(ec)) continue;
				const std::string abs = it->path().string();
				if (ProjectManager::IsTextureExtension(abs)) found.push_back(project.DisplayPath(abs));
			}
			std::sort(found.begin(), found.end(), NaturalLess);
			rels.insert(rels.end(), found.begin(), found.end());
			// Beside the folder, named after it: assets/textures/hero/ packs
			// to assets/textures/hero.png and hero.p3ds.
			if (outStem.empty())
			{
				std::string f = project.DisplayPath(absFolder);
				while (!f.empty() && (f[f.size() - 1] == '/' || f[f.size() - 1] == '\\')) f.erase(f.size() - 1);
				outStem = f;
			}
		}
		if (rels.empty()) { err = folder.empty() ? "needs images: [...] or folder" : ("no images in " + folder); return false; }
		if (outStem.empty()) { err = "needs output: where the atlas goes, without an extension (assets/textures/hero_atlas)"; return false; }
		{
			const size_t dot = outStem.find_last_of('.'), slash = outStem.find_last_of("/\\");
			if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) outStem = outStem.substr(0, dot);
		}
		const std::string relPng = outStem + ".png", relSheet = outStem + ".p3ds";
		const std::string absPng = project.AbsolutePath(relPng), absSheet = project.AbsolutePath(relSheet);
		const bool overwrite = a.value("overwrite", false);
		if (!overwrite && (fs::exists(absPng) || fs::exists(absSheet)))
		{ err = relPng + " or " + relSheet + " already exists - pass \"overwrite\":true to replace them"; return false; }

		std::vector<std::string> abs;
		for (size_t i = 0; i < rels.size(); i++)
		{
			const std::string p = project.AbsolutePath(rels[i]);
			// Packing an atlas into itself, on a second run over the same folder.
			if (p == absPng) continue;
			abs.push_back(p);
		}
		if (abs.empty()) { err = "nothing to pack but the atlas itself"; return false; }

		SpriteAtlasOptions2D opt;
		opt.padding = a.value("padding", opt.padding);
		opt.extrude = a.value("extrude", opt.extrude);
		opt.trim = a.value("trim", opt.trim);
		opt.alphaThreshold = a.value("alphaThreshold", opt.alphaThreshold);
		opt.powerOfTwo = a.value("powerOfTwo", opt.powerOfTwo);
		opt.maxSize = a.value("maxSize", opt.maxSize);

		SpriteSheet2D sheet;
		// What an existing sheet of the same name said about scale and
		// filtering is kept: re-packing after adding a frame must not undo
		// the settings made by hand.
		SpriteSheet2D old;
		const bool hadOld = fs::exists(absSheet) && LoadSpriteSheet2D(absSheet, old, NULL);
		if (!PackSpriteAtlasFiles2D(abs, opt, absPng, sheet, &err)) return false;
		if (hadOld) { sheet.pixelsPerUnit = old.pixelsPerUnit; sheet.filter = old.filter; sheet.normalImage = old.normalImage; }
		sheet.image = relPng;
		if (a.contains("pixelsPerUnit"))
		{
			const f32 ppu = a["pixelsPerUnit"].get<f32>();
			if (!(ppu > 0.f)) { err = "pixelsPerUnit must be greater than 0"; return false; }
			sheet.pixelsPerUnit = ppu;
		}
		if (a.contains("filter"))
		{
			const std::string f = LowerOf(a["filter"].get<std::string>());
			if (f != "nearest" && f != "linear") { err = "filter must be linear or nearest"; return false; }
			sheet.filter = f == "nearest" ? SpriteFilter2D::Nearest : SpriteFilter2D::Linear;
		}
		// Pivots and borders are a frame's own, by name: a re-pack keeps
		// the ones already set, and "pivot" sets the rest.
		for (size_t i = 0; i < sheet.frames.size(); i++)
		{
			SpriteFrame2D& f = sheet.frames[i];
			bool kept = false;
			if (hadOld)
				for (size_t k = 0; k < old.frames.size() && !kept; k++)
					if (old.frames[k].name == f.name)
					{ f.pivot = old.frames[k].pivot; f.border = old.frames[k].border; f.duration = old.frames[k].duration; kept = true; }
			if (!kept && a.contains("pivot") && a["pivot"].is_array() && a["pivot"].size() >= 2)
				f.pivot = Vec2(a["pivot"][0].get<f32>(), a["pivot"][1].get<f32>());
		}
		// Clips name frames by index, and the indices are new: carried over
		// by frame NAME, dropped where a frame is gone.
		if (hadOld)
			for (size_t c = 0; c < old.clips.size(); c++)
			{
				SpriteClip2D clip = old.clips[c];
				clip.frames.clear();
				for (size_t k = 0; k < old.clips[c].frames.size(); k++)
				{
					const int32 oi = old.clips[c].frames[k];
					if (!old.ValidFrame(oi)) continue;
					for (size_t n = 0; n < sheet.frames.size(); n++)
						if (sheet.frames[n].name == old.frames[(size_t)oi].name) { clip.frames.push_back((int32)n); break; }
				}
				if (!clip.frames.empty()) sheet.clips.push_back(clip);
			}
		const f32 clipFps = a.value("clipFps", 12.f);
		if (clipFps > 0.f) SpriteSheet2DClipsFromNames(sheet, clipFps);

		if (!SaveSpriteSheet2D(absSheet, sheet, &err)) return false;
		sheetRelOut = relSheet;

		// An open window on the old sheet shows the old frames: reload it.
		if (SpriteSheetDocument* open = FindSpriteSheetDocumentByPath(absSheet))
		{
			std::string lerr;
			open->LoadFromFile(absSheet, lerr);
			spriteSheetTex.erase(open->id);
		}
		for (size_t i = 0; i < sceneDocs.size(); i++)
			if (sceneDocs[i]) sceneDocs[i]->ReloadSpritesUsing(absSheet);

		if (infoOut)
		{
			nlohmann::json& r = *infoOut;
			r["path"] = relSheet;
			r["image"] = relPng;
			r["imageSize"] = { sheet.GetImageWidth(), sheet.GetImageHeight() };
			r["frameCount"] = sheet.frames.size();
			long long used = 0;
			nlohmann::json frames = nlohmann::json::array();
			for (size_t i = 0; i < sheet.frames.size(); i++)
			{
				const SpriteFrame2D& f = sheet.frames[i];
				used += (long long)f.w * f.h;
				nlohmann::json e;
				e["name"] = f.name;
				e["rect"] = { f.x, f.y, f.w, f.h };
				if (f.sourceW > 0) e["trimmedFrom"] = { f.sourceW, f.sourceH };
				frames.push_back(e);
			}
			r["frames"] = frames;
			r["fill"] = (double)used / std::max(1.0, (double)sheet.GetImageWidth() * (double)sheet.GetImageHeight());
			nlohmann::json clips = nlohmann::json::array();
			for (size_t i = 0; i < sheet.clips.size(); i++)
				clips.push_back({ { "name", sheet.clips[i].name }, { "frames", sheet.clips[i].frames.size() }, { "fps", sheet.clips[i].fps } });
			r["clips"] = clips;
		}
		return true;
	}
	catch (const std::exception& e) { err = std::string("pack_sprite_atlas: ") + e.what(); return false; }
}

void Editor::HostOpenSpriteSheet(const std::string& absPath)
{
	if (!instance) return;
	instance->OpenSpriteSheetDocument(absPath);
}

void Editor::HostCreateSpriteSheet(const std::string& imageRel, uint32 goId)
{
	if (!instance) return;
	std::string rel, err;
	if (!instance->CreateSpriteSheetFor(imageRel, rel, err)) { echo("ERROR: " + err); return; }
	// The object that asked draws from the sheet from now on.
	if (instance->sceneView && goId != 0)
	{
		std::string serr;
		if (!instance->sceneView->SetSpriteSourceById(goId, rel, serr)) echo("WARNING: " + serr);
	}
	instance->OpenSpriteSheetDocument(instance->project.AbsolutePath(rel));
}

// --- agent -------------------------------------------------------------------------

bool Editor::HandleSpriteSheetCommand(const std::string& name, const nlohmann::json& a, nlohmann::json& r)
{
	auto A = [&](const char* k) -> std::string { return a.is_object() ? a.value(k, std::string()) : std::string(); };

	// {"cmd":"create_spritesheet","args":{"image":"assets/textures/hero.png",
	//    "pixelsPerUnit":32,"filter":"nearest",
	//    "grid":{"cellWidth":32,"cellHeight":32,"margin":0,"spacing":0}
	//      | "count":{"columns":6,"rows":4} | "auto":{"alphaThreshold":8,"merge":1,"minSize":2}
	//      | "atlas":"assets/textures/hero.json",
	//    "prefix":"hero_","pivot":[0.5,0],
	//    "clips":[{"name":"run","from":0,"to":5,"fps":12,"loop":true,
	//              "events":[{"at":2,"name":"step"}]}]}}
	// Writes <image>.p3ds beside the image (or `path`) and returns what it
	// holds. Refuses to replace a sheet that exists unless "overwrite":true.
	// {"cmd":"pack_sprite_atlas","args":{"folder":"assets/textures/hero"}} - every
	// image in a folder (or "images":[...]) into one picture and one sheet:
	//   output         where, without an extension; a folder packs beside itself
	//   padding 2, extrude 0, trim true, powerOfTwo true, maxSize 4096
	//   pixelsPerUnit, filter, pivot [x, y]
	//   clipFps 12     walk_0, walk_1, ... become a clip "walk"; 0 for none
	//   overwrite      re-packing keeps pivots, borders and clips, by frame name
	if (name == "pack_sprite_atlas")
	{
		std::string rel, perr;
		if (!PackSpriteAtlas(a, rel, &r, perr)) throw std::runtime_error(perr);
		r["ok"] = true;
		return true;
	}

	if (name == "create_spritesheet")
	{
		if (!project.IsOpen()) throw std::runtime_error("open a project first");
		const std::string image = A("image");
		if (image.empty()) throw std::runtime_error("create_spritesheet: 'image' is required");
		const std::string absImage = project.AbsolutePath(image);
		int32 iw = 0, ih = 0;
		if (!TileSet2DReadImageSize(absImage, iw, ih)) throw std::runtime_error("could not read image " + image);
		const std::string rel = A("path").empty() ? SheetPathFor(project.DisplayPath(absImage)) : A("path");
		const std::string absSheet = project.AbsolutePath(rel);
		if (std::filesystem::exists(absSheet) && !(a.is_object() && a.value("overwrite", false)))
			throw std::runtime_error(rel + " already exists - pass \"overwrite\":true to replace it, or use edit_spritesheet");
		if (SpriteSheetDocument* open = FindSpriteSheetDocumentByPath(absSheet))
		{
			// Replacing a sheet that is open: close the stale window.
			for (size_t i = 0; i < spriteSheetDocs.size(); i++)
				if (spriteSheetDocs[i] == open)
				{
					if (activeSpriteSheetDoc == open) activeSpriteSheetDoc = nullptr;
					spriteSheetTex.erase(open->id);
					delete open;
					spriteSheetDocs.erase(spriteSheetDocs.begin() + i);
					break;
				}
		}

		SpriteSheetDocument d;
		d.projectRoot = project.GetProjectPath();
		d.imagePath = absImage;
		d.sheet.image = project.DisplayPath(absImage);
		d.sheet.SetImageSize(iw, ih);
		d.view.prefix = std::filesystem::path(absImage).stem().string() + "_";
		// One frame covering the image until something cuts it, so a sheet
		// made only to carry a pivot or a scale is a valid one.
		SpriteFrame2D whole;
		whole.name = std::filesystem::path(absImage).stem().string();
		whole.w = iw;
		whole.h = ih;
		d.sheet.frames.push_back(whole);

		std::string err;
		if (!ApplySheetEdits(d, a, err)) throw std::runtime_error("create_spritesheet: " + err);
		if (!d.SaveAs(absSheet, err)) throw std::runtime_error(err);
		for (size_t i = 0; i < sceneDocs.size(); i++)
			if (sceneDocs[i]) sceneDocs[i]->ReloadSpritesUsing(absSheet);
		r = SheetInfo(d, rel);
		r["ok"] = true;
		return true;
	}

	// {"cmd":"open_spritesheet","args":{"path":"assets/textures/hero.p3ds"}}
	if (name == "open_spritesheet")
	{
		const std::string rel = A("path");
		if (rel.empty()) throw std::runtime_error("open_spritesheet: 'path' is required");
		if (!OpenSpriteSheetDocument(project.AbsolutePath(rel)))
			throw std::runtime_error("could not open " + rel);
		r = SheetInfo(*activeSpriteSheetDoc, project.DisplayPath(activeSpriteSheetDoc->absolutePath));
		r["ok"] = true;
		return true;
	}

	// {"cmd":"edit_spritesheet","args":{ ...create_spritesheet's keys..., and:
	//    "frame":{"name":"hero_3","rename":"jump_up","rect":[0,0,32,32],
	//             "pivot":[0.5,0],"border":[4,4,4,4],"duration":0.2},
	//    "addFrame":{"name":"extra","rect":[64,0,16,16]},
	//    "removeFrames":["extra", 7],
	//    "addClip":{"name":"idle","frames":[0,1,2,1],"fps":6},
	//    "clip":{"name":"idle","fps":8,"pingPong":true,"rename":"stand"},
	//    "removeClip":"stand"}}
	// Edits the sheet open in the Sprite Sheet editor ("path" opens it first).
	// Each change is one undo entry, as the panel makes them; save_spritesheet
	// writes the file.
	if (name == "edit_spritesheet")
	{
		if (!A("path").empty() && !OpenSpriteSheetDocument(project.AbsolutePath(A("path"))))
			throw std::runtime_error("could not open " + A("path"));
		if (!activeSpriteSheetDoc) throw std::runtime_error("no sprite sheet open - open_spritesheet first, or pass 'path'");
		nlohmann::json edits = a.is_object() ? a : nlohmann::json::object();
		edits.erase("path");
		std::string err;
		if (!ApplySheetEdits(*activeSpriteSheetDoc, edits, err)) throw std::runtime_error("edit_spritesheet: " + err);
		r = SheetInfo(*activeSpriteSheetDoc, project.DisplayPath(activeSpriteSheetDoc->absolutePath));
		r["ok"] = true;
		return true;
	}

	// {"cmd":"spritesheet_info"} - the open sheet; {"path":...} reads one
	// from disk without opening it.
	if (name == "spritesheet_info")
	{
		if (!A("path").empty())
		{
			SpriteSheetDocument d;
			d.projectRoot = project.IsOpen() ? project.GetProjectPath() : std::string();
			std::string err;
			if (!d.LoadFromFile(project.AbsolutePath(A("path")), err)) throw std::runtime_error(err);
			r = SheetInfo(d, project.DisplayPath(d.absolutePath));
		}
		else
		{
			if (!activeSpriteSheetDoc) throw std::runtime_error("no sprite sheet open");
			r = SheetInfo(*activeSpriteSheetDoc, project.DisplayPath(activeSpriteSheetDoc->absolutePath));
		}
		r["ok"] = true;
		return true;
	}

	// {"cmd":"save_spritesheet"} - writes the focused sheet, and every open
	// scene's sprites that draw from it pick the change up.
	if (name == "save_spritesheet")
	{
		if (!activeSpriteSheetDoc) throw std::runtime_error("no sprite sheet open");
		if (!SaveSpriteSheetDocument(activeSpriteSheetDoc))
			throw std::runtime_error("could not save the sprite sheet");
		r["ok"] = true;
		r["path"] = project.DisplayPath(activeSpriteSheetDoc->absolutePath);
		return true;
	}

	return false;
}
