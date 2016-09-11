//============================================================================
// Name        : VR_Distortion_Geometry.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Export and Import for VS shared lib projects
//============================================================================

#ifndef VR_DISTORTION_GEOMETRY_H
#define VR_DISTORTION_GEOMETRY_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#include <Pyros3D/Materials/Shaders/Shaders.h>
#include <Pyros3D/VR/VR_Shaders.h>
#include <openvr.h>

namespace p3d {

	class VR_Distortion_Material : public CustomShaderMaterial {

	public:

		VR_Distortion_Material(Shader *shader) : CustomShaderMaterial(shader)
		{
			int tex = 1;
			AddUniform(Uniforms::Uniform("mytexture", Uniforms::DataType::Int, &tex));
		}

		virtual void PreRender() {}
		virtual void Render() {}
		virtual void AfterRender() {}

	};

	class Distortion_Geometry : public IGeometry {

	public:

		Distortion_Geometry() {}

		std::vector<Vec2> tVertex;
		std::vector<Vec2> tTexcoordRed, tTexcoordGreen, tTexcoordBlue;

		void CreateBuffers(bool calculateTangentBitangent = false)
		{
			// Calculate Bounding Sphere Radius
			CalculateBounding();

			// Create and Set Attribute Buffer
			AttributeBuffer* Vertex = new AttributeBuffer(Buffer::Type::Attribute, Buffer::Draw::Static);
			Vertex->AddAttribute("aPosition", Buffer::Attribute::Type::Vec2, &tVertex[0], tVertex.size());
			Vertex->AddAttribute("aTexCoordRed", Buffer::Attribute::Type::Vec2, &tTexcoordRed[0], tTexcoordRed.size());
			Vertex->AddAttribute("aTexCoordGreen", Buffer::Attribute::Type::Vec2, &tTexcoordGreen[0], tTexcoordGreen.size());
			Vertex->AddAttribute("aTexCoordBlue", Buffer::Attribute::Type::Vec2, &tTexcoordBlue[0], tTexcoordBlue.size());

			// Add Buffer to Attributes Buffer List
			Attributes.push_back(Vertex);
		}

		virtual const std::vector<__INDEX_C_TYPE__> &GetIndexData() const
		{
			return index;
		}
		virtual const std::vector<Vec3> &GetVertexData() const
		{
			return std::vector<Vec3>();
		}
		virtual const std::vector<Vec3> &GetNormalData() const
		{
			return std::vector<Vec3>();
		}
		virtual void CalculateBounding() {}
	};

	class VR_Distortion_Geometry : public Primitive
	{
	public:

		Distortion_Geometry* geometry;

		VR_Distortion_Geometry(vr::IVRSystem* m_pHMD);
		virtual ~VR_Distortion_Geometry();
		VR_Distortion_Material* material;
		Shader* shader;
	};

};
#endif /*VR_DISTORTION_GEOMETRY_H*/