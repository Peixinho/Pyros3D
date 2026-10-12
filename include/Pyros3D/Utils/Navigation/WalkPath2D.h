//============================================================================
// Name        : WalkPath2D.h
// Description : The way across a side-on grid for something that WALKS: it
//               stands on solid cells, steps along them, jumps up and across
//               and drops down - a platformer's enemy, a companion, a
//               creature that has to get to the player and cannot fly there.
//
//               PathGrid answers the other question, the one a top-down game
//               asks: every open cell is somewhere to be. Here an open cell
//               is only somewhere to be if there is ground under it, and the
//               ways between two such cells are the moves a walker has.
//============================================================================

#ifndef WALKPATH2D_H
#define WALKPATH2D_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>
#include <functional>
#include <vector>

namespace p3d {

	struct PYROS3D_API WalkPathOptions2D {
		// How many cells up it can jump, and how many across (a gap it can
		// clear, or how far forward a jump or a fall carries it).
		int32 jumpHeight = 3;
		int32 jumpDistance = 3;
		// The furthest it will fall. A drop is cheaper than a jump, so it
		// takes one where there is one.
		int32 maxDrop = 8;
		// How many cells tall it is: that many must be clear above where it
		// stands, and along the line of a jump.
		int32 height = 1;
		// Gives up once this many cells have been looked at.
		uint32 maxCells = 20000;
	};

	namespace WalkMove2D { enum { Start = 0, Walk = 1, Jump = 2, Drop = 3 }; }

	// One cell of the way, and how it was got to from the one before.
	struct PYROS3D_API WalkStep2D { int32 x = 0, y = 0; uint8 move = WalkMove2D::Start; };

	// `solid(x, y)` says whether a cell is ground; y runs UP. The search stays
	// inside the rectangle given (inclusive). `from` and `to` are where the
	// walker and its goal ARE, which need not be on the ground: each is
	// brought down to the first place below it that can be stood on. False
	// when there is no such place, or no way between them.
	PYROS3D_API bool FindWalkPath2D(const std::function<bool(int32, int32)> &solid,
		const int32 fromX, const int32 fromY, const int32 toX, const int32 toY,
		const int32 minX, const int32 minY, const int32 maxX, const int32 maxY,
		const WalkPathOptions2D &options, std::vector<WalkStep2D> &out);

}

#endif /* WALKPATH2D_H */
