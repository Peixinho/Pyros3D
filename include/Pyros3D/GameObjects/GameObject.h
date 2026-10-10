//============================================================================
// Name        : GameObject.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : GameObject
//============================================================================

#ifndef GAMEOBJECT_H
#define	GAMEOBJECT_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Rendering/RenderState.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Other/Export.h>
#include <vector>
#include <map>
#include <memory>
using namespace p3d::Math;

namespace p3d {

	// Circular Dependency
	class PYROS3D_API IComponent;

	class PYROS3D_API GameObject
	{
		friend class SceneGraph;

	public:

		// Constructor
		GameObject(bool isStatic = false);

		// Destructor
		virtual ~GameObject();

		// On Init Virtual Function
		virtual void Init();
		// Virtual Function To Update GameObject
		virtual void Update(const f64 time = 0);
		// Whether Update() has anything to do: true for any class that is not
		// plainly a GameObject, unless it says otherwise (see IComponent::
		// NeedsUpdate, and SceneGraph::UpdateObjectTree for what it is for).
		virtual bool WantsUpdate() const;
		// Back into the scene's walk, with everything above it. Anything that
		// changes what an object is or where it is calls this; the setters
		// here do. Cheap: it stops at the first ancestor already awake.
		void Wake();
		bool IsAwake() const { return _SubtreeAwake; }
		// This object and everything under it arrived while the game was
		// running (a streamed cell): its components are registered a few
		// milliseconds' worth a frame, not all in the frame it arrived - see
		// SceneGraph::SetStreamedRegistrationBudget. Cleared by the scene's
		// walk once all of it is in.
		void SetStreamedIn(const bool streamed) { _StreamedIn = streamed; }
		bool IsStreamedIn() const { return _StreamedIn; }
		// Destroy Function
		virtual void Destroy();

		// Recomputes this object's local/world transformation matrix from
		// its current Position/Rotation/Scale - normally handled for free
		// by SceneGraph::Update()'s traversal (GO->Update(); GO->
		// InternalUpdate();), which SetPosition()/SetRotation()/SetScale()
		// alone never trigger (they only flip the dirty flag). An object
		// deliberately kept outside the SceneGraph (e.g. a camera driven
		// directly every frame so scene load/unload never touches it)
		// needs to call this itself after changing its transform, or
		// GetWorldTransformation()/GetWorldPosition() keep returning a
		// stale matrix forever even though GetPosition() reads back the
		// new value correctly.
		// Brings this object's world matrix up to date NOW, for whoever is
		// about to read it. Its children are not touched here - they are
		// carried along by SettleTransformation, which is told by the flag.
		void RefreshTransformation() { UpdateTransformation(); _RefreshedLate = true; Wake(); if (_DrawTaken) { _DrawWorld = _WorldMatrix; _DrawPrvWorld = _PrvWorldMatrix; _DrawScale = _Scale; _DrawRadius = BoundingSphereRadiusWorldSpace; } }
		// The same, for whoever asks every frame (a layout that is solved again
		// each time it is drawn): the matrix is worked out, but only if it has come
		// out different is the object woken and what hangs from it told. True if it moved.
		bool RefreshTransformationIfChanged();

		// The transform looked at a second time, late in the frame, for
		// whatever moved after the scene's traversal had been past it - a
		// script that runs once the scene has updated places things, and
		// without this they were drawn where they had been the frame
		// before: a parachute a frame behind the body it hangs over, a
		// carried gun a frame behind the hands, every frame, by speed times
		// frame time. It does nothing for an object that has not moved, and
		// it leaves the previous-frame matrices alone, which the once-a-
		// frame update has already rolled: motion vectors still see one
		// frame's movement. Returns whether the world matrix changed, so the
		// walk knows the children have to follow.
		bool SettleTransformation(const bool parentMoved);

		// Local Space
		const Matrix &GetLocalTransformation() const;
		const Matrix &GetPrvLocalTransformation() const;

