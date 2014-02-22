//============================================================================
// Name        : Texture.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Texture
//============================================================================

#include "../Pyros3D/AssetManager/Assets/Texture/Texture.h"
#include "../Pyros3D/Ext/StringIDs/StringID.hpp"
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <SDL.h>

#define GLCHECK() { int error =0; error = glGetError(); if(error != GL_NO_ERROR) { std::cout <<  "GL Error: " << std::hex << error << std::endl; } }

namespace p3d {
    
    uint32 Texture::UnitBinded = 0;
    uint32 Texture::LastUnitBinded = 0;
    
    // List of Textures
    std::map<uint32, __Texture> Texture::__Textures;
    
    Texture::Texture() : GL_ID(-1), haveImage(false), isMipMap(false), pixelsRetrieved(false), Anysotropic(0) {}

    Texture::~Texture()
    {
        if (GL_ID!=-1)
        {
            glDeleteTextures (1, (GLuint*)&GL_ID);
        }
        
        __Textures[TextureInternalID].Using--;
        
        if (__Textures[TextureInternalID].Using==0)
        { 
            FreeImage_Unload(__Textures[TextureInternalID].Image);
            __Textures.erase(TextureInternalID);
        }
    }
    
    bool Texture::LoadTexture(const std::string& FileName, const uint32 &Type, bool Mipmapping)
    {
        bool failed = false;
        bool ImageLoaded = false;
        
        StringID TextureStringID(MakeStringID(FileName));

        if (__Textures.find(TextureStringID)==__Textures.end())
        {
            // Using SDL_Rwops
            SDL_RWops *file;
            file = SDL_RWFromFile(FileName.c_str(), "rb");
            std::vector<uchar>destination;
            int n_blocks = 1024;
            while(n_blocks != 0)
            {
                destination.resize(destination.size() + n_blocks);
                n_blocks = SDL_RWread(file, &destination[destination.size() - n_blocks], 1, n_blocks);
            }
            SDL_RWclose(file);

            FIBITMAP *Image = NULL;
            FIMEMORY *mem =  FreeImage_OpenMemory(&destination[0], destination.size());
            FREE_IMAGE_FORMAT format = FreeImage_GetFileTypeFromMemory(mem, 0);
            Image = FreeImage_LoadFromMemory(format, mem);
            Image = FreeImage_ConvertTo32Bits(Image);
            SwapRedBlue32(Image);
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
    
    bool Texture::LoadTextureFromMemory(std::vector<uchar> data, const uint32 &length, const uint32 &Type, bool Mipmapping)
    {

        bool failed = false;
        bool ImageLoaded = false;
        
        StringID TextureStringID(MakeStringIDFromChar(&data[0], length));
        if (__Textures.find(TextureStringID)==__Textures.end())
        {
            
            FIBITMAP *Image = NULL;
            FIMEMORY *mem =  FreeImage_OpenMemory(&data[0], length);
            FREE_IMAGE_FORMAT format = FreeImage_GetFileTypeFromMemory(mem, 0);
            Image = FreeImage_LoadFromMemory(format, mem);
            Image = FreeImage_ConvertTo32Bits(Image);
            SwapRedBlue32(Image);
            FreeImage_FlipVertical(Image);

            __Textures[TextureStringID].DataType = Type;

            // Save Texture Information
            __Textures[TextureStringID].Image = Image;
            __Textures[TextureStringID].Type = Type;
            __Textures[TextureStringID].TextureID = __Textures.size();
            __Textures[TextureStringID].Using = 1;
            __Textures[TextureStringID].Filename = FileName;
            
        } else {
            __Textures[TextureStringID].Using++;
        }
        
        this->TextureInternalID = TextureStringID;

        this->Width=FreeImage_GetWidth(__Textures[TextureStringID].Image);
        this->Height=FreeImage_GetHeight(__Textures[TextureStringID].Image);
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

    bool Texture::CreateTexture(bool Mipmapping)
    {
        
        // default texture
        switch(Type) {
            case TextureType::CubemapNegative_X:
                GLMode=GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
                GLSubMode=GL_TEXTURE_CUBE_MAP;
            break;
            case TextureType::CubemapNegative_Y:
                GLMode=GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
                GLSubMode=GL_TEXTURE_CUBE_MAP;
            break;
            case TextureType::CubemapNegative_Z:
                GLMode=GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
                GLSubMode=GL_TEXTURE_CUBE_MAP;
            break;
            case TextureType::CubemapPositive_X:
                GLMode=GL_TEXTURE_CUBE_MAP_POSITIVE_X;
                GLSubMode=GL_TEXTURE_CUBE_MAP;
            break;
            case TextureType::CubemapPositive_Y:
                GLMode=GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
                GLSubMode=GL_TEXTURE_CUBE_MAP;
            break;
            case TextureType::CubemapPositive_Z:
                GLMode=GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
                GLSubMode=GL_TEXTURE_CUBE_MAP;
            break;
            case TextureType::Texture:
            default:
                GLMode=GL_TEXTURE_2D;
                GLSubMode=GL_TEXTURE_2D;
                break;
        }
        
        switch(DataType)
        {
            
            case TextureDataType::DepthComponent:
            case TextureDataType::DepthComponent16:
            case TextureDataType::DepthComponent24:
            case TextureDataType::DepthComponent32:
                internalFormat=GL_DEPTH_COMPONENT;
                internalFormat2=GL_DEPTH_COMPONENT;
                internalFormat3=GL_FLOAT;
            break;
            case TextureDataType::ALPHA:
                internalFormat=GL_ALPHA;
                internalFormat2=GL_ALPHA;
                internalFormat3=GL_UNSIGNED_BYTE;
            break;          
            case TextureDataType::RGBA:
            default:
                internalFormat=GL_RGBA;
                internalFormat2=GL_RGBA;
                internalFormat3=GL_UNSIGNED_BYTE;
            break;
        }
        
        // bind
        glBindTexture(GLSubMode, GL_ID);
        
        if (Mipmapping)
        {
            glTexImage2D(GLMode,0,internalFormat, Width, Height, 0,internalFormat2,internalFormat3, (haveImage==false?NULL:__Textures[TextureInternalID].GetPixels()));
            glGenerateMipmap(GLMode);

        } else {
            glTexImage2D(GLMode,0,internalFormat, Width, Height, 0,internalFormat2,internalFormat3, (haveImage==false?NULL:__Textures[TextureInternalID].GetPixels()));
        }
        // default values
        glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GLSubMode, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GLSubMode, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        
        // unbind
        glBindTexture(GLSubMode, 0);
        
        return true;
    }
    
    bool Texture::CreateTexture(const uint32& Type, const uint32& TextureDataType, const int32& width, const int32& height, bool Mipmapping)
    {
                       
        Width=width;
        Height=height;
        this->Type=Type;
        this->DataType=TextureDataType;
        this->haveImage = false;
        
        if (GL_ID==-1) {
            glGenTextures(1, (GLuint*)&GL_ID);
        }
        if (GL_ID==-1) 
        {
            echo("ERROR: Couldn't Create Texture");
            return false;
        }
        
        // Create Texture
        return CreateTexture(Mipmapping);
    }
    
    void Texture::SetAnysotropy(const uint32& Anysotropic)
    {
        // bind
        glBindTexture(GLSubMode, GL_ID);
        
        this->Anysotropic = Anysotropic;
        
        if (Anysotropic>0)
        {
            f32 AnysotropicMax;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &AnysotropicMax);
            if (AnysotropicMax>Anysotropic)
                glTexParameteri(GLSubMode, GL_TEXTURE_MAX_ANISOTROPY_EXT, Anysotropic);
            else if (AnysotropicMax>0)
                glTexParameteri(GLSubMode, GL_TEXTURE_MAX_ANISOTROPY_EXT, AnysotropicMax);
        } else {
            glTexParameteri(GLSubMode, GL_TEXTURE_MAX_ANISOTROPY_EXT, 0);
        }
        
        // unbind
        glBindTexture(GLSubMode, 0);
        
    }
    
    void Texture::SetRepeat(const uint32& WrapS, const uint32& WrapT, const int32& WrapR)
    {
        
        SRepeat = WrapS;
        TRepeat = WrapT;
        if (WrapR>-1) RRepeat = WrapR;
        
        // bind
         glBindTexture(GLSubMode, GL_ID);
        
         switch (SRepeat)
        {
            case TextureRepeat::ClampToEdge:
                    glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                break;
            case TextureRepeat::Repeat:
                default:
                    glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_S, GL_REPEAT);
                break;
        };

        switch (TRepeat)
        {
            case TextureRepeat::ClampToEdge:
                glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                break;
            case TextureRepeat::Repeat:
            default:
                glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_T, GL_REPEAT);
                break;
        };
        // unbind
         glBindTexture(GLSubMode, 0);
        
    }
    
