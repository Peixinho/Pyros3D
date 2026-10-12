//=============================================================================
// Name        : SpriteSheetEditor.cpp
// Description : See the header.
//=============================================================================

#include "SpriteSheetEditor.h"
#include "../SpriteSheetDocument.h"
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace p3d;

namespace SpriteSheetEditor {

	namespace {

		bool IsSelected(const SpriteSheetDocument& doc, const int32 i)
		{
			return std::find(doc.view.selection.begin(), doc.view.selection.end(), i) != doc.view.selection.end();
		}

		// "0-3, 7, 5" <-> a frame list. Ranges may run backwards.
		std::string FramesToText(const std::vector<int32>& frames)
		{
			std::string out;
			size_t i = 0;
			while (i < frames.size())
			{
				size_t j = i;
				// A run of consecutive ascending frames is written a-b.
				while (j + 1 < frames.size() && frames[j + 1] == frames[j] + 1) j++;
				if (!out.empty()) out += ", ";
				if (j >= i + 2) out += std::to_string(frames[i]) + "-" + std::to_string(frames[j]);
				else { out += std::to_string(frames[i]); j = i; }
				i = j + 1;
			}
			return out;
		}

		bool TextToFrames(const std::string& text, const int32 count, std::vector<int32>& out)
		{
			out.clear();
			std::string tok;
			std::stringstream ss(text);
			while (std::getline(ss, tok, ','))
			{
				// Trim.
				size_t a = tok.find_first_not_of(" \t"), b = tok.find_last_not_of(" \t");
				if (a == std::string::npos) continue;
				tok = tok.substr(a, b - a + 1);
				const size_t dash = tok.find('-', 1);
				try {
					if (dash == std::string::npos)
					{
						const int v = std::stoi(tok);
						if (v < 0 || v >= count) return false;
						out.push_back(v);
					}
					else
					{
						const int from = std::stoi(tok.substr(0, dash)), to = std::stoi(tok.substr(dash + 1));
						if (from < 0 || to < 0 || from >= count || to >= count) return false;
						const int dir = to >= from ? 1 : -1;
						for (int v = from; v != to + dir; v += dir) out.push_back(v);
					}
				}
				catch (const std::exception&) { return false; }
			}
			return !out.empty();
		}

		void Checker(ImDrawList* dl, const ImVec2& p0, const ImVec2& p1)
		{
			// Two flat tones: enough to tell transparent from black art
			// without a pattern fine enough to fight pixel art.
			dl->AddRectFilled(p0, p1, IM_COL32(52, 54, 60, 255));
			const float step = 16.f;
			for (float y = p0.y; y < p1.y; y += step)
				for (float x = p0.x + (((int)((y - p0.y) / step)) % 2 ? step : 0.f); x < p1.x; x += step * 2.f)
					dl->AddRectFilled(ImVec2(x, y), ImVec2(std::min(x + step, p1.x), std::min(y + step, p1.y)),
						IM_COL32(64, 66, 73, 255));
		}

		// One frame of the sheet into a box, fitted and with its pivot marked.
		void DrawFrameInBox(ImDrawList* dl, const SpriteSheet2D& sheet, const int32 frame,
			void* tex, const ImVec2& p0, const ImVec2& size, const bool showPivot)
		{
			Checker(dl, p0, ImVec2(p0.x + size.x, p0.y + size.y));
			if (!sheet.ValidFrame(frame) || !tex) return;
			const SpriteFrame2D& f = sheet.frames[(size_t)frame];
			// Fitted to the SOURCE size and placed by the trim, so a clip of
			// trimmed frames holds still in the preview the way it will in
			// the game.
			const float sw = (float)f.SourceW(), sh = (float)f.SourceH();
			const float k = std::min(size.x / sw, size.y / sh);
			const ImVec2 o(p0.x + (size.x - sw * k) * 0.5f, p0.y + (size.y - sh * k) * 0.5f);
			const ImVec2 a(o.x + f.offsetX * k, o.y + f.offsetY * k);
			const Vec4 uv = sheet.UVRect(frame);
			dl->AddImage((ImTextureID)tex, a, ImVec2(a.x + f.w * k, a.y + f.h * k), ImVec2(uv.x, uv.y), ImVec2(uv.z, uv.w));
			if (showPivot)
			{
				const ImVec2 pv(o.x + f.pivot.x * sw * k, o.y + (1.f - f.pivot.y) * sh * k);
				dl->AddLine(ImVec2(pv.x - 6, pv.y), ImVec2(pv.x + 6, pv.y), IM_COL32(255, 90, 90, 255), 1.5f);
				dl->AddLine(ImVec2(pv.x, pv.y - 6), ImVec2(pv.x, pv.y + 6), IM_COL32(255, 90, 90, 255), 1.5f);
			}
		}

