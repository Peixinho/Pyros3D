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

#include <algorithm>
#include <cmath>

using namespace p3d;
using namespace p3d::Math;

Character2DPreview::~Character2DPreview()
{
	built.animation.reset();
	characterGO.reset();
	cameraGO.reset();
	lightGO.reset();
	if (scene) { delete scene; scene = NULL; }
	if (emptyScene) { delete emptyScene; emptyScene = NULL; }
}

void Character2DPreview::EnsureInit()
{
	if (scene) return;

	// Full ambient and NO light. A character is authored to be looked at, not lit: the artwork must read at its own
	// brightness - exposure 1 - so what you pick in the sprite panel is what you see. (How a part is lit depends on
	// the scene's lights, which a character asset cannot know: the `lit` flag has no visible effect here.)
	rendererView.Renderer()->SetBackground(Vec3(0.016f, 0.019f, 0.025f));
	rendererView.Renderer()->SetAmbient(Vec3(1.f, 1.f, 1.f));
	rendererView.Renderer()->SetAutoExposure(false, 0.f);
	rendererView.Renderer()->SetName("2D character preview");
	emptyScene = new SceneGraph();
	scene = new SceneGraph();

	cameraGO = std::make_shared<GameObject>();
	scene->Add(cameraGO);

	characterGO = std::make_shared<GameObject>();
	scene->Add(characterGO);

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
		// Pins are by bone id, and a rebuilt skeleton may number them anew.
		pins.clear();

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
	RefreshLimits(doc);
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

int Character2DPreview::InferChainRoot(int boneId) const
{
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
	if (!built.instance) return boneId;
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
	return rootId;
}

void Character2DPreview::NotePosed(const std::vector<int32>& moved)
{
	for (size_t i = 0; i < moved.size(); i++)
		if (std::find(posedBones.begin(), posedBones.end(), (int)moved[i]) == posedBones.end())
			posedBones.push_back((int)moved[i]);
}

void Character2DPreview::SolveTo(int boneId, const Vec3& target)
{
	const int rootId = InferChainRoot(boneId);
	if (rootId == boneId) return;
	IKSolver::Solve(built.instance, rootId, (int32)boneId, Vec3(target.x, target.y, 0.f), Vec3(0.f, 0.f, 0.f),
		30, limits.empty() ? NULL : &limits);
	NotePosed(IKSolver::BuildChain(built.instance, rootId, (int32)boneId));
}

void Character2DPreview::ApplyPins(int exceptBone)
{
	if (!built.instance || pins.empty()) return;
	// Pinned joints are held where they were pinned while everything else
	// moves: the foot stays on the ground as the hips are dragged down. Each
	// re-solves its own inferred chain, as dragging it would.
	for (std::map<int, Vec3>::const_iterator it = pins.begin(); it != pins.end(); ++it)
		if (it->first != exceptBone && (uint32)it->first < built.instance->GetNumberBones())
			SolveTo(it->first, it->second);
}

void Character2DPreview::DragJoint(int boneId, const Vec3& targetWorld)
{
	if (!built.instance || boneId < 0) return;

	// The character sits at the origin of its own scene, so model space and
	// world space are the same here - unlike in a level, where the drag had to
	// come back out through the owner's transform. One of several things this
	// viewport does not have to get right because the character is alone in it.
	const Vec3 target(targetWorld.x, targetWorld.y, 0.f);
	const std::vector<Bone>& bones = built.instance->GetSkeletonBones();

	if (InferChainRoot(boneId) != boneId)
		SolveTo(boneId, target);
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
		NotePosed(std::vector<int32>(1, (int32)boneId));
	}
	// A dragged pin moves its pin with it.
	if (pins.count(boneId)) pins[boneId] = target;
	ApplyPins(boneId);

	// The sprites follow the bones through their meshes' Pivot, and nothing
	// else will refresh them before this frame is drawn.
	if (characterRC) characterRC->RefreshSpriteParts2D();
}

int Character2DPreview::PickLimb(const Vec2& px) const
{
	if (!built.instance) return -1;
	const std::vector<Bone>& bones = built.instance->GetSkeletonBones();
	f32 best = 7.f;  // pixels
	int hit = -1;
	for (size_t b = 0; b < bones.size(); b++)
	{
		Vec2 a, c;
		if (!BoneToPixels(bones[b].self, a)) continue;
		std::vector<Vec2> tips;
		for (size_t k = 0; k < bones.size(); k++)
			if (bones[k].parent == bones[b].self && BoneToPixels(bones[k].self, c)) tips.push_back(c);
		if (tips.empty())
		{
			// A leaf has no child to draw a limb to: it gets a stub along its
			// own X axis, which is what a hand or a head turns.
			const Matrix g = built.instance->GetBoneGlobalTransform(bones[b].self);
			Vec2 dir(g.m[0], -g.m[1]);
			const f32 len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
			if (len < 1e-6f) continue;
			tips.push_back(Vec2(a.x + dir.x / len * kLeafStubPixels, a.y + dir.y / len * kLeafStubPixels));
		}
		for (size_t t = 0; t < tips.size(); t++)
		{
			const Vec2 ab(tips[t].x - a.x, tips[t].y - a.y);
			const f32 l2 = ab.x * ab.x + ab.y * ab.y;
			if (l2 < 1.f) continue;
			f32 u = ((px.x - a.x) * ab.x + (px.y - a.y) * ab.y) / l2;
			if (u < 0.15f || u > 1.f) continue;   // near the joint is the joint's
			const f32 dx = a.x + ab.x * u - px.x, dy = a.y + ab.y * u - px.y;
			const f32 d = std::sqrt(dx * dx + dy * dy);
			if (d < best) { best = d; hit = (int)bones[b].self; }
		}
	}
	return hit;
}

