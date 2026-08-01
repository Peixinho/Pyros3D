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
			Custom
		};
	}

	class PYROS3D_API PrimitiveGeometry : public IGeometry {

	public:
		std::vector<Vec3> tVertex, tNormal, tTangent, tBitangent;
		std::vector<Vec2> tTexcoord;

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

		void CalculateBounding() {}
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

	protected:

		bool isSmooth;
		bool isFlipped;
		bool calculateTangentBitangent;
	};
};

#endif /* PRIMITIVE_H */