		void DrawCanvas(SpriteSheetDocument& doc, void* tex)
		{
			SpriteSheet2D& sheet = doc.sheet;
			SpriteSheetDocument::View& v = doc.view;
			const float iw = (float)sheet.GetImageWidth(), ih = (float)sheet.GetImageHeight();

			// The side panel takes what it needs and no more than about two
			// fifths of a narrow window: the image is what is being worked on.
			const float sideW = std::max(240.f, std::min(340.f, ImGui::GetContentRegionAvail().x * 0.42f));
			ImGui::BeginChild("##canvas", ImVec2(-sideW, 0), true,
				ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoMove);
			const ImVec2 avail = ImGui::GetContentRegionAvail();
			if (v.fitPending && avail.x > 8.f && avail.y > 8.f && iw > 0.f)
			{
				// Whole image visible, at a whole-number zoom when it is
				// small enough for that to matter (pixel art).
				float z = std::min(avail.x / iw, avail.y / ih);
				if (z >= 1.f) z = floorf(z);
				v.zoom = std::max(0.1f, std::min(z, 16.f));
				v.fitPending = false;
			}

			ImDrawList* dl = ImGui::GetWindowDrawList();
			const ImVec2 origin = ImGui::GetCursorScreenPos();
			const ImVec2 size(iw * v.zoom, ih * v.zoom);
			ImGui::InvisibleButton("##image", ImVec2(std::max(size.x, 1.f), std::max(size.y, 1.f)));
			const bool hovered = ImGui::IsItemHovered();

			Checker(dl, origin, ImVec2(origin.x + size.x, origin.y + size.y));
			if (tex) dl->AddImage((ImTextureID)tex, origin, ImVec2(origin.x + size.x, origin.y + size.y));

			// Image pixel under the cursor.
			const ImVec2 mouse = ImGui::GetIO().MousePos;
			const float mx = (mouse.x - origin.x) / v.zoom, my = (mouse.y - origin.y) / v.zoom;

			// Frames. Unselected ones first so a selected frame's outline is
			// never hidden under a neighbour's.
			for (int pass = 0; pass < 2; pass++)
				for (int32 i = 0; i < (int32)sheet.frames.size(); i++)
				{
					const bool sel = IsSelected(doc, i);
					if ((pass == 1) != sel) continue;
					const SpriteFrame2D& f = sheet.frames[(size_t)i];
					const ImVec2 a(origin.x + f.x * v.zoom, origin.y + f.y * v.zoom);
					const ImVec2 b(a.x + f.w * v.zoom, a.y + f.h * v.zoom);
					if (sel) dl->AddRectFilled(a, b, IM_COL32(90, 170, 255, 45));
					dl->AddRect(a, b, sel ? IM_COL32(110, 200, 255, 255) : IM_COL32(255, 255, 255, 70), 0.f, 0, sel ? 2.f : 1.f);
					if (f.w * v.zoom > 26.f && f.h * v.zoom > 16.f)
					{
						char label[16];
						snprintf(label, sizeof(label), "%d", (int)i);
						dl->AddText(ImVec2(a.x + 3, a.y + 2), IM_COL32(255, 255, 255, sel ? 255 : 130), label);
					}
					if (!sel) continue;
					// Nine-slice borders: the lines the stretch happens between.
					if (f.HasBorder())
					{
						const ImU32 g = IM_COL32(120, 255, 140, 220);
						const float l = a.x + f.border.x * v.zoom, r = b.x - f.border.z * v.zoom;
						const float t = a.y + f.border.w * v.zoom, bt = b.y - f.border.y * v.zoom;
						dl->AddLine(ImVec2(l, a.y), ImVec2(l, b.y), g);
						dl->AddLine(ImVec2(r, a.y), ImVec2(r, b.y), g);
						dl->AddLine(ImVec2(a.x, t), ImVec2(b.x, t), g);
						dl->AddLine(ImVec2(a.x, bt), ImVec2(b.x, bt), g);
					}
					// Pivot, over the source rectangle.
					const float sx = a.x - f.offsetX * v.zoom, sy = a.y - f.offsetY * v.zoom;
					const ImVec2 pv(sx + f.pivot.x * f.SourceW() * v.zoom, sy + (1.f - f.pivot.y) * f.SourceH() * v.zoom);
					dl->AddCircleFilled(pv, 4.f, IM_COL32(255, 90, 90, 255));
					dl->AddCircle(pv, 5.5f, IM_COL32(20, 20, 20, 255), 0, 1.5f);
				}

			// --- the selected frame's handles -------------------------------
			// One frame selected: its corners and sides resize it, its body
			// moves it, its pivot dot moves the pivot. A frame the slicer got
			// a pixel wrong, or a pivot that belongs on the feet, is a drag
			// here and not four numbers in the side panel.
			const ImGuiIO& hio = ImGui::GetIO();
			const bool plain = !hio.KeyAlt && !hio.KeyShift && !hio.KeyCtrl && !hio.KeySuper;
			const bool single = v.tool == 0 && v.selection.size() == 1 && sheet.ValidFrame(v.selection[0]);
			int hot = -1;
			if (single)
			{
				const SpriteFrame2D& f = sheet.frames[(size_t)v.selection[0]];
				const float ax = origin.x + f.x * v.zoom, ay = origin.y + f.y * v.zoom;
				const float bx = ax + f.w * v.zoom, by = ay + f.h * v.zoom;
				const float xs[3] = { ax, (ax + bx) * 0.5f, bx };
				const float ys[3] = { ay, (ay + by) * 0.5f, by };
				const ImVec2 pv(ax - f.offsetX * v.zoom + f.pivot.x * f.SourceW() * v.zoom,
					ay - f.offsetY * v.zoom + (1.f - f.pivot.y) * f.SourceH() * v.zoom);
				// The side handles are left off a frame too small on screen
				// to hold them apart from its corners.
				const bool sidesX = (bx - ax) > 28.f, sidesY = (by - ay) > 28.f;
				// The canvas, not the image: a frame that ends at the image's
				// edge has its handles half outside it, and the half outside
				// has to work too.
				const bool overCanvas = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
				if (v.grab >= 0) hot = v.grab;
				else if (overCanvas && plain)
				{
					if (fabsf(mouse.x - pv.x) <= 6.f && fabsf(mouse.y - pv.y) <= 6.f) hot = 9;
					else
						for (int hy = 0; hy < 3 && hot < 0; hy++)
							for (int hx = 0; hx < 3 && hot < 0; hx++)
							{
								if (hx == 1 && hy == 1) continue;
								if ((hx == 1 && !sidesX) || (hy == 1 && !sidesY)) continue;
								if (fabsf(mouse.x - xs[hx]) <= 5.f && fabsf(mouse.y - ys[hy]) <= 5.f) hot = hy * 3 + hx;
							}
				}
				for (int hy = 0; hy < 3; hy++)
					for (int hx = 0; hx < 3; hx++)
					{
						if (hx == 1 && hy == 1) continue;
						if ((hx == 1 && !sidesX) || (hy == 1 && !sidesY)) continue;
						const bool on = hot == hy * 3 + hx;
						const float r = on ? 4.5f : 3.5f;
						dl->AddRectFilled(ImVec2(xs[hx] - r, ys[hy] - r), ImVec2(xs[hx] + r, ys[hy] + r),
							on ? IM_COL32(255, 255, 255, 255) : IM_COL32(110, 200, 255, 255));
						dl->AddRect(ImVec2(xs[hx] - r, ys[hy] - r), ImVec2(xs[hx] + r, ys[hy] + r), IM_COL32(20, 20, 20, 255));
					}
				if (hot == 9) dl->AddCircle(pv, 8.f, IM_COL32(255, 255, 255, 255), 0, 1.5f);
				if (hot == 9 || hot == 4) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
				else if (hot == 0 || hot == 8) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
				else if (hot == 2 || hot == 6) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW);
				else if (hot == 1 || hot == 7) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
				else if (hot == 3 || hot == 5) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			}
			else if (v.grab >= 0)
			{
				// The selection changed under a held handle (an undo, a
				// re-slice): there is nothing left to drag.
				doc.EndEdit("Edit Frame");
				v.grab = -1;
			}