		const Vec3 &GetPosition() const;
		const Vec3 &GetRotation() const;
		const Vec3 &GetScale() const;
		const Vec3 GetDirection() const;

		// World Space
		const Matrix &GetWorldTransformation() const;
		const Matrix &GetPrvWorldTransformation() const;
		// Where it is DRAWN: a copy of where it is, taken at one point of the
		// frame (TakeDrawTransforms) - so that what draws a frame can go on reading
		// it while the next frame's update is already moving the thing. The first
		// step towards a frame's drawing and the next one's game logic running side
		// by side; on with PYROS_FRAME_SPLIT=1, and until then (and for anything
		// not yet taken) it is simply where the thing is.
		const Matrix &GetDrawWorld() const { return _DrawTaken ? _DrawWorld : _WorldMatrix; }
		const Matrix &GetDrawPrvWorld() const { return _DrawTaken ? _DrawPrvWorld : _PrvWorldMatrix; }
		const Vec3 GetDrawWorldPosition() const { return GetDrawWorld().GetTranslation(); }
		const Vec3 &GetDrawScale() const { return _DrawTaken ? _DrawScale : _Scale; }
		f32 GetDrawRadiusWorldSpace() const { return _DrawTaken ? _DrawRadius : BoundingSphereRadiusWorldSpace; }
		static bool DrawCopies();
		static void NoteForDraw(GameObject* go);
		static void TakeDrawTransforms();
		// What leaves the scene while a frame that may still draw it is being
		// recorded by another thread is kept alive until that frame is done:
		// KeepUntilDrawn takes a hold on it while keeping is on (the player turns
		// it on as it hands a frame over, and lets go of everything at the join).
		static void SetKeeping(const bool on);
		static void KeepUntilDrawn(const std::shared_ptr<void> &what);
		static void ReleaseKept();
		const Vec3 GetWorldPosition() const;
		const Vec3 GetWorldRotation() const;

		// Set Properties
		void SetPosition(const Vec3 &position);
		void SetRotation(const Vec3 &rotation);
		void SetScale(const Vec3 &scale);

		// Set TransformationMatrix
		void SetTransformationMatrix(const Matrix &transformation);

		// LookAt Methods
		void LookAt(GameObject* GO);
		void LookAt(const Vec3 &Position);

		// Components - owned via shared_ptr (Lua + C++ share one refcount;
		// back-pointer Owner stays raw to avoid cycles - see SHARED_OWNERSHIP_PLAN.md).
		void Add(const std::shared_ptr<IComponent> &Component);
		void Remove(const std::shared_ptr<IComponent> &Component);
		void Remove(IComponent* Component);

		// Parent / children - same ownership model as components.
		void Add(const std::shared_ptr<GameObject> &Child);
		void Remove(const std::shared_ptr<GameObject> &Child);
		void Remove(GameObject* Child);
		GameObject* GetParent() { return _Owner; }
		bool HaveParent() { return _HaveOwner; }
		// The scene this object is a root of, or NULL. Read-only: membership
		// is SceneGraph's to change, through Add()/Remove(). Exposed so the
		// back-pointer can be asserted on rather than only inferred.
		SceneGraph* GetScene() const { return Scene; }
		// The scene this object is in, through whatever it hangs from (NULL: none).
		SceneGraph* GetOwningScene() { return FindScene(); }
		// Says this object's place or size in the world was worked out again
		// (RenderState's moved log) - once between one reading of the log and the next.
		void NoteMovedOnce()
		{
			const uint32_t epoch = RenderState::ReadEpoch.load(std::memory_order_relaxed);
			if (_MovedNoted == epoch) return;
			_MovedNoted = epoch;
			RenderState::NoteMoved(this);
		}
		uint32_t _MovedNoted = 0;
		// Whether LookAt(GameObject*) is currently in force. Goes false on
		// its own if the target is destroyed.
		bool IsLookingAtGameObject() const { return _IsLookingAtGameObject; }
		const std::vector<std::shared_ptr<GameObject>> &GetChildren() const { return _Childs; }

