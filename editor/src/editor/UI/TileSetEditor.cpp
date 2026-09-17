//=============================================================================
// Name        : TileSetEditor.cpp
// Description : See the header.
//=============================================================================

#include "TileSetEditor.h"
#include "../TileSetDocument.h"
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <algorithm>
#include <string>
#include <vector>

using namespace p3d;

namespace TileSetEditor {

	namespace {
		// Selection lives here rather than on the document: it is a view
		// concern, and a tileset reopened tomorrow should not remember which
		// cell was highlighted.
		int g_anchor = -1;
		std::vector<int32> g_selection;
		std::string g_newTag;
		float g_zoom = 2.0f;

		bool IsSelected(const int32 i)
		{
			return std::find(g_selection.begin(), g_selection.end(), i) != g_selection.end();
		}
	}

	void DrawWindow(TileSetDocument& doc, void* atlasTexId, FrameRequests& requests)
	{
		TileSet2D& set = doc.set;

		// --- toolbar --------------------------------------------------------
		if (ImGui::Button("Save")) requests.save = true;
		ImGui::SameLine();
		ImGui::BeginDisabled(!doc.undo.CanUndo());
		if (ImGui::Button("Undo")) doc.undo.Undo();
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!doc.undo.CanRedo());
		if (ImGui::Button("Redo")) doc.undo.Redo();
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::TextDisabled("%s", doc.set.image.c_str());

		ImGui::Separator();

		// --- how the sheet is cut ------------------------------------------
		int tw = set.tileW, th = set.tileH, mg = set.margin, sp = set.spacing, cols = set.columns;
		ImGui::SetNextItemWidth(90); ImGui::InputInt("Tile W", &tw); ImGui::SameLine();
		ImGui::SetNextItemWidth(90); ImGui::InputInt("Tile H", &th); ImGui::SameLine();
		ImGui::SetNextItemWidth(90); ImGui::InputInt("Margin", &mg); ImGui::SameLine();
		ImGui::SetNextItemWidth(90); ImGui::InputInt("Spacing", &sp);
		ImGui::SetNextItemWidth(90); ImGui::InputInt("Columns (0 = derive)", &cols);
		if (tw != set.tileW || th != set.tileH || mg != set.margin
			|| sp != set.spacing || cols != set.columns)
		{
			// Re-cutting changes what every index means, so anything selected
			// now refers to a different cell. Drop it rather than silently
			// repointing it.
			doc.SetGrid(tw, th, mg, sp, cols);
			g_selection.clear();
			g_anchor = -1;
		}

		if (!set.HaveImageSize())
		{
			ImGui::TextColored(ImVec4(1.f, 0.5f, 0.4f, 1.f),
				"The atlas could not be read, so the sheet cannot be cut.");
			return;
		}

