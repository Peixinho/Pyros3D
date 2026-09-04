//=============================================================================
// Name        : Character2DEditor.cpp
// Description : See the header.
//=============================================================================

#include "Character2DEditor.h"
#include "AnimationEditor.h"
#include "../Character2DDocument.h"
#include "../Character2DPreview.h"

#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace p3d;
using namespace p3d::Math;

namespace Character2DEditor {

namespace {

const float kTimelineHeight = 260.f;
const float kSidePanelWidth = 260.f;

// Scratch that has to survive between frames but belongs to no document -
// the contents of a text box being typed into, and which modal is up. Keyed by
// nothing, because only one character window has keyboard focus at a time.
struct UIScratch {
	char newBoneName[64] = "Bone";
	char newSpriteName[64] = "Sprite";
	char newClipName[64] = "Clip";
	char renameBuf[64] = "";
	std::string renaming;        // bone/sprite/clip currently being renamed
	int textureChoice = -1;
};
UIScratch ui;

void SetBuf(char* buf, size_t n, const std::string& s)
{
	const size_t len = std::min(s.size(), n - 1);
	memcpy(buf, s.c_str(), len);
	buf[len] = '\0';
}

// ---- the viewport ---------------------------------------------------------

// Draws the character image plus its overlays, and handles picking, dragging
// and panning. Shared by all three stages: you are always looking at the same
// character, and the difference between the stages is what you can grab.
void DrawViewport(Character2DDocument& doc, float availH)
{
	Character2DPreview* pv = doc.preview.get();
	if (!pv) return;

	ImGui::BeginChild("##charviewport", ImVec2(0, availH), true);

	// Toolbar over the viewport.
	ImGui::Checkbox("Sprites", &pv->showSprites);
	ImGui::SameLine();
	ImGui::Checkbox("Bones", &pv->showBones);
	ImGui::SameLine();
	ImGui::Checkbox("Grid", &pv->showGrid);
	ImGui::SameLine();
	if (ImGui::Button("Frame")) { pv->framed = false; pv->userAdjustedView = false; }
	ImGui::SameLine();
	ImGui::TextDisabled("(drag a joint to pose, RMB pan, wheel zoom)");

	const ImVec2 avail = ImGui::GetContentRegionAvail();
	const int w = std::max(64, (int)avail.x);
	const int h = std::max(64, (int)avail.y);
	pv->width = w;
	pv->height = h;

	// Now that the size is known, and only now: the fit depends on the
	// viewport's aspect, so framing before this point fits the character to
	// the wrong shape and crops it.
	// Re-fit when the panel's shape changes and the view has not been placed by
	// hand: switching stage resizes the viewport, and a fit computed for the
	// previous shape crops the character along one axis.
	const bool sizeChanged = (w != pv->framedForWidth || h != pv->framedForHeight);
	if (!pv->framed || (sizeChanged && !pv->userAdjustedView))
		pv->FrameCamera(doc);

	Texture* tex = pv->RenderFrame();
	void* tid = tex ? GetActiveRenderDevice().GetImGuiTextureID(tex->GetBindID(), tex->GetTextureType())
	                : NULL;
	const ImVec2 imgMin = ImGui::GetCursorScreenPos();

	// OpenGL's render targets are bottom-up, so the viewport texture has to be
	// sampled V-flipped to appear the right way up. Not merely cosmetic here:
	// every mouse mapping below - joint picking and joint dragging alike -
	// measures against this image, so a flipped one poses the wrong bone.
#if defined(_SDL2VULKAN) || defined(_SDL2METAL)
	const ImVec2 uv0(0, 0), uv1(1, 1);
#else
	const ImVec2 uv0(0, 1), uv1(1, 0);
#endif
	if (tid)
		ImGui::Image((ImTextureID)tid, ImVec2((float)w, (float)h), uv0, uv1);
	else
	{
		ImGui::Dummy(ImVec2((float)w, (float)h));
		ImDrawList* d = ImGui::GetWindowDrawList();
		d->AddText(ImVec2(imgMin.x + 12.f, imgMin.y + 12.f), IM_COL32(200, 120, 120, 220),
			"[preview texture unavailable]");
	}

	const bool hovered = ImGui::IsItemHovered();
	const ImVec2 m = ImGui::GetIO().MousePos;
	pv->mouse = Vec2(m.x - imgMin.x, m.y - imgMin.y);
	pv->mouseValid = hovered;

	// ---- input ----
	pv->dragEnded = false;

	if (hovered)
	{
		// Zoom about the cursor, so the thing you are pointing at stays put -
		// zooming to the centre means chasing a limb across the canvas.
		const float wheel = ImGui::GetIO().MouseWheel;
		if (wheel != 0.f)
		{
			Vec3 before;
			pv->CursorToWorld(pv->mouse, before);
			pv->halfWidth *= (wheel > 0.f) ? 0.88f : 1.136f;
			if (pv->halfWidth < 0.05f) pv->halfWidth = 0.05f;
			if (pv->halfWidth > 5000.f) pv->halfWidth = 5000.f;
			Vec3 after;
			pv->CursorToWorld(pv->mouse, after);
			pv->centerX += before.x - after.x;
			pv->centerY += before.y - after.y;
			pv->userAdjustedView = true;
		}

		pv->hoveredBone = pv->showBones ? pv->PickJoint(pv->mouse) : -1;

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
		{
			pv->panning = true;
			pv->panAnchor = pv->mouse;
			pv->panCenterX = pv->centerX;
			pv->panCenterY = pv->centerY;
		}

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && pv->hoveredBone >= 0)
		{
			pv->draggingBone = pv->hoveredBone;
			// Clicking a joint selects its bone - which is what a bone list and
			// a viewport showing the same rig should agree about.
			if (pv->draggingBone >= 0 && (size_t)pv->draggingBone < doc.asset.bones.size())
				doc.selectedBone = doc.asset.bones[pv->draggingBone].name;
			// One undo entry for the whole gesture, captured at grab.
			doc.anim.BeginInteractiveEdit();
		}
	}
	else
	{
		pv->hoveredBone = -1;
	}