void Character2DPreview::BeginRotate(int boneId, const Vec3& cursorWorld)
{
	if (!built.instance || boneId < 0) return;
	rotatingBone = boneId;
	const Vec3 j = built.instance->GetBoneGlobalTransform(boneId).GetTranslation();
	rotateGrabAngle = std::atan2(cursorWorld.y - j.y, cursorWorld.x - j.x);
	rotateStartLocal = built.instance->GetBoneLocalTransform(boneId);
}

void Character2DPreview::RotateTo(const Vec3& cursorWorld)
{
	if (!built.instance || rotatingBone < 0) return;
	const Vec3 j = built.instance->GetBoneGlobalTransform(rotatingBone).GetTranslation();
	const f32 now = std::atan2(cursorWorld.y - j.y, cursorWorld.x - j.x);
	// FK: the bone turns about its own joint by the angle the cursor swept
	// round it, children carried along - what IK cannot do for a head or a
	// hand, which have nothing below them to drag.
	Matrix spin;
	spin.RotationZ(now - rotateGrabAngle);
	Matrix local = rotateStartLocal;
	const Vec3 keep = local.GetTranslation();
	local.Translate(Vec3(0.f, 0.f, 0.f));
	local = local * spin;
	local.Translate(keep);
	built.instance->SetBoneLocalTransform(rotatingBone, local);
	built.instance->RefreshHierarchy();
	if (!limits.empty())
	{
		// The same clamp the solver uses, so a limit means one thing.
		std::map<int32, JointLimit>::const_iterator it = limits.find(rotatingBone);
		if (it != limits.end() && it->second.Enabled)
		{
			Matrix bind = built.instance->GetBindPoseLocal(rotatingBone);
			bind.Translate(Vec3(0.f, 0.f, 0.f));
			Matrix cur = built.instance->GetBoneLocalTransform(rotatingBone);
			cur.Translate(Vec3(0.f, 0.f, 0.f));
			const Matrix rel = bind.Inverse() * cur;
			f32 a = std::atan2(rel.m[1], rel.m[0]);
			a = std::max(it->second.Min.z, std::min(it->second.Max.z, a));
			Matrix r; r.RotationZ(a);
			Matrix clamped = bind * r;
			clamped.Translate(keep);
			built.instance->SetBoneLocalTransform(rotatingBone, clamped);
		}
	}
	built.instance->RefreshSkinning();
	NotePosed(std::vector<int32>(1, (int32)rotatingBone));
	ApplyPins(rotatingBone);
	if (characterRC) characterRC->RefreshSpriteParts2D();
}

bool Character2DPreview::TogglePin(int boneId)
{
	if (!built.instance || boneId < 0 || (uint32)boneId >= built.instance->GetNumberBones()) return false;
	if (pins.count(boneId)) { pins.erase(boneId); return false; }
	pins[boneId] = built.instance->GetBoneGlobalTransform(boneId).GetTranslation();
	return true;
}

void Character2DPreview::RefreshLimits(const Character2DDocument& doc)
{
	limits.clear();
	if (!built.instance) return;
	for (std::map<std::string, Character2DAsset::BoneLimit>::const_iterator it = doc.asset.limits.begin(); it != doc.asset.limits.end(); ++it)
	{
		if (!it->second.enabled) continue;
		const int id = doc.asset.FindBone(it->first);
		if (id < 0) continue;
		// About Z only; the other two axes never turn in the plane.
		limits[id] = JointLimit(Vec3(0.f, 0.f, (f32)DEGTORAD(it->second.minDeg)),
			Vec3(0.f, 0.f, (f32)DEGTORAD(it->second.maxDeg)));
	}
}

void Character2DPreview::RenderFrame()
{
	// Straight down -Z at the framed centre, like every other 2D view: into its texture, viewImage the image this frame
	viewImage = nullptr;
	if (!scene || !cameraGO || width < 16 || height < 16) return;
	Matrix camM;
	camM.Translate(Vec3(centerX, centerY, 50.f));
	cameraGO->SetTransformationMatrix(camM);
	scene->Update(0);
	p3d::View v;
	v.cameraWorld = camM; v.orthographic = true; v.orthoHeight = 2.f * halfWidth * (f32)height / (f32)width;
	v.nearZ = 0.1f; v.farZ = 1000.f; v.width = (uint32)width; v.height = (uint32)height;
	viewImage = rendererView.Draw(showSprites ? scene : emptyScene, v);
}