		// Name - purely a label (editor display, save-file identification),
		// not enforced unique, unlike Tags which are a hashed multi-value bag.
		const std::string &GetName() const { return Name; }
		void SetName(const std::string &name) { Name = name; }

		// Tags
		void AddTag(const std::string &tag);
		void RemoveTag(const std::string &tag);
		bool HaveTag(const uint32 tag);
		bool HaveTag(const std::string &tag);
		const std::map<uint32, std::string> &GetTags() const { return TagsList; }

		// Transient: built at load from something else in the scene (a
		// foliage block from its terrain tile's layer), so it is never
		// written to a scene file - saving it would duplicate it on every
		// reload - and the editor does not list it.
		bool IsTransient() const { return transient; }
		void SetTransient(const bool t) { transient = t; }

		// Static
		bool IsStatic() { return isStatic; }

		// Helpers
		void AddComponent(const std::shared_ptr<IComponent> &Component);
		void AddGameObject(const std::shared_ptr<GameObject> &Child);
		void RemoveComponent(const std::shared_ptr<IComponent> &Component);
		void RemoveComponent(IComponent* Component);
		void RemoveGameObject(const std::shared_ptr<GameObject> &Child);
		void RemoveGameObject(GameObject* Child);
		void LookAtGameObject(GameObject* GO);
		void LookAtVec(const Vec3 &center);

		// Not transformed boundings
		const Vec3 &GetBoundingMinValue() const { return minBounds; }
		const Vec3 &GetBoundingMaxValue() const { return maxBounds; }
		const Vec3 &GetBoundingSphereCenter() const { return BoundingSphereCenter; }
		const float &GetBoundingSphereRadius() const { return BoundingSphereRadius; }

		// World Space boundings
		const Vec3 GetBoundingMinValueWorldSpace() const { return minBoundsWorldSpace; }
		const Vec3 GetBoundingMaxValueWorldSpace() const { return maxBoundsWorldSpace; }
		const float GetBoundingSphereRadiusWorldSpace() const { return BoundingSphereRadiusWorldSpace; }

		// Get Components List
		const std::vector<std::shared_ptr<IComponent>> &GetComponents() const { return Components; }


		// Recomputes this object's bounds from the components it holds.
		//
		// Add() aggregates a component's bounds once, when it is added, and
		// nothing recomputes them afterwards - so a component that REPLACES
		// its geometry later leaves the object testing the bounds of whatever
		// it was constructed with. CullingBoxTest tests the object's box, not
		// the mesh's, so the symptom is the whole object vanishing as soon as
		// its original bounds leave the frustum: a tilemap built over a
		// placeholder Plane kept a 2x2 box at the origin and drew only while
		// the origin was on screen.
		//
		// Any caller that swaps a component's geometry out from under it must
		// call this (RenderingComponent::AdoptGeneratedRenderable does).
		void RefreshComponentBounds();

	private:

		// Update Components
		void UpdateComponents(const f64 time = 0);

		// Internal Update
		bool InternalUpdate();

		// Properties
		Vec3 _Position;
		Vec3 _Rotation;
		Vec3 _Scale;

		// Thread and User Properties
		Matrix _LocalMatrixUserEntered;
		Matrix _WorldMatrix;
		Matrix _PrvWorldMatrix;
		bool _IsDirty, _IsUsingCustomMatrix;
		// The scene's walk leaves out an object that has had nothing to do for
		// a few frames, and with it everything under it that is the same.
		// _SubtreeAwake is false only when this and all below it are idle; an
		// awake object's ancestors are all awake.
		bool _SubtreeAwake = true;
		bool _StreamedIn = false;
		// The scene that has this object in one of its name watches (SceneGraph::Watch).
		SceneGraph* _WatchScene = NULL;
		uint8_t _IdleFrames = 0;
		uint8_t _SleptFrames = 0;
		// (the local boxes of this and everything under it, for the scene's
		// bounds while it is not walked)
		Vec3 _TreeMin, _TreeMax;

