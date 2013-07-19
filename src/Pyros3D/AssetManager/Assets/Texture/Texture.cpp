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
    
    bool Texture::LoadTexture(const std::string& FileName, const uint32 &Type, bool Mipmapping)
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
                internalFormat=GL_DEPTH_COMPONENT;
                internalFormat2=GL_DEPTH_COMPONENT;
                internalFormat3=GL_FLOAT;
            break;
            case TextureDataType::FloatingPointTexture16F:
                internalFormat=GL_RGBA16F;
                internalFormat2=GL_RGBA;
                internalFormat3=GL_FLOAT;
            break;
            case TextureDataType::FloatingPointTexture32F:
                internalFormat=GL_RGBA32F;
                internalFormat2=GL_RGBA;
                internalFormat3=GL_FLOAT;
            break;
            default:
                internalFormat=GL_RGBA8;
                internalFormat2=GL_RGBA;
                internalFormat3=GL_UNSIGNED_BYTE;
            break;
        }
        
        // bind
        glBindTexture(GLSubMode, GL_ID);
        
        if (Mipmapping == true)
        {
            if (GLEW_VERSION_2_1)
            {
                glTexImage2D(GLMode,0,internalFormat, Width, Height, 0,internalFormat2,internalFormat3, (haveImage==false?NULL:Image.getPixelsPtr()));
                glGenerateMipmap(GLMode);
            } else {
                gluBuild2DMipmaps(GLMode,internalFormat,Width,Height,internalFormat2,internalFormat3, (haveImage==false?NULL:Image.getPixelsPtr()));
            }
            isMipMap = true;
        } else {
            glTexImage2D(GLMode,0,internalFormat, Width, Height, 0,internalFormat2,internalFormat3, (haveImage==false?NULL:Image.getPixelsPtr()));
        }
        
        // default values
        glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GLSubMode, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GLSubMode, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GLSubMode, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        
        // unbind
        glBindTexture(GLSubMode, 0);
//        
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
    
    void Texture::SetAnysotropy(const f32& Anysotropic)
    {
        // bind
         glBindTexture(GLSubMode, GL_ID);
         
         this->Anysotropic = Anysotropic;
         
         if (Anysotropic>0.0)
         {
            f32 AnysotropicMax;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &AnysotropicMax);
            if (AnysotropicMax>Anysotropic)
                glTexParameteri(GLSubMode, GL_TEXTURE_MAX_ANISOTROPY_EXT, Anysotropic);
            else if (AnysotropicMax>0.0)
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
        
        if (WrapR>-1 && GLMode==GL_TEXTURE_CUBE_MAP)
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
            case TextureFilter::NearestMipmapNearest:
                if (isMipMap == true)
                    glTexParameteri(GLSubMode, GL_TEXTURE_MAG_FILTER, GL_NEAREST_MIPMAP_NEAREST);
                else 
                    glTexParameteri(GLSubMode, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                break;
            case TextureFilter::NearestMipmapLinear:
                if (isMipMap == true)
                    glTexParameteri(GLSubMode, GL_TEXTURE_MAG_FILTER, GL_NEAREST_MIPMAP_LINEAR);
                else 
                    glTexParameteri(GLSubMode, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                break; 
            case TextureFilter::LinearMipmapNearest:
                if (isMipMap == true)
                    glTexParameteri(GLSubMode, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_NEAREST);
                else
                    glTexParameteri(GLSubMode, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                break;
            case TextureFilter::Linear:
            case TextureFilter::LinearMipmapLinear:
            default:
                if (isMipMap == true)
                    glTexParameteri(GLSubMode, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                else
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
        glTexParameteri(GLSubMode, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);
        
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
        glTexImage2D(GLSubMode,0,internalFormat, Width, Height, 0,internalFormat2,internalFormat3, (haveImage==false?NULL:Image.getPixelsPtr()));
        
        if (isMipMap == true)
        {
            glGenerateMipmap(GLSubMode);
            if (GLEW_VERSION_2_1)
            {
                glGenerateMipmap(GLSubMode);
            } else {
                gluBuild2DMipmaps(GLSubMode,internalFormat,Width,Height,internalFormat2,internalFormat3, (haveImage==false?NULL:Image.getPixelsPtr()));
            }
        }
        
        glBindTexture(GLSubMode, 0);
    }
    
    void Texture::UpdateData(void* srcPTR)
    {
        if (GL_ID>0)
        {
            // bind
            glBindTexture(GLSubMode, GL_ID);
            glTexImage2D(GLSubMode,0,internalFormat, Width, Height, 0,internalFormat2,internalFormat3, srcPTR);
            
            if (isMipMap == true)
            {
                glGenerateMipmap(GLSubMode);
                if (GLEW_VERSION_2_1)
                {
                    glGenerateMipmap(GLSubMode);
                } else {
                    gluBuild2DMipmaps(GLSubMode,internalFormat,Width,Height,internalFormat2,internalFormat3, srcPTR);
                }
            }
            
            // unbind
            glBindTexture(GLSubMode, 0);
        }
    }
    
    void Texture::UpdateMipmap()
    {
        // bind
        glBindTexture(GLSubMode, GL_ID);
        
        if (isMipMap == true)
        {
            glGenerateMipmap(GLSubMode);
            if (GLEW_VERSION_2_1)
            {
                glGenerateMipmap(GLSubMode);
            } else {
                gluBuild2DMipmaps(GLSubMode,internalFormat,Width,Height,internalFormat2,internalFormat3, (haveImage==false?NULL:Image.getPixelsPtr()));
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