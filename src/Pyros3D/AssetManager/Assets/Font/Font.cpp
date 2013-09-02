//============================================================================
// Name        : Font.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Font
//============================================================================


#include "Font.h"
#include "../../AssetManager.h"

namespace p3d {
    
    Font::Font(const std::string& font, const f32& size)
    {
        // Font path
        this->font = font;
        
        // Font Size
        fontSize = size;
        
        // Create Texture
        glyphMapHandle = AssetManager::CreateTexture(TextureType::Texture,TextureDataType::ALPHA,MAP_SIZE,MAP_SIZE,true);
        glyphMap = *static_cast<Texture*> (AssetManager::GetAsset(glyphMapHandle)->AssetPTR);
        glyphMap.SetRepeat(TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge);
        glyphMap.SetTextureByteAlignment(1);
        
        // Free Type Initialization
        if (FT_Init_FreeType(&ft)) echo("ERROR: Couldn't Start Freetype Lib");
        if (FT_New_Face(ft, font.c_str(), 0, &face)) echo("ERROR: Couldn't Load Font");
        if (FT_Set_Char_Size(face,0,fontSize*64,300,300)) echo("ERROR: Couldn't Set Char Size");
        if (FT_Set_Pixel_Sizes(face,0,fontSize)) echo("ERROR: Couldn't Set Pixel Size");
        
        glyphMap.UpdateData(glyphMapData);
        
        lastGlyphWidth = lastGlyphRow = 0;
    }
    
    std::map<char,glyph_properties> Font::GetGlyphs()
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
        for (uint32 i = 0;i<text.size();i++)
        {
            switch(text[i])
            {
                case '\n':
                case ' ':
                        // NONE
                break;
                default:

                    if (glyphs.find(text[i])==glyphs.end())
                    {
                        FT_Load_Glyph(face,FT_Get_Char_Index(face,text[i]),FT_LOAD_DEFAULT);
                    
                        // Create Glyph
                        FT_Glyph glyph;
                    
                        // Get Glyph
                        FT_Get_Glyph(face->glyph,&glyph);

                        // Transform to Grayscale Bitmap
                        FT_Glyph_To_Bitmap(&glyph,FT_RENDER_MODE_NORMAL,0,1);
                        FT_BitmapGlyph bitmap_glyph = (FT_BitmapGlyph)glyph;
                        FT_Bitmap& bitmap=bitmap_glyph->bitmap;
                        // Get Bounding Box of each Glyph
                        FT_BBox BBox;
                        FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_PIXELS, &BBox);
                        glyph_properties glp;
                        glp.offset = Vec2(BBox.xMin, -BBox.yMin);
                        glp.size = Vec2(bitmap.width, bitmap.rows);

                        if (lastGlyphWidth + (fontSize)>MAP_SIZE)
                        {
                            lastGlyphWidth = 0;
                            lastGlyphRow+=fontSize*MAP_SIZE;
                        }
                        
                        glp.startingPoint.x = (f32)lastGlyphWidth/MAP_SIZE;
                        glp.startingPoint.y = (f32)lastGlyphRow/(MAP_SIZE*MAP_SIZE);
                        
                        // Add To Texture
                        for (uint32 h=0;h<bitmap.rows;++h)
                            for (uint32 w=0;w<bitmap.width;++w)
                            {
                                index = h * MAP_SIZE;
                                glyphMapData[index + w + lastGlyphWidth + lastGlyphRow]=bitmap.buffer[w + bitmap.width * h];
                            }
                        
                        lastGlyphWidth += (fontSize);
                        
                        // Add this properties to each glyph
                        glyphs[text[i]]=glp;
                        
                    }
                    glyphMap.UpdateData(glyphMapData);
                    
                    
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
        
    }
    
    void Font::Dispose()
    {
        
    }
    
    Texture Font::GetTexture()
    {
        return glyphMap;
    }
}