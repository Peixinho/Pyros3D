#include <cstdlib>
#include <chrono>
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
			// A glyph drawn as shapes laid over each other (many display
			// fonts are: a letter built of rectangles) has edges INSIDE
			// its ink, and distances measured to those come out as hollow
			// letters. This has the module take the outline's union first.
			// Not in a browser: there the module's allocation for it fails
			// and takes the page down (measured with FreeType 2.14 under
			// emscripten). There the distances are measured from the
			// glyph's PICTURE instead - drawn first, as ink, where shapes
			// laid over each other are simply ink twice - by FreeType's
			// other distance-field module, which is told the same spread.
			// PYROS_FONT_FROM_BITMAP=1 takes that road on a desktop, to
			// look at what a browser will draw.
#ifdef EMSCRIPTEN
			sdfFromBitmap = true;
#else
			sdfFromBitmap = getenv("PYROS_FONT_FROM_BITMAP") != NULL;
#endif
			if (sdfFromBitmap)
				FT_Property_Set(ft, "bsdf", "spread", &spreadProp);
			else
			{
				FT_Bool overlapsProp = 1;
				FT_Property_Set(ft, "sdf", "overlaps", &overlapsProp);
			}
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

	std::string Font::ToLatin1(const std::string &utf8)
	{
		std::string out;
		out.reserve(utf8.size());
		for (size_t i = 0; i < utf8.size(); i++)
		{
			const unsigned char c = (unsigned char)utf8[i];
			if (c < 0x80) { out += (char)c; continue; }
			const unsigned char n = (i + 1 < utf8.size()) ? (unsigned char)utf8[i + 1] : 0;
			if ((c == 0xC2 || c == 0xC3) && (n & 0xC0) == 0x80)
			{
				out += (char)(((c & 0x03) << 6) | (n & 0x3F));
				i++;
				continue;
			}
			// A longer sequence: one '?' for the whole character.
			size_t extra = (c >= 0xF0) ? 3 : ((c >= 0xE0) ? 2 : ((c >= 0xC4) ? 1 : 0));
			bool valid = extra > 0;
			for (size_t k = 1; k <= extra && valid; k++)
				if (i + k >= utf8.size() || ((unsigned char)utf8[i + k] & 0xC0) != 0x80) valid = false;
			if (valid) { out += '?'; i += extra; }
			else out += (char)c;       // not UTF-8: a Latin-1 byte as it stands
		}
		return out;
	}

	f32 Font::MeasureAdvance(const std::string &utf8) const
	{
		const std::string text = ToLatin1(utf8);
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

	void Font::CreateText(const std::string& utf8)
	{
		const std::string text = ToLatin1(utf8);
		// Every character a game is likely to print, the first time anything is
		// asked of this font - while a scene loads, that is. A glyph the font
		// has not drawn yet means its picture is sent to the device again, and
		// with the device on its own thread that waited a frame out: a HUD's
		// counter showing a digit for the first time held the game up for 15
		// to 40 ms, every time a new digit came round.
		if (!commonBaked)
		{
			commonBaked = true;
			std::string common;
			// ...as many as the atlas has room for. A glyph takes a square
			// cell of the bake size (and an SDF's spread round it), and the
			// atlas is MAP_SIZE across: a big font holds few. A font too
			// big for the whole alphabet draws its glyphs as they are asked
			// for instead - a title has a handful of letters.
			const uint32 cellSize = (uint32)fontSize + (isSDF ? sdfSpread * 2 : 0);
			const uint32 perRow = cellSize > 0 ? MAP_SIZE / cellSize : 0;
			const uint32 room = perRow * perRow;
			if (room >= 100)
				for (char c = 33; c < 127; c++) common += c;
			// (and the accented letters, U+00A1 to U+00FF, as UTF-8)
			if (room >= 200)
				for (int c = 0xA1; c <= 0xFF; c++) { common += (char)(0xC0 | (c >> 6)); common += (char)(0x80 | (c & 0x3F)); }
			CreateText(common);
		}
		/*
			Get the bounding box
			The methods described by the FT tutorials are bad for getting accurate offsets for some reason.
			Thanks to Nuno Silva
		*/

		uint32 index = 0;
		bool atlasChanged = false;
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

					// (as a character's number, 0-255: a plain char is signed,
					// and an accented letter would be asked for as a negative one)
					FT_Load_Char(face, (FT_ULong)(unsigned char)text[i], FT_LOAD_DEFAULT);

					// The distance field from the glyph's picture (see the
					// constructor): drawn as ink in its slot, then that
					// turned into distances, and only then taken as a glyph.
					const bool fromBitmap = isSDF && sdfFromBitmap && face->glyph->format == FT_GLYPH_FORMAT_OUTLINE;
					FT_Error slotErr = 0;
					if (fromBitmap)
					{
						slotErr = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
						if (slotErr == 0) slotErr = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_SDF);
					}
					const f32 slotLeft = (f32)face->glyph->bitmap_left, slotTop = (f32)face->glyph->bitmap_top;

					FT_Get_Glyph(face->glyph, &glyph);

					if (fromBitmap || glyph->format != FT_GLYPH_FORMAT_BITMAP)
					{
						const FT_Error rasterErr = fromBitmap ? slotErr
							: FT_Glyph_To_Bitmap(&glyph, isSDF ? FT_RENDER_MODE_SDF : FT_RENDER_MODE_NORMAL, 0, 1);
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
							// (made from a picture, the picture's own place
							// already has the spread in it)
							if (fromBitmap) glp.offset = Vec2(slotLeft, (f32)bitmap.rows - slotTop);
							glp.size = Vec2((f32)bitmap.width, (f32)bitmap.rows);
							// See glyph_properties::advance.
							glp.advance = (f32)(face->glyph->advance.x >> 6);

							const uint32 cell = (uint32)fontSize + (isSDF ? sdfSpread * 2 : 0);
							// A glyph has the cell, or what it needs if that is
							// more: a W or an @ in a bold face is wider than
							// the font is tall, an accented capital taller.
							// (Left out for not fitting its cell, the editor's
							// own bold font lost letters.)
							const uint32 slotW = bitmap.width + 1 > cell ? bitmap.width + 1 : cell;
							const uint32 slotH = bitmap.rows + 1 > cell ? bitmap.rows + 1 : cell;
							if (lastGlyphWidth + slotW > MAP_SIZE)
							{
								lastGlyphWidth = 0;
								lastGlyphRow += (rowHeight > cell ? rowHeight : cell) * MAP_SIZE;
								rowHeight = 0;
							}
							if (slotH > rowHeight) rowHeight = slotH;
							// No room left. This used to write on regardless,
							// past the end of the atlas and over whatever came
							// after it in memory: a big font with many
							// different letters took the program down, or
							// worse, did not. The glyph is left out and said.
							if (lastGlyphRow / MAP_SIZE + slotH > MAP_SIZE || slotW > MAP_SIZE)
							{
								static bool warnedFull = false;
								if (!warnedFull)
								{
									warnedFull = true;
									echo("WARNING: Font - the glyph atlas of " + font + " is full at this size; some characters are not drawn (use a smaller font size and scale the text up)");
								}
								FT_Done_Glyph(glyph);
								break;
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

							lastGlyphWidth += slotW;

							// Add this properties to each glyph
							glyphs[text[i]] = glp;
							atlasChanged = true;
						}
					}


					FT_Done_Glyph(glyph);

				}
				break;
			}
		}

		// Once, and only if a glyph was baked. This used to sit inside the
		// switch, so every visible character of every string re-uploaded the
		// whole atlas even when all of them were cached - and on Vulkan each
		// upload waits for the GPU. A HUD label updated per frame stalled
		// the frame once per character.
		if (atlasChanged)
			glyphMap->UpdateData(glyphMapData);
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