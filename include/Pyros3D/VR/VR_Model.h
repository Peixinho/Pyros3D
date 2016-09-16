#ifndef VR_MODEL_H
#define VR_MODEL_H

//============================================================================
// Name        : VR_Model.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : VR Model
//============================================================================

#include <Pyros3D/Assets/Renderable/Renderables.h>
#include <openvr.h>

namespace p3d {

	class VR_Model_Geometry : public IGeometry {

		public:
			VR_Model_Geometry() {}
		
			std::vector<Vec3> tVertex, tNormal;
			std::vector<Vec2> tTexcoord;

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
			void CalculateBounding() {}

			void CreateBuffers();

	};

	class VR_Model : public Renderable {
		public:
			VR_Model(vr::RenderModel_t* model, Texture* texture);
			
			VR_Model_Geometry* geometry;

	};

}

#endif /* VR_SHADERS_H */