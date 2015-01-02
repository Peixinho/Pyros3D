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
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#define MAP_SIZE 1024

namespace p3d {
    
    struct glyph_properties
    {
        Vec2 offset;
        Vec2 size;
        Vec2 startingPoint;
    };
    
    class PYROS3D_API Font {
        
        private:
            
            // Font Path
            std::string font;
            
            f32 fontSize;
            
            // Font Map
            Texture* glyphMap;
        
            // Glyph Map Data
            uchar glyphMapData[MAP_SIZE*MAP_SIZE];
        
            // Font Glyphs Properties
            std::map<char,glyph_properties> glyphs;
        
            // Last Glyph Pixel Position
            uint32 lastGlyphWidth, lastGlyphRow;
        
            // Free Type Specifics
            FT_Library ft;
            FT_Face face;

            // From Memory
            std::vector<uchar> memory;
            
        public:
            
            // Create Font
            Font(const std::string &font, const f32 &size);
            
            // Create Text
            // It adds each char to the texture
            void CreateText(const std::string &text);
            
            virtual ~Font();
            
            Texture* GetTexture();
        
            f32 GetFontSize();
        
            std::map<char,glyph_properties> GetGlyphs();
    };
    
};

#endif /* FONT_H */