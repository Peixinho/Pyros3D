//============================================================================
// Name        : SceneEditorSprites.cpp
// Description : Everything the Scene Editor does with Sprite2D and
//               SpriteBatch2D: the edit operations, their Properties panel,
//               the agent commands that reach the same controls, and the
//               conversion of the Plane-and-material sprites scenes used to
//               be made of.
//
//               A separate file rather than more of SceneEditor.cpp for the
//               reason SceneEditorTerrain.cpp is one: it is one subject, and
//               that file is seventeen thousand lines already.
//============================================================================

#include "SceneEditor.h"
#include "SceneCommands.h"
#include <Pyros3D/Rendering/Components/Sprite2D/Sprite2D.h>
#include <Pyros3D/Rendering/Components/Sprite2D/SpriteBatch2D.h>
#include <Pyros3D/Rendering/Components/Sprite2D/SpriteMask2D.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/Assets/SpriteSheet2D/SpriteSheet2D.h>
#include <Pyros3D/Assets/TileSet2D/TileSet2D.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/AnimationManager/TextureAnimation.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>

using namespace p3d;
using json = nlohmann::json;

namespace {

	template <typename T>
	T* FindComponent(GameObject* go, const uint32 type)
	{
		if (!go) return NULL;
		const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
		for (size_t i = 0; i < comps.size(); i++)
			if (comps[i] && comps[i]->GetComponentType() == type)
				return static_cast<T*>(comps[i].get());
		return NULL;
	}

	Sprite2D* FindSprite(GameObject* go) { return FindComponent<Sprite2D>(go, ComponentType::Sprite2D); }
	SpriteBatch2D* FindBatch(GameObject* go) { return FindComponent<SpriteBatch2D>(go, ComponentType::SpriteBatch2D); }

	SceneObject* FindObjectByName(SceneObjects* so, const std::string& name)
	{
		const std::map<uint32, SceneObject*>& list = so->GetList();
		for (std::map<uint32, SceneObject*>::const_iterator i = list.begin(); i != list.end(); i++)
			if (i->second && i->second->GetType() == SceneObjectTypes::GAMEOBJECT
				&& i->second->GetName() == name)
				return i->second;
		return NULL;
	}

	std::string Lower(std::string s)
	{
		for (size_t i = 0; i < s.size(); i++) s[i] = (char)tolower((unsigned char)s[i]);
		return s;
	}

	bool EndsWith(const std::string &s, const std::string &tail)
	{
		return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
	}

	// The sprite's own texture, as something ImGui can draw - the very
	// texture the scene draws it with, so the frame picker cannot show a
	// different picture from the viewport.
	void* SpriteImGuiTexture(Sprite2D* sp)
	{
		std::shared_ptr<IMaterial> mat = sp->SharedMaterial();
		GenericShaderMaterial* gm = mat ? dynamic_cast<GenericShaderMaterial*>(mat.get()) : NULL;
		Texture* t = gm ? gm->GetColorMap() : NULL;
		if (!t) return NULL;
		return (void*)GetActiveRenderDevice().GetImGuiTextureID(t->GetBindID(), t->GetTextureType());
	}

	// Which sprite the Properties panel is playing a clip on while the scene
	// itself is not running, and when it last ticked.
	Sprite2D* g_previewSprite = NULL;
	double g_previewLast = 0.0;

}

Sprite2D* SceneEditor::SpriteOf(GameObject* go) { return FindSprite(go); }

bool SceneEditor::OwnerDrawsSprites(GameObject* go)
{
	return FindSprite(go) != NULL || FindBatch(go) != NULL;
}

// --- loading -----------------------------------------------------------------

bool SceneEditor::ApplySpriteSource(Sprite2D* sp, const std::string& path, std::string& errOut)
{
	if (!sp) { errOut = "no sprite"; return false; }
	if (path.empty()) { errOut = "a sprite needs an image or a .p3ds sprite sheet"; return false; }
	const std::string abs = ResolveAssetPath(path);
	if (!sp->LoadSource(abs, [this](const std::string &p) { return ResolveAssetPath(p); }, &errOut))
		return false;
	// What the scene file stores: project-relative, "assets/" prefix intact.
	sp->SetSourcePath(DisplayPath(abs));
	return true;
}

bool SceneEditor::ApplySpriteMaterial(Sprite2D* sp, const std::string& path, std::string& errOut)
{
	if (!sp) { errOut = "no sprite"; return false; }
	if (path.empty()) { sp->ClearCustomMaterial(); return true; }
	if (!ProjectManager::IsMaterialExtension(path)) { errOut = "'" + path + "' is not a material (.mat)"; return false; }
	const std::string abs = ResolveAssetPath(path);
	const std::string rel = DisplayPath(abs);
	std::shared_ptr<IMaterial> m = SceneSerializer::LoadMaterialFile(abs, rel);
	if (!m) { errOut = "could not load material " + path; return false; }
	sp->SetCustomMaterial(m, rel);
	return true;
}

int SceneEditor::ReloadSpritesUsing(const std::string& path)
{
	if (path.empty()) return 0;
	const std::string want = DisplayPath(ResolveAssetPath(path));
	int n = 0;
	std::vector<GameObject*> all;
	scene->CollectGameObjectsRecursive(all);
	for (size_t i = 0; i < all.size(); i++)
	{
		Sprite2D* sp = FindSprite(all[i]);
		if (!sp || sp->GetSourcePath() != want) continue;
		// Kept across the reload: which frame (by name, since re-cutting
		// renumbers them), and which clip it starts on.
		const std::string frameName = sp->GetFrameName();
		const int32 frame = sp->GetFrame();
		const std::string playing = sp->GetClip();
		std::string err;
		if (!ApplySpriteSource(sp, want, err)) { echo("WARNING: " + err); continue; }
		if (frameName.empty() || !sp->SetFrameByName(frameName)) sp->SetFrame(frame);
		if (!playing.empty()) sp->Play(playing, true);
		n++;
	}
	return n;
}

// --- operations ----------------------------------------------------------------

// A sprite: a Sprite2D drawing from an image or a .p3ds sprite sheet. The
// component makes its own RenderingComponent, so this is the whole of it.
bool SceneEditor::OpAddSprite(uint32 goId, const std::string& texturePath, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT)
	{
		errOut = "not a game object";
		return false;
	}
	GameObject* go = (GameObject*)obj->GetPTR();
	if (FindSprite(go)) { errOut = "this object already has a sprite"; return false; }
	const std::string before = SnapshotSubtree(goId);

	std::shared_ptr<Sprite2D> sp = std::make_shared<Sprite2D>();
	// With no image yet the sprite draws nothing until it is given one -
	// which is what Add > Sprite makes, and the Properties panel's Source
	// field is where it gets one.
	if (!texturePath.empty() && !ApplySpriteSource(sp.get(), texturePath, errOut))
		return false;
	// A scene that is 2D-lit should not need every sprite told so, but a
	// sprite that ignores the scene's lights by default is the one that
	// looks like its art. Left off; `Lit` is one checkbox.
	go->Add(std::static_pointer_cast<IComponent>(sp));
	MarkSceneDirty();
	PushReplaceCommand(goId, before, "Add Sprite");
	return true;
}

