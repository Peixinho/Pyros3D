//============================================================================
// Name        : Texture.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Texture - Emscripten
//============================================================================

#include "../../../src/Pyros3D/AssetManager/Assets/Texture/Texture.h"
#include "../../../src/Pyros3D/Ext/StringIDs/StringID.hpp"
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

namespace p3d {
    
#ifdef EMSCRIPTEN
    bool Texture::LoadTexture(const std::string& FileName, const uint32 &Type, bool Mipmapping)
    {
        bool failed = false;
        bool ImageLoaded = false;
        
        StringID TextureStringID(MakeStringID(FileName));

        if (__Textures.find(TextureStringID)==__Textures.end())
        {
            FILE *file;
            file = fopen(FileName.c_str(), "rb");
            std::vector<uchar>destination;
            int n_blocks = 1024;
            while(n_blocks != 0)
            {
                destination.resize(destination.size() + n_blocks);
                n_blocks = fread(&destination[destination.size() - n_blocks], 1, n_blocks, file);
            }
            fclose(file);

            FIBITMAP *Image = NULL;
            FIMEMORY *mem =  FreeImage_OpenMemory(&destination[0], destination.size());
            FREE_IMAGE_FORMAT format = FreeImage_GetFileTypeFromMemory(mem, 0);
            Image = FreeImage_LoadFromMemory(format, mem);
            Image = FreeImage_ConvertTo32Bits(Image);
            //SwapRedBlue32(Image);
            FreeImage_FlipVertical(Image);

            // Save Texture Information
            __Textures[TextureStringID].Image = Image;
            __Textures[TextureStringID].Type = Type;
            __Textures[TextureStringID].TextureID = __Textures.size();
            __Textures[TextureStringID].Using = 1;
            __Textures[TextureStringID].Filename = FileName;
            __Textures[TextureStringID].Width = FreeImage_GetWidth(Image);
            __Textures[TextureStringID].Height = FreeImage_GetHeight(Image);

        } else {
            __Textures[TextureStringID].Using++;
        }

        this->TextureInternalID = TextureStringID;
        this->Width=__Textures[TextureStringID].Width;
        this->Height=__Textures[TextureStringID].Height;
        this->haveImage=true;
        this->Type=Type;
        this->DataType=__Textures[TextureStringID].DataType;
        this->Transparency=TextureTransparency::Opaque;
        
        if (this->GL_ID==-1) {
            glGenTextures(1, (GLuint*)&this->GL_ID);
        }
        
        if (this->GL_ID==-1)
        {
            failed = true;
        }
        
        if (failed)
        {
            echo("ERROR: Failed to Load Texture.");
            return false;
        }
        
        // create default texture
        return CreateTexture(Mipmapping);
    }
#endif
}
