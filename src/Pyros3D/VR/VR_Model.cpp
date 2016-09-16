//============================================================================
// Name        : VR_Model.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : VR Model
//============================================================================

#include <Pyros3D/VR/VR_Model.h>

namespace p3d {

	VR_Model::VR_Model(vr::RenderModel_t* model, Texture* texture)
	{
		geometry = new VR_Model_Geometry();

		for (uint32 i = 0; i <= model->unVertexCount; i++)
		{
			geometry->tVertex.push_back(Vec3(model->rVertexData[i].vPosition.v[0], model->rVertexData[i].vPosition.v[1], model->rVertexData[i].vPosition.v[2]));
			geometry->tNormal.push_back(Vec3(model->rVertexData[i].vNormal.v[0], model->rVertexData[i].vNormal.v[1], model->rVertexData[i].vNormal.v[2]));
			geometry->tTexcoord.push_back(Vec2(model->rVertexData[i].rfTextureCoord[0], model->rVertexData[i].rfTextureCoord[1]));
		}

		for (uint32 i = 0; i < model->unTriangleCount*3; i++)
			geometry->index.push_back(model->rIndexData[i]);

		geometry->CreateBuffers();
		geometry->SendBuffers();

		// Material Defaults
		geometry->materialProperties.haveColor = false;
		geometry->materialProperties.haveBones = false;
		geometry->materialProperties.haveSpecular = false;
		geometry->materialProperties.haveColorMap = true;
		geometry->materialProperties.haveSpecularMap = false;
		geometry->materialProperties.haveNormalMap = false;

		// Add To Geometry List
		Geometries.push_back(geometry);

		// Calculate Model's Bounding Box
		CalculateBounding();

		// Execute Parent Build
		BuildMaterials();

		GenericShaderMaterial* mat = (GenericShaderMaterial*)Geometries[0]->Material;
		mat->SetColorMap(texture);
	}

	void VR_Model_Geometry::CreateBuffers()
	{
		// Calculate Bounding Sphere Radius
		CalculateBounding();

		// Create and Set Attribute Buffer
		AttributeBuffer* Vertex = new AttributeBuffer(Buffer::Type::Attribute, Buffer::Draw::Static);
		Vertex->AddAttribute("aPosition", Buffer::Attribute::Type::Vec3, &tVertex[0], tVertex.size());
		Vertex->AddAttribute("aNormal", Buffer::Attribute::Type::Vec3, &tNormal[0], tNormal.size());
		Vertex->AddAttribute("aTexcoord", Buffer::Attribute::Type::Vec2, &tTexcoord[0], tTexcoord.size());

		// Add Buffer to Attributes Buffer List
		Attributes.push_back(Vertex);
	}
}