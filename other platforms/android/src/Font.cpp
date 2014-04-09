//============================================================================
// Name        : Font.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Font - Android Specific
//============================================================================


#include "../Pyros3D/AssetManager/Assets/Font/Font.h"
#include "../Pyros3D/AssetManager/AssetManager.h"
#include <SDL.h>
namespace p3d {
    
#ifdef ANDROID

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
        
        // Using SDL_Rwops
        SDL_RWops *file;
        file = SDL_RWFromFile(font.c_str(), "rb");
        int n_blocks = 1024;
        while(n_blocks != 0)
        {
            memory.resize(memory.size() + n_blocks);
            n_blocks = SDL_RWread(file, &memory[memory.size() - n_blocks], 1, n_blocks);
        }
        SDL_RWclose(file);
        
        // Free Type Initialization
        if (FT_Init_FreeType(&ft)) echo("ERROR: Couldn't Start Freetype Lib");
        if (FT_New_Memory_Face(ft,&memory[0],memory.size(),0,&face)) echo("ERROR: Couldn't Load Font");
        if (FT_Set_Char_Size(face,0,fontSize*64,300,300)) echo("ERROR: Couldn't Set Char Size");
        if (FT_Set_Pixel_Sizes(face,0,fontSize)) echo("ERROR: Couldn't Set Pixel Size");

        glyphMap->UpdateData(glyphMapData);
        
        lastGlyphWidth = lastGlyphRow = 0;
    }
#endif
}