	if (pv->panning)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
		{
			const float wpp = pv->WorldPerPixel();
			pv->centerX = pv->panCenterX - (pv->mouse.x - pv->panAnchor.x) * wpp;
			pv->centerY = pv->panCenterY + (pv->mouse.y - pv->panAnchor.y) * wpp;
			pv->userAdjustedView = true;
		}
		else pv->panning = false;
	}

	if (pv->draggingBone >= 0)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			Vec3 target;
			if (pv->CursorToWorld(pv->mouse, target))
				pv->DragJoint(pv->draggingBone, target);
		}
		else
		{
			pv->draggingBone = -1;
			pv->dragEnded = true;
		}
	}

	// ---- overlays ----
	// The grid goes UNDER the rig, as an ImGui overlay rather than scene
	// geometry: it is a ruler for the person authoring, not part of the
	// character, and drawing it in the scene would put it in the render that
	// the sprites are composited into.
	if (pv->showGrid)
	{
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const float wpp = pv->WorldPerPixel();
		if (wpp > 0.f)
		{
			// One line per world unit until they crowd, then per 10 - a fixed
			// spacing is either a solid block or a single line depending on
			// zoom, which is no use as a ruler either way.
			float step = 1.f;
			while (step / wpp < 24.f) step *= 10.f;
			while (step / wpp > 240.f) step /= 10.f;

			const float halfH = pv->halfWidth * (float)h / (float)w;
			const ImU32 minor = IM_COL32(255, 255, 255, 18);
			const ImU32 axis  = IM_COL32(255, 255, 255, 70);

			const float x0 = pv->centerX - pv->halfWidth, x1 = pv->centerX + pv->halfWidth;
            const float y0 = pv->centerY - halfH, y1 = pv->centerY + halfH;
			for (float gx = std::ceil(x0 / step) * step; gx <= x1; gx += step)
			{
				const float px = imgMin.x + ((gx - x0) / (x1 - x0)) * (float)w;
				dl->AddLine(ImVec2(px, imgMin.y), ImVec2(px, imgMin.y + (float)h),
					(std::fabs(gx) < step * 0.01f) ? axis : minor);
			}
			for (float gy = std::ceil(y0 / step) * step; gy <= y1; gy += step)
			{
				// y is flipped: world y runs up, pixel rows run down.
				const float py = imgMin.y + (1.f - (gy - y0) / (y1 - y0)) * (float)h;
				dl->AddLine(ImVec2(imgMin.x, py), ImVec2(imgMin.x + (float)w, py),
					(std::fabs(gy) < step * 0.01f) ? axis : minor);
			}
		}
	}

	if (pv->showBones && pv->built.instance)
	{
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const float halfH = pv->halfWidth * (float)h / (float)w;
		const std::vector<Bone>& bones = pv->built.instance->GetSkeletonBones();
		const int selected = doc.SelectedBoneId();

		// The character is alone at the origin of its own scene, so a bone's
		// model-space transform IS its world transform - no owner matrix to
		// come back out through.
		struct Map {
			const Character2DPreview& v; ImVec2 origin; float w, h, halfH;
			ImVec2 operator()(const Vec3& p) const
			{
				const float u = (p.x - v.centerX) / (v.halfWidth > 0.f ? v.halfWidth : 1.f);
				const float t = (p.y - v.centerY) / (halfH > 0.f ? halfH : 1.f);
				return ImVec2(origin.x + (u * 0.5f + 0.5f) * w,
					origin.y + (1.f - (t * 0.5f + 0.5f)) * h);
			}
		} toPx{ *pv, imgMin, (float)w, (float)h, halfH };

		// Limbs first, joints on top, so a marker is never buried under the
		// limb leaving it.
		for (size_t b = 0; b < bones.size(); b++)
		{
			const int32 id = bones[b].self;
			if (id < 0 || (size_t)id >= bones.size()) continue;
			const ImVec2 p = toPx(pv->built.instance->GetBoneGlobalTransform(id).GetTranslation());

			// One limb per CHILD rather than one per bone: a hip parents
			// several, and each of them is a bone you can see and grab.
			for (size_t c = 0; c < bones.size(); c++)
			{
				if (bones[c].parent != id) continue;
				const ImVec2 cp = toPx(pv->built.instance->GetBoneGlobalTransform(bones[c].self).GetTranslation());

				const float dx = cp.x - p.x, dy = cp.y - p.y;
				const float len = std::sqrt(dx * dx + dy * dy);
				if (len < 0.5f) continue;
				const float nx = -dy / len, ny = dx / len;

				// Tapered head to tail, so a bone reads as pointing somewhere.
				float wdt = std::min(7.f, std::max(2.f, len * 0.16f));

				const bool sel = (selected == (int)bones[c].self);
				const ImU32 fill = sel ? IM_COL32(255, 200, 60, 190) : IM_COL32(90, 190, 255, 150);
				const ImU32 edge = sel ? IM_COL32(255, 225, 140, 255) : IM_COL32(150, 215, 255, 220);
				dl->AddTriangleFilled(ImVec2(p.x + nx * wdt, p.y + ny * wdt),
					ImVec2(p.x - nx * wdt, p.y - ny * wdt), cp, fill);
				dl->AddTriangle(ImVec2(p.x + nx * wdt, p.y + ny * wdt),
					ImVec2(p.x - nx * wdt, p.y - ny * wdt), cp, edge, 1.2f);
			}
		}

		for (size_t b = 0; b < bones.size(); b++)
		{
			const int32 id = bones[b].self;
			if (id < 0 || (size_t)id >= bones.size()) continue;
			const ImVec2 p = toPx(pv->built.instance->GetBoneGlobalTransform(id).GetTranslation());

			const bool isHovered = (pv->hoveredBone == (int)id);
			const bool sel = (selected == (int)id);

			// Radius in pixels, and smaller than the pick radius, so the
			// handle never claims to be bigger than it is.
			const float r = isHovered ? 7.f : 5.f;
			ImU32 fill = IM_COL32(255, 190, 40, 230);
			if (isHovered) fill = IM_COL32(255, 255, 255, 255);
			else if (sel) fill = IM_COL32(255, 130, 30, 255);

			dl->AddCircleFilled(p, r, fill, 16);
			dl->AddCircle(p, r, IM_COL32(30, 25, 15, 220), 16, 1.5f);

			// A root reads differently: it is the one you drag to move the
			// whole rig, not one you bend.
			if (bones[b].parent < 0)
			{
				const float x = r * 2.0f;
				dl->AddLine(ImVec2(p.x - x, p.y - x), ImVec2(p.x + x, p.y + x), fill, 1.5f);
				dl->AddLine(ImVec2(p.x - x, p.y + x), ImVec2(p.x + x, p.y - x), fill, 1.5f);
			}
		}
	}

	if (!doc.asset.bones.empty() && doc.asset.parts.empty() && doc.mode == Character2DDocument::Mode::Bones)
	{
		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->AddText(ImVec2(imgMin.x + 12.f, imgMin.y + 12.f), IM_COL32(200, 200, 210, 200),
			"No artwork yet - the Sprites stage pins textures to these bones.");
	}

	ImGui::EndChild();
}

