//============================================================================
// Name        : Text.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Text
//============================================================================

#ifndef TEXT_H
#define	TEXT_H

#include <Pyros3D/Assets/Renderable/Renderables.h>
#include <Pyros3D/Core/Buffers/GeometryBuffer.h>
#include <Pyros3D/Assets/Font/Font.h>
#include <Pyros3D/Other/Export.h>
#include <vector>

namespace p3d {

	class PYROS3D_API TextGeometry : public IGeometry {

	public:
		std::vector<Vec3> tVertex, tNormal;
		std::vector<Vec2> tTexcoord;

		TextGeometry(bool DynamicText = false) : IGeometry((DynamicText ? GeometryType::ARRAY : GeometryType::BUFFER)) {}

		void CreateBuffers()
		{
			// Calculate Bounding Sphere Radius
			CalculateBounding();

			AttributeArray* Vertex;

			if (Type == GeometryType::BUFFER)
				// Create and Set Attribute Buffer
				Vertex = new AttributeBuffer(Buffer::Type::Attribute, Buffer::Draw::Static);
			else
				Vertex = new AttributeArray();

			Vertex->AddAttribute("aPosition", Buffer::Attribute::Type::Vec3, &tVertex[0], tVertex.size());
			Vertex->AddAttribute("aNormal", Buffer::Attribute::Type::Vec3, &tNormal[0], tNormal.size());
			Vertex->AddAttribute("aTexcoord", Buffer::Attribute::Type::Vec2, &tTexcoord[0], tTexcoord.size());
			// Add Buffer to Attributes Buffer List
			Attributes.push_back(Vertex);
		}

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

		void CalculateBounding()
		{
			// Bounding Box
			for (uint32 i = 0; i < tVertex.size(); i++)
			{
				if (i == 0) {
					minBounds = tVertex[i];
					maxBounds = tVertex[i];
				}
				else {
					if (tVertex[i].x < minBounds.x) minBounds.x = tVertex[i].x;
					if (tVertex[i].y < minBounds.y) minBounds.y = tVertex[i].y;
					if (tVertex[i].z < minBounds.z) minBounds.z = tVertex[i].z;
					if (tVertex[i].x > maxBounds.x) maxBounds.x = tVertex[i].x;
					if (tVertex[i].y > maxBounds.y) maxBounds.y = tVertex[i].y;
					if (tVertex[i].z > maxBounds.z) maxBounds.z = tVertex[i].z;
				}
			}
			// Bounding Sphere
			BoundingSphereCenter = maxBounds - minBounds;
			f32 a = maxBounds.distance(BoundingSphereCenter);
			f32 b = minBounds.distance(BoundingSphereCenter);
			BoundingSphereRadius = (a > b ? a : b);
		}
	};

	class PYROS3D_API Text : public Renderable {
	public:

		TextGeometry* geometry;

		Text(Font* font, const std::string& text, const f32 charWidth, const f32 charHeight, const Vec4 &color = Vec4(1, 1, 1, 1), bool DynamicText = false);
		Text(Font* font, const std::string& text, const f32 charWidth, const f32 charHeight, const std::vector<Vec4> &color, bool DynamicText = false);

		virtual ~Text();

		void Build()
		{
			// Create Attributes Buffers
			geometry->CreateBuffers();
			// Send Buffers
			geometry->SendBuffers();

			// Clean Geometries List
			Geometries.clear();

			// Add To Geometry List
			Geometries.push_back(geometry);

			// Calculate Model's Bounding Box
			CalculateBounding();
		};

		void UpdateText(const std::string &text, const Vec4 &color = Vec4(1, 1, 1, 1));
		void UpdateText(const std::string &text, const std::vector<Vec4> &color);

	private:

		// Char Dimensions
		f32 charWidth, charHeight;
		Font* font;

		// String
		std::string text;

		// Initialized Flag
		bool Initialized;
	};
};

#endif	/* TEXT_H */