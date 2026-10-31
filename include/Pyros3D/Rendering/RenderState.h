//============================================================================
// Name        : RenderState.h
// Description : What something that keeps a list of what a scene draws needs
//               to know has changed, without looking
//============================================================================

#ifndef RENDERSTATE_H
#define RENDERSTATE_H

#include <Pyros3D/Other/Export.h>
#include <atomic>
#include <cstdint>

namespace p3d {

	class GameObject;

	// Two counts, for what keeps a list of what is beneath an object - a sprite batch, its merged mesh - and wants to
	// know, in two comparisons rather than a walk of the tree, that its list is still right:
	//
	//  Structure - goes up when the tree changes shape (a child added or taken away, a component added or removed)
	//    or what is drawn may have changed (Touch: a component enabled or disabled, a mesh listed or unlisted, a level
	//    of detail switched, a layer changed). Erring is toward saying "changed".
	//
	//  Moved - goes up each time an object's place or size in the world is worked out again (NoteMoved).
	struct PYROS3D_API RenderState
	{
		static std::atomic<uint64_t> Structure;
		static std::atomic<uint64_t> Moved;
		static void Touch() { Structure.fetch_add(1, std::memory_order_relaxed); }
		// (any thread: a streamed cell is put together on a worker)
		static void NoteMoved() { Moved.fetch_add(1, std::memory_order_relaxed); }
		static uint64_t MovedCount() { return Moved.load(std::memory_order_relaxed); }
	};

}

#endif /* RENDERSTATE_H */