// ---- stage 1: bones -------------------------------------------------------

void DrawBonesPanel(Character2DDocument& doc)
{
	ImGui::BeginChild("##bones", ImVec2(kSidePanelWidth, 0), true);

	ImGui::TextUnformatted("Bones");
	ImGui::Separator();

	ImGui::SetNextItemWidth(-60.f);
	ImGui::InputText("##newbone", ui.newBoneName, sizeof(ui.newBoneName));
	ImGui::SameLine();
	if (ImGui::Button("Add##bone"))
	{
		// A new bone is parented to the selection, which is what makes
		// building a chain a matter of clicking Add repeatedly.
		std::string err;
		const std::string name = doc.asset.UniqueBoneName(ui.newBoneName);
		// Placed a little along from its parent rather than on top of it: two
		// bones at the same point draw as nothing and cannot be told apart.
		const Vec2 pos = doc.selectedBone.empty() ? Vec2(0.f, 0.f) : Vec2(0.f, 0.5f);
		if (!doc.AddBone(name, doc.selectedBone, pos, err))
			ImGui::OpenPopup("##bonerr");
	}
	if (!doc.selectedBone.empty())
		ImGui::TextDisabled("New bones parent to '%s'", doc.selectedBone.c_str());
	else
		ImGui::TextDisabled("New bones become roots");

	ImGui::Separator();

	// The tree, drawn from the flat array. Parents always precede children
	// (Character2DAsset guarantees it), so one pass with a recursive helper is
	// enough.
	std::function<void(int)> drawBone = [&](int id) {
		const Bone& b = doc.asset.bones[id];
		bool hasChild = false;
		for (size_t i = 0; i < doc.asset.bones.size(); i++)
			if (doc.asset.bones[i].parent == (int32)id) { hasChild = true; break; }

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen
			| ImGuiTreeNodeFlags_SpanAvailWidth;
		if (!hasChild) flags |= ImGuiTreeNodeFlags_Leaf;
		if (doc.selectedBone == b.name) flags |= ImGuiTreeNodeFlags_Selected;

		const bool open = ImGui::TreeNodeEx((void*)(intptr_t)id, flags, "%s", b.name.c_str());
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			doc.selectedBone = b.name;

		// Drag a bone onto another to reparent it. The alternative is a parent
		// dropdown per bone, which is a lot of clicks for the one operation
		// you do constantly while blocking out a skeleton.
		if (ImGui::BeginDragDropSource())
		{
			const char* n = b.name.c_str();
			ImGui::SetDragDropPayload("CHAR2D_BONE", n, strlen(n) + 1);
			ImGui::TextUnformatted(b.name.c_str());
			ImGui::EndDragDropSource();
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("CHAR2D_BONE"))
			{
				std::string err;
				doc.ReparentBone((const char*)p->Data, b.name, err);
			}
			ImGui::EndDragDropTarget();
		}

		if (open)
		{
			for (size_t i = 0; i < doc.asset.bones.size(); i++)
				if (doc.asset.bones[i].parent == (int32)id) drawBone((int)i);
			ImGui::TreePop();
		}
	};

	for (size_t i = 0; i < doc.asset.bones.size(); i++)
		if (doc.asset.bones[i].parent < 0) drawBone((int)i);

	if (doc.asset.bones.empty())
		ImGui::TextDisabled("No bones yet.\nAdd one to start the skeleton.");

	// ---- the selected bone ----
	const int selId = doc.SelectedBoneId();
	if (selId >= 0)
	{
		ImGui::Separator();
		ImGui::TextUnformatted("Selected");

		if (ui.renaming == doc.selectedBone)
		{
			ImGui::SetNextItemWidth(-1.f);
			if (ImGui::InputText("##rename", ui.renameBuf, sizeof(ui.renameBuf),
				ImGuiInputTextFlags_EnterReturnsTrue))
			{
				std::string err;
				doc.RenameBone(doc.selectedBone, ui.renameBuf, err);
				ui.renaming.clear();
			}
		}
		else if (ImGui::Button("Rename"))
		{
			ui.renaming = doc.selectedBone;
			SetBuf(ui.renameBuf, sizeof(ui.renameBuf), doc.selectedBone);
		}
		ImGui::SameLine();
		if (ImGui::Button("Delete"))
		{
			std::string err;
			doc.RemoveBone(doc.selectedBone, err);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Deletes this bone and everything under it.\nSprites pinned to them are unpinned, not deleted.");

		if (selId >= 0 && selId < (int)doc.asset.bones.size())
		{
			const Bone& b = doc.asset.bones[selId];
			float pos[2] = { b.pos.x, b.pos.y };
			Quaternion q = b.rot;
			const Vec3 e = q.GetEulerFromQuaternion(RotationOrder::XYZ);
			float rotDeg = (float)RADTODEG(e.z);

			ImGui::TextDisabled("Rest pose, in the parent's frame");
			// One undo entry per drag, not per frame: SetBoneRest pushes its
			// own entry (right for an agent calling it once), and the open
			// gesture swallows those - see Character2DDocument::PushEdit.
			bool changed = false;
			ImGui::SetNextItemWidth(-1.f);
			const bool draggedPos = ImGui::DragFloat2("##restpos", pos, 0.01f);
			if (ImGui::IsItemActivated()) doc.BeginInteractiveEdit();
			changed |= draggedPos;
			const bool posReleased = ImGui::IsItemDeactivatedAfterEdit();

			ImGui::SetNextItemWidth(-1.f);
			const bool draggedRot = ImGui::DragFloat("##restrot", &rotDeg, 0.5f, -360.f, 360.f, "%.1f deg");
			if (ImGui::IsItemActivated()) doc.BeginInteractiveEdit();
			changed |= draggedRot;
			const bool rotReleased = ImGui::IsItemDeactivatedAfterEdit();

			if (changed)
			{
				std::string err;
				doc.SetBoneRest(doc.selectedBone, Vec2(pos[0], pos[1]), rotDeg, err);
			}
			if (posReleased || rotReleased)
				doc.EndInteractiveEdit("Move Bone '" + doc.selectedBone + "'");
		}
	}

	ImGui::EndChild();
}