bool SceneEditor::OpAddSpriteBatch2D(uint32 goId, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "not a game object"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();
	if (FindBatch(go)) { errOut = "this object already has a sprite batch"; return false; }
	if (FindSprite(go))
	{
		// Both draw through the object's one RenderingComponent.
		errOut = "a sprite batch goes on a PARENT of the sprites, not on a sprite";
		return false;
	}
	const std::string before = SnapshotSubtree(goId);
	go->Add(std::static_pointer_cast<IComponent>(std::make_shared<SpriteBatch2D>()));
	MarkSceneDirty();
	PushReplaceCommand(goId, before, "Add Sprite Batch");
	return true;
}

// Turns a Plane-and-material sprite - what Add Sprite made before Sprite2D
// existed - into a Sprite2D that looks the same. An animated one (frames cut
// into separate PNGs beside the sheet) gets a .p3ds over the ORIGINAL sheet,
// so the cut-up copies are no longer used.
bool SceneEditor::OpConvertLegacySprite(uint32 goId, std::string& errOut, const bool record)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "not a game object"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();
	if (FindSprite(go)) { errOut = "already a Sprite2D"; return false; }

	std::shared_ptr<IComponent> rcHeld;
	RenderingComponent* rc = NULL;
	const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
	for (size_t i = 0; i < comps.size(); i++)
		if (comps[i] && comps[i]->GetComponentType() == ComponentType::RenderingComponent)
		{ rcHeld = comps[i]; rc = static_cast<RenderingComponent*>(comps[i].get()); }
	if (!rc) { errOut = "no RenderingComponent"; return false; }
	if (rc->IsCharacter2D()) { errOut = "a 2D character, not a sprite"; return false; }
	Plane* plane = dynamic_cast<Plane*>(rc->GetRenderable());
	if (!plane) { errOut = "not a quad"; return false; }
	std::vector<RenderingMesh*> &meshes = rc->GetMeshes();
	GenericShaderMaterial* gm = (!meshes.empty() && meshes[0]->Material)
		? dynamic_cast<GenericShaderMaterial*>(meshes[0]->Material.get()) : NULL;
	if (!gm) { errOut = "not a generic material"; return false; }

	namespace fs = std::filesystem;
	TextureAnimationInstance* inst = static_cast<TextureAnimationInstance*>(rc->GetActiveTextureAnimation());
	TextureAnimation* anim = inst ? inst->GetOwner() : NULL;

	std::string source;      // what the sprite will draw from, resolved
	std::string autoPlay;
	int32 frameW = 0, frameH = 0;

	if (anim && anim->GetNumberFrames() > 0)
	{
		// Frames are <stem>_frames/frame_NN.png beside <stem>.png - see what
		// Slice Spritesheet used to write.
		Texture* f0 = anim->GetFrame(0);
		const std::string f0Path = f0 ? f0->GetFilename() : std::string();
		const fs::path framesDir = fs::path(f0Path).parent_path();
		const std::string dirName = framesDir.filename().string();
		if (f0Path.empty() || !EndsWith(dirName, "_frames"))
		{ errOut = "its animation frames are not a sliced sheet's"; return false; }
		const std::string stem = dirName.substr(0, dirName.size() - 7);
		fs::path sheetImage;
		const char* exts[] = { ".png", ".jpg", ".jpeg", ".tga", ".bmp" };
		for (int e = 0; e < 5 && sheetImage.empty(); e++)
			if (fs::exists(framesDir.parent_path() / (stem + exts[e])))
				sheetImage = framesDir.parent_path() / (stem + exts[e]);
		if (sheetImage.empty()) { errOut = "the sheet '" + stem + "' its frames were cut from is gone"; return false; }

		int32 sw = 0, sh = 0;
		if (!TileSet2DReadImageSize(f0Path, frameW, frameH)
			|| !TileSet2DReadImageSize(sheetImage.string(), sw, sh) || frameW < 1 || frameH < 1)
		{ errOut = "could not measure the sheet"; return false; }

		// <stem>.p3ds, unless a sheet of that name is already there and is
		// cut some other way - somebody's own, which this must not rewrite
		// and cannot use.
		const int32 wanted = (int32)anim->GetNumberFrames();
		auto usable = [&](const fs::path &file) {
			SpriteSheet2D have;
			if (!LoadSpriteSheet2D(file.string(), have)) return false;
			return (int32)have.frames.size() >= wanted && have.FindClip("default") >= 0
				&& have.frames[0].w == frameW && have.frames[0].h == frameH;
		};
		fs::path sheetFile = sheetImage.parent_path() / (stem + ".p3ds");
		if (fs::exists(sheetFile) && !usable(sheetFile))
			sheetFile = sheetImage.parent_path() / (stem + "_sliced.p3ds");
		if (!fs::exists(sheetFile) || !usable(sheetFile))
		{
			SpriteSheet2D s;
			s.image = DisplayPath(sheetImage.string());
			s.SetImageSize(sw, sh);
			if (s.SliceGrid(frameW, frameH, 0, 0, stem + "_") < 1)
			{ errOut = "the sheet does not divide into its frames"; return false; }
			// Sized so the sheet's art is as big as this sprite draws it;
			// the sprite then needs no scale of its own to match.
			s.pixelsPerUnit = (f32)frameH / (2.f * plane->GetHeight());
			const int32 n = std::min<int32>((int32)anim->GetNumberFrames(), (int32)s.frames.size());
			s.AddClipRange("default", 0, n - 1, inst->GetFrameSpeed(), inst->IsLooping());
			s.clips[0].pingPong = inst->IsYoyo();
			std::string werr;
			if (!SaveSpriteSheet2D(sheetFile.string(), s, &werr)) { errOut = werr; return false; }
		}
		source = sheetFile.string();
		autoPlay = "default";
	}
	else
	{
		Texture* t = gm->GetColorMap();
		if (!t || t->GetFilename().empty()) { errOut = "its material has no texture file"; return false; }
		source = t->GetFilename();
		if (!TileSet2DReadImageSize(source, frameW, frameH)) { errOut = "could not measure " + source; return false; }
	}

	const std::string before = record ? SnapshotSubtree(goId) : std::string();

	// What the old sprite looked like, read before it goes.
	const Vec4 tint = gm->GetColor();
	// A quad sprite was always lit one way or the other: by 2D lighting if
	// it had been opted in, by the scene's ordinary lighting if not.
	const int32 lighting = (gm->GetOptions() & ShaderUsage::Lighting2D) ? SpriteLighting2D::Lit2D
		: ((gm->GetOptions() & ShaderUsage::Diffuse) ? SpriteLighting2D::Lit3D : SpriteLighting2D::Unlit);
	Vec2 pivot(0.5f, 0.5f);
	const bool hadPivot = GetSpritePivot(rc, pivot);
	const f32 quadW = 2.f * plane->GetWidth(), quadH = 2.f * plane->GetHeight();

	std::shared_ptr<Sprite2D> sp = std::make_shared<Sprite2D>();
	if (!ApplySpriteSource(sp.get(), DisplayPath(source), errOut)) return false;

	// Same height on screen without touching the object's scale: the old quad
	// was quadH tall in local units, so the frame's pixels are spread over
	// that. A sheet this conversion wrote already says so itself.
	const f32 ppu = (f32)frameH / quadH;
	if (fabsf(sp->GetSheet().pixelsPerUnit - ppu) > 1e-3f) sp->SetPixelsPerUnit(ppu);
	// The old quad's width came from the texture's aspect, and anything else
	// was the object's scale - which stays. Only when the quad was NOT the
	// frame's shape does x need correcting, so the picture is not narrowed.
	const f32 newW = (f32)frameW / ppu;
	if (fabsf(newW - quadW) > 1e-3f && newW > 1e-6f)
	{
		const Vec3 sc = go->GetScale();
		go->SetScale(Vec3(sc.x * quadW / newW, sc.y, sc.z));
	}
	if (tint.x != 1.f || tint.y != 1.f || tint.z != 1.f || tint.w != 1.f) sp->SetColor(tint);
	sp->SetLighting(lighting);
	if (hadPivot && (fabsf(pivot.x - 0.5f) > 1e-4f || fabsf(pivot.y - 0.5f) > 1e-4f)) sp->SetPivot(pivot);
	if (!autoPlay.empty()) sp->SetAutoPlay(autoPlay);

	// The old mesh goes; the sprite makes its own.
	rc->SetActiveTextureAnimation(NULL);
	go->Remove(rcHeld);
	go->Add(std::static_pointer_cast<IComponent>(sp));

	MarkSceneDirty();
	if (record) PushReplaceCommand(goId, before, "Convert to Sprite2D");
	return true;
}

