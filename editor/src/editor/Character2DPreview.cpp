//=============================================================================
// Name        : Character2DPreview.cpp
// Description : See the header.
//=============================================================================

#include "Character2DPreview.h"
#include "Character2DDocument.h"

#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <Pyros3D/AnimationManager/IKSolver.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Assets/Renderable/Renderables.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>

#include <algorithm>
#include <cmath>

using namespace p3d;
using namespace p3d::Math;

Character2DPreview::~Character2DPreview()
{
	// Order matters: the character owns GPU resources registered with the
	// renderer, so it goes before the renderer that knows about them.
	built.animation.reset();
	characterGO.reset();
	cameraGO.reset();
	lightGO.reset();
	if (effects) { delete effects; effects = NULL; }
	if (renderer) { delete renderer; renderer = NULL; }
	if (scene) { delete scene; scene = NULL; }
}

void Character2DPreview::EnsureInit()
{
	if (renderer) return;

	renderer = new ForwardRenderer((uint32)width, (uint32)height);
	// Nothing here casts a shadow that matters, and building shadow maps for
	// a second viewport doubles the cost of every frame the window is open.
	renderer->SetSkipShadowMaps(true);
	effects = new PostEffectsManager((uint32)width, (uint32)height);
	effects->GetExternalFrameBuffer()->SetDebugName("2D character preview");
	scene = new SceneGraph();

	cameraGO = std::make_shared<GameObject>();
	scene->Add(cameraGO);

	characterGO = std::make_shared<GameObject>();
	scene->Add(characterGO);

	renderer->SetBackground(Vec4(0.13f, 0.14f, 0.17f, 1.f));
	// Full ambient and NO light. A character is authored to be looked at, not
	// lit: the artwork must read at its own brightness so what you pick in the
	// sprite panel is what you see. Adding a light on top of full ambient -
	// which this did, to preview the "lit by 2D lights" flag - pushed every
	// part past 1.0 and turned a dark red torso bright pink, so the preview
	// disagreed with every scene the character is placed in.
	//
	// The consequence is that the `lit` flag has no visible effect here. That
	// is the honest trade: how a part is lit depends on the scene's lights,
	// which a character asset cannot know.
	renderer->SetGlobalLight(Vec4(1.f, 1.f, 1.f, 1.f));

	scene->Update(0);
}

void Character2DPreview::Sync(Character2DDocument& doc)
{
	EnsureInit();
	if (!characterGO) return;

	if (builtRigRevision != doc.rigRevision)
	{
		builtRigRevision = doc.rigRevision;
		builtClipsRevision = doc.clipsRevision;
		builtPartsRevision = doc.partsRevision;

		// The old component goes before the new one is added: a GameObject
		// can hold several RenderingComponents, and leaving the previous one
		// on would draw the pre-edit character underneath the new one.
		if (characterRC)
		{
			characterGO->Remove(characterRC);
			characterRC = NULL;
		}
		built.animation.reset();
		built.instance = NULL;

		// A component needs SOME renderable to be constructed with;
		// ApplyCharacter2D replaces the geometry wholesale.
		std::shared_ptr<RenderingComponent> rc = std::make_shared<RenderingComponent>(
			std::make_shared<Plane>(1.f, 1.f), std::shared_ptr<IMaterial>());
		characterGO->Add(rc);
		characterRC = rc.get();

		// Textures are stored project-relative; the document knows what they
		// are relative to.
		const std::string root = doc.projectRoot;
		ApplyCharacter2D(characterRC, doc.asset,
			[root](const std::string& rel) -> std::string {
				if (rel.empty()) return rel;
				// An absolute path is already loadable - a texture dragged in
				// from outside the project has not been copied in yet.
				if (!rel.empty() && (rel[0] == '/' || (rel.size() > 1 && rel[1] == ':'))) return rel;
				return root.empty() ? rel : (root + "/" + rel);
			}, built);

		if (built.instance) built.instance->ResetToBindPose();

		// The dope sheet keys THIS instance's bones (see
		// AnimationEditorDocument::externalRig).
		doc.anim.externalRig = built.instance;
		doc.anim.externalPoseOverrides.clear();
		posedBones.clear();

		// Deliberately NOT framed here. Framing needs the viewport's real
		// aspect, and at this point width/height are still whatever the last
		// frame (or the constructor) left - so the fit was computed for the
		// wrong shape and the character came out cropped along one axis.
		// DrawViewport frames it once it has set the size.
		framed = false;
		userAdjustedView = false;
		scene->Update(0);
	}
	else
	{
		if (builtClipsRevision != doc.clipsRevision)
		{
			builtClipsRevision = doc.clipsRevision;
			SetCharacter2DClips(built, doc.asset.clips);
		}
		if (builtPartsRevision != doc.partsRevision && characterRC)
		{
			// Where the parts are drawn, not what they are: hand the component
			// the new numbers and let the next RefreshSpriteParts2D place them.
			// Deliberately NOT a rebuild - that re-loads every texture.
			builtPartsRevision = doc.partsRevision;
			std::vector<SpritePart2D> parts = characterRC->GetSpriteParts2D();
			for (size_t i = 0; i < parts.size() && i < doc.asset.parts.size(); i++)
			{
				parts[i].offset = doc.asset.parts[i].offset;
				parts[i].scale = doc.asset.parts[i].scale;
				parts[i].pivot = doc.asset.parts[i].pivot;
				parts[i].z = doc.asset.parts[i].z;
				parts[i].bone = doc.asset.parts[i].bone;
			}
			characterRC->SetSpriteParts2D(parts);
			characterRC->RefreshSpriteParts2D();
		}
	}
}