		ImGui::Text("Grid: %d x %d  =  %d tiles      Solid: %d",
			(int)set.Columns(), (int)set.Rows(), (int)set.TileCount(), doc.SolidCount());
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120);
		ImGui::SliderFloat("Zoom", &g_zoom, 1.0f, 6.0f, "%.1fx");

		ImGui::Separator();
		ImGui::TextDisabled("Click a cell to select, shift-click for a range. "
			"Space or the button below toggles solid.");

		// --- the sheet ------------------------------------------------------
		const int cols2 = set.Columns() > 0 ? set.Columns() : 1;
		const float cell = set.tileW * g_zoom;
		const float cellH = set.tileH * g_zoom;

		ImGui::BeginChild("##sheet", ImVec2(0, -150), true,
			ImGuiWindowFlags_HorizontalScrollbar);
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec2 origin = ImGui::GetCursorScreenPos();

		for (int32 i = 0; i < set.TileCount(); i++)
		{
			const int cx = i % cols2, cy = i / cols2;
			const ImVec2 p0(origin.x + cx * (cell + 2.f), origin.y + cy * (cellH + 2.f));
			const ImVec2 p1(p0.x + cell, p0.y + cellH);

			if (atlasTexId)
			{
				const Vec4 uv = set.UVRect(i);
				dl->AddImage((ImTextureID)atlasTexId, p0, p1,
					ImVec2(uv.x, uv.y), ImVec2(uv.z, uv.w));
			}
			else
				dl->AddRectFilled(p0, p1, IM_COL32(60, 60, 70, 255));

			// Solid is the whole point of this editor, so it is shown as a
			// filled wash over the cell rather than a corner dot - readable at
			// a glance across a 256-tile sheet.
			if (set.IsSolid(i))
			{
				// The collision outline, drawn as the shape it actually is.
				// A slope shown as a full blue square would be the same
				// picture as the block it was drawn to replace.
				const int32 sh = set.Shape(i);
				if (sh == TileShape2D::Box)
				{
					dl->AddRectFilled(p0, p1, IM_COL32(80, 170, 255, 70));
					dl->AddRect(p0, p1, IM_COL32(110, 200, 255, 220), 0.f, 0, 2.f);
				}
				else if (TileShape2DIsFloorProfile(sh) && sh != TileShape2D::SlopeBR
					&& sh != TileShape2D::SlopeBL)
				{
					// Curved floors, drawn from the SAME profile the collider
					// is built from - so what the sheet shows and what the
					// physics does cannot drift apart.
					const int kN = 12;
					ImVec2 pts[kN + 3];
					for (int k = 0; k <= kN; k++)
					{
						const float t = (float)k / (float)kN;
						const float hgt = TileShape2DHeight(sh, t);
						pts[k] = ImVec2(p0.x + t * (p1.x - p0.x),
							p1.y - hgt * (p1.y - p0.y));
					}
					pts[kN + 1] = ImVec2(p1.x, p1.y);
					pts[kN + 2] = ImVec2(p0.x, p1.y);
					dl->AddConvexPolyFilled(pts, kN + 3, IM_COL32(80, 170, 255, 70));
					dl->AddPolyline(pts, kN + 1, IM_COL32(110, 200, 255, 220), 0, 2.f);
				}
				else
				{
					// ImGui's y runs DOWN, so "bottom" here is p1.y.
					const ImVec2 bl(p0.x, p1.y), br(p1.x, p1.y);
					const ImVec2 tl(p0.x, p0.y), tr(p1.x, p0.y);
					ImVec2 a2, b2, c2;
					switch (sh)
					{
						case TileShape2D::SlopeBR: a2 = bl; b2 = br; c2 = tr; break;
						case TileShape2D::SlopeBL: a2 = bl; b2 = br; c2 = tl; break;
						case TileShape2D::SlopeTR: a2 = br; b2 = tr; c2 = tl; break;
						default:                   a2 = bl; b2 = tr; c2 = tl; break;
					}
					dl->AddTriangleFilled(a2, b2, c2, IM_COL32(80, 170, 255, 70));
					dl->AddTriangle(a2, b2, c2, IM_COL32(110, 200, 255, 220), 2.f);
				}
			}
			// An animated tile is marked, because the sheet otherwise gives no
			// hint that painting this cell puts a moving thing in the level.
			if (set.AutoTileForTile(i) >= 0)
				dl->AddRectFilled(ImVec2(p0.x + 1, p1.y - 5), ImVec2(p0.x + 7, p1.y - 1),
					IM_COL32(120, 235, 140, 230));
			if (set.AnimForTile(i) >= 0)
				dl->AddTriangleFilled(ImVec2(p1.x - 7, p0.y + 1), ImVec2(p1.x - 1, p0.y + 4),
					ImVec2(p1.x - 7, p0.y + 7), IM_COL32(255, 230, 120, 255));
			if (IsSelected(i))
				dl->AddRect(ImVec2(p0.x - 1, p0.y - 1), ImVec2(p1.x + 1, p1.y + 1),
					IM_COL32(255, 210, 90, 255), 0.f, 0, 2.f);

			ImGui::SetCursorScreenPos(p0);
			ImGui::PushID(i);
			if (ImGui::InvisibleButton("##c", ImVec2(cell, cellH)))
			{
				if (ImGui::GetIO().KeyShift && g_anchor >= 0)
				{
					g_selection.clear();
					const int lo = g_anchor < i ? g_anchor : i;
					const int hi = g_anchor < i ? i : g_anchor;
					for (int k = lo; k <= hi; k++) g_selection.push_back(k);
				}
				else if (ImGui::GetIO().KeyCtrl)
				{
					if (IsSelected(i))
						g_selection.erase(std::remove(g_selection.begin(), g_selection.end(), i),
							g_selection.end());
					else g_selection.push_back(i);
					g_anchor = i;
				}
				else { g_selection.assign(1, i); g_anchor = i; }
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("tile %d%s", (int)i, set.IsSolid(i) ? "  (solid)" : "");
			ImGui::PopID();
		}
		// Reserve the grid's area so the child scrolls. NOT by leaving the
		// cursor past the last row: ImGui asserts on using SetCursorPos to
		// extend a window's boundaries (ErrorCheckUsingSetCursorPosToExtend-
		// ParentBoundaries), and the assert aborts the process rather than
		// drawing wrong - every cell above is placed with SetCursorScreenPos,
		// which contributes nothing to the content size on its own.
		const int rows = (set.TileCount() + cols2 - 1) / cols2;
		ImGui::SetCursorScreenPos(origin);
		ImGui::Dummy(ImVec2(cols2 * (cell + 2.f), rows * (cellH + 2.f)));
		ImGui::EndChild();

		// --- what the selection is ------------------------------------------
		if (g_selection.empty())
		{
			ImGui::TextDisabled("No cell selected.");
			return;
		}

		ImGui::Text("%d cell%s selected", (int)g_selection.size(),
			g_selection.size() == 1 ? "" : "s");

		bool allSolid = true;
		for (size_t k = 0; k < g_selection.size(); k++)
			if (!set.IsSolid(g_selection[k])) { allSolid = false; break; }

		// The Space shortcut has to be gated on this window having focus AND
		// on no text field wanting the key. Unqualified, ImGui's key state is
		// global: Space typed into the "new tag" box below - or pressed over
		// the Scene View while this window merely sat open behind it - came
		// through here and silently flipped the selection's solidity. A
		// collision flag changing because you typed a space in a tag name is
		// the kind of edit nobody thinks to look for.
		const bool spaceHere = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
			&& !ImGui::GetIO().WantTextInput
			&& ImGui::IsKeyPressed(ImGuiKey_Space);
		if (ImGui::Button(allSolid ? "Make Passable" : "Make Solid", ImVec2(150, 0)) || spaceHere)
			doc.SetSolidRange(g_selection, !allSolid);
		ImGui::SameLine();
		ImGui::TextDisabled("Solid cells are what a tile map turns into colliders.");

		// Collision outline. Slopes exist because marking a diagonal tile
		// solid used to give it a full square collider - the art sloped and
		// the collision did not, so a "hill" walked like a staircase.
		ImGui::TextDisabled("Collision shape");
		int32 cur = set.Shape(g_selection[0]);
		bool mixed = false;
		for (size_t k = 1; k < g_selection.size(); k++)
			if (set.Shape(g_selection[k]) != cur) { mixed = true; break; }

		struct ShapeBtn { int32 shape; const char* label; const char* tip; };
		static const ShapeBtn kShapes[] = {
			{ TileShape2D::Box,     "Square",  "A full cell. What every solid tile was before." },
			{ TileShape2D::SlopeBR, "Floor /", "Straight floor rising to the RIGHT." },
			{ TileShape2D::SlopeBL, "Floor \\", "Straight floor rising to the LEFT." },
			{ TileShape2D::SlopeTR, "Ceil /",  "Ceiling sloping down to the LEFT." },
			{ TileShape2D::SlopeTL, "Ceil \\", "Ceiling sloping down to the RIGHT." },
			{ TileShape2D::ArcConvexBR,  "Crest /",  "CURVED floor rising right, bulging up - a hill crest." },
			{ TileShape2D::ArcConvexBL,  "Crest \\", "Curved floor rising left, bulging up." },
			{ TileShape2D::ArcConcaveBR, "Dip /",    "Curved floor rising right, dished down - a valley or loop wall." },
			{ TileShape2D::ArcConcaveBL, "Dip \\",   "Curved floor rising left, dished down." },
		};
		for (int b3i = 0; b3i < 9; b3i++)
		{
			if (b3i % 5) ImGui::SameLine();
			const bool on = !mixed && cur == kShapes[b3i].shape;
			if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.9f, 1.f));
			if (ImGui::Button(kShapes[b3i].label, ImVec2(72, 0)))
				doc.SetShapeRange(g_selection, kShapes[b3i].shape);
			if (on) ImGui::PopStyleColor();
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", kShapes[b3i].tip);
		}
		if (mixed) { ImGui::SameLine(); ImGui::TextDisabled("(mixed)"); }
		ImGui::TextDisabled("Picking a slope also marks the tile solid.");

		// --- terrain ---------------------------------------------------------
		// Sixteen consecutive tiles, indexed by which orthogonal neighbours
		// share the group. Painting the GROUP then picks the corner and edge
		// pieces, which is the difference between painting a level and
		// assembling one by hand.
		ImGui::TextDisabled("Terrain");
		{
			const int32 first = g_selection[0];
			const int32 grp = set.AutoTileForTile(first);
			if (grp >= 0)
			{
				ImGui::Text("tile %d is part of terrain \"%s\" (base %d)", (int)first,
					set.autotiles[(size_t)grp].name.empty() ? "unnamed"
						: set.autotiles[(size_t)grp].name.c_str(),
					(int)set.autotiles[(size_t)grp].base);
				ImGui::SameLine();
				if (ImGui::Button("Remove terrain")) doc.RemoveAutoTileForTile(first);
			}
			else if (first + TileSet2D::kAutoTileCount <= set.TileCount())
			{
				static std::string newName;
				ImGui::SetNextItemWidth(140);
				ImGui::InputTextWithHint("##terrname", "name (e.g. grass)", &newName);
				ImGui::SameLine();
				if (ImGui::Button("Make terrain from 16"))
				{
					doc.AddAutoTile(first, newName);
					newName.clear();
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Uses tile %d and the 15 after it, in neighbour-mask\n"
						"order: bit 1 north, 2 east, 4 south, 8 west.\n"
						"So +0 is an isolated lump and +15 is fully surrounded.", (int)first);
			}
			else
				ImGui::TextDisabled("Needs 16 tiles from here to the end of the sheet.");
		}

		// --- animation -------------------------------------------------------
		// The tile you PAINT is the first cell selected; the rest are frames.
		// A map stores only that key, so re-timing here re-times every map
		// using the tileset and nothing in a scene has to change.
		ImGui::TextDisabled("Animation");
		const int32 keyTile = g_selection[0];
		const int32 existing = set.AnimForTile(keyTile);
		if (existing >= 0)
		{
			float fps = set.anims[(size_t)existing].fps;
			ImGui::Text("tile %d cycles %d frames", (int)keyTile,
				(int)set.anims[(size_t)existing].frames.size());
			ImGui::SetNextItemWidth(120);
			if (ImGui::DragFloat("fps##anim", &fps, 0.25f, 0.25f, 60.f, "%.2f"))
				doc.SetAnimFps(existing, fps);
			ImGui::SameLine();
			if (ImGui::Button("Remove animation")) doc.RemoveAnimForTile(keyTile);
		}
		else if (g_selection.size() >= 2)
		{
			static float newFps = 8.f;
			ImGui::SetNextItemWidth(120);
			ImGui::DragFloat("fps##newanim", &newFps, 0.25f, 0.25f, 60.f, "%.2f");
			ImGui::SameLine();
			if (ImGui::Button("Animate selection"))
				doc.AddAnim(g_selection, newFps);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("The first cell selected becomes the tile you paint;\n"
					"the rest are its frames, in order.");
		}
		else
			ImGui::TextDisabled("Select two or more cells to make an animation.");

		// --- tags ------------------------------------------------------------
		const std::vector<std::string> tags = doc.AllTags();
		if (!tags.empty())
		{
			ImGui::Text("Tags:");
			for (size_t t = 0; t < tags.size(); t++)
			{
				ImGui::SameLine();
				// Tri-state in effect: checked when every selected cell has it.
				bool all = true;
				for (size_t k = 0; k < g_selection.size(); k++)
				{
					const TileInfo2D* info = set.Find(g_selection[k]);
					const bool has = info && std::find(info->tags.begin(), info->tags.end(),
						tags[t]) != info->tags.end();
					if (!has) { all = false; break; }
				}
				bool v = all;
				ImGui::PushID((int)t + 5000);
				if (ImGui::Checkbox(tags[t].c_str(), &v))
					for (size_t k = 0; k < g_selection.size(); k++)
						doc.SetTag(g_selection[k], tags[t], v);
				ImGui::PopID();
			}
		}

		ImGui::SetNextItemWidth(160);
		ImGui::InputTextWithHint("##newtag", "new tag (e.g. ice)", &g_newTag);
		ImGui::SameLine();
		if (ImGui::Button("Add Tag") && !g_newTag.empty())
		{
			for (size_t k = 0; k < g_selection.size(); k++)
				doc.SetTag(g_selection[k], g_newTag, true);
			g_newTag.clear();
		}
		ImGui::SameLine();
		ImGui::TextDisabled("Tags mean nothing to the engine - they are for your scripts.");
	}

} // namespace TileSetEditor
