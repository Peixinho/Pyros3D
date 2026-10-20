//============================================================================
// Name        : Primitive.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Interface to Create Primitives Shapes
//============================================================================

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>

namespace p3d {

	void PrimitiveGeometry::CreateBuffers(bool calculateTangentBitangent)
	{
		// Calculate Bounding Sphere Radius
		CalculateBounding();

		// Create and Set Attribute Buffer
		AttributeBuffer* Vertex = new AttributeBuffer(Buffer::Type::Attribute, Buffer::Draw::Static);
		Vertex->AddAttribute("aPosition", Buffer::Attribute::Type::Vec3, &tVertex[0], tVertex.size());
		Vertex->AddAttribute("aNormal", Buffer::Attribute::Type::Vec3, &tNormal[0], tNormal.size());
		Vertex->AddAttribute("aTexcoord", Buffer::Attribute::Type::Vec2, &tTexcoord[0], tTexcoord.size());
		if (!tColor.empty() && tColor.size() == tVertex.size())
			Vertex->AddAttribute("aColor", Buffer::Attribute::Type::Vec4, &tColor[0], tColor.size());

		if (calculateTangentBitangent)
		{
			tTangent.resize(tVertex.size());
			tBitangent.resize(tVertex.size());
			for (uint32 i = 0; i < index.size(); i += 3)
			{
				Vec3 Binormal, Tangent;
				Geometry::CalculateTriangleTangentAndBinormal(tVertex[index[i]], tTexcoord[index[i]], tVertex[index[i+1]], tTexcoord[index[i+1]], tVertex[index[i+2]], tTexcoord[index[i+2]], &Binormal, &Tangent);
				tTangent[index[i]] = Tangent;
				tTangent[index[i+1]] = Tangent;
				tTangent[index[i+2]] = Tangent;
				tBitangent[index[i]] = Binormal;
				tBitangent[index[i+1]] = Binormal;
				tBitangent[index[i+2]] = Binormal;
			}

			Vertex->AddAttribute("aTangent", Buffer::Attribute::Type::Vec3, &tTangent[0], tTangent.size());
			Vertex->AddAttribute("aBitangent", Buffer::Attribute::Type::Vec3, &tBitangent[0], tBitangent.size());
		}

		// Add Buffer to Attributes Buffer List
		Attributes.push_back(Vertex);
	}

	void CalculateBounding() {}

	Primitive::Primitive() { 
		geometry = new PrimitiveGeometry(); 
		isFlipped = false; 
		isSmooth = false; 
		calculateTangentBitangent = false; 
	}

	void Primitive::TileTexcoords(const f32 metres)
	{
		if (metres <= 0.f || geometry == NULL || geometry->tVertex.size() != geometry->tNormal.size()) return;
		tileMetres = metres;
		std::vector<Vec2> &uv = geometry->tTexcoord;
		uv.resize(geometry->tVertex.size());
		for (size_t i = 0; i < geometry->tVertex.size(); i++)
		{
			const Vec3 &p = geometry->tVertex[i], &n = geometry->tNormal[i];
			const f32 ax = fabs(n.x), ay = fabs(n.y), az = fabs(n.z);
			// (u along the face, the way the default mapping runs it; v up on a wall, along z on a floor)
			if (ax >= ay && ax >= az) uv[i] = Vec2((n.x > 0.f ? -p.z : p.z) / metres, -p.y / metres);
			else if (ay >= az)        uv[i] = Vec2(p.x / metres, (n.y > 0.f ? p.z : -p.z) / metres);
			else                      uv[i] = Vec2((n.z > 0.f ? p.x : -p.x) / metres, -p.y / metres);
		}
		// (sent again with them: tangents follow the new coordinates)
		geometry->Dispose();
		geometry->CreateBuffers(calculateTangentBitangent);
		geometry->SendBuffers();
	}

	void Primitive::Build()
	{
		// Flip Normals
		if (isFlipped)
		{
			for (uint32 i = 0; i < geometry->tNormal.size(); i++)
			{
				geometry->tNormal[i].negateSelf();
			}
		}

		if (isSmooth)
		{
			if (geometry->tNormal.size()>0)
			{
				std::vector<Vec3> CopyNormals;
				for (uint32 i = 0; i < geometry->tNormal.size(); i++) {
					CopyNormals.push_back(geometry->tNormal[i]);
				}
				for (uint32 i = 0; i < geometry->tNormal.size(); i++) {
					for (uint32 j = 0; j < geometry->tNormal.size(); j++) {
						if (i != j && geometry->tVertex[i] == geometry->tVertex[j])
							geometry->tNormal[j] += CopyNormals[i];
					}
				}
				for (uint32 i = 0; i < geometry->tNormal.size(); i++) {
					geometry->tNormal[i].normalizeSelf();
				}
			}
		}

		// Create Attributes Buffers
		geometry->CreateBuffers(calculateTangentBitangent);
		// Send Buffers
		geometry->SendBuffers();

		// Material Defaults
		geometry->materialProperties.haveColor = true;
		geometry->materialProperties.haveBones = false;
		geometry->materialProperties.haveSpecular = false;
		geometry->materialProperties.haveColorMap = false;
		geometry->materialProperties.haveSpecularMap = false;
		geometry->materialProperties.haveNormalMap = false;
		geometry->materialProperties.Color = Vec4(1.0, 1.0, 1.0, 1.0);

		// Add To Geometry List
		Geometries.push_back(geometry);

		// Calculate Model's Bounding Box
		CalculateBounding();
	}
};