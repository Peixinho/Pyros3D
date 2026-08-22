//=============================================================================
// Name        : AnimationPreview.h
// Description : The Animation Editor's 3D viewport: a private SceneGraph +
//               ForwardRenderer + PostEffectsManager rendering the bound
//               .p3dm into an ImGui::Image, with the skeleton drawn over it
//               through a DebugRenderer and a libgizmo manipulator on the
//               selected bone.
//
//               Modelled on MaterialPreview (same offscreen-render-into-an-
//               ImGui-image pattern, same alternate-frame rule - see
//               DrawAndUpdate), but it owns a rig rather than a sphere: a
//               Model, a SkeletonAnimation container and the
//               SkeletonAnimationInstance whose pose everything here reads
//               and writes.
//
//               Forward only, deliberately. This is a preview of an authored
//               rig, not of the project's lighting: models come out of a
//               .p3dm with GenericShaderMaterials that render correctly in
//               Forward regardless of the project's renderer setting, and
//               skipping the Deferred branch avoids the G-buffer +
//               overlay-compositing dance SceneEditor::ShowViewport needs.
//=============================================================================

#ifndef ANIMATIONPREVIEW_H
#define ANIMATIONPREVIEW_H

#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/Rendering/Renderer/DebugRenderer/DebugRenderer.h>
#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>
#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <Pyros3D/Core/Math/Math.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

class IGizmo;
struct AnimationEditorDocument;

namespace p3d { class RenderingComponent; }

// What the gizmo on the selected bone does.
enum class BonePoseMode { Rotate, Translate };

struct AnimationPreview {
	// ---- owned engine objects (built lazily by EnsureInit) -----------
	p3d::IRenderer* renderer = nullptr;
	p3d::PostEffectsManager* effects = nullptr;
	p3d::SceneGraph* scene = nullptr;
	p3d::DebugRenderer* debug = nullptr;

	std::shared_ptr<p3d::GameObject> modelGO;
	std::shared_ptr<p3d::GameObject> cameraGO;
	std::shared_ptr<p3d::GameObject> lightGO;
	// Non-owning; freed with modelGO.
	p3d::RenderingComponent* modelRC = nullptr;

	// Clip container the instance belongs to. Deliberately empty of clips:
	// the editor never Play()s anything, it poses the instance directly
	// (SkeletonAnimationInstance::ApplyAnimationAtTime) from the document's
	// own in-memory clips, so playback state can't drift from what the
	// timeline says. An instance still needs an owner, hence this.
	std::shared_ptr<p3d::SkeletonAnimation> animOwner;
	// Owned by animOwner (SkeletonAnimation's destructor deletes its
	// instances) - do not delete.
	p3d::SkeletonAnimationInstance* instance = nullptr;

	// Absolute path of the mesh currently loaded, so a rebind is a no-op
	// when nothing changed.
	std::string loadedMeshPath;
	// Set when the last LoadMesh failed; shown in the viewport instead of a
	// blank image.
	std::string loadError;

	// ---- viewport ----------------------------------------------------
	int width = 640, height = 420;
	// Orbit camera. framedDistance is what "Frame" resets to, derived from
	// the model's bounding box on load so a 2cm prop and a 200-unit
	// character both fill the view.
	float yaw = 0.6f, pitch = 0.25f, distance = 6.f, framedDistance = 6.f;
	p3d::Math::Vec3 panTarget = p3d::Math::Vec3(0, 0, 0);

