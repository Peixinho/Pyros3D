//=============================================================================
// Name        : AnimationPreview.cpp
// Description : See AnimationPreview.h.
//=============================================================================

#include "AnimationPreview.h"
#include "AnimationEditorDocument.h"
#include "libgizmo/IGizmo.h"
#include "libgizmo/GizmoTransformRender.h"

#include <Pyros3D/Assets/Renderable/Models/Model.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/Utils/Mouse3D/Mouse3D.h>

#include <imgui.h>
#include <algorithm>
#include <cmath>

using namespace p3d;

namespace {

// Bone axes are drawn this long relative to the bone's own length, so a
// finger joint doesn't get the same 30-unit tripod as the pelvis.
const float kAxisFraction = 0.35f;

Vec4 ColorForBone(bool selected, bool onSelectedChain)
{
	if (selected) return Vec4(1.f, 0.75f, 0.1f, 1.f);
	if (onSelectedChain) return Vec4(1.f, 0.5f, 0.15f, 1.f);
	return Vec4(0.45f, 0.75f, 1.f, 1.f);
}

} // namespace

AnimationPreview::~AnimationPreview()
{
	ClearMesh();
	if (gizmo) { delete gizmo; gizmo = nullptr; }
	cameraGO.reset();
	lightGO.reset();
	delete scene; scene = nullptr;
	delete effects; effects = nullptr;
	delete renderer; renderer = nullptr;
	delete debug; debug = nullptr;
}

void AnimationPreview::EnsureInit()
{
	if (renderer) return;

	renderer = new ForwardRenderer((uint32)width, (uint32)height);
	effects = new PostEffectsManager((uint32)width, (uint32)height);
	scene = new SceneGraph();
	debug = new DebugRenderer();

	cameraGO = std::make_shared<GameObject>();
	scene->Add(cameraGO);

	lightGO = std::make_shared<GameObject>();
	lightGO->Add(std::make_shared<DirectionalLight>(Vec4(1.f, 1.f, 1.f, 1.f), Vec3(-0.45f, -1.f, -0.35f)));
	scene->Add(lightGO);

	renderer->SetBackground(Vec4(0.13f, 0.14f, 0.17f, 1.f));
	renderer->SetGlobalLight(Vec4(0.35f, 0.35f, 0.4f, 1.f));

	scene->Update(0);
}

void AnimationPreview::ClearMesh()
{
	if (scene && modelGO) scene->Remove(modelGO);
	modelGO.reset();
	modelRC = nullptr;
	// Destroying the container destroys the instance it created (see
	// SkeletonAnimation's destructor), so the raw pointer must be dropped
	// first - not after.
	instance = nullptr;
	animOwner.reset();
	loadedMeshPath.clear();
	poseOverrides.clear();
	boneScreenPos.clear();
}

bool AnimationPreview::LoadMesh(const std::string& absPath)
{
	EnsureInit();
	loadError.clear();
	if (absPath.empty()) { ClearMesh(); return false; }
	if (absPath == loadedMeshPath && modelGO) return true;

	ClearMesh();

	std::shared_ptr<Renderable> mesh;
	try
	{
		mesh = std::make_shared<Model>(absPath, true);
	}
	catch (...)
	{
		loadError = "Could not load model: " + absPath;
		return false;
	}
	if (!mesh)
	{
		loadError = "Could not load model: " + absPath;
		return false;
	}

	// Diffuse + Skinning, matching what SceneSerializer builds for an
	// animated model. Skinning is masked back off per submesh for geometry
	// with no bone weights (see RenderingComponent's constructor), so a
	// rigid prop inside the model is unaffected - and passing it for a
	// model with no skeleton at all is harmless, which is what lets an
	// unskinned .p3dm still be opened here (it just has no bones to pose).
	auto rc = std::make_shared<RenderingComponent>(mesh, ShaderUsage::Diffuse | ShaderUsage::Skinning);
	rc->DisableCastShadows();
	modelRC = rc.get();

	modelGO = std::make_shared<GameObject>();
	modelGO->Add(rc);
	scene->Add(modelGO);
	scene->Update(0);

	if (modelRC->HasBones())
	{
		animOwner = std::make_shared<SkeletonAnimation>();
		instance = animOwner->CreateInstance(modelRC);
		// An instance starts every bone at identity, which for most rigs is
		// a folded-up heap rather than the modelled pose. Bind pose is the
		// honest "no animation" state to open on.
		instance->ResetToBindPose();
	}

	loadedMeshPath = absPath;

	// Fallback bounds for an unskinned .p3dm, replaced by the skeleton's own
	// bounds in RefreshFramingBounds() whenever there is a rig.
	fallbackMin = mesh->GetBoundingMinValue();
	fallbackMax = mesh->GetBoundingMaxValue();

	RefreshFramingBounds();
	FrameCamera();
	return true;
}