// ---- stage 2: sprites -----------------------------------------------------

void DrawSpritesPanel(Character2DDocument& doc, const std::vector<TextureChoice>& textures,
	FrameRequests& requests)
{
	ImGui::BeginChild("##sprites", ImVec2(kSidePanelWidth, 0), true);

	ImGui::TextUnformatted("Sprites");
	ImGui::Separator();

	ImGui::SetNextItemWidth(-60.f);
	ImGui::InputText("##newsprite", ui.newSpriteName, sizeof(ui.newSpriteName));
	ImGui::SameLine();
	if (ImGui::Button("Add##sprite"))
	{
		std::string err;
		// Pinned to the selected bone. Adding artwork while a bone is selected
		// is the whole gesture - pick the bone, add its sprite.
		doc.AddSprite(doc.asset.UniquePartName(ui.newSpriteName), std::string(),
			doc.selectedBone, err);
	}
	if (!doc.selectedBone.empty())
		ImGui::TextDisabled("New sprites pin to '%s'", doc.selectedBone.c_str());
	else
		ImGui::TextDisabled("Select a bone to pin new sprites to it");

	ImGui::Separator();

	// Listed front to back, matching what you see - the list reads like the
	// stack of cutouts it describes.
	std::vector<int> order;
	for (size_t i = 0; i < doc.asset.parts.size(); i++) order.push_back((int)i);
	std::sort(order.begin(), order.end(), [&](int a, int b) {
		return doc.asset.parts[a].z > doc.asset.parts[b].z;
	});

	for (size_t k = 0; k < order.size(); k++)
	{
		const int i = order[k];
		const SpritePart2D& p = doc.asset.parts[i];
		ImGui::PushID(i);
		const bool sel = (doc.selectedSprite == p.name);
		// A sprite with no bone is called out: it will sit at the character's
		// origin and never move, which otherwise looks like a broken import.
		if (p.bone.empty())
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.75f, 0.4f, 1.f));
		if (ImGui::Selectable(p.name.c_str(), sel))
			doc.selectedSprite = p.name;
		if (p.bone.empty())
		{
			ImGui::PopStyleColor();
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Not pinned to a bone - this will not move.");
		}
		ImGui::PopID();
	}

	if (doc.asset.parts.empty())
		ImGui::TextDisabled("No artwork yet.");

	// ---- the selected sprite ----
	const int spriteId = doc.FindSprite(doc.selectedSprite);
	if (spriteId >= 0)
	{
		SpritePart2D& p = doc.asset.parts[spriteId];
		ImGui::Separator();
		ImGui::TextUnformatted("Selected");

		if (ImGui::Button("Delete##sprite"))
		{
			std::string err;
			doc.RemoveSprite(doc.selectedSprite, err);
			ImGui::EndChild();
			return;
		}
		ImGui::SameLine();
		if (ImGui::Button("Front")) { std::string e; doc.ReorderSprite(p.name, 1, e); }
		ImGui::SameLine();
		if (ImGui::Button("Back")) { std::string e; doc.ReorderSprite(p.name, -1, e); }

		// Every field below edits the asset in place and pushes ONE undo entry
		// per gesture, rather than one per frame of a drag - which would make
		// Ctrl+Z walk back through a slider.
		//
		// The "before" state is captured when a widget is GRABBED, not at the
		// top of this function: Snapshot() serialises the whole character, and
		// doing that unconditionally every frame the panel is open costs the
		// same whether or not anything is being edited.
		const std::string label = "Edit Sprite '" + p.name + "'";
		bool changed = false;

		// Texture.
		ImGui::TextDisabled("Texture");
		ImGui::SetNextItemWidth(-1.f);
		if (ImGui::BeginCombo("##tex", p.texture.empty() ? "(none)" : p.texture.c_str()))
		{
			for (size_t t = 0; t < textures.size(); t++)
			{
				const bool isSel = (p.texture == textures[t].relativePath);
				if (ImGui::Selectable(textures[t].label.c_str(), isSel))
				{
					// A combo pick is a whole gesture in one frame.
					doc.BeginInteractiveEdit();
					p.texture = textures[t].relativePath;
					// A new texture is a new quad (the geometry is built at
					// the artwork's aspect), so this one does need a rebuild.
					doc.TouchRig();
					doc.EndInteractiveEdit(label);
				}
			}
			ImGui::EndCombo();
		}

		// Bone.
		ImGui::TextDisabled("Follows bone");
		ImGui::SetNextItemWidth(-1.f);
		if (ImGui::BeginCombo("##bone", p.bone.empty() ? "(none)" : p.bone.c_str()))
		{
			if (ImGui::Selectable("(none)", p.bone.empty()))
			{
				doc.BeginInteractiveEdit();
				p.bone.clear();
				changed = true;
				doc.EndInteractiveEdit(label);
			}
			for (size_t b = 0; b < doc.asset.bones.size(); b++)
			{
				const bool isSel = (p.bone == doc.asset.bones[b].name);
				if (ImGui::Selectable(doc.asset.bones[b].name.c_str(), isSel))
				{
					doc.BeginInteractiveEdit();
					p.bone = doc.asset.bones[b].name;
					changed = true;
					doc.EndInteractiveEdit(label);
				}
			}
			ImGui::EndCombo();
		}

		float off[2] = { p.offset.x, p.offset.y };
		float scl[2] = { p.scale.x, p.scale.y };
		float piv[2] = { p.pivot.x, p.pivot.y };

		// Each drag: capture on grab, push on release.
		ImGui::TextDisabled("Offset from the joint");
		ImGui::SetNextItemWidth(-1.f);
		const bool dragged_off = ImGui::DragFloat2("##off", off, 0.005f);
		// Before the write below, not after: on the frame a drag both starts
		// and moves, capturing afterwards would snapshot the edited value and
		// the undo entry would restore nothing.
		if (ImGui::IsItemActivated()) doc.BeginInteractiveEdit();
		if (dragged_off) { p.offset = Vec2(off[0], off[1]); changed = true; }
		if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndInteractiveEdit(label);

		ImGui::TextDisabled("Size");
		ImGui::SetNextItemWidth(-1.f);
		const bool dragged_scl = ImGui::DragFloat2("##scl", scl, 0.01f, 0.01f, 100.f);
		// Before the write below, not after: on the frame a drag both starts
		// and moves, capturing afterwards would snapshot the edited value and
		// the undo entry would restore nothing.
		if (ImGui::IsItemActivated()) doc.BeginInteractiveEdit();
		if (dragged_scl) { p.scale = Vec2(scl[0], scl[1]); changed = true; }
		if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndInteractiveEdit(label);

		ImGui::TextDisabled("Pivot (0.5,0.5 centred)");
		ImGui::SetNextItemWidth(-1.f);
		const bool dragged_piv = ImGui::DragFloat2("##piv", piv, 0.005f, 0.f, 1.f);
		// Before the write below, not after: on the frame a drag both starts
		// and moves, capturing afterwards would snapshot the edited value and
		// the undo entry would restore nothing.
		if (ImGui::IsItemActivated()) doc.BeginInteractiveEdit();
		if (dragged_piv) { p.pivot = Vec2(piv[0], piv[1]); changed = true; }
		if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndInteractiveEdit(label);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Where the artwork's own origin sits.\nPut it on the joint the limb turns about.");

		// Staged through a local: Checkbox writes through its pointer before it
		// returns, so binding it straight to p.lit would change the asset
		// before there was anything to snapshot.
		bool lit = p.lit;
		if (ImGui::Checkbox("Lit by 2D lights", &lit))
		{
			doc.BeginInteractiveEdit();
			p.lit = lit;
			// Lighting is a shader option baked into the part's material, so
			// this is a rebuild rather than a re-place.
			doc.TouchRig();
			doc.EndInteractiveEdit(label);
		}

		// Offset / size / pivot / bone only change where a part is DRAWN.
		if (changed) doc.TouchParts();
	}

	ImGui::EndChild();
}