int SceneEditor::ConvertLegacySprites(std::string& report)
{
	const std::map<uint32, SceneObject*> list = sceneObjects->GetList();
	int done = 0, skipped = 0;
	for (std::map<uint32, SceneObject*>::const_iterator i = list.begin(); i != list.end(); ++i)
	{
		if (!i->second || i->second->GetType() != SceneObjectTypes::GAMEOBJECT) continue;
		GameObject* go = (GameObject*)i->second->GetPTR();
		if (FindSprite(go)) continue;
		RenderingComponent* rc = FindComponent<RenderingComponent>(go, ComponentType::RenderingComponent);
		// Only what Add Sprite made: a quad, see-through, with a texture.
		// A plane used as a floor in a 3D scene is not a sprite.
		if (!rc || rc->IsCharacter2D() || !dynamic_cast<Plane*>(rc->GetRenderable())) continue;
		std::vector<RenderingMesh*> &meshes = rc->GetMeshes();
		GenericShaderMaterial* gm = (!meshes.empty() && meshes[0]->Material)
			? dynamic_cast<GenericShaderMaterial*>(meshes[0]->Material.get()) : NULL;
		if (!gm || !gm->IsTransparent() || !gm->GetColorMap()) continue;
		std::string err;
		if (OpConvertLegacySprite(i->first, err, false)) done++;
		else
		{
			skipped++;
			report += i->second->GetName() + ": " + err + "\n";
		}
	}
	if (done > 0)
	{
		// One edit a hundred objects wide: not something a per-object undo
		// entry describes, so the history starts again from here.
		sceneUndo.Clear();
		MarkSceneDirty();
	}
	report = std::to_string(done) + " converted, " + std::to_string(skipped) + " left as they were\n" + report;
	return done;
}

// --- properties ------------------------------------------------------------------