bool AnimationPreview::ComputeSkeletonBounds(Vec3& outMin, Vec3& outMax) const
{
	if (!instance || instance->GetNumberBones() == 0) return false;

	const std::vector<Bone>& bones = instance->GetSkeletonBones();
	bool any = false;
	for (size_t i = 0; i < bones.size(); i++)
	{
		const int id = bones[i].self;
		if (id < 0 || id >= (int)bones.size()) continue;
		const Vec3 p = instance->GetBoneGlobalTransform(id).GetTranslation();
		if (!any) { outMin = p; outMax = p; any = true; continue; }
		if (p.x < outMin.x) outMin.x = p.x;
		if (p.y < outMin.y) outMin.y = p.y;
		if (p.z < outMin.z) outMin.z = p.z;
		if (p.x > outMax.x) outMax.x = p.x;
		if (p.y > outMax.y) outMax.y = p.y;
		if (p.z > outMax.z) outMax.z = p.z;
	}
	if (!any) return false;

	// Joints sit inside the silhouette - the skull, the hands and the feet
	// all extend past the last bone's origin - so pad by a fraction of the
	// rig's own size rather than framing exactly to the joints.
	const Vec3 e = outMax - outMin;
	const float pad = std::max(std::max(e.x, e.y), e.z) * 0.12f;
	outMin = outMin - Vec3(pad, pad, pad);
	outMax = outMax + Vec3(pad, pad, pad);
	return true;
}

void AnimationPreview::RefreshFramingBounds()
{
	Vec3 mn, mx;
	if (ComputeSkeletonBounds(mn, mx)) { boundsMin = mn; boundsMax = mx; }
	else { boundsMin = fallbackMin; boundsMax = fallbackMax; }

	const Vec3 extent = boundsMax - boundsMin;
	haveBounds = (extent.x > 0.f || extent.y > 0.f || extent.z > 0.f);

	// Longest axis becomes the camera's up - see frameUp's comment in the
	// header for why this is not cosmetic. Ties go to Y, so a cube-ish prop
	// keeps the conventional orientation.
	frameUp = Vec3(0, 1, 0);
	if (haveBounds)
	{
		if (extent.z > extent.y && extent.z >= extent.x) frameUp = Vec3(0, 0, 1);
		else if (extent.x > extent.y && extent.x > extent.z) frameUp = Vec3(1, 0, 0);
	}
}

void AnimationPreview::FrameCamera()
{
	// Refit to whatever the rig is posed as right now, so "Frame" after
	// scrubbing to a crouch frames the crouch.
	RefreshFramingBounds();

	if (!haveBounds)
	{
		panTarget = Vec3(0, 0, 0);
		framedDistance = 6.f;
		distance = framedDistance;
		return;
	}

	panTarget = (boundsMin + boundsMax) * 0.5f;
	const Vec3 extent = boundsMax - boundsMin;

	// Half-extents split into "along the camera's up" and "across it". The
	// across term takes the larger of the two remaining axes because the
	// user can orbit to any of them, and a distance that only fits the
	// narrow side would clip the model the moment they turned it.
	float halfUp, halfAcross;
	if (frameUp.z > 0.5f)      { halfUp = extent.z * 0.5f; halfAcross = std::max(extent.x, extent.y) * 0.5f; }
	else if (frameUp.x > 0.5f) { halfUp = extent.x * 0.5f; halfAcross = std::max(extent.y, extent.z) * 0.5f; }
	else                       { halfUp = extent.y * 0.5f; halfAcross = std::max(extent.x, extent.z) * 0.5f; }

	// Match RenderFrame()'s projection: 45 degrees vertical, panel aspect.
	const float aspect = (height > 0 ? (float)width / (float)height : 1.f);
	const float tanHalfV = std::tan((float)DEGTORAD(45.f) * 0.5f);
	const float distV = halfUp / std::max(0.0001f, tanHalfV);
	const float distH = halfAcross / std::max(0.0001f, tanHalfV * aspect);

	// 1.25 leaves margin for the bone axes and the gizmo, which stick out
	// past the mesh, and keeps the model off the very edge of the panel.
	framedDistance = std::max(0.5f, std::max(distV, distH) * 1.25f);
	distance = framedDistance;
}

int AnimationPreview::FindBone(const std::string& name) const
{
	if (!instance) return -1;
	const std::vector<Bone>& bones = instance->GetSkeletonBones();
	for (size_t i = 0; i < bones.size(); i++)
		if (bones[i].name == name) return (int)bones[i].self;
	return -1;
}

std::string AnimationPreview::BoneName(int boneId) const
{
	if (!instance || boneId < 0 || boneId >= (int)instance->GetNumberBones()) return std::string();
	return instance->GetSkeletonBones()[boneId].name;
}