	// The model's own vertical, i.e. its longest bounding-box axis, used as
	// the camera's up vector and as the orbit axis.
	//
	// Not a cosmetic choice: this engine's assets are not consistently Y-up.
	// The stock human.p3dm (and anything else exported from 3ds Max/Biped)
	// is Z-up and 18 units tall along Z, so orbiting it around +Y points the
	// camera straight down the character's length - the mesh appears to lie
	// on its side, its near end looms and its far end recedes, and no
	// distance frames it well because its silhouette is at its narrowest.
	// Taking the longest axis as "up" makes a character stand upright on
	// screen whichever convention it was exported in, and leaves a Y-up
	// model exactly where it was. Nothing about the asset is modified - this
	// is purely where the camera stands.
	p3d::Math::Vec3 frameUp = p3d::Math::Vec3(0, 1, 0);
	// AABB the framing is computed from, in model space, kept so
	// FrameCamera() can refit against the panel's current aspect ratio
	// rather than the one that happened to be in effect at load.
	//
	// Taken from the posed SKELETON, not from the mesh's vertex bounds.
	// Those two disagree for a skinned model: the .p3dm's vertices are in
	// the modelled pose, but what is drawn is those vertices pushed through
	// bindPoseGlobal * boneOffset, and for the stock human.p3dm that lands
	// the character flat in XY even though its raw vertex AABB is 18 units
	// long in Z. Framing off the vertex bounds therefore picked the wrong
	// "up" axis and pointed the camera down the character's length -
	// measured by rendering the bind pose and seeing the mesh lie in the
	// grid plane. Bones are where the character actually is.
	p3d::Math::Vec3 boundsMin, boundsMax;
	// The renderable's own vertex AABB, used only when the model has no
	// skeleton at all (an unskinned .p3dm opened here to start a clip for).
	p3d::Math::Vec3 fallbackMin, fallbackMax;
	bool haveBounds = false;
	// See MaterialPreview::skipRenderThisCall - the same hard "never render
	// in the same frame as the main viewport" guarantee, for the same
	// reason (shared GlobalMatrices UBO / framebuffer state).
	bool skipRenderThisCall = true;

	// ---- display toggles ---------------------------------------------
	// How the skeleton is drawn. Octahedral is Blender's shape and the
	// default for the same reason: a plain line between two joints shows
	// where a bone is but not which way it is ROLLED, and roll is exactly
	// what you need to predict where a rotation will take the limb. The
	// octahedron's cross-section is built from the joint's own local axes,
	// so it turns with them. Stick stays available because a dense rig
	// (hands, fingers) reads better as lines.
	enum class BoneDrawStyle { Octahedral, Stick };
	BoneDrawStyle boneStyle = BoneDrawStyle::Octahedral;
	bool showBones = true;
	bool showBoneNames = false;
	bool showMesh = true;
	bool showGrid = true;

	// ---- bone posing --------------------------------------------------
	IGizmo* gizmo = nullptr;
	BonePoseMode poseMode = BonePoseMode::Rotate;
	bool gizmoDragging = false;
	// The matrix libgizmo writes into while dragging. Must be a stable
	// address for the whole drag (SetEditMatrix keeps the pointer), so it
	// lives here rather than in a frame-local.
	p3d::Math::Matrix gizmoBoneLocal;
	// Anchor/parent matrices handed to the gizmo each frame; members for
	// the same lifetime reason as gizmoBoneLocal.
	p3d::Math::Matrix gizmoAnchorWorld, gizmoParentWorld;
	// Orthographic frustum bounds handed to both the hit-test ray and
	// IGizmo::SetScreenDimension - they must describe the same space (see
	// DrawAndUpdate). Refreshed each frame before the gizmo is fed.
	float gizmoOrthoL = -5.f, gizmoOrthoR = 5.f, gizmoOrthoB = -5.f, gizmoOrthoT = 5.f;

	// Bones the user has moved but not yet keyed, by bone id. These are
	// applied on top of whatever the clip samples to at the playhead, so a
	// pose survives the per-frame re-sample; they are consumed (and
	// cleared) by the Key action, and dropped when the playhead moves - see
	// AnimationEditor::SyncPose.
	std::map<int, p3d::Math::Matrix> poseOverrides;

	// blendRevision the current Play() set was built from, so SyncPose can
	// tell a weight tweak (cheap: ApplyBlendWeights) from a structural change
	// (needs a full RebuildBlend).
	uint32_t blendBuiltRevision = 0;
	// Clips the container was last handed. The preview plays the document's
	// in-memory clips through SkeletonAnimation::SetAnimations, so this has
	// to be refreshed whenever the clips themselves change.
	uint32_t blendBuiltClipsRevision = 0;
	bool blendActive = false;

	// Screen-space position of each bone from the last render, in viewport
	// pixels, used for click-to-select. Rebuilt every DrawAndUpdate;
	// parallel to bone ids (index == bone id), z < 0 meaning "behind the
	// camera, not clickable".
	std::vector<p3d::Math::Vec3> boneScreenPos;
	// Top-left of the viewport image in DESKTOP screen coordinates, from the
	// last DrawAndUpdate. boneScreenPos is relative to this, so
	// imageScreenOrigin + boneScreenPos is where a joint actually is on the
	// user's display - which is what an external harness needs in order to
	// click one (see the agent's animation_skeleton response).
	float imageScreenX = 0.f, imageScreenY = 0.f;

