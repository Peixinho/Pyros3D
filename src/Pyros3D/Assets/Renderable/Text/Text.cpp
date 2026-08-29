//============================================================================
// Name        : Text.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Text
//============================================================================

#include <string>

#include <Pyros3D/Assets/Renderable/Text/Text.h>

namespace p3d {

	Text::Text(Font* font, const std::string& text, const f32 charWidth, const f32 charHeight, const Vec4 &color, bool DynamicText)
	{
		this->charWidth = charWidth;
		this->charHeight = charHeight;
		this->font = font;
		this->Initialized = false;

		geometry = new TextGeometry();

		// Generate Font
		font->CreateText(text);

		UpdateText(text, color);
	}

	Text::Text(Font* font, const std::string& text, const f32 charWidth, const f32 charHeight, const std::vector<Vec4> &colors, bool DynamicText)
	{
		this->charWidth = charWidth;
		this->charHeight = charHeight;
		this->font = font;
		this->Initialized = false;

		geometry = new TextGeometry();

		// Generate Font
		font->CreateText(text);

		UpdateText(text, colors);
	}

	void Text::SetCharSize(const f32 charWidth, const f32 charHeight)
	{
		if (this->charWidth == charWidth && this->charHeight == charHeight) return;
		this->charWidth = charWidth;
		this->charHeight = charHeight;
		// Both UpdateText() overloads early-out when nothing they can see
		// has changed, and neither of them can see charWidth/charHeight -
		// so the cached string has to be cleared for the rebuild at the
		// new size to actually happen.
		const std::string current = text;
		text.clear();
		if (charColors.empty()) UpdateText(current, color);
		else UpdateText(current, charColors);
	}

	void Text::UpdateText(const std::string &text, const Vec4 &color)
	{
		// Colour is per-vertex here (it rides in the normal attribute, see
		// below), so a colour change is a mesh change - the early-out has
		// to test for it too. It did not, so recolouring text in place
		// silently did nothing unless the string happened to change with
		// it, and callers worked around that by blanking the text first.
		const bool changed = (this->text != text) || (this->color != color) || !this->charColors.empty();
		this->color = color;
		this->charColors.clear();
		if (changed)
		{
			this->text = text;

			if (Initialized)
			{
				geometry->Dispose();
				geometry->index.clear();
				geometry->tVertex.clear();
				geometry->tNormal.clear();
				geometry->tTexcoord.clear();
			}

			Initialized = true;

			f32 width = 0.0f;
			f32 height = 0.0f;

			f32 offsetX = 0;
			f32 offsetY = 0;

			uint32 quads = 0;

			for (uint32 i = 0; i<text.size(); i++)
			{
				switch (text[i])
				{
				case '\n':
					// The font's designed leading, not 1.5x the tallest
					// glyph seen so far - that made line spacing depend on
					// which characters happened to appear above.
					offsetY -= font->GetLineHeight();
					offsetX = 0.0f;
					break;
				case ' ':
					offsetX += font->GetSpaceAdvance();
					break;
				default:

					glyph_properties glp = font->GetGlyphs()[text[i]];
					width = glp.size.x;
					height = glp.size.y;
					// Build Quads to the right
					f32 w2 = width; f32 h2 = height;


					// The left side bearing positions the ink; it is not
					// part of the step to the next glyph. Adding it to the
					// advance instead (which is what this did) turned every
					// bearing into a gap on the wrong side of its own
					// character, so text came out uniformly too loose and
					// letters with an unusual bearing sat visibly wrong.
					const f32 penX = offsetX + glp.offset.x;
					Vec3 a = Vec3(penX, offsetY - glp.offset.y, 0);
					Vec3 b = Vec3(w2 + penX, offsetY - glp.offset.y, 0);
					Vec3 c = Vec3(w2 + penX, h2 + offsetY - glp.offset.y, 0);
					Vec3 d = Vec3(penX, h2 + offsetY - glp.offset.y, 0);

					// Apply Dimensions
					a.x = charWidth * a.x / font->GetFontSize();
					a.y = charHeight * a.y / font->GetFontSize();

					b.x = charWidth * b.x / font->GetFontSize();
					b.y = charHeight * b.y / font->GetFontSize();

					c.x = charWidth * c.x / font->GetFontSize();
					c.y = charHeight * c.y / font->GetFontSize();

					d.x = charWidth * d.x / font->GetFontSize();
					d.y = charHeight * d.y / font->GetFontSize();

					Vec3 normal = Vec3(color.x, color.y, color.z);

					geometry->tVertex.push_back(a);   geometry->tNormal.push_back(normal);
					geometry->tVertex.push_back(b);   geometry->tNormal.push_back(normal);
					geometry->tVertex.push_back(c);   geometry->tNormal.push_back(normal);
					geometry->tVertex.push_back(d);   geometry->tNormal.push_back(normal);

					Texture* t = font->GetTexture();
					geometry->tTexcoord.push_back(Vec2(glp.startingPoint.x, glp.startingPoint.y + glp.size.y / (f32)t->GetHeight()));
					geometry->tTexcoord.push_back(Vec2(glp.startingPoint.x + glp.size.x / (f32)t->GetWidth(), glp.startingPoint.y + glp.size.y / (f32)t->GetHeight()));
					geometry->tTexcoord.push_back(Vec2(glp.startingPoint.x + glp.size.x / (f32)t->GetWidth(), glp.startingPoint.y));
					geometry->tTexcoord.push_back(Vec2(glp.startingPoint.x, glp.startingPoint.y));

					geometry->index.push_back(quads * 4 + 0);
					geometry->index.push_back(quads * 4 + 1);
					geometry->index.push_back(quads * 4 + 2);
					geometry->index.push_back(quads * 4 + 2);
					geometry->index.push_back(quads * 4 + 3);
					geometry->index.push_back(quads * 4 + 0);

					offsetX += glp.advance;
					quads++;

					break;
				}
			}

			if (offsetX > advanceWidth) advanceWidth = offsetX;
			// Reported in the mesh's own units, like everything else here.
			advanceWidth *= charWidth / font->GetFontSize();

			// Build and Send Buffers
			Build();
		}
	}