void AnimationPreview::SyncPose(AnimationEditorDocument& doc, float dt)
{
	if (!instance) return;

	if (doc.blendMode)
	{
		// Structural change (entries added/removed, layers edited, clips
		// edited) needs the whole Play() set rebuilt; a weight tweak only
		// needs the values pushed, or every clip would snap back to frame 0
		// mid-drag.
		if (blendBuiltRevision != doc.blendRevision || blendBuiltClipsRevision != doc.clipsRevision || !blendActive)
			RebuildBlend(doc);
		else
			ApplyBlendWeights(doc);

		if (doc.blendPlaying)
		{
			// SkeletonAnimation::Update wants a clock counting up from the
			// first call - it derives each clip's position by subtracting
			// the time it first saw one - so this accumulates and is never
			// rewound while playing.
			doc.blendClock += dt;
		}
		if (animOwner) animOwner->Update(doc.blendClock);
		return;
	}

	if (blendActive) StopBlend();

	const Animation* clip = doc.ActiveClip();
	if (clip)
		instance->ApplyAnimationAtTime(*clip, doc.playhead);
	else
		instance->ResetToBindPose();

	if (!poseOverrides.empty())
	{
		for (std::map<int, Matrix>::const_iterator it = poseOverrides.begin(); it != poseOverrides.end(); ++it)
		{
			if (it->first >= 0 && it->first < (int)instance->GetNumberBones())
				instance->SetBoneLocalTransform(it->first, it->second);
		}
		// One hierarchy walk for the whole batch rather than per bone.
		instance->RefreshSkinning();
	}
}

void AnimationPreview::StopBlend()
{
	if (instance) instance->Stop();
	blendActive = false;
	blendBuiltRevision = 0;
	blendBuiltClipsRevision = 0;
}

void AnimationPreview::RebuildBlend(AnimationEditorDocument& doc)
{
	if (!instance || !animOwner) return;

	// Hand the container the document's CURRENT clips, including unsaved
	// edits - the whole point of previewing a blend here rather than in the
	// game. SetAnimations stops every playing entry, since Play() holds raw
	// pointers into the vector being replaced.
	animOwner->SetAnimations(doc.clips);
	instance->Stop();

	// Layers must exist before any Play() names one: Play() looks the layer
	// up immediately and would otherwise store a null Layer pointer that
	// Update() then dereferences.
	for (size_t i = 0; i < doc.blendLayers.size(); i++)
	{
		const AnimationBlendLayer& layer = doc.blendLayers[i];
		if (layer.name.empty()) continue;
		bool used = false;
		for (size_t e = 0; e < doc.blendEntries.size(); e++)
			if (doc.blendEntries[e].layer == layer.name) { used = true; break; }
		if (!used) continue;

		instance->CreateLayer(layer.name);
		for (size_t b = 0; b < layer.bones.size(); b++)
			instance->AddBone(layer.name, layer.bones[b]);
	}

	for (size_t i = 0; i < doc.blendEntries.size(); i++)
	{
		AnimationBlendEntry& e = doc.blendEntries[i];
		e.playOrder = -1;
		if (e.clip < 0 || e.clip >= (int)doc.clips.size()) continue;
		e.playOrder = instance->Play((uint32)e.clip, 0.f, e.repetition, e.speed,
			BlendWeightToScale(e.weight), e.layer);
	}

	blendBuiltRevision = doc.blendRevision;
	blendBuiltClipsRevision = doc.clipsRevision;
	blendActive = true;
}

void AnimationPreview::ApplyBlendWeights(AnimationEditorDocument& doc)
{
	if (!instance) return;
	for (size_t i = 0; i < doc.blendEntries.size(); i++)
	{
		const AnimationBlendEntry& e = doc.blendEntries[i];
		if (e.playOrder < 0 || e.playOrder >= (int)instance->GetNumberPlayingAnimations()) continue;
		// Feeding back the clip's own current progress keeps it where it is
		// instead of restarting - the same thing the skeleton_anim demo does
		// when it re-weights two clips every frame.
		const f32 progress = instance->GetAnimationCurrentProgress((uint32)e.playOrder);
		instance->ChangeProperties((uint32)e.playOrder, progress, e.repetition, e.speed,
			BlendWeightToScale(e.weight));
	}
}

void AnimationPreview::SetPoseMode(BonePoseMode mode)
{
	poseMode = mode;
	if (gizmo) { delete gizmo; gizmo = nullptr; }
	EnsureGizmo();
}

void AnimationPreview::EnsureGizmo()
{
	if (gizmo) return;
	gizmo = (poseMode == BonePoseMode::Translate) ? CreateMoveGizmo() : CreateRotateGizmo();
	// Bones are always manipulated in their own frame. A world-space bone
	// rotation would have to be conjugated back through the parent chain
	// before it could be written as a local key (the dance
	// SceneEditor::LocalizeWorldRotation does for GameObjects), and posing
	// a limb in its own axes is what an animator actually wants anyway.
	gizmo->SetLocation(IGizmo::LOCATE_LOCAL);
}

