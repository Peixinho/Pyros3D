//============================================================================
// Name        : Font.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Font
//============================================================================


#include <Pyros3D/Assets/Font/Font.h>
#include <Pyros3D/Core/File/File.h>
#include <cstring>
#include <cstdio>

namespace p3d {

	Font::Font(const std::string& font, const f32 size, bool sdf)
	{
		isSDF = sdf;
		// Proportional to the bake size, not FreeType's fixed default of 8.
		// A spread has to be small next to the glyph's own features or there
		// is no interior left: at 16px with spread 8, a 2px stem lies
		// entirely inside the transition band, the field never rises past
		// the halfway value, and thresholding at 0.5 renders nothing at all.
		// One eighth of the em is comfortably inside a stem at any size.
		// Clamped to FreeType's own 2..32 range.
		sdfSpread = (uint32)(size / 8.f);
		if (sdfSpread < 2) sdfSpread = 2;
		if (sdfSpread > 32) sdfSpread = 32;
		// Font path
		this->font = font;

		// Font Size
		fontSize = size;

		// Create Texture
		glyphMap = std::make_shared<Texture>();
		glyphMap->CreateEmptyTexture(TextureType::Texture, TextureDataType::R8, MAP_SIZE, MAP_SIZE, true);
		glyphMap->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		glyphMap->SetTextureByteAlignment(1);

		File* file = new File();
		file->Open(font.c_str());

		memory.resize(file->Size());
		memcpy(&memory[0], &file->GetData()[0], sizeof(uchar)*file->Size());

		file->Close();
		delete file;

		// Free Type Initialization
		if (FT_Init_FreeType(&ft)) echo("ERROR: Couldn't Start Freetype Lib");
		if (isSDF)
		{
			// Must be set on the library before any glyph is rendered - the
			// sdf module reads it per render, and the padding maths below
			// assumes this exact value.
			FT_Int spreadProp = (FT_Int)sdfSpread;
			FT_Property_Set(ft, "sdf", "spread", &spreadProp);
		}
		if (FT_New_Memory_Face(ft, &memory[0], memory.size(), 0, &face)) echo("ERROR: Couldn't Load Font");
		if (FT_Set_Char_Size(face, 0, (FT_F26Dot6)(fontSize * 64), 300, 300)) echo("ERROR: Couldn't Set Char Size");
		if (FT_Set_Pixel_Sizes(face, 0, (FT_F26Dot6)fontSize)) echo("ERROR: Couldn't Set Pixel Size");

		// Zeroed before its first upload. This is a raw member array, so
		// every texel no glyph has written yet was whatever happened to be
		// on the stack/heap - and with linear filtering a glyph's edge
		// texels sample just outside its own rect, so that garbage showed
		// up as fringing around characters.
		memset(glyphMapData, 0, sizeof(glyphMapData));

		// Font metrics, read once. FreeType keeps these in 26.6 fixed point.
		FT_Load_Char(face, ' ', FT_LOAD_DEFAULT);
		spaceAdvance = (f32)(face->glyph->advance.x >> 6);
		lineHeight = (f32)(face->size->metrics.height >> 6);
		ascender = (f32)(face->size->metrics.ascender >> 6);
		descender = (f32)(face->size->metrics.descender >> 6);
		if (spaceAdvance <= 0.f) spaceAdvance = fontSize * 0.5f;
		if (lineHeight <= 0.f) lineHeight = fontSize * 1.2f;
		if (ascender <= 0.f) ascender = fontSize * 0.8f;
		if (descender >= 0.f) descender = -fontSize * 0.2f;

		glyphMap->UpdateData(glyphMapData);

		lastGlyphWidth = lastGlyphRow = 0;
	}

	f32 Font::MeasureAdvance(const std::string &text) const
	{
		f32 width = 0.f;
		for (size_t i = 0; i < text.size(); i++)
		{
			if (text[i] == ' ') { width += spaceAdvance; continue; }
			std::map<char, glyph_properties>::const_iterator g = glyphs.find(text[i]);
			if (g != glyphs.end()) width += g->second.advance;
		}
		return width;
	}

