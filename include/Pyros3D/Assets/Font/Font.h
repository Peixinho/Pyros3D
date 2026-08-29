//============================================================================
// Name        : Font.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Font
//============================================================================

#ifndef FONT_H
#define FONT_H

#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Other/Export.h>
#include <memory>
#define generic GenericFromFreeTypeLibrary
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#undef generic

#define MAP_SIZE 1024

namespace p3d {

	struct glyph_properties
	{
		glyph_properties() : advance(0.f) {}

		// offset.x is the glyph's LEFT SIDE BEARING and offset.y how far
		// its ink drops below the baseline - both in pixels, both from
		// FreeType's control box.
		Vec2 offset;
		Vec2 size;
		Vec2 startingPoint;
		// The font's own designed advance for this glyph: how far the pen
		// moves to the next character, which is not the same as the ink
		// width plus a bearing. Text used to step by (width + bearing),
		// which is why text set with this renderer looked evenly but
		// wrongly spaced - the left bearing ended up as a gap AFTER each
		// glyph instead of before it.
		f32 advance;
	};

	class PYROS3D_API Font {

	private:

		// Font Path
		std::string font;

		f32 fontSize;

		// Font Map
		std::shared_ptr<Texture> glyphMap;

		// Glyph Map Data
		uchar glyphMapData[MAP_SIZE*MAP_SIZE];

		// Font Glyphs Properties
		std::map<char, glyph_properties> glyphs;

		// Last Glyph Pixel Position
		uint32 lastGlyphWidth, lastGlyphRow;

		// Free Type Specifics
		FT_Library ft;
		FT_Face face;

		// From Memory
		std::vector<uchar> memory;

		// See GetSpaceAdvance()/GetLineHeight()/GetAscender().
		f32 spaceAdvance;
		f32 lineHeight;
		f32 ascender, descender;

	public:

		// Create Font
		Font(const std::string &font, const f32 size);

		// Create Text
		// It adds each char to the texture
		void CreateText(const std::string &text);

		virtual ~Font();

		// Observing raw for draw paths; Font keeps the owning shared_ptr.
		Texture* GetTexture() { return glyphMap.get(); }
		const std::shared_ptr<Texture> &GetTextureShared() const { return glyphMap; }

		// Not exposed before this - the constructor's path was stored in
		// the private `font` member (used later by CreateText()) but
		// never readable back, same as Model/Texture were before Phase 1
		// of scene serialization added their path getters.
		const std::string &GetPath() const { return font; }

		f32 GetFontSize();

		// The font's own metrics, so callers do not have to invent them.
		// A space has no bitmap, so it never reaches the glyph map at all
		// and its advance has to be carried separately; line height is the
		// designed leading, not a multiple of whatever the tallest glyph
		// seen so far happened to be.
		f32 GetSpaceAdvance() const { return spaceAdvance; }
		f32 GetLineHeight() const { return lineHeight; }
		// Above and below the baseline, in pixels. Descender is negative.
		// These describe the box the font was DESIGNED to occupy, which is
		// what text should be aligned by - aligning by the ink instead
		// makes "PYROS3D" and "Play" sit at different heights in the same
		// row, because one has no descenders and the other does.
		f32 GetAscender() const { return ascender; }
		f32 GetDescender() const { return descender; }

		std::map<char, glyph_properties> GetGlyphs();
	};

};

#endif /* FONT_H */