// ---- stage 3: animate -----------------------------------------------------

void DrawClipBar(Character2DDocument& doc)
{
	ImGui::SetNextItemWidth(220.f);
	const char* label = (doc.anim.activeClip >= 0 && doc.anim.activeClip < (int)doc.anim.clips.size())
		? doc.anim.clips[doc.anim.activeClip].AnimationName.c_str() : "(no clip)";
	if (ImGui::BeginCombo("##clip", label))
	{
		for (size_t i = 0; i < doc.anim.clips.size(); i++)
		{
			const bool sel = (doc.anim.activeClip == (int)i);
			if (ImGui::Selectable(doc.anim.clips[i].AnimationName.c_str(), sel))
			{
				doc.anim.activeClip = (int)i;
				doc.anim.playhead = 0.f;
				doc.anim.selectedKeys.clear();
			}
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	ImGui::SetNextItemWidth(110.f);
	ImGui::InputText("##newclip", ui.newClipName, sizeof(ui.newClipName));
	ImGui::SameLine();
	if (ImGui::Button("New Clip"))
	{
		std::string err;
		doc.AddClip(doc.asset.UniqueClipName(ui.newClipName), 1.f, err);
	}

	if (doc.anim.activeClip >= 0 && doc.anim.activeClip < (int)doc.anim.clips.size())
	{
		Animation& clip = doc.anim.clips[doc.anim.activeClip];

		ImGui::SameLine();
		if (ImGui::Button("Rename##clip"))
		{
			ui.renaming = "##clip:" + clip.AnimationName;
			SetBuf(ui.renameBuf, sizeof(ui.renameBuf), clip.AnimationName);
		}
		ImGui::SameLine();
		if (ImGui::Button("Delete##clip"))
		{
			std::string err;
			doc.RemoveClip(clip.AnimationName, err);
			return;
		}

		ImGui::SameLine();
		ImGui::SetNextItemWidth(90.f);
		float dur = clip.Duration;
		if (ImGui::DragFloat("##dur", &dur, 0.05f, 0.05f, 600.f, "%.2fs"))
		{
			clip.Duration = std::max(0.05f, dur);
			doc.SyncClipsToAsset();
			doc.dirty = true;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clip length, in seconds.");

		// The clip a scene gets when it places this character and says nothing
		// else. Stored in the asset, so a character that walks by default
		// walks everywhere without every scene repeating itself.
		ImGui::SameLine();
		bool isDefault = (doc.asset.defaultClip == clip.AnimationName);
		if (ImGui::Checkbox("Default", &isDefault))
		{
			const std::string before = doc.Snapshot();
			doc.asset.defaultClip = isDefault ? clip.AnimationName : std::string();
			doc.PushEdit(before, "Set Default Clip");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("The clip this character starts on when a scene places it\nand does not pick one itself.");

		if (ui.renaming == "##clip:" + clip.AnimationName)
		{
			ImGui::SetNextItemWidth(200.f);
			if (ImGui::InputText("##cliprename", ui.renameBuf, sizeof(ui.renameBuf),
				ImGuiInputTextFlags_EnterReturnsTrue))
			{
				std::string err;
				doc.RenameClip(clip.AnimationName, ui.renameBuf, err);
				ui.renaming.clear();
			}
		}
	}
}

} // namespace

void DrawWindow(Character2DDocument& doc, const std::vector<TextureChoice>& textures,
	float dt, FrameRequests& requests)
{
	// The viewport builds the character, and the dope sheet keys the rig it
	// builds, so this has to happen before anything is drawn.
	if (!doc.preview) doc.preview.reset(new Character2DPreview());
	doc.preview->Sync(doc);

	// ---- toolbar ----
	if (ImGui::Button("Save")) requests.save = true;
	ImGui::SameLine();
	if (ImGui::Button("Save As...")) requests.saveAs = true;
	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();

	// The three stages, in the order you do them.
	const struct { const char* label; Character2DDocument::Mode mode; const char* tip; } stages[] = {
		{ "Bones",   Character2DDocument::Mode::Bones,   "Build the skeleton." },
		{ "Sprites", Character2DDocument::Mode::Sprites, "Pin artwork to the bones." },
		{ "Animate", Character2DDocument::Mode::Animate, "Pose the rig and key clips." },
	};
	for (int i = 0; i < 3; i++)
	{
		if (i) ImGui::SameLine();
		if (ImGui::RadioButton(stages[i].label, doc.mode == stages[i].mode))
			doc.mode = stages[i].mode;
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", stages[i].tip);
	}

	ImGui::SameLine();
	ImGui::TextDisabled("|  %d bones, %d sprites, %d clips",
		(int)doc.asset.bones.size(), (int)doc.asset.parts.size(), (int)doc.anim.clips.size());

	ImGui::Separator();

	const bool animating = (doc.mode == Character2DDocument::Mode::Animate);
	if (animating) DrawClipBar(doc);

	const float availH = ImGui::GetContentRegionAvail().y;
	const float upperH = animating ? std::max(180.f, availH - kTimelineHeight) : availH;

	ImGui::BeginChild("##upper", ImVec2(0, upperH), false);
	switch (doc.mode)
	{
	case Character2DDocument::Mode::Bones:   DrawBonesPanel(doc); break;
	case Character2DDocument::Mode::Sprites: DrawSpritesPanel(doc, textures, requests); break;
	case Character2DDocument::Mode::Animate:
		// The .p3da editor's own bone list, pointed at this character's rig -
		// it already does bone selection plus numeric pose fields.
		AnimationEditor::DrawSkeletonPanel(doc.anim,
			"This character has no bones yet.\nBuild the skeleton in the Bones stage first.");
		break;
	}
	ImGui::SameLine();

	// Pose the rig from the clip before it is drawn, in animate mode only:
	// the other two stages show the REST pose, which is what you are
	// authoring there. Seeing a walk cycle's frame 12 while placing artwork
	// would mean pinning sprites to a pose the character is rarely in.
	if (animating)
	{
		AnimationEditor::ApplyTimelinePose(doc.anim);
		if (doc.preview->characterRC) doc.preview->characterRC->RefreshSpriteParts2D();
	}
	else if (doc.preview->built.instance)
	{
		doc.preview->built.instance->ResetToBindPose();
		if (doc.preview->characterRC) doc.preview->characterRC->RefreshSpriteParts2D();
	}

	DrawViewport(doc, 0.f);
	ImGui::EndChild();

	if (animating)
	{
		// A viewport drag posed bones that the next ApplyTimelinePose would
		// sample straight back over. Handing them to the document as pending
		// overrides is what makes "Key Pose" mean "key what I just moved".
		Character2DPreview* pv = doc.preview.get();
		for (size_t i = 0; i < pv->posedBones.size(); i++)
			if (pv->built.instance)
				doc.anim.externalPoseOverrides[pv->posedBones[i]] =
					pv->built.instance->GetBoneLocalTransform(pv->posedBones[i]);

		if (pv->dragEnded)
		{
			doc.anim.EndInteractiveEdit("Pose Bone");
			if (doc.anim.autoKey) AnimationEditor::KeyPendingPose(doc.anim);
			pv->posedBones.clear();
		}

		AnimationEditor::DrawTransportBar(doc.anim, dt);
		AnimationEditor::DrawTimelinePanel(doc.anim);

		// The dope sheet writes to `anim.clips`; the asset is what gets saved.
		if (doc.anim.dirty)
		{
			doc.SyncClipsToAsset();
			doc.anim.dirty = false;
			doc.dirty = true;
		}
	}
}

} // namespace Character2DEditor