Vec3 AnimationPreview::ComputeEye() const
{
	// Orbit around frameUp rather than around world +Y, so yaw circles the
	// model's own vertical and pitch rises towards its head whatever axis
	// that happens to be.
	const Vec3 up = frameUp;
	// Any vector not parallel to up, to seed the basis.
	const Vec3 seed = (std::fabs(up.y) < 0.9f) ? Vec3(0, 1, 0) : Vec3(0, 0, 1);
	const Vec3 right = up.cross(seed).normalize();
	const Vec3 fwd = right.cross(up).normalize();

	const Vec3 offset = (right * (distance * cosf(pitch) * sinf(yaw)))
		+ (up * (distance * sinf(pitch)))
		+ (fwd * (distance * cosf(pitch) * cosf(yaw)));
	return panTarget + offset;
}

void AnimationPreview::DrawBoneOctahedron(const Vec3& head, const Vec3& tail,
	const Matrix& orient, const Vec4& color)
{
	if (!debug) return;

	Vec3 dir = tail - head;
	const float len = dir.magnitude();
	if (len < 1e-5f)
	{
		// Coincident joints (a helper node parented at its parent's origin)
		// have no direction to build a shape from.
		debug->drawPoint(head, 5.f, color);
		return;
	}
	dir = dir / len;

	// Square the waist to the joint's own local X/Z, orthogonalised against
	// the bone direction - this is what makes roll readable. A joint whose
	// local X happens to run along the bone gives a degenerate cross product,
	// so fall back to any perpendicular in that case.
	Vec3 ax(orient.m[0], orient.m[1], orient.m[2]);
	ax = ax - dir * ax.dotProduct(dir);
	if (ax.magnitudeSQR() < 1e-8f)
	{
		Vec3 seed(orient.m[8], orient.m[9], orient.m[10]);
		ax = seed - dir * seed.dotProduct(dir);
	}
	if (ax.magnitudeSQR() < 1e-8f)
	{
		const Vec3 seed = (std::fabs(dir.y) < 0.9f) ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
		ax = seed - dir * seed.dotProduct(dir);
	}
	ax.normalizeSelf();
	const Vec3 az = dir.cross(ax).normalize();

	// Blender's proportions: the waist sits ~15% along and is ~10% of the
	// length across, which keeps a long bone slim and a short one still
	// visible.
	const float waistDist = len * 0.15f;
	const float w = len * 0.10f;
	const Vec3 waist = head + dir * waistDist;
	const Vec3 v[4] = {
		waist + ax * w,
		waist + az * w,
		waist - ax * w,
		waist - az * w
	};

	for (int i = 0; i < 4; i++)
	{
		debug->drawLine(head, v[i], color);          // head fan
		debug->drawLine(v[i], v[(i + 1) % 4], color); // waist ring
		debug->drawLine(v[i], tail, color);           // tail fan
	}
}

void AnimationPreview::DrawSkeleton(int selectedBone)
{
	if (!debug || !instance) return;

	const std::vector<Bone>& bones = instance->GetSkeletonBones();

	// Which bones descend from the selection - drawn warmer, so you can see
	// what a rotation is about to carry with it.
	std::vector<bool> onChain(bones.size(), false);
	if (selectedBone >= 0 && selectedBone < (int)bones.size())
	{
		onChain[selectedBone] = true;
		// Parents always precede children in bone id order for every rig
		// the importer produces; walking up per bone instead of trusting
		// that keeps this correct either way.
		for (size_t i = 0; i < bones.size(); i++)
		{
			int p = bones[i].parent;
			int guard = 0;
			while (p >= 0 && p < (int)bones.size() && guard++ < (int)bones.size())
			{
				if (p == selectedBone) { onChain[i] = true; break; }
				p = bones[p].parent;
			}
		}
	}

	for (size_t i = 0; i < bones.size(); i++)
	{
		const int id = bones[i].self;
		if (id < 0 || id >= (int)bones.size()) continue;

		const Matrix boneWorld = instance->GetBoneGlobalTransform(id);
		const Vec3 head = boneWorld.GetTranslation();
		const bool isSelected = (id == selectedBone);
		const Vec4 color = ColorForBone(isSelected, onChain[i]);

		// Bone body: the segment from the parent's origin (head) to this
		// bone's origin (tail) - a joint hierarchy has no explicit bone
		// lengths, so the body between two joints is the bone. A root has
		// nothing to draw a body to and gets only its joint marker.
		float boneLength = 0.f;
		if (bones[i].parent >= 0 && bones[i].parent < (int)bones.size())
		{
			const Matrix parentXform = instance->GetBoneGlobalTransform(bones[i].parent);
			const Vec3 parentHead = parentXform.GetTranslation();
			boneLength = (head - parentHead).magnitude();

			if (boneStyle == BoneDrawStyle::Octahedral)
			{
				// Oriented by the PARENT's axes: this segment is the parent
				// joint's bone, and rotating that joint is what rolls it.
				DrawBoneOctahedron(parentHead, head, parentXform, color);
			}
			else debug->drawLine(parentHead, head, color);
		}

		// Joint marker, always drawn: it is the thing you actually click, and
		// on a leaf bone (a fingertip, with no child to form a body toward)
		// it is the only mark there is.
		debug->drawPoint(head, isSelected ? 9.f : 5.f, color);

		// Local axes, so the animator can see which way a rotation will go.
		// Length follows the bone so a hand's fingers don't each sprout a
		// tripod the size of the torso.
		const float axisLen = std::max(0.02f, (boneLength > 0.f ? boneLength : framedDistance * 0.04f) * kAxisFraction);
		if (isSelected)
		{
			const Matrix rot = boneWorld;
			const Vec3 ax(rot.m[0], rot.m[1], rot.m[2]);
			const Vec3 ay(rot.m[4], rot.m[5], rot.m[6]);
			const Vec3 az(rot.m[8], rot.m[9], rot.m[10]);
			debug->drawLine(head, head + ax * axisLen, Vec4(1.f, 0.2f, 0.2f, 1.f));
			debug->drawLine(head, head + ay * axisLen, Vec4(0.2f, 1.f, 0.2f, 1.f));
			debug->drawLine(head, head + az * axisLen, Vec4(0.3f, 0.5f, 1.f, 1.f));
		}
	}
}