	std::map<char, glyph_properties> Font::GetGlyphs()
	{
		return glyphs;
	}

	void Font::CreateText(const std::string& text)
	{
		/*
			Get the bounding box
			The methods described by the FT tutorials are bad for getting accurate offsets for some reason.
			Thanks to Nuno Silva
		*/

		uint32 index = 0;
		for (uint32 i = 0; i < text.size(); i++)
		{
			switch (text[i])
			{
			case '\n':
			case ' ':
				// NONE
				break;
			default:

				if (glyphs.find(text[i]) == glyphs.end())
				{

					// Create Glyph
					FT_Glyph        glyph;

					FT_Load_Char(face, text[i], FT_LOAD_DEFAULT);

					FT_Get_Glyph(face->glyph, &glyph);

					if (glyph->format != FT_GLYPH_FORMAT_BITMAP)
					{
						const FT_Error rasterErr = FT_Glyph_To_Bitmap(&glyph, isSDF ? FT_RENDER_MODE_SDF : FT_RENDER_MODE_NORMAL, 0, 1);
						if (rasterErr != 0 && isSDF)
						{
							static bool warnedSDF = false;
							if (!warnedSDF)
							{
								warnedSDF = true;
								char buf[128];
								snprintf(buf, sizeof(buf), "WARNING: FreeType could not render an SDF glyph (error %d) - this font falls back to a coverage atlas", (int)rasterErr);
								echo(buf);
							}
						}
						if (rasterErr == 0) {
							FT_BitmapGlyph bitmap_glyph = (FT_BitmapGlyph)glyph;
							FT_Bitmap& bitmap = bitmap_glyph->bitmap;

							// Get Bounding Box of each Glyph
							FT_BBox BBox;
							FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_PIXELS, &BBox);
							glyph_properties glp;
							// An SDF bitmap is the ink grown by `spread` on
							// every side, so the quad has to grow with it and
							// start that much earlier - otherwise every glyph
							// renders inset and shifted by the spread, which
							// looks like bad kerning rather than like a bug.
							const f32 pad = isSDF ? (f32)sdfSpread : 0.f;
							glp.offset = Vec2((f32)BBox.xMin - pad, (f32)-BBox.yMin + pad);
							glp.size = Vec2((f32)bitmap.width, (f32)bitmap.rows);
							// See glyph_properties::advance.
							glp.advance = (f32)(face->glyph->advance.x >> 6);

							const uint32 cell = (uint32)fontSize + (isSDF ? sdfSpread * 2 : 0);
							if (lastGlyphWidth + cell > MAP_SIZE)
							{
								lastGlyphWidth = 0;
								lastGlyphRow += cell*MAP_SIZE;
							}

							glp.startingPoint.x = (f32)lastGlyphWidth / MAP_SIZE;
							glp.startingPoint.y = (f32)lastGlyphRow / (MAP_SIZE*MAP_SIZE);

							// Add To Texture
							for (uint32 h = 0; h < bitmap.rows; ++h)
								for (uint32 w = 0; w < bitmap.width; ++w)
								{
									index = h * MAP_SIZE;
									glyphMapData[index + w + lastGlyphWidth + lastGlyphRow] = bitmap.buffer[w + bitmap.width * h];
								}

							lastGlyphWidth += cell;

							// Add this properties to each glyph
							glyphs[text[i]] = glp;
						}
					}


					FT_Done_Glyph(glyph);

				}
				glyphMap->UpdateData(glyphMapData);


				break;
			}
		}
	}

	f32 Font::GetFontSize()
	{
		return fontSize;
	}

	Font::~Font()
	{
		FT_Done_Face(face);
		FT_Done_FreeType(ft);
		glyphMap.reset();
	}
}