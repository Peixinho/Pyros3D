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

#include "imgui.h"

#include <algorithm>
#include <functional>
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
	ImGui::TextDisabled("|");
	ImGui::SameLine();
	if (ImGui::RadioButton("Pose", pv->tool == Character2DPreview::Tool::Pose)) pv->tool = Character2DPreview::Tool::Pose;
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Drag a joint: the limb above it bends to follow (IK).\nAlt+drag turns one bone instead.");
	ImGui::SameLine();
	if (ImGui::RadioButton("Rotate", pv->tool == Character2DPreview::Tool::Rotate)) pv->tool = Character2DPreview::Tool::Rotate;
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Drag a bone (or the stub of a hand or head) to turn just that bone\nabout its joint, children carried along.");
	if (doc.mode == Character2DDocument::Mode::Animate)
	{
		ImGui::SameLine();
		ImGui::Checkbox("Onion", &doc.anim.onionSkin);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ghost bones before (blue) and after (green) the playhead.\nRight-click for how many and how far apart.");
		if (ImGui::BeginPopupContextItem("##onion2d"))
		{
			ImGui::SetNextItemWidth(100.f); ImGui::SliderInt("Before", &doc.anim.onionBefore, 0, 8);
			ImGui::SetNextItemWidth(100.f); ImGui::SliderInt("After", &doc.anim.onionAfter, 0, 8);
			ImGui::SetNextItemWidth(100.f); ImGui::SliderInt("Every n frames", &doc.anim.onionStep, 1, 15);
			ImGui::EndPopup();
		}
	}
	ImGui::SameLine();
	ImGui::TextDisabled(doc.mode == Character2DDocument::Mode::Animate
		? "(double-click a joint to pin it, RMB pan, wheel zoom)" : "(RMB pan, wheel zoom)");

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

	pv->RenderFrame();
	void* tid = pv->viewImage;     // (Character2DPreview::RenderFrame's texture)
	const ImVec2 imgMin = ImGui::GetCursorScreenPos();
	pv->imageX = imgMin.x;
	pv->imageY = imgMin.y;

	// OpenGL's render targets are bottom-up, so the viewport texture has to be
	// sampled V-flipped to appear the right way up. Not merely cosmetic here:
	// every mouse mapping below - joint picking and joint dragging alike -
	// measures against this image, so a flipped one poses the wrong bone.
	const ImVec2 uv0(0, 0), uv1(1, 1);
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
		const bool rotateGesture = pv->tool == Character2DPreview::Tool::Rotate || ImGui::GetIO().KeyAlt;
		const int hoveredLimb = (pv->showBones && rotateGesture && pv->hoveredBone < 0) ? pv->PickLimb(pv->mouse) : -1;
		pv->hoveredLimb = hoveredLimb;

		// Double-click a joint: pin it there (or let it go).
		if (doc.mode == Character2DDocument::Mode::Animate && pv->hoveredBone >= 0
			&& ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			pv->TogglePin(pv->hoveredBone);

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
		{
			pv->panning = true;
			pv->panAnchor = pv->mouse;
			pv->panCenterX = pv->centerX;
			pv->panCenterY = pv->centerY;
		}

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && rotateGesture && (pv->hoveredBone >= 0 || hoveredLimb >= 0))
		{
			const int b = pv->hoveredBone >= 0 ? pv->hoveredBone : hoveredLimb;
			Vec3 cursor;
			if (pv->CursorToWorld(pv->mouse, cursor)) pv->BeginRotate(b, cursor);
			if ((size_t)b < doc.asset.bones.size()) doc.selectedBone = doc.asset.bones[b].name;
			doc.anim.BeginInteractiveEdit();
		}
		else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && pv->hoveredBone >= 0)
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

	if (pv->rotatingBone >= 0)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			Vec3 cursor;
			if (pv->CursorToWorld(pv->mouse, cursor)) pv->RotateTo(cursor);
		}
		else
		{
			pv->rotatingBone = -1;
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

	// Onion skin: the clip a few frames either side, as ghost bones.
	if (pv->showBones && pv->built.instance && doc.anim.onionSkin
		&& doc.mode == Character2DDocument::Mode::Animate && doc.anim.HasActiveClip())
	{
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const Animation& clip = doc.anim.clips[doc.anim.activeClip];
		const std::vector<Bone>& bones = pv->built.instance->GetSkeletonBones();
		const float fps = doc.anim.snapFps > 0.f ? doc.anim.snapFps : 30.f;
		// Held and put back exactly: this runs after this frame's drag, and
		// re-sampling the clip instead would throw the drag away.
		std::vector<Matrix> held;
		pv->built.instance->CapturePose(held);
		for (int k = -doc.anim.onionBefore; k <= doc.anim.onionAfter && clip.Duration > 0.f; k++)
		{
			if (k == 0) continue;
			float t = doc.anim.playhead + k * std::max(1, doc.anim.onionStep) / fps;
			if (doc.anim.looping) { t = std::fmod(t, clip.Duration); if (t < 0.f) t += clip.Duration; }
			else if (t < 0.f || t > clip.Duration) continue;
			pv->built.instance->ApplyAnimationAtTime(clip, t);
			const int reach = k < 0 ? doc.anim.onionBefore : doc.anim.onionAfter;
			const int alpha = (int)(170.f * (1.f - (std::abs(k) - 1) / (float)std::max(1, reach))) + 30;
			const ImU32 col = k < 0 ? IM_COL32(90, 150, 255, alpha) : IM_COL32(100, 255, 130, alpha);
			for (size_t b = 0; b < bones.size(); b++)
			{
				Vec2 a, c;
				if (bones[b].parent < 0) continue;
				if (pv->BoneToPixels(bones[b].parent, a) && pv->BoneToPixels(bones[b].self, c))
					dl->AddLine(ImVec2(imgMin.x + a.x, imgMin.y + a.y), ImVec2(imgMin.x + c.x, imgMin.y + c.y), col, 2.f);
			}
		}
		pv->built.instance->ApplyPose(held);
	}

	if (pv->showBones && pv->built.instance)
	{
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const float halfH = pv->halfWidth * (float)h / (float)w;
		const std::vector<Bone>& bones = pv->built.instance->GetSkeletonBones();
		const int selected = doc.SelectedBoneId();

		// Leaf stubs: in the rotate tool a hand or a head has something to grab.
		if (pv->tool == Character2DPreview::Tool::Rotate || ImGui::GetIO().KeyAlt)
			for (size_t b = 0; b < bones.size(); b++)
			{
				bool leaf = true;
				for (size_t c = 0; c < bones.size() && leaf; c++) if (bones[c].parent == bones[b].self) leaf = false;
				if (!leaf) continue;
				Vec2 a;
				if (!pv->BoneToPixels(bones[b].self, a)) continue;
				const Matrix g = pv->built.instance->GetBoneGlobalTransform(bones[b].self);
				Vec2 dir(g.m[0], -g.m[1]);
				const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
				if (len < 1e-6f) continue;
				const ImVec2 p0(imgMin.x + a.x, imgMin.y + a.y);
				const ImVec2 p1(p0.x + dir.x / len * Character2DPreview::kLeafStubPixels, p0.y + dir.y / len * Character2DPreview::kLeafStubPixels);
				const bool hot = pv->hoveredLimb == (int)bones[b].self || pv->rotatingBone == (int)bones[b].self;
				dl->AddLine(p0, p1, hot ? IM_COL32(255, 230, 120, 255) : IM_COL32(150, 215, 255, 200), hot ? 3.f : 2.f);
			}

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
				// In the rotate tool a limb is the parent's: it turns about p.
				const bool hot = (pv->hoveredLimb == (int)id || pv->rotatingBone == (int)id);
				const ImU32 fill = hot ? IM_COL32(255, 230, 120, 200) : sel ? IM_COL32(255, 200, 60, 190) : IM_COL32(90, 190, 255, 150);
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

		// Pins: where each pinned joint is held, and a line to where it is.
		for (std::map<int, Vec3>::const_iterator it = pv->pins.begin(); it != pv->pins.end(); ++it)
		{
			const ImVec2 at = toPx(it->second);
			Vec2 now;
			if (pv->BoneToPixels(it->first, now))
				dl->AddLine(at, ImVec2(imgMin.x + now.x, imgMin.y + now.y), IM_COL32(255, 80, 80, 160), 1.f);
			dl->AddRectFilled(ImVec2(at.x - 4.f, at.y - 4.f), ImVec2(at.x + 4.f, at.y + 4.f), IM_COL32(240, 60, 60, 255));
			dl->AddLine(ImVec2(at.x, at.y + 4.f), ImVec2(at.x, at.y + 12.f), IM_COL32(240, 60, 60, 255), 2.f);
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

			// Joint limit: how far the bone may turn from rest when a limb is
			// dragged or a bone rotated. Also how a knee is told which way it
			// folds: 0..150 bends one way, -150..0 the other.
			ImGui::Separator();
			std::map<std::string, Character2DAsset::BoneLimit>::const_iterator li = doc.asset.limits.find(doc.selectedBone);
			bool limited = li != doc.asset.limits.end() && li->second.enabled;
			float range[2] = { li != doc.asset.limits.end() ? li->second.minDeg : -180.f,
				li != doc.asset.limits.end() ? li->second.maxDeg : 180.f };
			std::string err;
			if (ImGui::Checkbox("Joint limit", &limited))
				doc.SetBoneLimit(doc.selectedBone, limited, range[0], range[1], err);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Clamp how far this bone turns from its rest angle when posed.\nA knee limited to 0..150 only ever folds one way.");
			if (limited)
			{
				ImGui::SetNextItemWidth(-1.f);
				if (ImGui::DragFloat2("##limit", range, 1.f, -180.f, 180.f, "%.0f deg"))
					doc.SetBoneLimit(doc.selectedBone, true, range[0], range[1], err);
				if (ImGui::IsItemActivated()) doc.BeginInteractiveEdit();
				if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndInteractiveEdit("Joint Limit '" + doc.selectedBone + "'");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Min and max, in degrees from the rest angle.");
			}
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

		// Bend: one picture across two bones, bending at the joint between.
		{
			ImGui::Separator();
			const std::string none = "(rigid)";
			ImGui::SetNextItemWidth(-1.f);
			if (ImGui::BeginCombo("##bend", p.bendBone.empty() ? none.c_str() : ("bends with " + p.bendBone).c_str()))
			{
				std::string pick = p.bendBone;
				bool picked = false;
				if (ImGui::Selectable(none.c_str(), p.bendBone.empty())) { pick.clear(); picked = true; }
				const int own = doc.asset.FindBone(p.bone);
				for (size_t b = 0; b < doc.asset.bones.size(); b++)
				{
					// Bones under the part's own bone - the next one down the limb.
					bool below = false;
					for (int q = doc.asset.bones[b].parent; q >= 0 && !below; q = doc.asset.bones[q].parent) if (q == own) below = true;
					if (!below) continue;
					if (ImGui::Selectable(doc.asset.bones[b].name.c_str(), p.bendBone == doc.asset.bones[b].name))
					{ pick = doc.asset.bones[b].name; picked = true; }
				}
				if (picked && pick != p.bendBone)
				{
					doc.BeginInteractiveEdit();
					p.bendBone = pick;
					doc.TouchRig();       // a different mesh, not a re-place
					doc.EndInteractiveEdit(label);
				}
				ImGui::EndCombo();
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Bend this picture across a second bone: an arm drawn as one image that\n"
					"bends at the elbow, instead of two images that split there.");
			if (!p.bendBone.empty())
			{
				int seg = p.bendSegments;
				ImGui::SetNextItemWidth(-1.f);
				if (ImGui::SliderInt("##bendseg", &seg, 2, 32, "smoothness %d"))
				{
					p.bendSegments = seg;
					doc.TouchRig();
				}
				if (ImGui::IsItemActivated()) doc.BeginInteractiveEdit();
				if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndInteractiveEdit(label);
				float bl = p.bendBlend;
				ImGui::SetNextItemWidth(-1.f);
				if (ImGui::SliderFloat("##bendblend", &bl, 0.02f, 1.f, "joint softness %.2f"))
				{
					p.bendBlend = bl;
					doc.TouchRig();
				}
				if (ImGui::IsItemActivated()) doc.BeginInteractiveEdit();
				if (ImGui::IsItemDeactivatedAfterEdit()) doc.EndInteractiveEdit(label);
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("How far the bend reaches either side of each joint: low is a sharp elbow, high a soft tail.\nThe picture follows every bone from its own down to the bend bone (up to four).");
			}
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

// ---- stage 3: animate - parts and pins ------------------------------------

// A part's property at the playhead: what the clip's track gives, or the
// part's own rest value.
SkeletonAnimationInstance::TrackValue PartValue(Character2DDocument& doc, const SpritePart2D& part, const char* prop)
{
	SkeletonAnimationInstance::TrackValue v;
	SkeletonAnimationInstance* inst = doc.preview ? doc.preview->built.instance : NULL;
	if (inst && inst->GetTrackValue(part.name, prop, v)) return v;
	const std::string p = prop;
	if (p == "z") v.value = Vec4(part.z, 0.f, 0.f, 0.f);
	else if (p == "color") v.value = Vec4(1.f, 1.f, 1.f, 1.f);
	else if (p == "visible") v.value = Vec4(1.f, 0.f, 0.f, 0.f);
	else if (p == "sprite") { v.text = part.texture; v.hasText = true; }
	v.hasValue = true;
	return v;
}

// Keys one of a part's properties at the playhead. The interpolation of a
// picture or a visibility key is a step: there is nothing between them.
void KeyPart(Character2DDocument& doc, const std::string& part, const char* prop, const Vec4& value,
	const std::string& text, bool interactive)
{
	if (!doc.anim.HasActiveClip()) return;
	const float t = doc.anim.SnapTime(doc.anim.playhead);
	const std::string p = prop;
	const int mode = (p == "sprite" || p == "visible") ? (int)p3d::INTERP_STEP : (int)p3d::INTERP_LINEAR;
	const auto write = [&]() {
		const int track = doc.anim.FindOrCreateTrack(doc.anim.activeClip, part, prop);
		doc.anim.SetTrackKey(doc.anim.activeClip, track, t, value, text, mode);
	};
	if (interactive) { write(); doc.anim.dirty = true; doc.anim.clipsRevision++; }
	else doc.anim.PushSnapshotEdit("Key " + part + " " + p, write);
}

void DrawPartKeysPanel(Character2DDocument& doc, const std::vector<TextureChoice>& textures)
{
	if (doc.asset.parts.empty()) { ImGui::TextDisabled("No sprites yet - add artwork in the Sprites stage."); return; }
	if (!doc.anim.HasActiveClip()) { ImGui::TextDisabled("Make a clip to key parts in."); return; }
	ImGui::TextWrapped("Key a part's picture, draw order, colour or visibility at the playhead.");
	ImGui::Separator();
	ImGui::BeginChild("##partlist", ImVec2(0, 110.f), true);
	for (size_t i = 0; i < doc.asset.parts.size(); i++)
		if (ImGui::Selectable(doc.asset.parts[i].name.c_str(), doc.selectedSprite == doc.asset.parts[i].name))
			doc.selectedSprite = doc.asset.parts[i].name;
	ImGui::EndChild();
	const int pi = doc.FindSprite(doc.selectedSprite);
	if (pi < 0) { ImGui::TextDisabled("Pick a part."); return; }
	const SpritePart2D& part = doc.asset.parts[pi];
	const std::string name = part.name;

	// Picture.
	{
		const SkeletonAnimationInstance::TrackValue v = PartValue(doc, part, "sprite");
		std::string shown = v.text;
		const size_t slash = shown.find_last_of("/\\");
		if (slash != std::string::npos) shown = shown.substr(slash + 1);
		ImGui::SetNextItemWidth(-1.f);
		if (ImGui::BeginCombo("##picture", shown.empty() ? "(none)" : shown.c_str()))
		{
			for (size_t t = 0; t < textures.size(); t++)
				if (ImGui::Selectable(textures[t].label.c_str(), textures[t].relativePath == v.text))
					KeyPart(doc, name, "sprite", Vec4(0.f, 0.f, 0.f, 0.f), textures[t].relativePath, false);
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Picture - swaps the part's artwork from this key on\n(a hand opening, a mouth shape, a blink).");
	}
	// Draw order.
	{
		SkeletonAnimationInstance::TrackValue v = PartValue(doc, part, "z");
		float z = v.value.x;
		ImGui::SetNextItemWidth(-1.f);
		if (ImGui::DragFloat("##z", &z, 0.01f, -100.f, 100.f, "draw order %.2f")) KeyPart(doc, name, "z", Vec4(z, 0.f, 0.f, 0.f), "", true);
		if (ImGui::IsItemActivated()) doc.anim.BeginInteractiveEdit();
		if (ImGui::IsItemDeactivatedAfterEdit()) doc.anim.EndInteractiveEdit("Key " + name + " z");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Draw order: higher is nearer. An arm swinging behind the body and back.");
	}
	// Colour.
	{
		SkeletonAnimationInstance::TrackValue v = PartValue(doc, part, "color");
		float c[4] = { v.value.x, v.value.y, v.value.z, v.value.w };
		ImGui::SetNextItemWidth(-1.f);
		if (ImGui::ColorEdit4("##tint", c, ImGuiColorEditFlags_AlphaBar)) KeyPart(doc, name, "color", Vec4(c[0], c[1], c[2], c[3]), "", true);
		if (ImGui::IsItemActivated()) doc.anim.BeginInteractiveEdit();
		if (ImGui::IsItemDeactivatedAfterEdit()) doc.anim.EndInteractiveEdit("Key " + name + " colour");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Tint and opacity - a hit flash, a fade out.");
	}
	// Visible.
	{
		SkeletonAnimationInstance::TrackValue v = PartValue(doc, part, "visible");
		bool vis = v.value.x > 0.5f;
		if (ImGui::Checkbox("Visible", &vis)) KeyPart(doc, name, "visible", Vec4(vis ? 1.f : 0.f, 0.f, 0.f, 0.f), "", false);
	}
	ImGui::Separator();
	if (ImGui::SmallButton("Clear this part's keys"))
	{
		doc.anim.PushSnapshotEdit("Clear " + name + " keys", [&]() {
			std::vector<PropertyTrack>& tr = doc.anim.clips[doc.anim.activeClip].Tracks;
			tr.erase(std::remove_if(tr.begin(), tr.end(), [&](const PropertyTrack& t) { return t.Target == name; }), tr.end());
		});
		doc.anim.selectedKeys.clear();
	}
	ImGui::TextDisabled("Keys show in the timeline as '%s . <property>'.", name.c_str());
}

// Solves every pin across a range, keying the bones it moves - the 2D
// counterpart of the 3D IK bake: a foot that stays put while the clip moves
// the hips.
int BakePinsImpl(Character2DDocument& doc, float from, float to)
{
	Character2DPreview* pv = doc.preview.get();
	if (!pv || !pv->built.instance || pv->pins.empty() || !doc.anim.HasActiveClip()) return 0;
	if (to < from) std::swap(from, to);
	const float step = 1.f / (doc.anim.snapFps > 0.f ? doc.anim.snapFps : 30.f);
	const Animation clip = doc.anim.clips[doc.anim.activeClip];
	int keyed = 0;
	doc.anim.PushSnapshotEdit("Bake pins", [&]() {
		for (float t = from; t <= to + step * 0.5f; t += step)
		{
			const float snapped = doc.anim.SnapTime(t);
			pv->built.instance->ApplyAnimationAtTime(clip, snapped);
			pv->posedBones.clear();
			pv->ApplyPins(-1);
			for (size_t i = 0; i < pv->posedBones.size(); i++)
				if (AnimationEditor::KeyBoneAtTime(doc.anim, pv->posedBones[i], snapped)) keyed++;
		}
		// A key a frame is what a bake leaves; keep only the ones that matter.
		keyed -= doc.anim.ReduceKeys(doc.anim.activeClip, 0.0005f, 0.1f);
	});
	pv->posedBones.clear();
	doc.anim.externalPoseOverrides.clear();
	return keyed;
}

void DrawPinsPanel(Character2DDocument& doc)
{
	Character2DPreview* pv = doc.preview.get();
	if (!pv) return;
	ImGui::TextWrapped("Pinned joints stay where they are while you pose the rest. Double-click a joint in the viewport to pin it.");
	ImGui::Separator();
	int unpin = -1;
	for (std::map<int, Vec3>::const_iterator it = pv->pins.begin(); it != pv->pins.end(); ++it)
	{
		ImGui::PushID(it->first);
		const std::string bone = (size_t)it->first < doc.asset.bones.size() ? doc.asset.bones[it->first].name : std::string("?");
		ImGui::Text("%s  (%.2f, %.2f)", bone.c_str(), it->second.x, it->second.y);
		ImGui::SameLine();
		if (ImGui::SmallButton("Unpin")) unpin = it->first;
		ImGui::PopID();
	}
	if (unpin >= 0) pv->pins.erase(unpin);
	if (pv->pins.empty()) ImGui::TextDisabled("No pins.");
	const int sel = doc.SelectedBoneId();
	if (sel >= 0 && !pv->pins.count(sel) && ImGui::SmallButton(("Pin " + doc.selectedBone).c_str())) pv->TogglePin(sel);
	if (!pv->pins.empty() && ImGui::SmallButton("Unpin all")) pv->pins.clear();

	if (!pv->pins.empty() && doc.anim.HasActiveClip())
	{
		ImGui::Separator();
		ImGui::TextDisabled("Bake: key the pinned limbs across a range");
		static float from = 0.f, to = -1.f;
		const float dur = doc.anim.clips[doc.anim.activeClip].Duration;
		if (to < 0.f || to > dur) to = dur;
		ImGui::SetNextItemWidth(80.f); ImGui::DragFloat("From", &from, 0.02f, 0.f, dur, "%.2fs");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80.f); ImGui::DragFloat("To", &to, 0.02f, 0.f, dur, "%.2fs");
		if (ImGui::Button("Bake pins"))
		{
			const int keyed = BakePinsImpl(doc, from, to);
			echo("Character 2D: baked " + std::to_string(keyed) + " keys from " + std::to_string(pv->pins.size()) + " pin(s)");
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Every frame in the range: the clip is sampled, each pin's limb is solved\nback onto its pin, and the result keyed - feet that stay planted.");
	}
}

void DrawAnimateSidePanel(Character2DDocument& doc, const std::vector<TextureChoice>& textures)
{
	ImGui::BeginChild("##animside", ImVec2(kSidePanelWidth + 10.f, 0), false);
	if (ImGui::BeginTabBar("##animtabs"))
	{
		if (ImGui::BeginTabItem("Bones"))
		{
			// The .p3da editor's own bone list, pointed at this character's
			// rig - bone selection, numeric pose fields, pose copy and mirror.
			AnimationEditor::DrawSkeletonPanel(doc.anim,
				"This character has no bones yet.\nBuild the skeleton in the Bones stage first.");
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Parts"))
		{
			DrawPartKeysPanel(doc, textures);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Pins"))
		{
			DrawPinsPanel(doc);
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::EndChild();
}

} // namespace

int BakePins(Character2DDocument& doc, float from, float to) { return BakePinsImpl(doc, from, to); }

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
	case Character2DDocument::Mode::Animate: DrawAnimateSidePanel(doc, textures); break;
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
