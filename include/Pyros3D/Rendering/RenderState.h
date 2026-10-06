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
		static bool MovedSince(const uint64_t seq, std::vector<GameObject*> &out);
	};

}

#endif /* RENDERSTATE_H */
