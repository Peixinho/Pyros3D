//============================================================================
// Name        : Primitive.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Interface to Create Primitives Shapes
//============================================================================

#ifndef PRIMITIVE_H
#define PRIMITIVE_H

#include <Pyros3D/Assets/Renderable/Renderables.h>
#include <Pyros3D/Core/Buffers/GeometryBuffer.h>
#include <Pyros3D/Utils/Geometry/Geometry.h>
#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Other/Global.h>
#include <vector>

namespace p3d {

	// Real type tag + the 3 flags every shape ctor already takes -
	// needed so generic code (scene serialization) can identify which
	// concrete shape a Primitive* is and read back its construction
	// params (otherwise fully discarded after Build() runs - these
	// classes are construct-once with zero introspection today).
	namespace PrimitiveType {
		enum {
			Cube = 0,
			Sphere,
			Cone,
			Cylinder,
			Plane,
			Capsule,
			Torus,
			TorusKnot,
			GrassTuft,
			Tree,
			Custom
		};
	}

	class PYROS3D_API PrimitiveGeometry : public IGeometry {

	public:
		std::vector<Vec3> tVertex, tNormal, tTangent, tBitangent;
		std::vector<Vec2> tTexcoord;
		// A colour for each vertex (aColor: ShaderUsage::VertexColor). Left empty, the
		// geometry has no such attribute - every shape but the ones that fill it.
		std::vector<Vec4> tColor;

		void CreateBuffers(bool calculateTangentBitangent = false);

		virtual const std::vector<__INDEX_C_TYPE__> &GetIndexData() const
		{
			return index;
		}
		virtual const std::vector<Vec3> &GetVertexData() const
		{
			return tVertex;
		}
		virtual const std::vector<Vec3> &GetNormalData() const
		{
			return tNormal;
		}

	protected:

		// Its box and sphere, from its vertices (it was empty: every primitive's geometry said a sphere of
		// radius 0 at the origin - the shape's own Primitive kept the real one)
		void CalculateBounding();
	};

	class PYROS3D_API Primitive : public Renderable {

	public:

		PrimitiveGeometry* geometry;

		Primitive();

		void Build();

		virtual uint32 GetPrimitiveType() const { return PrimitiveType::Custom; }
		bool IsSmooth() const { return isSmooth; }
		bool IsFlipped() const { return isFlipped; }
		bool HasTangentBitangent() const { return calculateTangentBitangent; }

		// The picture repeated every `metres` along the surface (0: once a face, as made) - a wall
		// of tiles, a floor of planks, at their real size whatever the primitive's: texture
		// coordinates projected from each vertex's place, on the plane its normal faces most.
		void TileTexcoords(const f32 metres);
		f32 GetTileMetres() const { return tileMetres; }

	protected:

		f32 tileMetres = 0.f;

		bool isSmooth;
		bool isFlipped;
		bool calculateTangentBitangent;
	};
};

#endif /* PRIMITIVE_H */