	void Text::UpdateText(const std::string &text, const std::vector<Vec4> &colors)
	{
		// See the single-color overload's identical comment.
		const bool changed = (this->text != text) || (this->charColors != colors);
		this->charColors = colors;
		if (changed)
		{
			this->text = text;

			if (Initialized)
			{
				geometry->Dispose();
				geometry->index.clear();
				geometry->tVertex.clear();
				geometry->tNormal.clear();
				geometry->tTexcoord.clear();
			}

			Initialized = true;

			f32 width = 0.0f;
			f32 height = 0.0f;

			f32 offsetX = 0;
			f32 offsetY = 0;

			uint32 quads = 0;

			for (uint32 i = 0; i<text.size(); i++)
			{
				switch (text[i])
				{
				case '\n':
					// The font's designed leading, not 1.5x the tallest
					// glyph seen so far - that made line spacing depend on
					// which characters happened to appear above.
					offsetY -= font->GetLineHeight();
					offsetX = 0.0f;
					break;
				case ' ':
					offsetX += font->GetSpaceAdvance();
					break;
				default:

					glyph_properties glp = font->GetGlyphs()[text[i]];
					width = glp.size.x;
					height = glp.size.y;
					// Build Quads to the right
					f32 w2 = width; f32 h2 = height;


					// The left side bearing positions the ink; it is not
					// part of the step to the next glyph. Adding it to the
					// advance instead (which is what this did) turned every
					// bearing into a gap on the wrong side of its own
					// character, so text came out uniformly too loose and
					// letters with an unusual bearing sat visibly wrong.
					const f32 penX = offsetX + glp.offset.x;
					Vec3 a = Vec3(penX, offsetY - glp.offset.y, 0);
					Vec3 b = Vec3(w2 + penX, offsetY - glp.offset.y, 0);
					Vec3 c = Vec3(w2 + penX, h2 + offsetY - glp.offset.y, 0);
					Vec3 d = Vec3(penX, h2 + offsetY - glp.offset.y, 0);

					// Apply Dimensions
					a.x = charWidth * a.x / font->GetFontSize();
					a.y = charHeight * a.y / font->GetFontSize();

					b.x = charWidth * b.x / font->GetFontSize();
					b.y = charHeight * b.y / font->GetFontSize();

					c.x = charWidth * c.x / font->GetFontSize();
					c.y = charHeight * c.y / font->GetFontSize();

					d.x = charWidth * d.x / font->GetFontSize();
					d.y = charHeight * d.y / font->GetFontSize();

					// Set Color
					Vec3 normal = Vec3(colors[i].x, colors[i].y, colors[i].z);

					geometry->tVertex.push_back(a);   geometry->tNormal.push_back(normal);
					geometry->tVertex.push_back(b);   geometry->tNormal.push_back(normal);
					geometry->tVertex.push_back(c);   geometry->tNormal.push_back(normal);
					geometry->tVertex.push_back(d);   geometry->tNormal.push_back(normal);

					Texture* t = font->GetTexture();
					geometry->tTexcoord.push_back(Vec2(glp.startingPoint.x, glp.startingPoint.y + glp.size.y / (f32)t->GetHeight()));
					geometry->tTexcoord.push_back(Vec2(glp.startingPoint.x + glp.size.x / (f32)t->GetWidth(), glp.startingPoint.y + glp.size.y / (f32)t->GetHeight()));
					geometry->tTexcoord.push_back(Vec2(glp.startingPoint.x + glp.size.x / (f32)t->GetWidth(), glp.startingPoint.y));
					geometry->tTexcoord.push_back(Vec2(glp.startingPoint.x, glp.startingPoint.y));

					geometry->index.push_back(quads * 4 + 0);
					geometry->index.push_back(quads * 4 + 1);
					geometry->index.push_back(quads * 4 + 2);
					geometry->index.push_back(quads * 4 + 2);
					geometry->index.push_back(quads * 4 + 3);
					geometry->index.push_back(quads * 4 + 0);

					offsetX += glp.advance;
					quads++;

					break;
				}
			}

			if (offsetX > advanceWidth) advanceWidth = offsetX;
			// Reported in the mesh's own units, like everything else here.
			advanceWidth *= charWidth / font->GetFontSize();

			// ReBuild and Send Buffers (VBOS)
			Build();
		
		}
	}

	Text::~Text()
	{

	}
};