		// Local Transformation Matrix
		Matrix _LocalMatrix;
		Matrix _PrvLocalMatrix;

		// Looking At
		bool _IsLookingAtGameObject, _IsLookingAtPosition;
		GameObject* _IsLookingAtGameObjectPTR;
		Vec3 _IsLookingAtPositionVec;
		// Everyone currently pointing their _IsLookingAtGameObjectPTR at THIS
		// object. The other direction of the same link, kept purely so the
		// destructor can clear it - a look-at target is not owned by the
		// object watching it and can be destroyed at any time, and
		// UpdateTransformation() dereferences that pointer every single frame
		// the flag is set. Without this there is nothing to tell the watchers.
		std::vector<GameObject*> _LookedAtBy;
		// Both ends of the link in one place, so LookAt(), LookAt(Vec3) and
		// ~GameObject() cannot drift apart on which side they maintain.
		void StopLookingAtGameObject();

		// Components
		std::vector<std::shared_ptr<IComponent>> Components;

	protected:

		// Update Transformation. Rolls this object's previous-world matrix,
		// so call it once per frame per object. walkAncestors=true refreshes
		// the parent chain first (RefreshTransformation, objects outside the
		// traversal); the scene traversal passes false because it has just
		// updated the parent, and only re-walks if an ancestor went dirty
		// after that.
		bool UpdateTransformation(const uint32 order = 0, const bool walkAncestors = true);

	private:

		// Local matrix from position/rotation/scale (and look-at). Clears
		// the dirty flag; touches no world or previous-world matrix.
		bool UpdateLocalTransformation(const uint32 order);
		// Refreshed by hand since the traversal: the children still stand
		// where the old world matrix put them.
		bool _RefreshedLate = false;
		Matrix _DrawWorld, _DrawPrvWorld;
		Vec3 _DrawScale;
		f32 _DrawRadius = 0.f;
		bool _DrawTaken = false;
		int32 _DrawSlot = -1;
		// The world-space box, from the local one and the world matrix.
		void UpdateWorldBounds();
		// Brings this object's world matrix up to date with its ancestors'
		// WITHOUT rolling any previous-world matrix - those belong to the
		// once-per-frame UpdateTransformation of each object.
		void RefreshWorldChain();

	protected:

		// Static
		bool isStatic;
		bool transient = false;

		// Name
		std::string Name;

		// GameObject Owner
		GameObject* _Owner;
		bool _HaveOwner;

		// GameObject Childs
		std::vector<std::shared_ptr<GameObject>> _Childs;

		// Components Add/Removed
		bool _ComponentsChanged;

		// Register and Unregister
		void RegisterComponents(SceneGraph* Scene);

		void UnregisterComponents(SceneGraph* Scene);
		// Same, for this object and every descendant. A child is never in
		// the scene's own object lists - only roots are - so detaching or
		// removing a subtree has to walk it to take its components back out
		// of the scene's registries, or they keep being rendered and updated
		// after the objects owning them are gone.
		void UnregisterComponentsTree(SceneGraph* Scene);
		// The SceneGraph this object belongs to, directly or through its
		// parents. Only roots carry the Scene pointer, so a child has to ask
		// upwards.
		SceneGraph* FindScene();
		// Scene Pointer
		SceneGraph* Scene;

		// Tags
		std::map<uint32, std::string> TagsList;

		// Bounds of the Component
		f32 BoundingSphereRadius;
		Vec3 BoundingSphereCenter;
		Vec3 maxBounds;
		Vec3 minBounds;

		f32 BoundingSphereRadiusWorldSpace;
		Vec3 maxBoundsWorldSpace;
		Vec3 minBoundsWorldSpace;
		// What the world box above was last worked out from: with the same
		// matrix and the same local box it is still right, and most of a scene
		// never moves (UpdateWorldBounds).
		Matrix _BoundsFromWorld;
		Vec3 _BoundsFromMin, _BoundsFromMax;
		bool _BoundsKnown = false;
	};

};

#endif	/* GAMEOBJECT_H */
