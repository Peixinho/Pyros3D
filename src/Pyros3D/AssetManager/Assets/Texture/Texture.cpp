//============================================================================
// Name        : Texture.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Texture
//============================================================================


#include "Texture.h"
#include "../../AssetManager.h"
#include "GL/glew.h"

#define GLCHECK() { int error = glGetError(); if(error != GL_NO_ERROR) { std::cout <<  "GL Error: " << std::hex << error << std::endl; } }

namespace p3d {
    
    uint32 Texture::UnitBinded = 0;
    uint32 Texture::LastUnitBinded = 0;
    
    Texture::Texture() : GL_ID(-1), haveImage(false), isMipMap(false)
    {
        
    }

    Texture::~Texture() {}   
    
    bool Texture::LoadTexture(const std::string& FileName, const uint32 &Type,const uint32 &SubType, bool Mipmapping)
    {
        bool failed = false;
        
        std::string Filename = FileName;
        
        sf::Image image;
        bool ImageLoaded = image.loadFromFile(Filename);
        if (!ImageLoaded)
        {
            echo("ERROR: Texture Not Found!");
            Filename = "textures/texture_not_found.png";
            ImageLoaded = image.loadFromFile(Filename);
        }
        if (ImageLoaded)
        {
            this->FileName=Filename;
            this->Width=image.getSize().x;
            this->Height=image.getSize().y;
            this->Image=image;
            this->haveImage=true;
            this->Type=Type;
            this->SubType=SubType;
            this->Transparency=TextureTransparency::Opaque;
            if (this->GL_ID==-1) {
                glGenTextures(1, (GLuint*)&this->GL_ID);
            }
            if (this->GL_ID==-1)
            {
                failed = true;
            }
        } else {
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
        switch(SubType) {
            case TextureSubType::CubemapNegative_X:
                subMode=GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
                mode=GL_TEXTURE_CUBE_MAP;
                internalFormat=GL_RGBA8;
                internalFormat2=GL_RGBA;
                internalFormat3=GL_UNSIGNED_BYTE;
                break;
            case TextureSubType::CubemapNegative_Y:
                subMode=GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
                mode=GL_TEXTURE_CUBE_MAP;
                internalFormat=GL_RGBA8;
                internalFormat2=GL_RGBA;
                internalFormat3=GL_UNSIGNED_BYTE;
                break;
            case TextureSubType::CubemapNegative_Z:
                subMode=GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
                mode=GL_TEXTURE_CUBE_MAP;
                internalFormat=GL_RGBA8;
                internalFormat2=GL_RGBA;
                internalFormat3=GL_UNSIGNED_BYTE;
                break;
            case TextureSubType::CubemapPositive_X:
                subMode=GL_TEXTURE_CUBE_MAP_POSITIVE_X;
                mode=GL_TEXTURE_CUBE_MAP;
                internalFormat=GL_RGBA8;
                internalFormat2=GL_RGBA;
                internalFormat3=GL_UNSIGNED_BYTE;
                break;
            case TextureSubType::CubemapPositive_Y:
                subMode=GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
                mode=GL_TEXTURE_CUBE_MAP;
                internalFormat=GL_RGBA8;
                internalFormat2=GL_RGBA;
                internalFormat3=GL_UNSIGNED_BYTE;
                break;
            case TextureSubType::CubemapPositive_Z:
                subMode=GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
                mode=GL_TEXTURE_CUBE_MAP;
                internalFormat=GL_RGBA8;
                internalFormat2=GL_RGBA;
                internalFormat3=GL_UNSIGNED_BYTE;
                break;
            case TextureSubType::DepthComponent:
                subMode=GL_TEXTURE_2D;                
                mode=GL_TEXTURE_2D;
                internalFormat=GL_DEPTH_COMPONENT;
                internalFormat2=GL_DEPTH_COMPONENT;
                internalFormat3=GL_UNSIGNED_BYTE;
                break;
            case TextureSubType::FloatingPointTexture16F:
                mode=GL_TEXTURE_2D;
                subMode=GL_TEXTURE_2D;
                internalFormat=GL_RGBA16F;
                internalFormat2=GL_RGBA;
                internalFormat3=GL_FLOAT;
                break;
            case TextureSubType::FloatingPointTexture32F:
                mode=GL_TEXTURE_2D;
                subMode=GL_TEXTURE_2D;
                internalFormat=GL_RGBA32F;
                internalFormat2=GL_RGBA;
                internalFormat3=GL_FLOAT;
                break;
            case TextureSubType::NormalTexture:
            default:
                mode=GL_TEXTURE_2D;
                subMode=GL_TEXTURE_2D;
                internalFormat=GL_RGBA8;
                internalFormat2=GL_RGBA;
                internalFormat3=GL_UNSIGNED_BYTE;
                break;
        }
        
        // bind
        glBindTexture(mode, GL_ID);

        if (Mipmapping == true)
        {
            if (GLEW_VERSION_2_1)
            {
                glTexImage2D(subMode,0,internalFormat, Width, Height, 0,internalFormat2,internalFormat3, (haveImage==false?NULL:Image.getPixelsPtr()));
                glGenerateMipmap(mode);
            } else {
                gluBuild2DMipmaps(subMode,internalFormat,Width,Height,internalFormat2,internalFormat3, (haveImage==false?NULL:Image.getPixelsPtr()));
            }
            isMipMap = true;
        } else {
            glTexImage2D(subMode,0,internalFormat, Width, Height, 0,internalFormat2,internalFormat3, (haveImage==false?NULL:Image.getPixelsPtr()));
        }
        
        // default values
        glTexParameteri(mode, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(mode, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(mode, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(mode, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        
        // unbind
        glBindTexture(mode, 0);
        
        return true;
    }
    
    bool Texture::CreateTexture(const uint32& Type, const uint32& SubType, const int& width, const int& height, bool Mipmapping)
    {
                       
        Width=width;
        Height=height;
        this->Type=Type;
        this->SubType=SubType;

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
    
    void Texture::SetAnysotropy(const f32& Anysotropic)
    {
        // bind
         glBindTexture(mode, GL_ID);
         
         this->Anysotropic = Anysotropic;
         
         if (Anysotropic>0.0)
         {
            f32 AnysotropicMax;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &AnysotropicMax);
            if (AnysotropicMax>Anysotropic)
                glTexParameteri(mode, GL_TEXTURE_MAX_ANISOTROPY_EXT, Anysotropic);
            else if (AnysotropicMax>0.0)
                glTexParameteri(mode, GL_TEXTURE_MAX_ANISOTROPY_EXT, AnysotropicMax);
        } else {
             glTexParameteri(mode, GL_TEXTURE_MAX_ANISOTROPY_EXT, 0);
        }
         
         // unbind
         glBindTexture(mode, 0);
         
    }
    
    void Texture::SetRepeat(const uint32& WrapS, const uint32& WrapT)
    {
        
        SRepeat = WrapS;
        TRepeat = WrapT;
        
        // bind
         glBindTexture(mode, GL_ID);
        
         switch (SRepeat)
        {
            case TextureRepeat::ClampToEdge:
                glTexParameteri(mode, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                break;
            case TextureRepeat::ClampToBorder:
                glTexParameteri(mode, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                break;    
            case TextureRepeat::Clamp:
                glTexParameteri(mode, GL_TEXTURE_WRAP_S, GL_CLAMP);
                break;
            case TextureRepeat::Repeat:
            default:
                glTexParameteri(mode, GL_TEXTURE_WRAP_S, GL_REPEAT);
                break;
        }

        switch (TRepeat)
        {
            case TextureRepeat::ClampToEdge:
                glTexParameteri(mode, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                break;
            case TextureRepeat::ClampToBorder:
                glTexParameteri(mode, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                break;                
            case TextureRepeat::Clamp:
                glTexParameteri(mode, GL_TEXTURE_WRAP_T, GL_CLAMP);
                break;
            case TextureRepeat::Repeat:
            default:
                glTexParameteri(mode, GL_TEXTURE_WRAP_T, GL_REPEAT);
                break;
        }
        
        // unbind
         glBindTexture(mode, 0);
        
    }
    
    void Texture::SetMinMagFilter(const uint32& MinFilter, const uint32& MagFilter)
    {
        
        this->MinFilter = MinFilter;
        this->MagFilter = MagFilter;
        
        // bind
        glBindTexture(mode, GL_ID);
        
        switch (MagFilter)
        {
            case TextureFilter::Nearest:
            case TextureFilter::NearestMipmapNearest:
                if (isMipMap == true)
                    glTexParameteri(mode, GL_TEXTURE_MAG_FILTER, GL_NEAREST_MIPMAP_NEAREST);
                else 
                    glTexParameteri(mode, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                break;
            case TextureFilter::NearestMipmapLinear:
                if (isMipMap == true)
                    glTexParameteri(mode, GL_TEXTURE_MAG_FILTER, GL_NEAREST_MIPMAP_LINEAR);
                else 
                    glTexParameteri(mode, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                break; 
            case TextureFilter::LinearMipmapNearest:
                if (isMipMap == true)
                    glTexParameteri(mode, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_NEAREST);
                else
                    glTexParameteri(mode, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                break;
            case TextureFilter::Linear:
            case TextureFilter::LinearMipmapLinear:
            default:
                if (isMipMap == true)
                    glTexParameteri(mode, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                else
                    glTexParameteri(mode, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                break;                
        }
        
        switch (MinFilter)
        {
            case TextureFilter::Nearest:
            case TextureFilter::NearestMipmapNearest:
                if (isMipMap == true)
                    glTexParameteri(mode, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
                else 
                    glTexParameteri(mode, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                break;
            case TextureFilter::NearestMipmapLinear:
                if (isMipMap == true)
                    glTexParameteri(mode, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
                else 
                    glTexParameteri(mode, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                break; 
            case TextureFilter::LinearMipmapNearest:
                if (isMipMap == true)
                    glTexParameteri(mode, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
                else
                    glTexParameteri(mode, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                break;
            case TextureFilter::Linear:
            case TextureFilter::LinearMipmapLinear:
            default:
                if (isMipMap == true)
                    glTexParameteri(mode, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                else
                    glTexParameteri(mode, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                break;                
        }
        
        // unbind
         glBindTexture(mode, 0);        
        
    }
    void Texture::EnableCompareMode()
    {
        // USED ONLY FOR DEPTH MAPS
        // Bind
        glBindTexture(mode, GL_ID);
        
        GLfloat l_ClampColor[] = {1.0, 1.0, 1.0, 1.0};
        glTexParameterfv(mode, GL_TEXTURE_BORDER_COLOR, l_ClampColor);
        
        // This is to allow usage of shadow2DProj function in the shader
        glTexParameteri(mode,GL_TEXTURE_COMPARE_MODE,GL_COMPARE_R_TO_TEXTURE);
        glTexParameteri(mode,GL_TEXTURE_COMPARE_FUNC,GL_LEQUAL);
        glTexParameteri(mode, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);
        
        // Unbind
        glBindTexture(mode, 0);
    }
    void Texture::SetTransparency(const f32& Transparency)
    {
        
        this->Transparency = Transparency;
        
    }
    
    void Texture::Resize(const uint32& Width, const uint32& Height)
    {
        glBindTexture(mode, GL_ID);
        this->Width=Width;
        this->Height=Height;
        glTexImage2D(subMode,0,internalFormat, Width, Height, 0,internalFormat2,internalFormat3, (haveImage==false?NULL:Image.getPixelsPtr()));
        
        if (isMipMap == true)
        {
            glGenerateMipmap(mode);
            if (GLEW_VERSION_2_1)
            {
                glGenerateMipmap(mode);
            } else {
                gluBuild2DMipmaps(subMode,internalFormat,Width,Height,internalFormat2,internalFormat3, (haveImage==false?NULL:Image.getPixelsPtr()));
            }
        }
        
        glBindTexture(mode, 0);
    }
    
    void Texture::UpdateData(void* srcPTR)
    {
        if (GL_ID>0)
        {
            // bind
            glBindTexture(mode, GL_ID);
            glTexImage2D(subMode,0,internalFormat, Width, Height, 0,internalFormat2,internalFormat3, srcPTR);
            
            if (isMipMap == true)
            {
                glGenerateMipmap(mode);
                if (GLEW_VERSION_2_1)
                {
                    glGenerateMipmap(mode);
                } else {
                    gluBuild2DMipmaps(subMode,internalFormat,Width,Height,internalFormat2,internalFormat3, srcPTR);
                }
            }
            
            // unbind
            glBindTexture(mode, 0);
        }
    }
    
    void Texture::UpdateMipmap()
    {
        // bind
        glBindTexture(mode, GL_ID);
        
        if (isMipMap == true)
        {
            glGenerateMipmap(mode);
            if (GLEW_VERSION_2_1)
            {
                glGenerateMipmap(mode);
            } else {
                gluBuild2DMipmaps(subMode,internalFormat,Width,Height,internalFormat2,internalFormat3, (haveImage==false?NULL:Image.getPixelsPtr()));
            }
        }
        
        // unbind
        glBindTexture(mode, 0);           
    }
    
    void Texture::Bind()
    {
        glActiveTexture(GL_TEXTURE0 + UnitBinded);
        glBindTexture(mode, GL_ID);
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
        glBindTexture(mode, 0);

        // Save Last Unit Binded
        LastUnitBinded = UnitBinded;
    }
    
    uint32 Texture::GetLastBindedUnit()
    { 
        return LastUnitBinded;
    }
    
    void Texture::DeleteTexture()
    {
        if (GL_ID!=-1)
        glDeleteTextures (1, (GLuint*)&GL_ID);
    }
    
    const uint32 Texture::GetBindID() const
    {
        return GL_ID;
    }
    
    void Texture::Dispose()
    {
        DeleteTexture();
    }
    
}