void Character2DPreview::FrameCamera(Character2DDocument& doc)
{
	if (!built.instance) { framed = true; return; }

	// The ARTWORK first, the bones only as a fallback. Framing a character on
	// its skeleton shows a close-up of its chest: the sprites routinely reach
	// well past the joints, and how far is a property of the artwork, not
	// something a margin multiplier can guess. A rig with no sprites yet still
	// has to be visible, hence the fallback.
	Vec2 mn(0.f, 0.f), mx(0.f, 0.f);
	bool any = false;
	if (characterRC) any = characterRC->GetSpriteParts2DBounds(mn, mx);

	if (!any)
	{
		const std::vector<Bone>& bones = built.instance->GetSkeletonBones();
		for (size_t b = 0; b < bones.size(); b++)
		{
			const int32 id = bones[b].self;
			if (id < 0 || (size_t)id >= bones.size()) continue;
			const Vec3 p = built.instance->GetBoneGlobalTransform(id).GetTranslation();
			if (!any) { mn = mx = Vec2(p.x, p.y); any = true; continue; }
			mn.x = std::min(mn.x, p.x); mx.x = std::max(mx.x, p.x);
			mn.y = std::min(mn.y, p.y); mx.y = std::max(mx.y, p.y);
		}
	}
	if (!any) { mn = mx = Vec2(0.f, 0.f); }

	centerX = (mn.x + mx.x) * 0.5f;
	centerY = (mn.y + mx.y) * 0.5f;

	const f32 aspect = (height > 0) ? ((f32)width / (f32)height) : 1.f;
	f32 halfW = (mx.x - mn.x) * 0.5f;
	const f32 halfH = (mx.y - mn.y) * 0.5f;
	if (halfH * aspect > halfW) halfW = halfH * aspect;
	if (halfW < 0.25f) halfW = 0.25f;
	// A tenth of a margin, so the character is not flush against the edge and
	// there is somewhere to drag a limb to. Small, because the bounds above
	// are the real ones - the old 2.4x was compensating for measuring the
	// wrong thing.
	halfWidth = halfW * 1.1f;
	framed = true;
	framedForWidth = width;
	framedForHeight = height;
	userAdjustedView = false;
}

f32 Character2DPreview::WorldPerPixel() const
{
	if (width < 1) return 0.01f;
	return (halfWidth * 2.f) / (f32)width;
}

bool Character2DPreview::CursorToWorld(const Vec2& px, Vec3& outWorld) const
{
	if (width < 1 || height < 1) return false;
	const f32 halfH = halfWidth * (f32)height / (f32)width;
	// Pixel centre to normalised, then to world. y is flipped: pixel rows run
	// down, world y runs up.
	const f32 u = (px.x / (f32)width) * 2.f - 1.f;
	const f32 v = 1.f - (px.y / (f32)height) * 2.f;
	outWorld = Vec3(centerX + u * halfWidth, centerY + v * halfH, 0.f);
	return true;
}

bool Character2DPreview::BoneToPixels(int boneId, Vec2& outPx) const
{
	if (!built.instance || boneId < 0 || width < 1 || height < 1) return false;
	if ((uint32)boneId >= built.instance->GetNumberBones()) return false;
	const Vec3 p = built.instance->GetBoneGlobalTransform(boneId).GetTranslation();
	const f32 halfH = halfWidth * (f32)height / (f32)width;
	if (halfWidth <= 0.f || halfH <= 0.f) return false;
	const f32 u = (p.x - centerX) / halfWidth;
	const f32 v = (p.y - centerY) / halfH;
	outPx = Vec2((u * 0.5f + 0.5f) * (f32)width, (0.5f - v * 0.5f) * (f32)height);
	return true;
}