	~AnimationPreview();

	// Builds renderer/scene/camera/light/debug. Safe to call every frame.
	void EnsureInit();
	// Loads (or reloads) the skinned model at absPath, replacing whatever
	// was there, and creates a fresh SkeletonAnimationInstance for it.
	// Returns false and sets loadError on failure. Frames the camera on the
	// new model.
	bool LoadMesh(const std::string& absPath);
	void ClearMesh();
	bool HasSkeleton() const { return instance != nullptr && instance->GetNumberBones() > 0; }

	// Poses the rig for `doc`: in timeline mode by sampling the active clip
	// at the playhead and laying any poseOverrides on top; in blend mode by
	// ticking the engine's own playback. Call once per frame before
	// rendering. `dt` advances blend playback and is ignored otherwise.
	void SyncPose(AnimationEditorDocument& doc, float dt = 0.f);

	// Tears down and rebuilds the instance's Play() set from doc's blend
	// entries and layers. Called automatically by SyncPose when doc's
	// blendRevision moves, so callers only have to edit the document.
	void RebuildBlend(AnimationEditorDocument& doc);
	// Pushes current weights/speeds onto the already-playing entries without
	// restarting them, so dragging a weight slider crossfades rather than
	// snapping every clip back to its first frame.
	void ApplyBlendWeights(AnimationEditorDocument& doc);
	// Drops all playback and returns the rig to the timeline path.
	void StopBlend();

	// Resolves a bone name to its id in the loaded skeleton, or -1.
	int FindBone(const std::string& name) const;
	std::string BoneName(int boneId) const;

	// Renders the frame and draws it as an ImGui::Image at the current
	// cursor, then handles camera navigation, bone picking and the bone
	// gizmo for whatever is selected in `doc`. Returns true when the user's
	// interaction changed the bone pose this frame (so the caller can mark
	// the document dirty / auto-key). `outDragEnded` is set on the frame a
	// gizmo drag is released, which is when auto-key fires.
	bool DrawAndUpdate(AnimationEditorDocument& doc, bool& outDragEnded);

	// Points the camera at the whole model again, refitting to the pose the
	// rig is currently in.
	void FrameCamera();
	// Switches the manipulator, rebuilding the gizmo object.
	void SetPoseMode(BonePoseMode mode);

private:
	p3d::Math::Vec3 ComputeEye() const;
	void RenderFrame();
	// Submits the skeleton as lines/points into `debug`. Model space ==
	// world space here (the model GO sits at the origin), so bone globals
	// go in unmodified.
	void DrawSkeleton(int selectedBone);
	// One bone body as a wireframe octahedron from head to tail: a fan to the
	// head, a square "waist" a short way along, and a fan to the tail. `orient`
	// supplies the local axes the waist is squared to - that is what makes the
	// roll visible. Wireframe rather than solid because DebugRenderer draws
	// with depth testing off (so gizmos stay on top), and filled bones would
	// paint over the mesh they are supposed to be inside.
	void DrawBoneOctahedron(const p3d::Math::Vec3& head, const p3d::Math::Vec3& tail,
		const p3d::Math::Matrix& orient, const p3d::Math::Vec4& color);
	void EnsureGizmo();
	// AABB over the current posed bone positions (model space). Returns
	// false when there is no skeleton, in which case the caller falls back
	// to the renderable's vertex bounds.
	bool ComputeSkeletonBounds(p3d::Math::Vec3& outMin, p3d::Math::Vec3& outMax) const;
	// Refreshes boundsMin/boundsMax/frameUp from whichever source applies.
	void RefreshFramingBounds();
	// Feeds libgizmo this frame's camera/screen/matrices for `boneId`.
	void PrepareGizmo(int boneId, const p3d::Math::Matrix& view, const p3d::Math::Matrix& proj);
	// Projects every bone into viewport pixels for picking; fills boneScreenPos.
	void ProjectBones(const p3d::Math::Matrix& view, const p3d::Math::Matrix& proj);
};

#endif /* ANIMATIONPREVIEW_H */
