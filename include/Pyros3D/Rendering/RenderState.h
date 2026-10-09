//============================================================================
// Name        : RenderState.h
// Description : What a renderer needs to know has changed, without looking
//============================================================================

#ifndef RENDERSTATE_H
#define RENDERSTATE_H

#include <Pyros3D/Other/Export.h>
#include <atomic>
#include <cstdint>
#include <vector>

namespace p3d {

	class GameObject;

	// A renderer keeps the list of what a scene draws from one frame to the
	// next (IRenderer::FrameList) instead of asking every mesh again each
	// frame what it is and where. Two things tell it when that list is out
	// of date:
	//
	//  Version - goes up when WHAT is drawn, or how, may have changed: a mesh
	//    entering or leaving a scene, a level of detail switching, a
	//    component enabled or disabled, a material made see-through, a tag.
	//    Anything that changes one of those says so (Touch()); the list is
	//    then made again from scratch, which is what every frame used to do.
	//
	//  The moved log - every object whose place in the world was worked out
	//    again, in order. A renderer takes what has been added since it last
	//    looked and puts those objects' spheres right; nothing else is read.
	//
	// Erring is always toward saying "changed".
	class RenderingMesh;
	class RenderingComponent;
	class SceneGraph;
	struct PYROS3D_API RenderState
	{
		static std::atomic<uint32_t> Version;
		// (PYROS_TRACE_TOUCH=1 prints who has been calling it, now and then.)
		static void Touch();

		// (any thread: a streamed cell is put together on a worker)
		static void NoteMoved(GameObject* object);
		// Goes up each time the log is read: an object already noted since the
		// last reading need not be noted again (GameObject keeps the number).
		static std::atomic<uint32_t> ReadEpoch;
		// How many have ever been noted; MovedSince copies those after `seq`
		// into out and returns false if some of them are no longer kept (the
		// caller then has to assume everything moved).
		static uint64_t MovedCount();

		// The listed log - a mesh put on a scene's list of what it draws, or
		// taken off it, and a rendering component joining or leaving a scene.
		// A renderer puts those into the list it keeps, in place, where it
		// used to make the whole list again (Touch) for every door a game
		// built and every level of detail that changed.
		struct Listed
		{
			// MeshOn: put at the end of the scene's list. MeshOff: taken out of
			// it, what came after closing up. MeshSwap: `other` put in the
			// place `mesh` had.
			enum { MeshOn = 1, MeshOff = 2, MeshSwap = 3, ComponentOn = 4, ComponentOff = 5 };
			RenderingMesh* mesh;
			RenderingMesh* other;
			RenderingComponent* component;
			GameObject* owner;                  // (the component's, as it was then: a name to look things up by, never followed)
			SceneGraph* scene;
			uint32_t what;
		};
		static void NoteListed(const uint32_t what, SceneGraph* scene, RenderingComponent* component, GameObject* owner, RenderingMesh* mesh = 0, RenderingMesh* other = 0);
		static uint64_t ListedCount();
		// Those after `seq` into out; false if some of them are no longer kept
		// (the caller then makes its list again).
		static bool ListedSince(const uint64_t seq, std::vector<Listed> &out);
		static bool MovedSince(const uint64_t seq, std::vector<GameObject*> &out);
	};

}

#endif /* RENDERSTATE_H */