int Character2DPreview::PickJoint(const Vec2& px) const
{
	if (!built.instance) return -1;
	const f32 wpp = WorldPerPixel();
	if (wpp <= 0.f) return -1;

	Vec3 cursor;
	if (!CursorToWorld(px, cursor)) return -1;

	// Screen-space radius converted to world: a joint is a point with no
	// geometry, so it is picked by proximity like any other handle.
	const f32 pickWorld = 14.f * wpp;
	f32 bestSQR = pickWorld * pickWorld;
	int best = -1;

	const std::vector<Bone>& bones = built.instance->GetSkeletonBones();
	for (size_t b = 0; b < bones.size(); b++)
	{
		const int32 id = bones[b].self;
		if (id < 0 || (size_t)id >= bones.size()) continue;
		const Vec3 p = built.instance->GetBoneGlobalTransform(id).GetTranslation();
		const f32 dx = p.x - cursor.x, dy = p.y - cursor.y;
		const f32 d = dx * dx + dy * dy;
		if (d < bestSQR) { bestSQR = d; best = (int)id; }
	}
	return best;
}

void Character2DPreview::DragJoint(int boneId, const Vec3& targetWorld)
{
	if (!built.instance || boneId < 0) return;

	// The character sits at the origin of its own scene, so model space and
	// world space are the same here - unlike in a level, where the drag had to
	// come back out through the owner's transform. One of several things this
	// viewport does not have to get right because the character is alone in it.
	const Vec3 target(targetWorld.x, targetWorld.y, 0.f);

	// Two bones up where the chain allows it, which is the classic elbow/knee
	// case and what the closed-form two-bone solve is for; otherwise as far up
	// as it goes.
	//
	// But never up THROUGH a bone with more than one child. A spine parents
	// both arms, both legs and the neck, so walking blindly two up from a hand
	// roots the chain at the spine - and dragging the hand then rotates the
	// spine, taking the head and the other arm with it. A branch point is
	// where one limb ends and the body begins, which is exactly where an
	// inferred chain should stop.
	const std::vector<Bone>& bones = built.instance->GetSkeletonBones();
	int32 rootId = (int32)boneId;
	for (int step = 0; step < 2; step++)
	{
		if (rootId < 0 || (size_t)rootId >= bones.size()) break;
		const int32 p = bones[rootId].parent;
		if (p < 0) break;
		int children = 0;
		for (size_t c = 0; c < bones.size(); c++)
			if (bones[c].parent == p) children++;
		if (children > 1) break;
		rootId = p;
	}

	std::vector<int32> moved;
	if (rootId != (int32)boneId)
	{
		IKSolver::Solve(built.instance, rootId, (int32)boneId, target, Vec3(0.f, 0.f, 0.f));
		moved = IKSolver::BuildChain(built.instance, rootId, (int32)boneId);
	}
	else if ((size_t)boneId < bones.size())
	{
		// A root joint has no chain above it to solve. Dragging one means
		// moving the whole rig, so translate it and let the children follow -
		// rather than swallowing the drag and appearing to do nothing.
		Matrix local = built.instance->GetBoneLocalTransform((int32)boneId);
		const Vec3 keepRot = local.GetEulerFromRotationMatrix();
		local.identity();
		local.Translate(target);
		local.SetRotationFromEuler(keepRot);
		built.instance->SetBoneLocalTransform((int32)boneId, local);
		built.instance->RefreshSkinning();
		moved.push_back((int32)boneId);
	}

	for (size_t i = 0; i < moved.size(); i++)
	{
		if (std::find(posedBones.begin(), posedBones.end(), (int)moved[i]) == posedBones.end())
			posedBones.push_back((int)moved[i]);
	}

	// The sprites follow the bones through their meshes' Pivot, and nothing
	// else will refresh them before this frame is drawn.
	if (characterRC) characterRC->RefreshSpriteParts2D();
}

Texture* Character2DPreview::RenderFrame()
{
	if (!renderer || !effects || !scene || !cameraGO) return NULL;
	if (width < 16 || height < 16) return NULL;

	// Straight down -Z at the framed centre, like every other 2D view.
	Matrix camM;
	camM.Translate(Vec3(centerX, centerY, 50.f));
	cameraGO->SetTransformationMatrix(camM);
	scene->Update(0);

	const f32 halfH = halfWidth * (f32)height / (f32)width;
	Projection proj;
	proj.Ortho(-halfWidth, halfWidth, -halfH, halfH, 0.1f, 1000.f);

	IRenderer::InvalidateSharedUniformCaches();
	renderer->Resize((uint32)width, (uint32)height);
	effects->Resize((uint32)width, (uint32)height);
	effects->ProcessPostEffects(&proj);
	renderer->ResetViewPort();
	renderer->SetViewPort(0, 0, (uint32)width, (uint32)height);
	renderer->PreRender(cameraGO.get(), scene);
	renderer->ApplyBackgroundClearColor();
	effects->CaptureFrame();
	if (showSprites)
		renderer->RenderScene(proj, cameraGO.get(), scene);
	effects->EndCapture();

#if defined(_SDL2VULKAN) || defined(_SDL2METAL)
	// This and the main viewport share one GlobalMatrices UBO; the same rule
	// AnimationPreview and the camera preview follow.
	GetActiveRenderDevice().WaitIdle();
#endif
	IRenderer::InvalidateSharedUniformCaches();
	return effects->GetViewportColor();
}
