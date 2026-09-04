//=============================================================================
// Name        : Character2DPreview.h
// Description : The Character 2D editor's viewport: a private SceneGraph +
//               ForwardRenderer holding ONE GameObject - the character being
//               authored - viewed through an orthographic camera.
//
//               Private, not the open scene. Authoring a character inside a
//               level means every click near a joint is ambiguous with
//               selecting and moving the props around it, the character is lit
//               by that level's lights rather than seen plainly, and half the
//               viewport code becomes "hide everything that is not the
//               character". A character is not part of any scene, so it is not
//               edited in one.
//
//               Built through ApplyCharacter2D - the same call the scene
//               loader uses - so what you author is what a game will show.
//=============================================================================

#ifndef CHARACTER2DPREVIEW_H
#define CHARACTER2DPREVIEW_H

#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Renderer/DebugRenderer/DebugRenderer.h>
#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>
#include <Pyros3D/Assets/Character2D/Character2DInstance.h>
#include <Pyros3D/Core/Math/Math.h>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct Character2DDocument;

struct Character2DPreview {
	// ---- owned engine objects (built lazily by EnsureInit) -----------
	p3d::ForwardRenderer* renderer = nullptr;
	p3d::PostEffectsManager* effects = nullptr;
	p3d::SceneGraph* scene = nullptr;

	std::shared_ptr<p3d::GameObject> characterGO;
	std::shared_ptr<p3d::GameObject> cameraGO;
	std::shared_ptr<p3d::GameObject> lightGO;
	// Non-owning; freed with characterGO.
	p3d::RenderingComponent* characterRC = nullptr;

	// The clips and the instance whose pose everything here reads and writes.
	// Held as a Character2DInstance because the SkeletonAnimation inside it
	// owns the SkeletonAnimationInstance - letting it go frees the pose the
	// sprites are placed from.
	p3d::Character2DInstance built;

	// Revisions of the document this preview was last built from, so a
	// rebuild happens exactly when the character changed rather than every
	// frame (rebuilding re-uploads every texture).
	uint32_t builtRigRevision = 0;
	uint32_t builtClipsRevision = 0;
	uint32_t builtPartsRevision = 0;

	// ---- viewport ----------------------------------------------------
	int width = 640, height = 480;
	// Orthographic framing, in world units.
	float centerX = 0.f, centerY = 0.f, halfWidth = 3.f;
	bool framed = false;
	// Set by a pan or a zoom. Once the view has been placed by hand it is not
	// taken back: a viewport that re-fitted itself whenever the panel resized
	// would undo the framing you just chose. Until then, though, a resize
	// SHOULD re-fit - switching stage changes the panel's shape, and a fit
	// computed for the old one crops the character.
	bool userAdjustedView = false;
	// The size the current fit was computed for.
	int framedForWidth = 0, framedForHeight = 0;

	// Pixel grid behind the character, so artwork can be placed against
	// something. On by default here (unlike the Scene View): a character is
	// authored against measurements, not against a background.
	bool showGrid = true;
	bool showBones = true;
	bool showSprites = true;

	// ---- interaction --------------------------------------------------
	// Cursor in viewport pixels, and whether it is over the image.
	p3d::Math::Vec2 mouse = p3d::Math::Vec2(0.f, 0.f);
	bool mouseValid = false;
	int hoveredBone = -1;
	// Joint being dragged, and the pose at the moment it was grabbed, so the
	// whole gesture is one undo entry rather than one per frame.
	int draggingBone = -1;
	bool panning = false;
	p3d::Math::Vec2 panAnchor = p3d::Math::Vec2(0.f, 0.f);
	float panCenterX = 0.f, panCenterY = 0.f;

	// Bones posed but not yet keyed, mirrored into the animation document so
	// the next frame's ApplyTimelinePose does not sample the clip straight
	// back over a drag.
	std::vector<int> posedBones;
	// Raised on the frame a drag is released, so auto-key can fire off the
	// same gesture that posed the rig.
	bool dragEnded = false;

	~Character2DPreview();

	void EnsureInit();
	// Rebuilds the character from `doc.asset` if its rigRevision moved, and
	// re-hands the clips if clipsRevision did. Cheap to call every frame.
	void Sync(Character2DDocument& doc);
	// Frames the view on the character - its artwork if it has any, its bones
	// otherwise (a skeleton with no sprites yet is still worth looking at).
	void FrameCamera(Character2DDocument& doc);

	// Renders into the offscreen target and returns its colour texture, or
	// NULL if it could not.
	p3d::Texture* RenderFrame();

	// ---- coordinates ---------------------------------------------------
	// World units per viewport pixel, so handles can be sized in pixels and
	// stay the same size on screen at any zoom.
	float WorldPerPixel() const;
	// Cursor (viewport pixels) to a point on the z = 0 plane, in world space.
	bool CursorToWorld(const p3d::Math::Vec2& px, p3d::Math::Vec3& outWorld) const;
	// Nearest joint to the cursor within a pixel radius, or -1.
	int PickJoint(const p3d::Math::Vec2& px) const;
	// Where a bone's joint is on screen, in viewport pixels.
	bool BoneToPixels(int boneId, p3d::Math::Vec2& outPx) const;

	// Poses `boneId` to reach `targetWorld` - what a joint drag does. Records
	// what it moved in `posedBones`.
	void DragJoint(int boneId, const p3d::Math::Vec3& targetWorld);
};

#endif /* CHARACTER2DPREVIEW_H */
