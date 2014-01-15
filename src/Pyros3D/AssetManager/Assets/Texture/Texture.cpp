//============================================================================
// Name        : Texture.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Texture
//============================================================================

#include "Texture.h"
#include "GL/glew.h"
#include "../../../Ext/StringIDs/StringID.hpp"
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
            __Textures.erase(TextureInternalID);
    }
    
    bool Texture::LoadTexture(const std::string& FileName, const uint32 &Type, bool Mipmapping)
    {
        bool failed = false;
        bool ImageLoaded = false;
        
        StringID TextureStringID(MakeStringID(FileName));
        if (__Textures.find(TextureStringID)==__Textures.end())
        {
            sf::Image Image = sf::Image();
            ImageLoaded = Image.loadFromFile(FileName);
            
            if (!ImageLoaded)
            {
                echo("ERROR: Texture Not Found!");
                ImageLoaded = Image.loadFromFile("textures/texture_not_found.png");
            }
            
            // Save Texture Information
            __Textures[TextureStringID].Image = Image;
            __Textures[TextureStringID].Type = TextureType::Texture;
            __Textures[TextureStringID].DataType = TextureDataType::RGBA;
            __Textures[TextureStringID].TextureID = __Textures.size();
            __Textures[TextureStringID].Using = 1;
            __Textures[TextureStringID].Filename = FileName;
            
        } else {
            __Textures[TextureStringID].Using++;
        }
        
        this->TextureInternalID = TextureStringID;
        this->Width=__Textures[TextureStringID].Image.getSize().x;
        this->Height=__Textures[TextureStringID].Image.getSize().y;
        this->haveImage=true;
        this->Type=Type;
        this->DataType=TextureDataType::RGBA;
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
            case TextureDataType::DepthComponent24:    
                internalFormat=GL_DEPTH_COMPONENT24;
                internalFormat2=GL_DEPTH_COMPONENT;
                internalFormat3=GL_FLOAT;
            break;
            case TextureDataType::DepthComponent16:
                internalFormat=GL_DEPTH_COMPONENT16;
                internalFormat2=GL_DEPTH_COMPONENT;
                internalFormat3=GL_FLOAT;
            break;
            case TextureDataType::DepthComponent32:    
                internalFormat=GL_DEPTH_COMPONENT32;
                internalFormat2=GL_DEPTH_COMPONENT;
                internalFormat3=GL_FLOAT;
            break;
            case TextureDataType::R16F:
                internalFormat=GL_R16F;
                internalFormat2=GL_RED;
                internalFormat3=GL_FLOAT;
            break;
            case TextureDataType::R32F:
                internalFormat=GL_R32F;
                internalFormat2=GL_RED;
                internalFormat3=GL_FLOAT;
            break;
            case TextureDataType::R16I:
                internalFormat=GL_R16I;
                internalFormat2=GL_R;
                internalFormat3=GL_UNSIGNED_BYTE;
            break;
            case TextureDataType::R32I:
                internalFormat=GL_R32I;
                internalFormat2=GL_R;
                internalFormat3=GL_UNSIGNED_BYTE;
            break;            
            case TextureDataType::RG:
                internalFormat=GL_RG8;
                internalFormat2=GL_RG;
                internalFormat3=GL_UNSIGNED_BYTE;
            break;
            case TextureDataType::RG16F:
                internalFormat=GL_RG16F;
                internalFormat2=GL_RG;
                internalFormat3=GL_FLOAT;
            break;
            case TextureDataType::RG32F:
                internalFormat=GL_RG32F;
                internalFormat2=GL_RG;
                internalFormat3=GL_FLOAT;
            break;
            case TextureDataType::RG16I:
                internalFormat=GL_RG16I;
                internalFormat2=GL_RG;
                internalFormat3=GL_UNSIGNED_BYTE;
            break;
            case TextureDataType::RG32I:
                internalFormat=GL_RG32I;
                internalFormat2=GL_RG;
                internalFormat3=GL_UNSIGNED_BYTE;
            break;
            case TextureDataType::RGB:
                internalFormat=GL_RGB8;
                internalFormat2=GL_RGB;
                internalFormat3=GL_UNSIGNED_BYTE;
            break;
            case TextureDataType::RGB16F:
                internalFormat=GL_RGB16F;
                internalFormat2=GL_RGB;
                internalFormat3=GL_FLOAT;
            break;
            case TextureDataType::RGB32F:
                internalFormat=GL_RGB32F;
                internalFormat2=GL_RGB;
                internalFormat3=GL_FLOAT;
            break;
                case TextureDataType::RGB16I:
                internalFormat=GL_RGB16I;
                internalFormat2=GL_RGB;
                internalFormat3=GL_UNSIGNED_BYTE;
            break;
            case TextureDataType::RGB32I:
                internalFormat=GL_RGB32I;
                internalFormat2=GL_RGB;
                internalFormat3=GL_UNSIGNED_BYTE;
            break;
            case TextureDataType::RGBA16F:
                internalFormat=GL_RGBA16F;
                internalFormat2=GL_RGBA;
                internalFormat3=GL_FLOAT;
            break;
            case TextureDataType::RGBA32F:
                internalFormat=GL_RGBA32F;
                internalFormat2=GL_RGBA;
                internalFormat3=GL_FLOAT;
            break;
                case TextureDataType::RGBA16I:
                internalFormat=GL_RGBA16I;
                internalFormat2=GL_RGBA;
                internalFormat3=GL_UNSIGNED_BYTE;
            break;
            case TextureDataType::RGBA32I:
                internalFormat=GL_RGBA32I;
                internalFormat2=GL_RGBA;
                internalFormat3=GL_UNSIGNED_BYTE;
            break;            
            case TextureDataType::R:
                internalFormat=GL_R8;
                internalFormat2=GL_R;
                internalFormat3=GL_UNSIGNED_BYTE;
            break;
            case TextureDataType::ALPHA:
                internalFormat=GL_ALPHA;
                internalFormat2=GL_ALPHA;
                internalFormat3=GL_UNSIGNED_BYTE;
            break;
            case TextureDataType::RGBA:
            default:
                internalFormat=GL_RGBA8;
                internalFormat2=GL_RGBA;
                internalFormat3=GL_UNSIGNED_BYTE;
            break;
        }
        
        // bind
        glBindTexture(GLSubMode, GL_ID);
        
        if (Mipmapping)
        {
            if (GLEW_VERSION_2_1)
            {
                glTexImage2D(GLMode,0,internalFormat, Width, Height, 0,internalFormat2,internalFormat3, (haveImage==false?NULL:__Textures[TextureInternalID].Image.getPixelsPtr()));
                glGenerateMipmap(GLMode);
            } else {
                gluBuild2DMipmaps(GLMode,internalFormat,Width,Height,internalFormat2,internalFormat3, (haveImage==false?NULL:__Textures[TextureInternalID].Image.getPixelsPtr()));
            }
            isMipMap = true;
        } else {
            glTexImage2D(GLMode,0,internalFormat, Width, Height, 0,internalFormat2,internalFormat3, (haveImage==false?NULL:__Textures[TextureInternalID].Image.getPixelsPtr()));
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
            case TextureRepeat::ClampToBorder:
                glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                break;    
            case TextureRepeat::Clamp:
                glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_S, GL_CLAMP);
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
            case TextureRepeat::ClampToBorder:
                glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                break;                
            case TextureRepeat::Clamp:
                glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_T, GL_CLAMP);
                break;
            case TextureRepeat::Repeat:
            default:
                glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_T, GL_REPEAT);
                break;
        };
        
        if (WrapR>-1 && GLSubMode==GL_TEXTURE_CUBE_MAP)
        {
            switch (RRepeat)
            {
                case TextureRepeat::ClampToEdge:
                    glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
                    break;
                case TextureRepeat::ClampToBorder:
                    glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
                    break;                
                case TextureRepeat::Clamp:
                    glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_R, GL_CLAMP);
                    break;
                case TextureRepeat::Repeat:
                default:
                    glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_R, GL_REPEAT);
                    break;
            };
        }
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
        // USED ONLY FOR DEPTH MAPS
        // Bind
        glBindTexture(GLSubMode, GL_ID);
        
        GLfloat l_ClampColor[] = {1.0, 1.0, 1.0, 1.0};
        glTexParameterfv(GLSubMode, GL_TEXTURE_BORDER_COLOR, l_ClampColor);
        
        // This is to allow usage of shadow2DProj function in the shader
        glTexParameteri(GLSubMode,GL_TEXTURE_COMPARE_MODE,GL_COMPARE_R_TO_TEXTURE);
        glTexParameteri(GLSubMode,GL_TEXTURE_COMPARE_FUNC,GL_LEQUAL);
        // glTexParameteri(GLSubMode, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY); // Not used Anymore
        glTexParameteri(GLSubMode, GL_DEPTH_TEXTURE_MODE, GL_LUMINANCE);
        
        // Unbind
        glBindTexture(GLSubMode, 0);
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
        glTexImage2D(GLSubMode,0,internalFormat, Width, Height, 0,internalFormat2,internalFormat3, (haveImage==false?NULL:__Textures[TextureInternalID].Image.getPixelsPtr()));
        
        if (isMipMap)
        {
            if (GLEW_VERSION_2_1)
            {
                glGenerateMipmap(GLSubMode);
            } else {
                gluBuild2DMipmaps(GLSubMode,internalFormat,Width,Height,internalFormat2,internalFormat3, (haveImage==false?NULL:__Textures[TextureInternalID].Image.getPixelsPtr()));
            }
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
                if (GLEW_VERSION_2_1)
                {
                    glGenerateMipmap(GLSubMode);
                } else {
                    gluBuild2DMipmaps(GLSubMode,internalFormat,Width,Height,internalFormat2,internalFormat3, srcPTR);
                }
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
            if (GLEW_VERSION_2_1)
            {
                glGenerateMipmap(GLSubMode);
            } else {
                gluBuild2DMipmaps(GLSubMode,internalFormat,Width,Height,internalFormat2,internalFormat3, (haveImage==false?NULL:__Textures[TextureInternalID].Image.getPixelsPtr()));
            }
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
        switch(internalFormat)
        {
            case GL_DEPTH_COMPONENT16:
                pixels.resize(sizeof(uchar)*2*Width*Height);
            break;
            case GL_DEPTH_COMPONENT24:
                pixels.resize(sizeof(uchar)*3*Width*Height);
            break;
            case GL_DEPTH_COMPONENT32:
                pixels.resize(sizeof(f32)*Width*Height);
            break;
            case GL_R16F:
                pixels.resize(sizeof(uchar)*2*Width*Height);
            break;
            case GL_R32F:
                pixels.resize(sizeof(f32)*Width*Height);
            break;
            case GL_RG8:
                pixels.resize(sizeof(uchar)*Width*Height*2);
            break;
            case GL_R16I:
                pixels.resize(sizeof(uchar)*2*Width*Height);
            break;
            case GL_R32I:
                pixels.resize(sizeof(int32)*Width*Height);
            break;
            case GL_RG16F:
                pixels.resize(sizeof(uchar)*2*Width*Height*2);
            break;
            case GL_RG32F:
                pixels.resize(sizeof(f32)*Width*Height*2);
            break;
            case GL_RG16I:
                pixels.resize(sizeof(uchar)*2*Width*Height);
            break;
            case GL_RG32I:
                pixels.resize(sizeof(int32)*Width*Height*2);
            break;
            case GL_RGB8:
                pixels.resize(sizeof(uchar)*Width*Height*2);
            break;
            case GL_RGB16F:
                pixels.resize(sizeof(uchar)*2*Width*Height*3);
            break;
            case GL_RGB32F:
                pixels.resize(sizeof(f32)*Width*Height*3);
            break;
            case GL_RGB16I:
                pixels.resize(sizeof(uchar)*2*Width*Height*3);
            break;
            case GL_RGB32I:
                pixels.resize(sizeof(int32)*Width*Height*3);
            break;
            case GL_RGBA16F:
                pixels.resize(sizeof(uchar)*2*Width*Height*4);
            break;
            case GL_RGBA32F:
                pixels.resize(sizeof(f32)*Width*Height*4);
            break;
            case GL_RGBA16I:
                pixels.resize(sizeof(uchar)*2*Width*Height*4);
            break;
            case GL_RGBA32I:
                pixels.resize(sizeof(int32)*Width*Height*4);
            break;
            case GL_R8:
                pixels.resize(sizeof(uchar)*Width*Height*4);
            break;
            case GL_ALPHA:
                pixels.resize(sizeof(uchar)*Width*Height);
            break;
            default:
                pixels.resize(sizeof(uchar)*Width*Height*4);
            break;
        }
        glBindTexture(GLSubMode, GL_ID);
        glGetTexImage(GLSubMode,0,internalFormat2,internalFormat3,&pixels[0]);
        glBindTexture(GLSubMode, 0);
        pixelsRetrieved = true;
        return pixels;
    }
    
}