			if (v.grab >= 0)
			{
				SpriteFrame2D& f = sheet.frames[(size_t)v.selection[0]];
				const int W = (int)iw, H = (int)ih;
				if (v.grab == 9)
				{
					// Whole pixels: a pivot between two texels is a sprite
					// that shimmers when it is drawn pixel-snapped.
					const float px = (floorf(mx + 0.5f) - (float)(f.x - f.offsetX)) / (float)std::max(1, f.SourceW());
					const float py = 1.f - (floorf(my + 0.5f) - (float)(f.y - f.offsetY)) / (float)std::max(1, f.SourceH());
					const Vec2 np(std::max(-1.f, std::min(px, 2.f)), std::max(-1.f, std::min(py, 2.f)));
					if (np.x != f.pivot.x || np.y != f.pivot.y) { f.pivot = np; doc.dirty = true; }
				}
				else
				{
					const int dx = (int)floorf(mx - v.grabX + 0.5f), dy = (int)floorf(my - v.grabY + 0.5f);
					int x0 = v.grabRect[0], y0 = v.grabRect[1];
					int x1 = x0 + v.grabRect[2], y1 = y0 + v.grabRect[3];
					const int hx = v.grab % 3, hy = v.grab / 3;
					if (v.grab == 4)
					{
						// Moved whole, and stopped at the image's edge with
						// its size intact.
						const int mxd = std::max(-x0, std::min(dx, W - x1));
						const int myd = std::max(-y0, std::min(dy, H - y1));
						x0 += mxd; x1 += mxd; y0 += myd; y1 += myd;
					}
					else
					{
						if (hx == 0) x0 = std::max(0, std::min(x0 + dx, x1 - 1));
						if (hx == 2) x1 = std::max(x0 + 1, std::min(x1 + dx, W));
						if (hy == 0) y0 = std::max(0, std::min(y0 + dy, y1 - 1));
						if (hy == 2) y1 = std::max(y0 + 1, std::min(y1 + dy, H));
					}
					if (x0 != f.x || y0 != f.y || x1 - x0 != f.w || y1 - y0 != f.h)
					{
						f.x = x0; f.y = y0; f.w = x1 - x0; f.h = y1 - y0;
						doc.dirty = true;
					}
					ImGui::SetTooltip("%d, %d   %d x %d", f.x, f.y, f.w, f.h);
				}
				if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
				{
					doc.EndEdit(v.grab == 9 ? "Move Pivot" : (v.grab == 4 ? "Move Frame" : "Resize Frame"));
					v.grab = -1;
				}
			}
			else if (hot >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				const SpriteFrame2D& f = sheet.frames[(size_t)v.selection[0]];
				v.grab = hot;
				v.grabX = mx; v.grabY = my;
				v.grabRect[0] = f.x; v.grabRect[1] = f.y; v.grabRect[2] = f.w; v.grabRect[3] = f.h;
				doc.BeginEdit();
			}
			else if (hovered && v.tool == 1)
			{
				// Drawing a frame: press, drag, release.
				if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) { v.dragging = true; v.dragX0 = mx; v.dragY0 = my; }
			}
			else if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				// The smallest frame under the cursor, so a frame inside
				// another can still be picked.
				int32 hit = -1;
				int64 hitArea = 0;
				for (int32 i = 0; i < (int32)sheet.frames.size(); i++)
				{
					const SpriteFrame2D& f = sheet.frames[(size_t)i];
					if (mx < f.x || my < f.y || mx >= f.x + f.w || my >= f.y + f.h) continue;
					const int64 area = (int64)f.w * f.h;
					if (hit < 0 || area < hitArea) { hit = i; hitArea = area; }
				}
				const ImGuiIO& io = ImGui::GetIO();
				if (io.KeyAlt && !v.selection.empty())
				{
					// Alt-click: put the selection's pivot here.
					const int32 pi = v.selection.back();
					const SpriteFrame2D& f = sheet.frames[(size_t)pi];
					const float px = (mx - (f.x - f.offsetX)) / (float)f.SourceW();
					const float py = 1.f - (my - (f.y - f.offsetY)) / (float)f.SourceH();
					doc.SetPivot(v.selection, Vec2(px, py));
				}
				else if (hit < 0) { if (!io.KeyShift && !io.KeyCtrl && !io.KeySuper) { v.selection.clear(); v.anchor = -1; } }
				else if (io.KeyShift && v.anchor >= 0)
				{
					v.selection.clear();
					const int dir = hit >= v.anchor ? 1 : -1;
					for (int i = v.anchor; i != hit + dir; i += dir) v.selection.push_back(i);
				}
				else if (io.KeyCtrl || io.KeySuper)
				{
					std::vector<int32>::iterator at = std::find(v.selection.begin(), v.selection.end(), hit);
					if (at != v.selection.end()) v.selection.erase(at);
					else v.selection.push_back(hit);
					v.anchor = hit;
				}
				else if (single && hit == v.selection[0])
				{
					// Pressed on the frame already selected: it is being
					// moved. (A click that goes nowhere moves it nowhere.)
					const SpriteFrame2D& f = sheet.frames[(size_t)hit];
					v.grab = 4;
					v.grabX = mx; v.grabY = my;
					v.grabRect[0] = f.x; v.grabRect[1] = f.y; v.grabRect[2] = f.w; v.grabRect[3] = f.h;
					doc.BeginEdit();
				}
				else { v.selection.assign(1, hit); v.anchor = hit; }
			}

			if (v.dragging)
			{
				const float x0 = std::max(0.f, std::min(std::min(v.dragX0, mx), iw));
				const float y0 = std::max(0.f, std::min(std::min(v.dragY0, my), ih));
				const float x1 = std::max(0.f, std::min(std::max(v.dragX0, mx), iw));
				const float y1 = std::max(0.f, std::min(std::max(v.dragY0, my), ih));
				// Snapped to whole pixels: a frame is a rectangle of texels.
				const int rx = (int)floorf(x0), ry = (int)floorf(y0);
				const int rw = (int)ceilf(x1) - rx, rh = (int)ceilf(y1) - ry;
				dl->AddRect(ImVec2(origin.x + rx * v.zoom, origin.y + ry * v.zoom),
					ImVec2(origin.x + (rx + rw) * v.zoom, origin.y + (ry + rh) * v.zoom), IM_COL32(255, 220, 90, 255), 0.f, 0, 2.f);
				if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
				{
					v.dragging = false;
					if (rw >= 2 && rh >= 2)
					{
						const int made = doc.AddFrame(rx, ry, rw, rh, std::string());
						if (made >= 0) { v.selection.assign(1, made); v.anchor = made; }
					}
				}
			}

			// Ctrl/Cmd + wheel zooms about the cursor's column of the image;
			// the plain wheel scrolls, as it does everywhere.
			if (hovered && (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper) && ImGui::GetIO().MouseWheel != 0.f)
				v.zoom = std::max(0.1f, std::min(v.zoom * (ImGui::GetIO().MouseWheel > 0.f ? 1.25f : 0.8f), 32.f));

			if (hovered && v.grab < 0 && mx >= 0 && my >= 0 && mx < iw && my < ih)
				ImGui::SetTooltip("%d, %d", (int)mx, (int)my);
			ImGui::EndChild();
		}

		void DrawSheetSection(SpriteSheetDocument& doc)
		{
			SpriteSheet2D& sheet = doc.sheet;
			float ppu = sheet.pixelsPerUnit;
			ImGui::SetNextItemWidth(120);
			ImGui::DragFloat("Pixels Per Unit", &ppu, 0.5f, 0.01f, 4096.f, "%.2f");
			if (ImGui::IsItemActivated()) doc.BeginEdit();
			if (ppu > 0.f && ppu != sheet.pixelsPerUnit) { sheet.pixelsPerUnit = ppu; doc.dirty = true; }
			if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndEdit("Set Pixels Per Unit");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("How many image pixels make one world unit. A 32px frame at 32\nis exactly one unit tall. Shared by every sprite on the sheet.");

			int filter = sheet.filter;
			const char* filters[] = { "Linear (smooth)", "Nearest (pixel art)" };
			ImGui::SetNextItemWidth(160);
			if (ImGui::Combo("Filter", &filter, filters, 2)) doc.SetFilter(filter);

			// Committed on Enter or on leaving the box, so a path half
			// typed is never a file that fails to load.
			static std::string normalText;
			static const SpriteSheetDocument* normalTextFor = NULL;
			if (normalTextFor != &doc || (!ImGui::IsAnyItemActive() && normalText != sheet.normalImage))
			{ normalText = sheet.normalImage; normalTextFor = &doc; }
			ImGui::SetNextItemWidth(200);
			if (ImGui::InputTextWithHint("Normal Map", "none", &normalText, ImGuiInputTextFlags_EnterReturnsTrue)
				|| ImGui::IsItemDeactivatedAfterEdit())
				doc.SetNormalImage(normalText);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("An image laid out exactly like this one, holding which way each\ntexel faces (assets/textures/hero_n.png). Sprites set to 2D or 3D\nlighting are then shaded by it; a light's z is its height above\nthe plane. Empty for none.");

			ImGui::TextDisabled("%d x %d px, %d frame%s, %d clip%s", (int)sheet.GetImageWidth(), (int)sheet.GetImageHeight(),
				(int)sheet.frames.size(), sheet.frames.size() == 1 ? "" : "s",
				(int)sheet.clips.size(), sheet.clips.size() == 1 ? "" : "s");
		}

		void DrawSliceSection(SpriteSheetDocument& doc)
		{
			SpriteSheetDocument::View& v = doc.view;
			const char* modes[] = { "Grid, by cell size", "Grid, by count", "Automatic" };
			ImGui::SetNextItemWidth(180);
			ImGui::Combo("##slicemode", &v.sliceMode, modes, 3);
			if (v.sliceMode == 0)
			{
				ImGui::SetNextItemWidth(80); ImGui::InputInt("Cell W", &v.cellW, 0); ImGui::SameLine();
				ImGui::SetNextItemWidth(80); ImGui::InputInt("Cell H", &v.cellH, 0);
				ImGui::SetNextItemWidth(80); ImGui::InputInt("Margin", &v.margin, 0); ImGui::SameLine();
				ImGui::SetNextItemWidth(80); ImGui::InputInt("Spacing", &v.spacing, 0);
			}
			else if (v.sliceMode == 1)
			{
				ImGui::SetNextItemWidth(80); ImGui::InputInt("Columns", &v.columns, 0); ImGui::SameLine();
				ImGui::SetNextItemWidth(80); ImGui::InputInt("Rows", &v.rows, 0);
			}
			else
			{
				ImGui::SetNextItemWidth(80); ImGui::InputInt("Alpha above", &v.alphaThreshold, 0);
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("A pixel counts as part of a sprite when its alpha (0-255)\nis above this.");
				ImGui::SetNextItemWidth(80); ImGui::InputInt("Join within (px)", &v.mergeDistance, 0);
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pieces this close together are one sprite - a character's\ndetached hand, the dot of an i.");
				ImGui::SetNextItemWidth(80); ImGui::InputInt("Ignore under (px)", &v.minSize, 0);
			}
			v.cellW = std::max(1, v.cellW); v.cellH = std::max(1, v.cellH);
			v.margin = std::max(0, v.margin); v.spacing = std::max(0, v.spacing);
			v.columns = std::max(1, v.columns); v.rows = std::max(1, v.rows);
			v.alphaThreshold = std::max(0, std::min(254, v.alphaThreshold));
			v.mergeDistance = std::max(0, v.mergeDistance); v.minSize = std::max(1, v.minSize);

			ImGui::SetNextItemWidth(160);
			ImGui::InputText("Name prefix", &v.prefix);

			if (ImGui::Button("Slice"))
			{
				int made = 0;
				if (v.sliceMode == 0) made = doc.SliceGrid(v.cellW, v.cellH, v.margin, v.spacing, v.prefix);
				else if (v.sliceMode == 1) made = doc.SliceCount(v.columns, v.rows, v.prefix);
				else doc.AutoSlice(v.alphaThreshold, v.mergeDistance, v.minSize, v.prefix, made);
				(void)made;
			}
			if (!doc.sheet.clips.empty())
			{
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(1.f, 0.75f, 0.4f, 1.f), "replaces the frames and drops the clips");
			}

			ImGui::Spacing();
			ImGui::SetNextItemWidth(200);
			ImGui::InputTextWithHint("##atlas", "atlas .json (Aseprite / TexturePacker)", &v.atlasPath);
			ImGui::SameLine();
			if (ImGui::Button("Import") && !v.atlasPath.empty())
			{
				// As typed, or beside the sheet.
				std::string path = v.atlasPath;
				std::ifstream in(path.c_str());
				if (!in.is_open())
				{
					const size_t slash = doc.absolutePath.find_last_of("/\\");
					path = (slash == std::string::npos ? std::string() : doc.absolutePath.substr(0, slash + 1)) + v.atlasPath;
					in.open(path.c_str());
				}
				if (!in.is_open()) v.atlasPath = "could not open that file";
				else
				{
					std::stringstream ss;
					ss << in.rdbuf();
					std::string err;
					if (!doc.ImportAtlas(ss.str(), err)) v.atlasPath = err;
				}
			}
		}

		void DrawFrameSection(SpriteSheetDocument& doc)
		{
			SpriteSheet2D& sheet = doc.sheet;
			SpriteSheetDocument::View& v = doc.view;
			if (v.selection.empty())
			{
				ImGui::TextDisabled("Click a frame to select it. Shift-click for a range,\nCtrl/Cmd-click to add one. Alt-click sets the pivot.\nWith one selected: drag its corners and sides to\nresize it, its body to move it, the red dot to\nplace its pivot.");
				return;
			}
			const int32 index = v.selection.back();
			if (!sheet.ValidFrame(index)) { v.selection.clear(); return; }
			SpriteFrame2D f = sheet.frames[(size_t)index];
			if (v.selection.size() > 1) ImGui::TextDisabled("%d frames selected - pivot and borders apply to all", (int)v.selection.size());
			else ImGui::TextDisabled("frame %d", (int)index);

			ImGui::SetNextItemWidth(200);
			if (ImGui::InputText("Name", &f.name, ImGuiInputTextFlags_EnterReturnsTrue) || ImGui::IsItemDeactivatedAfterEdit())
				doc.SetFrame(index, f);

			int rect[4] = { f.x, f.y, f.w, f.h };
			ImGui::SetNextItemWidth(220);
			if (ImGui::InputInt4("Rect x y w h", rect, ImGuiInputTextFlags_EnterReturnsTrue) || ImGui::IsItemDeactivatedAfterEdit())
			{
				SpriteFrame2D g = sheet.frames[(size_t)index];
				g.x = rect[0]; g.y = rect[1]; g.w = rect[2]; g.h = rect[3];
				doc.SetFrame(index, g);
			}

			float pv[2] = { f.pivot.x, f.pivot.y };
			ImGui::SetNextItemWidth(150);
			ImGui::DragFloat2("Pivot", pv, 0.005f, -1.f, 2.f, "%.3f");
			if (ImGui::IsItemActivated()) doc.BeginEdit();
			if (pv[0] != f.pivot.x || pv[1] != f.pivot.y)
			{
				for (size_t i = 0; i < v.selection.size(); i++)
					if (sheet.ValidFrame(v.selection[i])) sheet.frames[(size_t)v.selection[i]].pivot = Vec2(pv[0], pv[1]);
				doc.dirty = true;
			}
			if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndEdit("Set Pivot");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Where the object's origin sits on the frame, 0..1, y up.\n(0.5, 0) is bottom-centre: a standing character's feet.");
			struct P { const char* name; f32 x, y; };
			static const P presets[] = {
				{ "Bottom", 0.5f, 0.f }, { "Centre", 0.5f, 0.5f }, { "Top", 0.5f, 1.f },
				{ "Left", 0.f, 0.5f }, { "Right", 1.f, 0.5f }, { "BL", 0.f, 0.f },
			};
			for (int pi = 0; pi < 6; pi++)
			{
				if (pi) ImGui::SameLine();
				if (ImGui::SmallButton(presets[pi].name)) doc.SetPivot(v.selection, Vec2(presets[pi].x, presets[pi].y));
			}

			float bd[4] = { f.border.x, f.border.y, f.border.z, f.border.w };
			ImGui::SetNextItemWidth(220);
			ImGui::DragFloat4("Border L B R T", bd, 0.25f, 0.f, 4096.f, "%.0f");
			if (ImGui::IsItemActivated()) doc.BeginEdit();
			if (bd[0] != f.border.x || bd[1] != f.border.y || bd[2] != f.border.z || bd[3] != f.border.w)
			{
				for (size_t i = 0; i < v.selection.size(); i++)
					if (sheet.ValidFrame(v.selection[i]))
						sheet.frames[(size_t)v.selection[i]].border = Vec4(std::max(0.f, bd[0]), std::max(0.f, bd[1]), std::max(0.f, bd[2]), std::max(0.f, bd[3]));
				doc.dirty = true;
			}
			if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndEdit("Set Nine-Slice Borders");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Nine-slice borders in pixels. A sprite drawn Sliced keeps these\nedges their own size and stretches only the middle.");

			float dur = f.duration;
			ImGui::SetNextItemWidth(100);
			ImGui::DragFloat("Hold (s)", &dur, 0.005f, 0.f, 10.f, dur > 0.f ? "%.3f" : "clip's rate");
			if (ImGui::IsItemActivated()) doc.BeginEdit();
			if (dur != f.duration) { sheet.frames[(size_t)index].duration = std::max(0.f, dur); doc.dirty = true; }
			if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndEdit("Set Frame Duration");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("How long this frame is held, whatever the clip's rate.\n0 leaves it to the clip.");

			if (ImGui::Button(v.selection.size() > 1 ? "Delete Frames" : "Delete Frame"))
				doc.RemoveFrames(v.selection);
		}

		void DrawClipSection(SpriteSheetDocument& doc, void* tex)
		{
			SpriteSheet2D& sheet = doc.sheet;
			SpriteSheetDocument::View& v = doc.view;

			ImGui::SetNextItemWidth(130);
			ImGui::InputText("##newclip", &v.newClipName);
			ImGui::SameLine();
			ImGui::BeginDisabled(v.selection.empty());
			if (ImGui::Button("New Clip From Selection"))
			{
				const int made = doc.AddClip(v.newClipName, v.selection, 12.f, true);
				if (made >= 0) { v.clip = made; v.clipTime = 0.f; }
			}
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("Select the frames in the order they should play - click the\nfirst, shift-click the last - then press this.");

			for (int i = 0; i < (int)sheet.clips.size(); i++)
			{
				ImGui::PushID(i);
				char label[160];
				snprintf(label, sizeof(label), "%s  (%d frames, %.0f fps)", sheet.clips[(size_t)i].name.c_str(),
					(int)sheet.clips[(size_t)i].frames.size(), sheet.clips[(size_t)i].fps);
				if (ImGui::Selectable(label, v.clip == i)) { v.clip = i; v.clipTime = 0.f; v.framesTextFor = -2; }
				ImGui::PopID();
			}
			if (v.clip < 0 || v.clip >= (int)sheet.clips.size()) return;

			ImGui::Separator();
			SpriteClip2D c = sheet.clips[(size_t)v.clip];

			ImGui::SetNextItemWidth(160);
			if (ImGui::InputText("Clip name", &c.name, ImGuiInputTextFlags_EnterReturnsTrue) || ImGui::IsItemDeactivatedAfterEdit())
				doc.SetClip(v.clip, c);

			float fps = c.fps;
			ImGui::SetNextItemWidth(90);
			ImGui::DragFloat("FPS", &fps, 0.1f, 0.1f, 240.f, "%.1f");
			if (ImGui::IsItemActivated()) doc.BeginEdit();
			if (fps > 0.f && fps != c.fps) { sheet.clips[(size_t)v.clip].fps = fps; doc.dirty = true; }
			if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndEdit("Set Clip Rate");

			bool loop = c.loop, pp = c.pingPong;
			ImGui::SameLine();
			if (ImGui::Checkbox("Loop", &loop)) { c = sheet.clips[(size_t)v.clip]; c.loop = loop; doc.SetClip(v.clip, c); }
			ImGui::SameLine();
			if (ImGui::Checkbox("Ping-pong", &pp)) { c = sheet.clips[(size_t)v.clip]; c.pingPong = pp; doc.SetClip(v.clip, c); }

			if (v.framesTextFor != v.clip) { v.framesText = FramesToText(sheet.clips[(size_t)v.clip].frames); v.framesTextFor = v.clip; }
			ImGui::SetNextItemWidth(220);
			if (ImGui::InputText("Frames", &v.framesText, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				std::vector<int32> frames;
				if (TextToFrames(v.framesText, (int32)sheet.frames.size(), frames))
				{
					c = sheet.clips[(size_t)v.clip];
					c.frames = frames;
					doc.SetClip(v.clip, c);
				}
				v.framesTextFor = -2;
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Frame numbers in play order: 0-3, 7, 5-4.\nA range may run backwards; a frame may repeat. Enter applies.");
			ImGui::BeginDisabled(v.selection.empty());
			if (ImGui::SmallButton("Use Selection"))
			{
				c = sheet.clips[(size_t)v.clip];
				c.frames = v.selection;
				doc.SetClip(v.clip, c);
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::SmallButton("Select Its Frames"))
			{
				v.selection = sheet.clips[(size_t)v.clip].frames;
				// One entry per frame: a clip may repeat one.
				std::vector<int32> once;
				for (size_t i = 0; i < v.selection.size(); i++)
					if (std::find(once.begin(), once.end(), v.selection[i]) == once.end()) once.push_back(v.selection[i]);
				v.selection = once;
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Delete Clip")) { doc.RemoveClip(v.clip); return; }

			// --- preview ---------------------------------------------------
			const SpriteClip2D& playing = sheet.clips[(size_t)v.clip];
			const double now = ImGui::GetTime();
			if (v.playing) v.clipTime += (float)std::min(0.1, now - v.lastTick);
			v.lastTick = now;
			const f32 length = SpriteClip2DLength(sheet, playing);
			// A one-shot runs again after a beat rather than freezing.
			if (!playing.loop && length > 0.f && v.clipTime > length + 0.4f) v.clipTime = 0.f;
			const SpriteClipCursor2D cur = SpriteClip2DAt(sheet, playing, v.clipTime);

			const ImVec2 box(160.f, 160.f);
			const ImVec2 p0 = ImGui::GetCursorScreenPos();
			ImGui::InvisibleButton("##preview", box);
			DrawFrameInBox(ImGui::GetWindowDrawList(), sheet, cur.frame, tex, p0, box, true);
			ImGui::SameLine();
			ImGui::BeginGroup();
			if (ImGui::Button(v.playing ? "Pause" : "Play")) v.playing = !v.playing;
			ImGui::Text("step %d", (int)cur.step);
			ImGui::Text("frame %d", (int)cur.frame);
			ImGui::Text("%.2fs", length);
			ImGui::EndGroup();

			// --- events ----------------------------------------------------
			ImGui::TextDisabled("Events - a script hears these by name (sprite.onEvent)");
			for (int e = 0; e < (int)playing.events.size(); e++)
			{
				ImGui::PushID(1000 + e);
				SpriteEvent2D ev = playing.events[(size_t)e];
				int at = ev.at;
				ImGui::SetNextItemWidth(60);
				const bool atDone = ImGui::InputInt("##at", &at, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue) || ImGui::IsItemDeactivatedAfterEdit();
				ImGui::SameLine();
				ImGui::SetNextItemWidth(130);
				const bool nameDone = ImGui::InputText("##name", &ev.name, ImGuiInputTextFlags_EnterReturnsTrue) || ImGui::IsItemDeactivatedAfterEdit();
				ImGui::SameLine();
				const bool remove = ImGui::SmallButton("x");
				if (atDone || nameDone || remove)
				{
					c = sheet.clips[(size_t)v.clip];
					if (remove) c.events.erase(c.events.begin() + e);
					else
					{
						c.events[(size_t)e].at = std::max(0, std::min(at, (int)c.frames.size() - 1));
						c.events[(size_t)e].name = ev.name;
					}
					doc.SetClip(v.clip, c);
					ImGui::PopID();
					break;
				}
				ImGui::PopID();
			}
			if (ImGui::SmallButton("Add Event At This Step"))
			{
				c = sheet.clips[(size_t)v.clip];
				SpriteEvent2D ev;
				ev.at = cur.step;
				ev.name = "event";
				c.events.push_back(ev);
				doc.SetClip(v.clip, c);
			}
		}

	}

	void DrawWindow(SpriteSheetDocument& doc, void* imageTexId, FrameRequests& requests)
	{
		SpriteSheetDocument::View& v = doc.view;

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
		ImGui::TextDisabled("|");
		ImGui::SameLine();
		if (ImGui::RadioButton("Select", v.tool == 0)) v.tool = 0;
		ImGui::SameLine();
		if (ImGui::RadioButton("Draw Frame", v.tool == 1)) v.tool = 1;
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Drag a rectangle on the image to add a frame by hand.");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(110);
		ImGui::SliderFloat("##zoom", &v.zoom, 0.25f, 16.f, "%.2fx", ImGuiSliderFlags_Logarithmic);
		ImGui::SameLine();
		if (ImGui::Button("Fit")) v.fitPending = true;
		ImGui::SameLine();
		ImGui::TextDisabled("%s", doc.sheet.image.c_str());

		ImGui::Separator();

		if (!doc.sheet.HaveImageSize())
		{
			ImGui::TextColored(ImVec4(1.f, 0.5f, 0.4f, 1.f),
				"The image could not be read, so the sheet cannot be cut.");
			return;
		}

		// Delete removes the selected frames, when no text field has the keys.
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput
			&& !v.selection.empty() && (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)))
			doc.RemoveFrames(v.selection);

		DrawCanvas(doc, imageTexId);
		ImGui::SameLine();
		ImGui::BeginChild("##side", ImVec2(0, 0), false);
		if (ImGui::CollapsingHeader("Sheet", ImGuiTreeNodeFlags_DefaultOpen)) DrawSheetSection(doc);
		if (ImGui::CollapsingHeader("Slice", ImGuiTreeNodeFlags_DefaultOpen)) DrawSliceSection(doc);
		if (ImGui::CollapsingHeader("Frame", ImGuiTreeNodeFlags_DefaultOpen)) DrawFrameSection(doc);
		if (ImGui::CollapsingHeader("Clips", ImGuiTreeNodeFlags_DefaultOpen)) DrawClipSection(doc, imageTexId);
		ImGui::EndChild();
	}

} // namespace SpriteSheetEditor