void AnimationPreview::ProjectBones(const Matrix& view, const Matrix& proj)
{
	boneScreenPos.clear();
	if (!instance) return;

	const std::vector<Bone>& bones = instance->GetSkeletonBones();
	boneScreenPos.resize(bones.size(), Vec3(0, 0, -1));
	const Matrix viewProj = proj * view;

	for (size_t i = 0; i < bones.size(); i++)
	{
		const int id = bones[i].self;
		if (id < 0 || id >= (int)bones.size()) continue;
		const Vec3 p = instance->GetBoneGlobalTransform(id).GetTranslation();
		const Vec4 clip = viewProj * Vec4(p.x, p.y, p.z, 1.f);
		if (clip.w <= 0.0001f) { boneScreenPos[id] = Vec3(0, 0, -1); continue; }
		const float ndcX = clip.x / clip.w;
		const float ndcY = clip.y / clip.w;
		boneScreenPos[id] = Vec3(
			(ndcX * 0.5f + 0.5f) * (float)width,
			(1.f - (ndcY * 0.5f + 0.5f)) * (float)height,
			clip.w);
	}
}

void AnimationPreview::PrepareGizmo(int boneId, const Matrix& view, const Matrix& proj)
{
	if (!gizmo || !instance || boneId < 0 || boneId >= (int)instance->GetNumberBones()) return;

	const std::vector<Bone>& bones = instance->GetSkeletonBones();
	const int parent = bones[boneId].parent;

	// Same three-matrix arrangement SceneEditor uses for a GameObject with
	// a parent - bone local transform stands in for LocalTransform, and the
	// parent bone's model-space matrix for the parent's world matrix:
	//   anchor = parentWorld * local
	// The gizmo writes into the edit matrix (gizmoBoneLocal) as the mouse
	// moves; DrawAndUpdate reads it back out and pushes it onto the rig.
	gizmoParentWorld = (parent >= 0 && parent < (int)bones.size())
		? instance->GetBoneGlobalTransform(parent) : Matrix();
	gizmoAnchorWorld = gizmoParentWorld * gizmoBoneLocal;

	// What SetGlobalTransform has to be for a BONE, which is not what the
	// name suggests.
	//
	// CGizmoTransformRotate::Rotate1Axe's local branch builds its rotation as
	//     newLocal = R(inverse(globalTransform) * m_Axis, angle) * oldLocal
	// and m_Axis is GetVector(id) - a RAW coordinate axis, not the world-space
	// vector its comment claims. Feeding the parent's world matrix (the
	// obvious reading, and what SceneEditor does for a child GameObject)
	// therefore conjugates a local axis through an unrelated frame, and the
	// bone turns about a skewed axis: measured 0.72 alignment with any
	// coordinate axis on a single-ring drag, where a correct one is 1.00. It
	// only looks right for SceneEditor's root objects, where the parent is
	// identity and the conjugation does nothing.
	//
	// Rotating the bone about its OWN axis e_i means
	//     newWorld = R_world(rot(parentWorld*local) * e_i, angle) * parentWorld * local
	// which reduces to needing the conjugated axis to equal rot(local) * e_i,
	// i.e. inverse(globalTransform) == rot(local). Hence the inverse of the
	// bone's local rotation, with translation stripped so only the rotation
	// takes part.
	Matrix boneRotationOnly = gizmoBoneLocal;
	boneRotationOnly.Translate(Vec3(0, 0, 0));
	gizmoAxisFrame = boneRotationOnly.Inverse();

	gizmo->SetDisplayScale(0.15f);
	gizmo->SetScreenDimension(width, height, true, gizmoOrthoL, gizmoOrthoR, gizmoOrthoB, gizmoOrthoT);
	gizmo->SetLocalTransform((float*)&gizmoAnchorWorld.m);
	gizmo->SetGlobalTransform((float*)&gizmoAxisFrame.m);
	gizmo->SetEditMatrix((float*)&gizmoBoneLocal.m);
	gizmo->SetCameraMatrix(view.m, proj.m);
}

