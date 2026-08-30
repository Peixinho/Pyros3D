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

	// Inserts newlines so no line exceeds `width`. Deliberately a
	// pre-processing pass over the string rather than a change to the glyph
	// loop below: the loop already handles '\n', and a wrapped line is just
	// a line. Measuring uses the same advances the loop steps by, so what is
	// measured is what gets drawn.
	//
	// A single word longer than the width is broken mid-word rather than
	// allowed to overflow - a label that spills out of its own rect is worse
	// than one that hyphenates badly, and the alternative is silently
	// ignoring the width the caller asked for.
	// srcIndex, when given, receives the index into `text` that each
	// character of the result came from. The per-character-colour overload
	// needs it: inserting newlines shifts every later position, so indexing
	// the colour list by the laid-out position would recolour the text from
	// the first wrap onwards.
	static std::string WrapToWidth(Font* font, const std::string &text, const f32 scale, const f32 width,
		std::vector<uint32>* srcIndex = NULL)
	{
		if (!font || width <= 0.f || text.empty())
		{
			if (srcIndex)
			{
				srcIndex->resize(text.size());
				for (size_t k = 0; k < text.size(); k++) (*srcIndex)[k] = (uint32)k;
			}
			return text;
		}
		const std::map<char, glyph_properties> glyphs = font->GetGlyphs();
		const f32 spaceAdvance = font->GetSpaceAdvance() * scale;

		auto advanceOf = [&](const char c) -> f32 {
			if (c == ' ') return spaceAdvance;
			std::map<char, glyph_properties>::const_iterator g = glyphs.find(c);
			return (g == glyphs.end()) ? 0.f : g->second.advance * scale;
		};

		std::string out;
		out.reserve(text.size() + 8);
		if (srcIndex) { srcIndex->clear(); srcIndex->reserve(text.size() + 8); }
		// An inserted newline has no source character; it reuses the last
		// real one, which nothing reads (the newline branch of the glyph
		// loop does not touch colours).
		auto emit = [&](const char c, const size_t from) {
			out += c;
			if (srcIndex) srcIndex->push_back((uint32)from);
		};
		f32 lineWidth = 0.f;
		size_t i = 0;
		while (i < text.size())
		{
			if (text[i] == '\n') { emit('\n', i); lineWidth = 0.f; i++; continue; }
			if (text[i] == ' ')
			{
				// A space at the start of a wrapped line is dropped, so the
				// next line begins at the margin rather than indented by one.
				if (lineWidth > 0.f) { emit(' ', i); lineWidth += spaceAdvance; }
				i++;
				continue;
			}

			size_t end = i;
			f32 wordWidth = 0.f;
			while (end < text.size() && text[end] != ' ' && text[end] != '\n')
			{
				wordWidth += advanceOf(text[end]);
				end++;
			}

			if (lineWidth > 0.f && lineWidth + wordWidth > width)
			{
				// Drop the trailing space the break replaces.
				if (!out.empty() && out[out.size() - 1] == ' ')
				{
					out.erase(out.size() - 1);
					if (srcIndex) srcIndex->pop_back();
				}
				emit('\n', i);
				lineWidth = 0.f;
			}

			if (wordWidth > width)
			{
				// Too long for a line of its own: break it wherever it runs out.
				for (size_t k = i; k < end; k++)
				{
					const f32 a = advanceOf(text[k]);
					if (lineWidth > 0.f && lineWidth + a > width) { emit('\n', k); lineWidth = 0.f; }
					emit(text[k], k);
					lineWidth += a;
				}
			}
			else
			{
				for (size_t k = i; k < end; k++) emit(text[k], k);
				lineWidth += wordWidth;
			}
			i = end;
		}
		return out;
	}

	void Text::SetFont(Font* newFont)
	{
		if (!newFont || newFont == font) return;
		font = newFont;
		// The new atlas has none of this string's glyphs yet.
		font->CreateText(text);
		// Same forced rebuild the other setters use - UpdateText's early-out
		// cannot see a font change.
		const std::string current = text;
		text.clear();
		if (charColors.empty()) UpdateText(current, color);
		else UpdateText(current, charColors);
	}

	void Text::SetWrapWidth(const f32 width)
	{
		if (this->wrapWidth == width) return;
		this->wrapWidth = width;
		// Same forced rebuild SetCharSize does, and for the same reason:
		// UpdateText's early-out cannot see this.
		const std::string current = text;
		text.clear();
		if (charColors.empty()) UpdateText(current, color);
		else UpdateText(current, charColors);
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
			// Reset per layout. Without this advanceWidth only ever grew,
			// because it is a running maximum - so every rebuild reported the
			// widest the text had EVER been, and lineCount was never assigned
			// at all. UIText aligns by both, so a label that got shorter
			// stayed aligned as though it had not.
			advanceWidth = 0.f;
			lineCount = 1;

			// The string as laid out, which is the one the caller gave plus
			// whatever newlines wrapping needed. `this->text` stays what was
			// set, so GetText() and serialization are unaffected by it.
			const std::string laid = WrapToWidth(font, this->text, charWidth / font->GetFontSize(), wrapWidth);
			for (uint32 i = 0; i<laid.size(); i++)
			{
				switch (laid[i])
				{
				case '\n':
					// The font's designed leading, not 1.5x the tallest
					// glyph seen so far - that made line spacing depend on
					// which characters happened to appear above.
					if (offsetX > advanceWidth) advanceWidth = offsetX;
					offsetY -= font->GetLineHeight();
					offsetX = 0.0f;
					lineCount++;
					break;
				case ' ':
					offsetX += font->GetSpaceAdvance();
					break;
				default:

					glyph_properties glp = font->GetGlyphs()[laid[i]];
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
			// Reset per layout. Without this advanceWidth only ever grew,
			// because it is a running maximum - so every rebuild reported the
			// widest the text had EVER been, and lineCount was never assigned
			// at all. UIText aligns by both, so a label that got shorter
			// stayed aligned as though it had not.
			advanceWidth = 0.f;
			lineCount = 1;

			// The string as laid out, which is the one the caller gave plus
			// whatever newlines wrapping needed. `this->text` stays what was
			// set, so GetText() and serialization are unaffected by it.
			std::vector<uint32> laidFrom;
			const std::string laid = WrapToWidth(font, this->text, charWidth / font->GetFontSize(), wrapWidth, &laidFrom);
			for (uint32 i = 0; i<laid.size(); i++)
			{
				switch (laid[i])
				{
				case '\n':
					// The font's designed leading, not 1.5x the tallest
					// glyph seen so far - that made line spacing depend on
					// which characters happened to appear above.
					if (offsetX > advanceWidth) advanceWidth = offsetX;
					offsetY -= font->GetLineHeight();
					offsetX = 0.0f;
					lineCount++;
					break;
				case ' ':
					offsetX += font->GetSpaceAdvance();
					break;
				default:

					glyph_properties glp = font->GetGlyphs()[laid[i]];
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
					// By source index, not by laid-out index - see WrapToWidth.
					// Clamped because a caller may pass fewer colours than
					// characters, which used to read past the end.
					const uint32 ci = (i < laidFrom.size()) ? laidFrom[i] : (uint32)i;
					const Vec4 cc = colors.empty() ? Vec4(1.f, 1.f, 1.f, 1.f)
						: colors[ci < colors.size() ? ci : colors.size() - 1];
					Vec3 normal = Vec3(cc.x, cc.y, cc.z);

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
