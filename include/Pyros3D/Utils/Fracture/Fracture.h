//============================================================================
// Name        : Fracture.h
// Description : Breaking a mesh into pieces at run time: Voronoi cells round
//               points scattered through it - thicker near where it was hit -
//               each the part of the mesh inside its cell, closed where it was
//               cut. A piece keeps the faces it had (their materials and
//               texture coordinates) and gets new ones of the source's first
//               material over the cut. Exact for a convex mesh (a crate, a
//               wall, a pane, a column); a hollow one gets caps over its
//               hollows.
//============================================================================
#ifndef FRACTURE_H
#define FRACTURE_H

#include <Pyros3D/Assets/Renderable/Renderables.h>
#include <Pyros3D/Assets/Renderable/Models/Model.h>
#include <Pyros3D/Other/Export.h>
#include <memory>
#include <vector>

namespace p3d {

	// A piece's mesh: one geometry per source geometry that has faces in it, the
	// cut's faces in the first of them (so they take that material).
	class PYROS3D_API FracturedPiece : public Renderable {
	public:
		FracturedPiece() {}
		virtual ~FracturedPiece() {}
		void Finish() { CalculateBounding(); }
		// Which source geometry each of Geometries came from (a material per source).
		std::vector<uint32> SourceGeometry;
	};

	namespace Fracture {
		struct Piece
		{
			std::shared_ptr<FracturedPiece> mesh;   // its vertices about its own centre
			Vec3 centre;                            // where that centre is, in the space the source was given in
			std::vector<Vec3> hull;                 // its points about the centre, for a convex body
			f32 volume = 0.f;                       // (of the hull) for its share of the mass
		};
		// The source's geometries (a Model's or a Primitive's), each vertex moved by `world`
		// first - the pieces are in that space - broken into `count` pieces, the cells packed
		// round `impact` (the same space). `seed` makes it repeatable.
		PYROS3D_API std::vector<Piece> Split(const Renderable &source, const Matrix &world, const uint32 count,
			const Vec3 &impact, const uint32 seed = 1);
	}
}

#endif /* FRACTURE_H */