void AnimationPreview::RenderFrame()
{
	if (!renderer || !effects || !scene || !cameraGO) return;

	const Vec3 eye = ComputeEye();
	Matrix view;
	view.LookAt(eye, panTarget, frameUp);
	cameraGO->SetTransformationMatrix(view.Inverse());
	scene->Update(ImGui::GetTime());

	Projection proj;
	proj.Perspective(45.f, (f32)width / (f32)height, 0.05f, std::max(500.f, framedDistance * 20.f));

	IRenderer::InvalidateSharedUniformCaches();
	renderer->Resize((uint32)width, (uint32)height);
	effects->Resize((uint32)width, (uint32)height);
	effects->ProcessPostEffects(&proj);
	renderer->ResetViewPort();
	renderer->SetViewPort(0, 0, (uint32)width, (uint32)height);
	renderer->PreRender(cameraGO.get(), scene);
	renderer->ApplyBackgroundClearColor();
	effects->CaptureFrame();
	if (showMesh)
		renderer->RenderScene(proj, cameraGO.get(), scene);

	// Overlay pass. DebugRenderer draws with depth test off by design, so
	// bones/gizmo stay visible through the mesh - which is what you want
	// when posing a skeleton inside a body.
	GetActiveRenderDevice().SetViewport(0, 0, (uint32)width, (uint32)height);
	if (debug)
	{
		if (showGrid)
		{
			// A plain line grid rather than SceneEditor's Grid renderable:
			// that one is a real RenderObject drawn through the renderer so
			// it can depth-test against the scene, machinery this preview
			// has no need for.
			//
			// Laid in the plane perpendicular to frameUp and dropped to the
			// model's lowest point along that axis, so it reads as the floor
			// the character stands on. A world-XZ grid would cut a Z-up
			// model off at the waist (see frameUp's comment).
			const Vec3 up = frameUp;
			const Vec3 seed = (std::fabs(up.y) < 0.9f) ? Vec3(0, 1, 0) : Vec3(0, 0, 1);
			const Vec3 ax = up.cross(seed).normalize();
			const Vec3 az = ax.cross(up).normalize();
			// Foot of the model along the up axis, or the origin when nothing
			// is loaded.
			float floorOffset = 0.f;
			if (haveBounds)
				floorOffset = (up.x * boundsMin.x) + (up.y * boundsMin.y) + (up.z * boundsMin.z);
			const Vec3 origin = up * floorOffset;

			const float step = std::max(0.25f, framedDistance * 0.1f);
			const int lines = 10;
			const float extent = step * lines;
			const Vec4 gridColor(0.32f, 0.32f, 0.38f, 1.f);
			for (int i = -lines; i <= lines; i++)
			{
				const float o = step * i;
				debug->drawLine(origin + ax * o - az * extent, origin + ax * o + az * extent, gridColor);
				debug->drawLine(origin + az * o - ax * extent, origin + az * o + ax * extent, gridColor);
			}
		}

		// The skeleton and the gizmo are submitted by DrawAndUpdate before
		// it calls this (it is the one that knows the selection), so this
		// only flushes the batch.
		debug->Render(cameraGO->GetWorldTransformation().Inverse(), proj.GetProjectionMatrix());
	}

	effects->EndCapture();
	IRenderer::InvalidateSharedUniformCaches();
}

