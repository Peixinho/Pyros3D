//============================================================================
// Name        : Font.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Font - Emscripten Specific
//============================================================================


#include "../../../src/Pyros3D/AssetManager/Assets/Font/Font.h"
#include "../../../src/Pyros3D/AssetManager/Assets/Font/Font.cpp"
#include "../../../src/Pyros3D/AssetManager/AssetManager.h"

namespace p3d {
    
#ifdef EMSCRIPTEN

    Font::Font(const std::string& font, const f32& size)
    {
        // Font path
        this->font = font;
        
        // Font Size
        fontSize = size;
        
        // Create Texture
        glyphMap = new Texture();
        glyphMap->CreateTexture(TextureType::Texture,TextureDataType::ALPHA,MAP_SIZE,MAP_SIZE,true);
        glyphMap->SetRepeat(TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge);
        glyphMap->SetTextureByteAlignment(1);
        
        FILE *file;
        file = fopen(font.c_str(), "rb");
        std::vector<uchar>destination;
        int n_blocks = 1024;
        while(n_blocks != 0)
        {
            destination.resize(destination.size() + n_blocks);
            n_blocks = fread(&destination[destination.size() - n_blocks], 1, n_blocks, file);
        }
        fclose(file);

        // Free Type Initialization
        if (FT_Init_FreeType(&ft)) echo("ERROR: Couldn't Start Freetype Lib");
        if (FT_New_Memory_Face(ft,&destination[0],destination.size(),0,&face)) echo("ERROR: Couldn't Load Font");
        if (FT_Set_Char_Size(face,0,fontSize*64,300,300)) echo("ERROR: Couldn't Set Char Size");
        if (FT_Set_Pixel_Sizes(face,0,fontSize)) echo("ERROR: Couldn't Set Pixel Size");

        glyphMap->UpdateData(glyphMapData);
        
        lastGlyphWidth = lastGlyphRow = 0;
    }
#endif
}
