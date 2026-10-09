//============================================================================
// Name        : PathGrid.h
// Description : Ways across a grid of cells, found on the job system.
//============================================================================

#ifndef PATHGRID_H
#define PATHGRID_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace p3d {

	// A square grid, one number to a cell: 0 cannot be crossed, anything else
	// is what the cell costs to cross (1 the cheapest). A way between two
	// cells is the cheapest one stepping to any of a cell's eight neighbours -
	// not diagonally across the corner between two cells that cannot be
	// crossed - and is found with A*.
	//
	// Request() hands the search to the job system and returns at once;
	// Poll() says when it is done. So a crowd asking the way costs the frame
	// that asked nothing: a search of a large map is tens of thousands of
	// cells, and a script doing it a few hundred cells a frame was most of
	// what the scripts of a game with twenty walkers spent.
	class PYROS3D_API PathGrid {
	public:
		// `cost`: width * height numbers, row by row.
		PathGrid(const uint32 width, const uint32 height, const std::vector<uint8> &cost);
		~PathGrid();

		uint32 Width() const { return w; }
		uint32 Height() const { return h; }
		uint32 Cost(const int32 i, const int32 j) const;

		enum { Pending = 0, Found = 1, NoWay = 2, Unknown = 3 };
		// From cell (si, sj) to cell (ti, tj), giving up once `maxCells` have
		// been looked at. A ticket for Poll and Cancel.
		uint32 Request(const int32 si, const int32 sj, const int32 ti, const int32 tj, const uint32 maxCells = 60000);
		// Found: `cells` is the way, start to goal, each cell as j * width + i,
		// and the ticket is spent. NoWay: spent too. Pending: ask again.
		uint32 Poll(const uint32 ticket, std::vector<uint32> &cells);
		void Cancel(const uint32 ticket);

		// The same search, now, on the calling thread.
		static bool Find(const std::vector<uint8> &cost, const uint32 width, const uint32 height,
			const int32 si, const int32 sj, const int32 ti, const int32 tj, const uint32 maxCells, std::vector<uint32> &cells);

	private:
		struct Answer { std::mutex lock; uint32 state = Pending; std::vector<uint32> cells; };
		uint32 w, h;
		std::shared_ptr<const std::vector<uint8> > costs;          // (shared with the searches under way: they may outlive the grid)
		std::unordered_map<uint32, std::shared_ptr<Answer> > asked;
		uint32 lastTicket = 0;
	};

}

#endif /* PATHGRID_H */