bool AnimationPreview::DrawAndUpdate(AnimationEditorDocument& doc, bool& outDragEnded)
{
	outDragEnded = false;
	EnsureInit();

	if (!loadError.empty())
	{
		ImGui::TextColored(ImVec4(1.f, 0.5f, 0.4f, 1.f), "%s", loadError.c_str());
		return false;
	}
	if (loadedMeshPath.empty())
	{
		ImGui::TextDisabled("No mesh bound. Pick a .p3dm above to preview these clips on a rig.");
		return false;
	}

	bool poseChanged = false;

	// Camera matrices for this frame - needed by the gizmo and by bone
	// picking whether or not we actually render this frame.
	const Vec3 eye = ComputeEye();
	Matrix view;
	view.LookAt(eye, panTarget, frameUp);
	// LookAt() already yields the VIEW matrix - RenderFrame() hands its
	// inverse to the camera GameObject (which stores a world transform) and
	// DebugRenderer is then given that world matrix's inverse, i.e. this
	// same view matrix, back again. Taking .Inverse() here handed the gizmo
	// and the bone projection the camera's *world* matrix instead: the
	// gizmo's hit test then never matched the cursor (dragging a bone did
	// nothing at all) and every bone projected behind the near plane, so
	// click-to-select silently picked nothing.
	const Matrix viewMat = view;
	Projection projection;
	projection.Perspective(45.f, (f32)width / (f32)height, 0.05f, std::max(500.f, framedDistance * 20.f));
	const Matrix projMat = projection.GetProjectionMatrix();

	// Everything the gizmo draws goes through whichever DebugRenderer was
	// installed last - a process-wide static shared with every SceneEditor
	// (see SceneEditor::BindSharedEditorHooks). Claim it before submitting,
	// every frame, or the bone gizmo ends up batched into a scene tab's
	// debug renderer and drawn in that viewport instead of this one.
	if (debug) CGizmoTransformRender::SetDebugRenderer(debug);

	// Same alternate-frame rule as MaterialPreview: this offscreen render
	// and the Scene View's must never land in the same frame.
	skipRenderThisCall = !skipRenderThisCall;
	if (!skipRenderThisCall)
	{
		// Submit the skeleton (with the right selection colouring) and the
		// gizmo into the debug batch, then render.
		if (showBones) DrawSkeleton(doc.selectedBone);
		if (doc.selectedBone >= 0 && instance)
		{
			EnsureGizmo();
			gizmoBoneLocal = instance->GetBoneLocalTransform(doc.selectedBone);
			PrepareGizmo(doc.selectedBone, viewMat, projMat);
			gizmo->Draw();
		}
		RenderFrame();
	}
	ProjectBones(viewMat, projMat);

	Texture* color = effects->GetViewportColor();
	if (!color)
	{
		ImGui::TextDisabled("No preview texture");
		return false;
	}
	void* tid = GetActiveRenderDevice().GetImGuiTextureID(color->GetBindID(), color->GetTextureType());
	if (!tid)
	{
		ImGui::TextDisabled("[preview texture unavailable]");
		return false;
	}

	// OpenGL's render targets are bottom-up, so the viewport texture has to
	// be sampled V-flipped to appear the right way up - the same guard
	// SceneEditor::ShowViewport uses for its own viewport image. Without it
	// the whole preview renders vertically mirrored: the model hangs
	// head-down, the floor grid sits above it, and (worse than cosmetic)
	// every screen-space mouse mapping below - bone picking and the gizmo
	// hit test alike - is measuring against a flipped image.
#if defined(_SDL2VULKAN) || defined(_SDL2METAL)
	const ImVec2 uv0(0, 0), uv1(1, 1);
#else
	const ImVec2 uv0(0, 1), uv1(1, 0);
#endif
	const ImVec2 imageOrigin = ImGui::GetCursorScreenPos();
	imageScreenX = imageOrigin.x;
	imageScreenY = imageOrigin.y;
	ImGui::Image((ImTextureID)tid, ImVec2((f32)width, (f32)height), uv0, uv1);
	const bool hovered = ImGui::IsItemHovered();

	ImGuiIO& io = ImGui::GetIO();
	const ImVec2 localMouse(io.MousePos.x - imageOrigin.x, io.MousePos.y - imageOrigin.y);
	const bool inside = localMouse.x >= 0 && localMouse.y >= 0
		&& localMouse.x < (float)width && localMouse.y < (float)height;

	// ---- gizmo interaction ------------------------------------------
	// Runs before camera navigation so dragging an axis never also orbits.
	if (gizmo && doc.selectedBone >= 0 && instance)
	{
		// The ortho ray libgizmo wants for its hit tests, same setup
		// SceneEditor::ShowViewport/PrepareGizmoForDraw builds: SYMMETRIC,
		// aspect-corrected ortho bounds, and the very same l/r/b/t handed to
		// SetScreenDimension below. Passing 0..width/0..height (as this
		// first did) describes a completely different space from the ortho
		// projection the ray is unprojected through, so every hit test
		// missed and the gizmo could not be grabbed.
		float gl, gr, gb, gt;
		// Scaled to the framed model rather than SceneEditor's fixed 5: the
		// bounds set the world-units-per-screen-unit the gizmo converts drag
		// deltas with, so a rig 30 units away needs proportionally larger
		// ones.
		const float zoomOrtho = std::max(0.001f, distance * 0.2f);
		if (width > height)      { gr = zoomOrtho; gl = -gr; gt = (f32)height * zoomOrtho / (f32)width; gb = -gt; }
		else if (width < height) { gr = (f32)width * zoomOrtho / (f32)height; gl = -gr; gt = zoomOrtho; gb = -gt; }
		else                     { gr = zoomOrtho; gl = -gr; gt = zoomOrtho; gb = -gt; }

		Projection orthoProj;
		orthoProj.Ortho(gl, gr, gb, gt, 0.1f, 100000.f);
		Mouse3D mray;
		mray.GenerateRay((f32)width, (f32)height, localMouse.x, localMouse.y, Matrix(),
			viewMat, orthoProj.GetProjectionMatrix());
		gizmo->SetOrthoMouse(mray.GetOrigin().x, mray.GetOrigin().y, mray.GetOrigin().z,
			mray.GetDirection().x, mray.GetDirection().y, mray.GetDirection().z);
		gizmoOrthoL = gl; gizmoOrthoR = gr; gizmoOrthoB = gb; gizmoOrthoT = gt;

		// Clamped into the image, exactly as SceneEditor does - libgizmo
		// indexes its own screen-space buffers with these.
		const unsigned gx = (unsigned)std::min(std::max(0.f, localMouse.x), (float)width - 1.f);
		const unsigned gy = (unsigned)std::min(std::max(0.f, localMouse.y), (float)height - 1.f);

		if (hovered && inside && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			gizmoBoneLocal = instance->GetBoneLocalTransform(doc.selectedBone);
			PrepareGizmo(doc.selectedBone, viewMat, projMat);
			// OnMouseMove before OnMouseDown: libgizmo decides which axis is
			// grabbed from the axis its LAST move call highlighted, so a
			// click with no preceding move grabs nothing.
			gizmo->OnMouseMove(gx, gy);
			if (gizmo->OnMouseDown(gx, gy))
				gizmoDragging = true;
		}
		else if (gizmoDragging && ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			PrepareGizmo(doc.selectedBone, viewMat, projMat);
			gizmo->OnMouseMove(gx, gy);
			// gizmoBoneLocal now holds the dragged local transform - stash
			// it as a pending override so SyncPose keeps showing it instead
			// of the clip's own sampled value.
			poseOverrides[doc.selectedBone] = gizmoBoneLocal;
			instance->SetBoneLocalTransform(doc.selectedBone, gizmoBoneLocal);
			instance->RefreshSkinning();
			poseChanged = true;
		}
		else if (gizmoDragging && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			gizmo->OnMouseUp(gx, gy);
			gizmoDragging = false;
			outDragEnded = true;
			poseChanged = true;
		}
		else if (hovered && inside)
		{
			// Axis hover highlight while not dragging. libgizmo only lights
			// up the axis under the cursor in response to a move call, so
			// without this the gizmo looks inert until you actually click -
			// there is no feedback telling you which ring you are about to
			// grab.
			PrepareGizmo(doc.selectedBone, viewMat, projMat);
			gizmo->OnMouseMove(gx, gy);
		}
	}

	if (!hovered || gizmoDragging) return poseChanged;

	// ---- bone picking -------------------------------------------------
	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && inside && instance)
	{
		int best = -1;
		float bestDist = 14.f; // pixels
		for (size_t i = 0; i < boneScreenPos.size(); i++)
		{
			if (boneScreenPos[i].z <= 0.f) continue;
			const float dx = boneScreenPos[i].x - localMouse.x;
			const float dy = boneScreenPos[i].y - localMouse.y;
			const float d = std::sqrt(dx * dx + dy * dy);
			if (d < bestDist) { bestDist = d; best = (int)i; }
		}
		if (best >= 0)
		{
			doc.selectedBone = best;
			doc.selectedBoneName = BoneName(best);
		}
	}

	// ---- camera navigation --------------------------------------------
	if (io.MouseWheel != 0.f)
	{
		distance -= io.MouseWheel * (distance * 0.12f);
		distance = std::max(0.1f, std::min(distance, framedDistance * 12.f));
	}
	// Middle-drag orbits, matching the Scene View's navigation rather than
	// MaterialPreview's left-drag: left is taken here by picking and the
	// gizmo.
	if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
	{
		yaw -= io.MouseDelta.x * 0.01f;
		pitch += io.MouseDelta.y * 0.01f;
		pitch = std::max(-1.5f, std::min(pitch, 1.5f));
	}
	if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
	{
		const Vec3 fwd = (panTarget - eye).normalize();
		const Vec3 right = (fwd.cross(frameUp)).normalize();
		const Vec3 up = (right.cross(fwd)).normalize();
		const float panSpeed = distance * 0.0015f;
		panTarget += (right * (-io.MouseDelta.x * panSpeed)) + (up * (io.MouseDelta.y * panSpeed));
	}

	return poseChanged;
}