void SceneEditor::DrawSprite2DProperties(GameObject* go, uint32 goId, Sprite2D* sp)
{
	(void)go;
	ImGui::Text("Sprite");

	// Source. Typed, or dropped from the Assets panel.
	if (spritePropsFor != sp) { spritePropsFor = sp; spritePropsSource = sp->GetSourcePath(); }
	bool apply = ImGui::InputText("Source", &spritePropsSource, ImGuiInputTextFlags_EnterReturnsTrue);
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_REL"))
		{
			const char* rel = (const char*)payload->Data;
			if (rel && (ProjectManager::IsTextureExtension(rel) || ProjectManager::IsSpriteSheetExtension(rel)))
			{ spritePropsSource = rel; apply = true; }
		}
		ImGui::EndDragDropTarget();
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("An image, or a .p3ds sprite sheet. Drop one here from Assets,\nor type its path and press Enter.");
	if (apply && spritePropsSource != sp->GetSourcePath())
	{
		const std::string b = SnapshotSubtree(goId);
		std::string err;
		if (ApplySpriteSource(sp, spritePropsSource, err))
		{ MarkSceneDirty(); PushReplaceCommand(goId, b, "Set Sprite Source"); }
		else echo("ERROR: " + err);
		spritePropsSource = sp->GetSourcePath();
	}

	const SpriteSheet2D &sheet = sp->GetSheet();
	const bool isSheet = ProjectManager::IsSpriteSheetExtension(sp->GetSourcePath());
	if (isSheet)
	{
		if (ImGui::Button("Edit Sprite Sheet...") && hostOpenSpriteSheet)
			hostOpenSpriteSheet(ResolveAssetPath(sp->GetSourcePath()));
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Frames, pivots, nine-slice borders and clips belong to the\nsheet. Changes there reach every sprite that uses it.");
	}
	else if (!sp->GetSourcePath().empty())
	{
		if (ImGui::Button("Make Sprite Sheet...") && hostCreateSpriteSheet)
			hostCreateSpriteSheet(sp->GetSourcePath(), goId);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Cuts this image into frames and clips. The sprite then\ndraws from the sheet instead of the whole picture.");
	}

	if (sheet.frames.empty())
	{
		ImGui::TextDisabled("No image yet - set a Source.");
		return;
	}

	const Vec4 rect = sp->GetLocalRect();
	ImGui::TextDisabled("%d frame%s, %d clip%s - %.2f x %.2f units", (int)sheet.frames.size(),
		sheet.frames.size() == 1 ? "" : "s", (int)sheet.clips.size(), sheet.clips.size() == 1 ? "" : "s",
		rect.z - rect.x, rect.w - rect.y);

	// --- frame ------------------------------------------------------------
	if (sheet.frames.size() > 1)
	{
		void* tex = SpriteImGuiTexture(sp);
		const std::string current = sp->GetFrameName();
		if (ImGui::BeginCombo("Frame", current.c_str(), ImGuiComboFlags_HeightLarge))
		{
			const float cell = 48.f;
			const float avail = ImGui::GetContentRegionAvail().x;
			const int perRow = std::max(1, (int)(std::max(avail, 260.f) / (cell + 6.f)));
			for (int32 i = 0; i < (int32)sheet.frames.size(); i++)
			{
				ImGui::PushID(i);
				if (i % perRow) ImGui::SameLine();
				const ImVec2 p = ImGui::GetCursorScreenPos();
				const bool picked = ImGui::InvisibleButton("##f", ImVec2(cell, cell));
				ImDrawList* dl = ImGui::GetWindowDrawList();
				dl->AddRectFilled(p, ImVec2(p.x + cell, p.y + cell), IM_COL32(40, 42, 48, 255));
				const SpriteFrame2D &f = sheet.frames[(size_t)i];
				if (tex && f.w > 0 && f.h > 0)
				{
					// Fitted, not stretched: a wide frame and a tall one both
					// keep their shape in a square cell.
					const float k = std::min(cell / (float)f.w, cell / (float)f.h);
					const float w = f.w * k, h = f.h * k;
					const ImVec2 a(p.x + (cell - w) * 0.5f, p.y + (cell - h) * 0.5f);
					const Vec4 uv = sheet.UVRect(i);
					dl->AddImage((ImTextureID)tex, a, ImVec2(a.x + w, a.y + h), ImVec2(uv.x, uv.y), ImVec2(uv.z, uv.w));
				}
				if (i == sp->GetFrame())
					dl->AddRect(p, ImVec2(p.x + cell, p.y + cell), IM_COL32(110, 200, 255, 255), 0.f, 0, 2.f);
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%d  %s", (int)i, f.name.c_str());
				if (picked)
				{
					const std::string b = SnapshotSubtree(goId);
					sp->SetFrame(i);
					MarkSceneDirty();
					PushReplaceCommand(goId, b, "Set Sprite Frame");
				}
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}
	}

	// --- look ---------------------------------------------------------------
	bool fx = sp->GetFlipX(), fy = sp->GetFlipY();
	{
		const std::string b = SnapshotSubtree(goId);
		if (ImGui::Checkbox("Flip X", &fx)) { sp->SetFlipX(fx); MarkSceneDirty(); PushReplaceCommand(goId, b, "Flip Sprite"); }
		ImGui::SameLine();
		if (ImGui::Checkbox("Flip Y", &fy)) { sp->SetFlipY(fy); MarkSceneDirty(); PushReplaceCommand(goId, b, "Flip Sprite"); }
	}

	Vec4 col = sp->GetColor();
	BeginComponentUndo(goId);
	if (ImGui::ColorEdit4("Colour", (float*)&col)) { sp->SetColor(col); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Sprite Colour");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Multiplied over the art, alpha included. It lives in the\nsprite's vertices, so tinting one never costs another draw call.");

	int mode = sp->GetDrawMode();
	const char* modes[] = { "Simple", "Sliced (nine-slice)", "Tiled" };
	{
		const std::string b = SnapshotSubtree(goId);
		if (ImGui::Combo("Draw Mode", &mode, modes, 3))
		{
			// Going from Simple, start at the size it already is: a panel
			// that jumps to 1x1 the moment it is made stretchable is the
			// first thing anyone would have to fix.
			if (sp->GetDrawMode() == SpriteDraw2D::Simple)
				sp->SetSize(Vec2(rect.z - rect.x, rect.w - rect.y));
			sp->SetDrawMode(mode);
			MarkSceneDirty();
			PushReplaceCommand(goId, b, "Set Sprite Draw Mode");
		}
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Simple: the frame at its own size.\nSliced: stretched to Size, corners kept (set the borders in the sheet).\nTiled: repeated to fill Size.");
	if (sp->GetDrawMode() != SpriteDraw2D::Simple)
	{
		Vec2 sz = sp->GetSize();
		BeginComponentUndo(goId);
		if (ImGui::DragFloat2("Size", (float*)&sz, 0.02f, 0.01f, 10000.f)) { sp->SetSize(sz); MarkSceneDirty(); }
		EndComponentUndo(goId, "Set Sprite Size");
		if (sp->GetDrawMode() == SpriteDraw2D::Sliced && sheet.ValidFrame(sp->GetFrame())
			&& !sheet.frames[(size_t)sp->GetFrame()].HasBorder())
			ImGui::TextColored(ImVec4(1.f, 0.75f, 0.4f, 1.f), "This frame has no borders, so it only stretches.");
	}

	f32 ppu = sp->GetPixelsPerUnit();
	BeginComponentUndo(goId);
	if (ImGui::DragFloat("Pixels Per Unit", &ppu, 0.5f, 0.f, 4096.f, ppu > 0.f ? "%.1f" : "sheet's"))
	{ sp->SetPixelsPerUnit(ppu); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Pixels Per Unit");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("How many image pixels make one world unit. 0 uses the\nsheet's (%.1f), which keeps every sprite on it the same scale.", sheet.pixelsPerUnit);

	bool ownPivot = sp->HasPivot();
	{
		const std::string b = SnapshotSubtree(goId);
		if (ImGui::Checkbox("Own Pivot", &ownPivot))
		{
			if (ownPivot) sp->SetPivot(sp->GetPivot()); else sp->ClearPivot();
			MarkSceneDirty();
			PushReplaceCommand(goId, b, "Set Sprite Pivot");
		}
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Off, the pivot is the frame's own (set in the sheet).\nOn, this sprite has one of its own.");
	if (sp->HasPivot())
	{
		Vec2 pv = sp->GetPivot();
		BeginComponentUndo(goId);
		if (ImGui::DragFloat2("Pivot", (float*)&pv, 0.01f, -1.f, 2.f)) { sp->SetPivot(pv); MarkSceneDirty(); }
		EndComponentUndo(goId, "Set Sprite Pivot");
		struct P { const char* name; f32 x, y; };
		static const P presets[] = {
			{ "Bottom", 0.5f, 0.f }, { "Centre", 0.5f, 0.5f },
			{ "Top", 0.5f, 1.f }, { "Left", 0.f, 0.5f }, { "Right", 1.f, 0.5f },
		};
		for (int pi = 0; pi < 5; pi++)
		{
			if (pi) ImGui::SameLine();
			if (ImGui::SmallButton(presets[pi].name))
			{
				const std::string b = SnapshotSubtree(goId);
				sp->SetPivot(Vec2(presets[pi].x, presets[pi].y));
				MarkSceneDirty();
				PushReplaceCommand(goId, b, "Set Sprite Pivot");
			}
		}
	}

	int lighting = sp->GetLighting();
	const char* lightings[] = { "Unlit", "2D lighting", "3D lighting" };
	{
		const std::string b = SnapshotSubtree(goId);
		if (ImGui::Combo("Lighting", &lighting, lightings, 3))
		{ sp->SetLighting(lighting); MarkSceneDirty(); PushReplaceCommand(goId, b, "Set Sprite Lighting"); }
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Unlit: the art exactly, whatever lights the scene has.\n2D lighting: distance falloff from each light and shadows from\nOccluder 2D - what a lit 2D scene wants.\n3D lighting: the scene's ordinary lighting, for a sprite standing\nin a 3D world.\nA Sprite Batch merges lit sprites in cells, by the lights near each.");

	{
		// Typed, or a .mat dropped on it from Assets. Committed on Enter
		// or on leaving the box.
		static std::string matText;
		static Sprite2D* matTextFor = NULL;
		if (matTextFor != sp || (!ImGui::IsAnyItemActive() && matText != sp->GetMaterialPath()))
		{ matText = sp->GetMaterialPath(); matTextFor = sp; }
		ImGui::SetNextItemWidth(200);
		std::string want;
		bool commit = ImGui::InputTextWithHint("Material", "its own", &matText, ImGuiInputTextFlags_EnterReturnsTrue)
			|| ImGui::IsItemDeactivatedAfterEdit();
		if (commit) want = matText;
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_REL"))
			{
				const char* rel = (const char*)payload->Data;
				if (rel && ProjectManager::IsMaterialExtension(rel)) { want = rel; commit = true; }
			}
			ImGui::EndDragDropTarget();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("A material (.mat) in place of the one the sprite makes for itself -\nfor a dissolve, an outline, a flash. The sprite still supplies its\nframe's shape, texture coordinates and tint; the material shades it\nand names its own textures. Lighting and Blend are then not used.\nEmpty for the sprite's own.");
		if (commit && want != sp->GetMaterialPath())
		{
			const std::string b = SnapshotSubtree(goId);
			std::string merr;
			if (ApplySpriteMaterial(sp, want, merr)) { MarkSceneDirty(); PushReplaceCommand(goId, b, "Set Sprite Material"); }
			else echo("ERROR: " + merr);
			matText = sp->GetMaterialPath();
		}
	}

	{
		int mi = sp->GetMaskInteraction();
		const char* modes[] = { "None", "Visible inside mask", "Visible outside mask" };
		{
			const std::string b = SnapshotSubtree(goId);
			if (ImGui::Combo("Mask", &mi, modes, 3))
			{ sp->SetMaskInteraction(mi); MarkSceneDirty(); PushReplaceCommand(goId, b, "Set Sprite Mask Interaction"); }
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Cut to a Sprite Mask's shape: shown only where the mask is, or only\nwhere it is not. Which mask: the nearest one on this object or above\nit in the tree, unless one is named below.");
		if (sp->GetMaskInteraction() != Sprite2D::MaskNone)
		{
			const std::string cur = sp->GetMaskName();
			if (ImGui::BeginCombo("Mask Object", cur.empty() ? "(nearest above)" : cur.c_str()))
			{
				const std::string b = SnapshotSubtree(goId);
				if (ImGui::Selectable("(nearest above)", cur.empty()))
				{ sp->SetMaskName(std::string()); MarkSceneDirty(); PushReplaceCommand(goId, b, "Set Sprite Mask"); }
				const std::map<uint32, SceneObject*>& list = sceneObjects->GetList();
				for (std::map<uint32, SceneObject*>::const_iterator i = list.begin(); i != list.end(); ++i)
				{
					if (!i->second || i->second->GetType() != SceneObjectTypes::GAMEOBJECT) continue;
					if (!FindComponent<SpriteMask2D>((GameObject*)i->second->GetPTR(), ComponentType::SpriteMask2D)) continue;
					const std::string n = i->second->GetName();
					if (ImGui::Selectable(n.c_str(), n == cur))
					{ sp->SetMaskName(n); MarkSceneDirty(); PushReplaceCommand(goId, b, "Set Sprite Mask"); }
				}
				ImGui::EndCombo();
			}
			if (!SpriteMask2D::Find(go, sp->GetMaskName()))
				ImGui::TextColored(ImVec4(1.f, 0.6f, 0.4f, 1.f), cur.empty() ? "No enabled Sprite Mask on this object or above it."
					: "No enabled Sprite Mask on an object of that name.");
		}
	}

	int blend = sp->GetBlend();
	const char* blends[] = { "Alpha", "Additive", "Premultiplied" };
	{
		const std::string b = SnapshotSubtree(goId);
		if (ImGui::Combo("Blend", &blend, blends, 3))
		{ sp->SetBlend(blend); MarkSceneDirty(); PushReplaceCommand(goId, b, "Set Sprite Blend"); }
	}

	int order = sp->GetSortOrder();
	BeginComponentUndo(goId);
	if (ImGui::DragInt("Sort Order", &order, 0.1f, -32768, 32767)) { sp->SetSortOrder(order); MarkSceneDirty(); }
	EndComponentUndo(goId, "Set Sort Order");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Drawn over everything with a lower order, whatever its z.\nAmong equal orders, z decides.");

	// --- clips ----------------------------------------------------------------
	if (!sheet.clips.empty())
	{
		const std::string ap = sp->GetAutoPlay();
		if (ImGui::BeginCombo("Plays", ap.empty() ? "(nothing)" : ap.c_str()))
		{
			const std::string b = SnapshotSubtree(goId);
			if (ImGui::Selectable("(nothing)", ap.empty()))
			{ sp->SetAutoPlay(std::string()); sp->Stop(); MarkSceneDirty(); PushReplaceCommand(goId, b, "Set Sprite Clip"); }
			for (size_t ci = 0; ci < sheet.clips.size(); ci++)
				if (ImGui::Selectable(sheet.clips[ci].name.c_str(), sheet.clips[ci].name == ap))
				{
					sp->SetAutoPlay(sheet.clips[ci].name);
					MarkSceneDirty();
					PushReplaceCommand(goId, b, "Set Sprite Clip");
				}
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("The clip this sprite starts on when the game runs.\nA script switches clips with sprite:play(\"name\").");

		f32 speed = sp->GetSpeed();
		BeginComponentUndo(goId);
		if (ImGui::DragFloat("Speed", &speed, 0.01f, 0.f, 20.f)) { sp->SetSpeed(speed); MarkSceneDirty(); }
		EndComponentUndo(goId, "Set Sprite Speed");

		// Preview while the scene is not running: its clocks are stopped, so
		// the panel ticks the one sprite it is showing.
		if (!playMode)
		{
			const bool previewing = (g_previewSprite == sp);
			if (ImGui::Button(previewing ? "Stop Preview" : "Preview Clip"))
			{
				if (previewing) { g_previewSprite = NULL; sp->Stop(); }
				else
				{
					const std::string clip = ap.empty() ? sheet.clips[0].name : ap;
					sp->Play(clip, true);
					g_previewSprite = sp;
					g_previewLast = ImGui::GetTime();
				}
			}
			if (g_previewSprite == sp)
			{
				const double now = ImGui::GetTime();
				sp->Advance((f32)std::min(0.1, now - g_previewLast));
				g_previewLast = now;
				// A one-shot runs again rather than freezing on its last frame.
				if (sp->IsFinished()) sp->Play(sp->GetClip(), true);
				ImGui::SameLine();
				ImGui::TextDisabled("%s  %.2fs", sp->GetClip().c_str(), sp->GetClipTime());
			}
		}
	}
	else if (isSheet)
		ImGui::TextDisabled("This sheet has no clips yet.");

	if (SpriteBatch2D* b = sp->GetBatch())
		ImGui::TextDisabled("Drawn by a Sprite Batch (%u sprites, %u draw call%s).",
			b->SpriteCount(), b->DrawCalls(), b->DrawCalls() == 1 ? "" : "s");
}

void SceneEditor::DrawSpriteBatch2DProperties(uint32 goId, SpriteBatch2D* b)
{
	ImGui::Text("Sprite Batch");
	bool en = b->IsEnabled();
	{
		const std::string before = SnapshotSubtree(goId);
		if (ImGui::Checkbox("Enabled", &en))
		{ b->SetEnabled(en); MarkSceneDirty(); PushReplaceCommand(goId, before, "Toggle Sprite Batch"); }
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Off, every sprite beneath draws itself again - for telling\nwhether the batch is the cause of something.");
	ImGui::TextDisabled("%u sprites in %u draw call%s", b->SpriteCount(), b->DrawCalls(),
		b->DrawCalls() == 1 ? "" : "s");
	bool lit = b->BatchesLit();
	{
		const std::string before = SnapshotSubtree(goId);
		if (ImGui::Checkbox("Batch Lit Sprites", &lit))
		{ b->SetBatchLit(lit); MarkSceneDirty(); PushReplaceCommand(goId, before, "Batch Lit Sprites"); }
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Sprites that take lighting are merged too, cut into cells: each\ncell is lit by the lights near it. Off, they draw themselves.");
	if (lit)
	{
		float cell = b->GetLightCellSize();
		ImGui::SetNextItemWidth(120);
		BeginComponentUndo(goId);
		if (ImGui::DragFloat("Light Cell", &cell, 0.25f, 0.25f, 1000.f, "%.2f")) { b->SetLightCellSize(cell); MarkSceneDirty(); }
		EndComponentUndo(goId, "Set Light Cell");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("How big a cell is, in units. Smaller: lighting closer to what each\nsprite would get alone, more draw calls. A cell is lit by at most\nfour lights, so keep it no bigger than the gap between lights.");
	}
	ImGui::TextDisabled("Merges the sprites beneath this object, one draw per run\nthat shares a texture (and, for lit ones, a cell).");
}

bool SceneEditor::SetSpriteSourceById(uint32 goId, const std::string& path, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "not a game object"; return false; }
	Sprite2D* sp = FindSprite((GameObject*)obj->GetPTR());
	if (!sp) { errOut = "that object has no Sprite2D"; return false; }
	const std::string before = SnapshotSubtree(goId);
	if (!ApplySpriteSource(sp, path, errOut)) return false;
	spritePropsFor = NULL;
	MarkSceneDirty();
	PushReplaceCommand(goId, before, "Set Sprite Source");
	return true;
}

bool SceneEditor::AgentSetSpriteBatch(const std::string& name, const json& p, std::string& errOut)
{
	if (playMode) { errOut = "editor is in play mode"; return false; }
	SceneObject* obj = FindObjectByName(sceneObjects, name);
	if (!obj) { errOut = "object '" + name + "' not found"; return false; }
	SpriteBatch2D* b = FindBatch((GameObject*)obj->GetPTR());
	if (!b) { errOut = "'" + name + "' has no SpriteBatch2D"; return false; }
	const std::string before = SnapshotSubtree(obj->GetID());
	if (p.contains("enabled") && p["enabled"].is_boolean()) b->SetEnabled(p["enabled"].get<bool>());
	if (p.contains("lit") && p["lit"].is_boolean()) b->SetBatchLit(p["lit"].get<bool>());
	if (p.contains("lightCell") && p["lightCell"].is_number()) b->SetLightCellSize(p["lightCell"].get<f32>());
	MarkSceneDirty();
	PushReplaceCommand(obj->GetID(), before, "Set Sprite Batch");
	return true;
}

void SceneEditor::StopSpritePreview()
{
	g_previewSprite = NULL;
}

// --- agent ---------------------------------------------------------------------

bool SceneEditor::AgentConvertSprite(const std::string& name, std::string& errOut)
{
	if (playMode) { errOut = "editor is in play mode"; return false; }
	SceneObject* obj = FindObjectByName(sceneObjects, name);
	if (!obj) { errOut = "object '" + name + "' not found"; return false; }
	return OpConvertLegacySprite(obj->GetID(), errOut);
}

bool SceneEditor::AgentAddSpriteBatch(const std::string& name, std::string& errOut)
{
	if (playMode) { errOut = "editor is in play mode"; return false; }
	SceneObject* obj = FindObjectByName(sceneObjects, name);
	if (!obj) { errOut = "object '" + name + "' not found"; return false; }
	return OpAddSpriteBatch2D(obj->GetID(), errOut);
}

// The keys set_sprite and add_sprite share, applied to a sprite. `allowed`
// names what else the caller's own command accepts, so an unknown key is
// still refused by name.
bool SceneEditor::ApplySpriteArgs(Sprite2D* sp, const json& p, const char* const* allowed, std::string& errOut)
{
	if (!p.is_object()) { errOut = "args must be an object"; return false; }

	// Refused by name: an unknown key is a typo, and a typo that answers
	// {"ok":true} is found ten screenshots later.
	static const char* known[] = { "source", "frame", "flipX", "flipY", "color", "alpha",
		"drawMode", "size", "pixelsPerUnit", "pivot", "lit", "lighting", "material", "blend", "sortOrder", "autoPlay", "speed",
		"maskInteraction", "mask",
		"play", "restart", "stop", "clipTime", NULL };
	for (json::const_iterator it = p.begin(); it != p.end(); ++it)
	{
		bool ok = false;
		for (int k = 0; known[k] && !ok; k++) ok = (it.key() == known[k]);
		for (int k = 0; allowed && allowed[k] && !ok; k++) ok = (it.key() == allowed[k]);
		if (!ok) { errOut = "unknown key '" + it.key() + "'"; return false; }
	}

	try {
		if (p.contains("source") && !ApplySpriteSource(sp, p["source"].get<std::string>(), errOut)) return false;
		if (p.contains("frame"))
		{
			if (p["frame"].is_string())
			{
				if (!sp->SetFrameByName(p["frame"].get<std::string>()))
				{ errOut = "the sheet has no frame named '" + p["frame"].get<std::string>() + "'"; return false; }
			}
			else
			{
				const int32 f = p["frame"].get<int32>();
				if (f < 0 || f >= sp->FrameCount())
				{ errOut = "frame " + std::to_string(f) + " is out of range (the sheet has " + std::to_string(sp->FrameCount()) + ")"; return false; }
				sp->SetFrame(f);
			}
		}
		if (p.contains("flipX")) sp->SetFlipX(p["flipX"].get<bool>());
		if (p.contains("flipY")) sp->SetFlipY(p["flipY"].get<bool>());
		if (p.contains("color"))
		{
			const json &c = p["color"];
			if (!c.is_array() || c.size() < 3) { errOut = "color must be [r, g, b] or [r, g, b, a]"; return false; }
			sp->SetColor(Vec4(c[0].get<f32>(), c[1].get<f32>(), c[2].get<f32>(), c.size() > 3 ? c[3].get<f32>() : sp->GetColor().w));
		}
		if (p.contains("alpha")) sp->SetAlpha(p["alpha"].get<f32>());
		if (p.contains("drawMode"))
		{
			const std::string m = Lower(p["drawMode"].get<std::string>());
			if (m == "simple") sp->SetDrawMode(SpriteDraw2D::Simple);
			else if (m == "sliced") sp->SetDrawMode(SpriteDraw2D::Sliced);
			else if (m == "tiled") sp->SetDrawMode(SpriteDraw2D::Tiled);
			else { errOut = "drawMode must be simple, sliced or tiled"; return false; }
		}
		if (p.contains("size"))
		{
			const json &sz = p["size"];
			if (!sz.is_array() || sz.size() < 2) { errOut = "size must be [width, height]"; return false; }
			sp->SetSize(Vec2(sz[0].get<f32>(), sz[1].get<f32>()));
		}
		if (p.contains("pixelsPerUnit")) sp->SetPixelsPerUnit(p["pixelsPerUnit"].get<f32>());
		if (p.contains("pivot"))
		{
			const json &v = p["pivot"];
			if (v.is_null()) sp->ClearPivot();
			else if (v.is_array() && v.size() >= 2) sp->SetPivot(Vec2(v[0].get<f32>(), v[1].get<f32>()));
			else { errOut = "pivot must be [x, y], or null for the sheet's own"; return false; }
		}
		if (p.contains("material"))
		{
			if (!p["material"].is_string()) { errOut = "material is a .mat path, or \"\" for the sprite's own"; return false; }
			if (!ApplySpriteMaterial(sp, p["material"].get<std::string>(), errOut)) return false;
		}
		if (p.contains("mask"))
		{
			if (!p["mask"].is_string()) { errOut = "mask is the mask object's name, or \"\" for the nearest one above the sprite"; return false; }
			sp->SetMaskName(p["mask"].get<std::string>());
		}
		if (p.contains("maskInteraction"))
		{
			const std::string mi = Lower(p["maskInteraction"].get<std::string>());
			if (mi == "none") sp->SetMaskInteraction(Sprite2D::MaskNone);
			else if (mi == "inside") sp->SetMaskInteraction(Sprite2D::MaskInside);
			else if (mi == "outside") sp->SetMaskInteraction(Sprite2D::MaskOutside);
			else { errOut = "maskInteraction must be none, inside or outside"; return false; }
		}
		if (p.contains("lit")) sp->SetLit(p["lit"].get<bool>());
		if (p.contains("lighting"))
		{
			const std::string l = Lower(p["lighting"].get<std::string>());
			if (l == "unlit") sp->SetLighting(SpriteLighting2D::Unlit);
			else if (l == "2d") sp->SetLighting(SpriteLighting2D::Lit2D);
			else if (l == "3d") sp->SetLighting(SpriteLighting2D::Lit3D);
			else { errOut = "lighting must be unlit, 2d or 3d"; return false; }
		}
		if (p.contains("blend"))
		{
			const std::string b = Lower(p["blend"].get<std::string>());
			if (b == "alpha") sp->SetBlend(SpriteBlend2D::Alpha);
			else if (b == "additive") sp->SetBlend(SpriteBlend2D::Additive);
			else if (b == "premultiplied") sp->SetBlend(SpriteBlend2D::Premultiplied);
			else { errOut = "blend must be alpha, additive or premultiplied"; return false; }
		}
		if (p.contains("sortOrder")) sp->SetSortOrder(p["sortOrder"].get<int32>());
		if (p.contains("speed")) sp->SetSpeed(p["speed"].get<f32>());
		if (p.contains("autoPlay"))
		{
			const std::string c = p["autoPlay"].get<std::string>();
			if (!c.empty() && !sp->HasClip(c)) { errOut = "the sheet has no clip named '" + c + "'"; return false; }
			sp->SetAutoPlay(c);
			if (c.empty()) sp->Stop();
		}
		if (p.contains("play"))
		{
			const std::string c = p["play"].get<std::string>();
			if (!sp->Play(c, p.value("restart", false))) { errOut = "the sheet has no clip named '" + c + "'"; return false; }
		}
		if (p.value("stop", false)) sp->Stop();
		if (p.contains("clipTime")) sp->SetClipTime(p["clipTime"].get<f32>());
	}
	catch (const std::exception &e) {
		errOut = std::string("a value has the wrong type (") + e.what() + ")";
		return false;
	}
	return true;
}

bool SceneEditor::AgentSetSprite(const std::string& name, const json& p, std::string& errOut)
{
	SceneObject* obj = FindObjectByName(sceneObjects, name);
	if (!obj) { errOut = "object '" + name + "' not found"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();
	const uint32 goId = obj->GetID();
	Sprite2D* sp = FindSprite(go);
	if (!sp) { errOut = "'" + name + "' has no Sprite2D - add_sprite first (or convert_sprites for an old one)"; return false; }

	// In play mode these are run-time pokes at a live sprite - useful for
	// testing a clip - and nothing is recorded.
	const std::string before = playMode ? std::string() : SnapshotSubtree(goId);
	static const char* own[] = { "object", "name", NULL };
	if (!ApplySpriteArgs(sp, p, own, errOut)) { errOut = "set_sprite: " + errOut; return false; }
	if (!playMode)
	{
		MarkSceneDirty();
		PushReplaceCommand(goId, before, "Set Sprite");
	}
	return true;
}

bool SceneEditor::AgentAddSprite(const std::string& name, const std::string& texturePath,
	const json& p, const std::string& parentName, std::string& errOut)
{
	if (playMode) { errOut = "editor is in play mode"; return false; }
	SceneObject* parent = NULL;
	if (!parentName.empty())
	{
		parent = FindObjectByName(sceneObjects, parentName);
		if (!parent) { errOut = "parent '" + parentName + "' not found"; return false; }
	}
	SceneObject* obj = sceneObjects->CreateGameObject(name.empty() ? "Sprite" : name);
	if (!obj) { errOut = "failed to create game object"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();

	// "source" is the name the component uses; "texture" is what this
	// command was first given, and still works.
	std::string source = texturePath;
	if (p.is_object() && p.contains("source") && p["source"].is_string()) source = p["source"].get<std::string>();

	std::shared_ptr<Sprite2D> sp = std::make_shared<Sprite2D>();
	bool ok = source.empty() || ApplySpriteSource(sp.get(), source, errOut);
	if (ok && p.is_object())
	{
		json rest = p;
		rest.erase("source");
		static const char* own[] = { "name", "texture", "parent", "position", "rotation", "scale", NULL };
		ok = ApplySpriteArgs(sp.get(), rest, own, errOut);
		if (!ok) errOut = "add_sprite: " + errOut;
	}
	if (!ok)
	{
		// Not left behind: a failed add that still made an empty object is
		// one the caller has to notice and delete.
		sceneObjects->DestroySceneObject(obj->GetID());
		return false;
	}
	go->Add(std::static_pointer_cast<IComponent>(sp));

	if (p.is_object())
	{
		auto vec3 = [&p](const char* key, Vec3 &out) {
			if (!p.contains(key) || !p[key].is_array() || p[key].size() != 3) return false;
			out = Vec3(p[key][0].get<f32>(), p[key][1].get<f32>(), p[key][2].get<f32>());
			return true;
		};
		Vec3 v;
		if (vec3("position", v)) go->SetPosition(v);
		if (vec3("rotation", v)) go->SetRotation(v);
		if (vec3("scale", v)) go->SetScale(v);
	}
	if (parent) sceneObjects->ReparentGameObject(obj->GetID(), parent->GetID());
	MarkSceneDirty();
	PushAddCommand(obj);
	return true;
}

json SceneEditor::AgentSpriteInfo(const std::string& name, std::string& errOut)
{
	json r;
	SceneObject* obj = FindObjectByName(sceneObjects, name);
	if (!obj) { errOut = "object '" + name + "' not found"; return r; }
	GameObject* go = (GameObject*)obj->GetPTR();
	if (SpriteBatch2D* b = FindBatch(go))
	{
		r["batch"] = { { "enabled", b->IsEnabled() }, { "lit", b->BatchesLit() }, { "lightCell", b->GetLightCellSize() }, { "sprites", b->SpriteCount() },
			{ "drawCalls", b->DrawCalls() }, { "rebuilds", b->Rebuilds() } };
	}
	Sprite2D* sp = FindSprite(go);
	if (!sp)
	{
		if (r.contains("batch")) return r;
		errOut = "'" + name + "' has no Sprite2D";
		return r;
	}
	const SpriteSheet2D &sheet = sp->GetSheet();
	const Vec4 rect = sp->GetLocalRect();
	r["source"] = sp->GetSourcePath();
	r["image"] = sheet.image.empty() ? sp->GetSourcePath() : sheet.image;
	r["imageSize"] = { sheet.GetImageWidth(), sheet.GetImageHeight() };
	r["frame"] = sp->GetFrame();
	r["frameName"] = sp->GetFrameName();
	r["frameCount"] = sp->FrameCount();
	r["flipX"] = sp->GetFlipX();
	r["flipY"] = sp->GetFlipY();
	r["color"] = { sp->GetColor().x, sp->GetColor().y, sp->GetColor().z, sp->GetColor().w };
	r["drawMode"] = sp->GetDrawMode() == SpriteDraw2D::Sliced ? "sliced"
		: (sp->GetDrawMode() == SpriteDraw2D::Tiled ? "tiled" : "simple");
	r["size"] = { sp->GetSize().x, sp->GetSize().y };
	r["pixelsPerUnit"] = sp->EffectivePixelsPerUnit();
	r["pivot"] = { sp->GetPivot().x, sp->GetPivot().y };
	r["ownPivot"] = sp->HasPivot();
	r["lit"] = sp->IsLit();
	if (!sp->GetMaterialPath().empty()) r["material"] = sp->GetMaterialPath();
	r["maskInteraction"] = sp->GetMaskInteraction() == Sprite2D::MaskInside ? "inside"
		: (sp->GetMaskInteraction() == Sprite2D::MaskOutside ? "outside" : "none");
	if (sp->GetMaskInteraction() != Sprite2D::MaskNone)
	{
		r["mask"] = sp->GetMaskName();
		// The mask it actually found, by its object's name - "" when there
		// is none, in which case the sprite is drawn whole.
		SpriteMask2D* found = SpriteMask2D::Find(go, sp->GetMaskName());
		r["maskFound"] = (found && found->GetOwner()) ? found->GetOwner()->GetName() : std::string();
	}
	r["lighting"] = sp->GetLighting() == SpriteLighting2D::Lit2D ? "2d"
		: (sp->GetLighting() == SpriteLighting2D::Lit3D ? "3d" : "unlit");
	r["blend"] = sp->GetBlend() == SpriteBlend2D::Additive ? "additive"
		: (sp->GetBlend() == SpriteBlend2D::Premultiplied ? "premultiplied" : "alpha");
	r["sortOrder"] = sp->GetSortOrder();
	r["localRect"] = { rect.x, rect.y, rect.z, rect.w };
	r["clips"] = sp->ClipNames();
	r["autoPlay"] = sp->GetAutoPlay();
	r["clip"] = sp->GetClip();
	r["clipTime"] = sp->GetClipTime();
	r["playing"] = sp->IsPlaying();
	r["speed"] = sp->GetSpeed();
	r["batched"] = sp->GetBatch() != NULL;
	return r;
}