    void Texture::SetMinMagFilter(const uint32& MinFilter, const uint32& MagFilter)
    {
        
        this->MinFilter = MinFilter;
        this->MagFilter = MagFilter;
        
        // bind
        glBindTexture(GLSubMode, GL_ID);
        
        switch (MagFilter)
        {
            case TextureFilter::Nearest:
                    glTexParameteri(GLSubMode, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                break;
            default:
                    glTexParameteri(GLSubMode, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                break;                
        }
        
        switch (MinFilter)
        {
            case TextureFilter::Nearest:
            case TextureFilter::NearestMipmapNearest:
                if (isMipMap == true)
                    glTexParameteri(GLSubMode, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
                else 
                    glTexParameteri(GLSubMode, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                break;
            case TextureFilter::NearestMipmapLinear:
                if (isMipMap == true)
                    glTexParameteri(GLSubMode, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
                else 
                    glTexParameteri(GLSubMode, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                break; 
            case TextureFilter::LinearMipmapNearest:
                if (isMipMap == true)
                    glTexParameteri(GLSubMode, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
                else
                    glTexParameteri(GLSubMode, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                break;
            case TextureFilter::Linear:
            case TextureFilter::LinearMipmapLinear:
            default:
                if (isMipMap == true)
                    glTexParameteri(GLSubMode, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                else
                    glTexParameteri(GLSubMode, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                break;                
        }
        
        // unbind
         glBindTexture(GLSubMode, 0);        
        
    }
    void Texture::EnableCompareMode()
    {

    }
    void Texture::SetTransparency(const f32& Transparency)
    {
        
        this->Transparency = Transparency;
        
    }
    
    void Texture::Resize(const uint32& Width, const uint32& Height)
    {
        glBindTexture(GLSubMode, GL_ID);
        this->Width=Width;
        this->Height=Height;
        glTexImage2D(GLSubMode,0,internalFormat, Width, Height, 0,internalFormat2,internalFormat3, (haveImage==false?NULL:__Textures[TextureInternalID].GetPixels()));
        
        if (isMipMap)
        {
            glGenerateMipmap(GLSubMode);
        }
        
        glBindTexture(GLSubMode, 0);
    }
    
    void Texture::SetTextureByteAlignment(const uint32& Value)
    {
        glBindTexture(GLSubMode, GL_ID);
        
        glPixelStorei(GL_UNPACK_ALIGNMENT, Value);
        
        glBindTexture(GLSubMode, 0);
    }
    
    void Texture::UpdateData(void* srcPTR)
    {
        if (GL_ID>0)
        {
            // bind
            glBindTexture(GLSubMode, GL_ID);
            glTexImage2D(GLSubMode,0,internalFormat, Width, Height, 0,internalFormat2,internalFormat3, srcPTR);
            
            if (isMipMap)
            {
                glGenerateMipmap(GLSubMode);
                UpdateMipmap();
            }
            
            // unbind
            glBindTexture(GLSubMode, 0);
        }
    }
    
    void Texture::UpdateMipmap()
    {
        // bind
        glBindTexture(GLSubMode, GL_ID);
        
        if (isMipMap)
        {
            glGenerateMipmap(GLSubMode);
        }
        
        // unbind
        glBindTexture(GLSubMode, 0);           
    }
    
    void Texture::Bind()
    {
        glActiveTexture(GL_TEXTURE0 + UnitBinded);
        glBindTexture(GLSubMode, GL_ID);

        // For Transparency
        if (Transparency==TextureTransparency::Transparent)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        } 
        // Save Last Unit Binded
        LastUnitBinded = UnitBinded;
        // Set New Unit value
        UnitBinded++;
    }
    
    void Texture::Unbind()
    {
        UnitBinded--;
        if (Transparency==TextureTransparency::Transparent) glDisable(GL_BLEND);
        glActiveTexture(GL_TEXTURE0 + UnitBinded);
        glBindTexture(GLSubMode, 0);

        // Save Last Unit Binded
        LastUnitBinded = UnitBinded;
    }
    
    uint32 Texture::GetLastBindedUnit()
    { 
        return LastUnitBinded;
    }
    
    const uint32 Texture::GetWidth() const
    {
        return Width;
    }
    const uint32 Texture::GetHeight() const
    {
        return Height;
    }
    
    const uint32 Texture::GetBindID() const
    {
        return GL_ID;
    }
    
    std::vector<uchar> Texture::GetTextureData()
    {
        return pixels;
    }
    
}
