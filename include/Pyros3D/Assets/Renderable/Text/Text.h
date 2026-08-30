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

		TextGeometry() : IGeometry() {}

		void CreateBuffers()
		{
			// Calculate Bounding Sphere Radius
			CalculateBounding();

			// An empty string is a legitimate label ("", or a value that
			// has not arrived yet), and it produces no quads at all -
			// at which point &tVertex[0] below is indexing an empty
			// vector, which crashed outright.
			if (tVertex.empty()) return;

			AttributeArray* Vertex;

			// Create and Set Attribute Buffer
			Vertex = new AttributeBuffer(Buffer::Type::Attribute, Buffer::Draw::Static);

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

		// Resize the glyph quads in place. charWidth/charHeight were
		// constructor-only, so changing a text's size meant throwing the
		// whole Renderable away and rebuilding every RenderingComponent
		// pointing at it - fine for a one-off, useless for a UI label whose
		// size is a style property.
		void SetCharSize(const f32 charWidth, const f32 charHeight);

		// Break lines at word boundaries so the text fits this width, in the
		// same units the mesh is built in (so: canvas units for a UIText).
		// Zero, the default, means no wrapping at all - explicit newlines
		// only, which is exactly what this did before.
		void SetWrapWidth(const f32 width);

		// Re-lays the text against a different Font. The glyph metrics and
		// the atlas both come from it, so this is a full rebuild - which is
		// why it exists at all rather than callers throwing the Renderable
		// away: everything pointing at this mesh keeps pointing at it.
		void SetFont(Font* newFont);
		f32 GetWrapWidth() const { return wrapWidth; }

		// Real getters - neither constructor nor either UpdateText()
		// overload stored any of this before (the color arg(s) only ever
		// fed per-vertex mesh-build math, then were discarded - same
		// class of dead-field bug as GenericShaderMaterial::SetColor's
		// Kd from the Phase 1 serialization work). Needed for scene
		// serialization to read a Text object's actual current state.
		Font* GetFont() const { return font; }
		const std::string &GetText() const { return text; }
		// The typeset width of the widest line and how many lines there
		// are, both in the same units as the mesh. Measured from the pen,
		// not from the ink - so a leading space or a glyph with a wide
		// bearing still reports the width the text actually occupies,
		// which is what alignment has to use.
		f32 GetAdvanceWidth() const { return advanceWidth; }
		uint32 GetLineCount() const { return lineCount; }
		f32 GetCharWidth() const { return charWidth; }
		f32 GetCharHeight() const { return charHeight; }
		// Valid when charColors is empty (i.e. the single-color
		// constructor/UpdateText overload was used).
		const Vec4 &GetColor() const { return color; }
		// Non-empty only when the per-character-color overload was used.
		const std::vector<Vec4> &GetCharColors() const { return charColors; }

	private:

		// Char Dimensions
		f32 charWidth, charHeight;
		Font* font;

		// String
		std::string text;

		// See GetColor()/GetCharColors()'s comments - set unconditionally
		// at the top of both UpdateText() overloads (even when the
		// text-unchanged early-out skips a real mesh rebuild), so these
		// always reflect the most recent call's color argument(s).
		Vec4 color;
		std::vector<Vec4> charColors;

		// See GetAdvanceWidth()/GetLineCount().
		f32 advanceWidth = 0.f;
		uint32 lineCount = 1;

		// See SetWrapWidth().
		f32 wrapWidth = 0.f;

		// Initialized Flag
		bool Initialized;
	};
};

#endif	/* TEXT_H */
