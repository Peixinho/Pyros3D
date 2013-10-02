//============================================================================
// Name        : Texture.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Texture
//============================================================================


#include "Texture.h"
#include "GL/glew.h"

#define GLCHECK() { int error = glGetError(); if(error != GL_NO_ERROR) { std::cout <<  "GL Error: " << std::hex << error << std::endl; } }

namespace Fishy3D {

    unsigned long Texture::ListSize;
    unsigned Texture::UnitBinded = 0;
    unsigned Texture::LastUnitBinded = 0;
    
    Texture::Texture() : ID(-1), haveImage(false), isMipMap(false)
    {
        // reset the assets list size
        ListSize = 0;
    }

    Texture::~Texture() {}   
    
    bool Texture::LoadTexture(const std::string& FileName, const unsigned &Type,const unsigned &SubType, bool Mipmapping)
    {
        sf::Image image;
        if (image.loadFromFile(FileName)) {        

            this->FileName=FileName;                        
            this->Width=image.getSize().x;
            this->Height=image.getSize().y;
            this->Image=image;
            this->haveImage=true;
            this->Type=Type;
            this->SubType=SubType;
            this->Transparency=TextureTransparency::Opaque;
            
            if (this->ID==-1) {
                glGenTextures(1, (GLuint*)&this->ID);
            }
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
                break;
            case TextureSubType::CubemapNegative_Y:
                subMode=GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
                mode=GL_TEXTURE_CUBE_MAP;
                internalFormat=GL_RGBA8;
                internalFormat2=GL_RGBA;
                break;
            case TextureSubType::CubemapNegative_Z:
                subMode=GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
                mode=GL_TEXTURE_CUBE_MAP;
                internalFormat=GL_RGBA8;
                internalFormat2=GL_RGBA;
                break;
            case TextureSubType::CubemapPositive_X:
                subMode=GL_TEXTURE_CUBE_MAP_POSITIVE_X;
                mode=GL_TEXTURE_CUBE_MAP;
                internalFormat=GL_RGBA8;
                internalFormat2=GL_RGBA;
                break;
            case TextureSubType::CubemapPositive_Y:
                subMode=GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
                mode=GL_TEXTURE_CUBE_MAP;
                internalFormat=GL_RGBA8;
                internalFormat2=GL_RGBA;
                break;
            case TextureSubType::CubemapPositive_Z:
                subMode=GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
                mode=GL_TEXTURE_CUBE_MAP;
                internalFormat=GL_RGBA8;
                internalFormat2=GL_RGBA;
                break;
            case TextureSubType::DepthComponent:
                subMode=GL_TEXTURE_2D;                
                mode=GL_TEXTURE_2D;
                internalFormat=GL_DEPTH_COMPONENT;
                internalFormat2=GL_DEPTH_COMPONENT;
                break;
            case TextureSubType::FloatingPointTexture16F:
                mode=GL_TEXTURE_2D;
                subMode=GL_TEXTURE_2D;
                internalFormat=GL_RGBA16F;
                internalFormat2=GL_RGBA;
                break;
            case TextureSubType::FloatingPointTexture32F:
                mode=GL_TEXTURE_2D;
                subMode=GL_TEXTURE_2D;
                internalFormat=GL_RGBA32F;
                internalFormat2=GL_RGBA;
                break;
            case TextureSubType::NormalTexture:
            default:
                mode=GL_TEXTURE_2D;
                subMode=GL_TEXTURE_2D;
                internalFormat=GL_RGBA8;
                internalFormat2=GL_RGBA;
                break;
        }                        
        
        // Set Default Filtering (LINEAR)
        SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
        
        // bind
        glBindTexture(mode, ID);
        // default values
        glTexParameteri(mode, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(mode, GL_TEXTURE_WRAP_T, GL_REPEAT);
        if (Mipmapping == true)
        {
			if (GLEW_VERSION_2_1)
            {
				glTexImage2D(subMode,0,internalFormat, Width, Height, 0,internalFormat2,GL_UNSIGNED_BYTE, (haveImage==false?NULL:Image.getPixelsPtr()));
                glGenerateMipmap(mode);
            } else {
				gluBuild2DMipmaps(subMode,internalFormat,Width,Height,internalFormat2,GL_UNSIGNED_BYTE, (haveImage==false?NULL:Image.getPixelsPtr()));
			}
			isMipMap = true;
        } else {
			glTexImage2D(subMode,0,internalFormat, Width, Height, 0,internalFormat2,GL_UNSIGNED_BYTE, (haveImage==false?NULL:Image.getPixelsPtr()));
		}
        
        // unbind
        glBindTexture(mode, 0);

        return true;
    }
    
    bool Texture::CreateTexture(const unsigned& Type, const unsigned& SubType, const int& width, const int& height, bool Mipmapping)
    {
                       
        Width=width;
        Height=height;
        this->Type=Type;
        this->SubType=SubType;

        if (ID==-1) {
            glGenTextures(1, (GLuint*)&ID);
        }
        
        // Create Texture
        CreateTexture(Mipmapping);

        return true;
    }
    
    void Texture::SetAnysotropy(const float& Anysotropic)
    {
        // bind
         glBindTexture(mode, ID);
         
         this->Anysotropic = Anysotropic;
         
         if (Anysotropic>0.0)
         {
            float AnysotropicMax;
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
    
    void Texture::SetRepeat(const unsigned int& WrapS, const unsigned int& WrapT)
    {
        
        SRepeat = WrapS;
        TRepeat = WrapT;
        
        // bind
         glBindTexture(mode, ID);
        
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
    
    void Texture::SetMinMagFilter(const unsigned int& MinFilter, const unsigned int& MagFilter)
    {
        
        this->MinFilter = MinFilter;
        this->MagFilter = MagFilter;
        
        // bind
        glBindTexture(mode, ID);
        
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
    void Texture::SetTransparency(const float& Transparency)
    {
        
        this->Transparency  = Transparency;
        
    }
    
    void Texture::Resize(const unsigned& Width, const unsigned& Height)
    {
        glBindTexture(mode, ID);
        this->Width=Width;
        this->Height=Height;
        glTexImage2D(subMode,0,internalFormat, Width, Height, 0,internalFormat2,GL_UNSIGNED_BYTE, (haveImage==false?NULL:Image.getPixelsPtr()));
        
        if (isMipMap == true)
        {
            glGenerateMipmap(mode);
			if (GLEW_VERSION_2_1)
            {
                glGenerateMipmap(mode);
            } else {
				gluBuild2DMipmaps(subMode,internalFormat,Width,Height,internalFormat2,GL_UNSIGNED_BYTE, (haveImage==false?NULL:Image.getPixelsPtr()));
			}
		}
        
        glBindTexture(mode, 0);
    }
    
    void Texture::UpdateData(void* srcPTR)
    {
        if (ID>0)
        {
            // bind
            glBindTexture(mode, ID);
            glTexImage2D(subMode,0,internalFormat, Width, Height, 0,internalFormat2,GL_UNSIGNED_BYTE, srcPTR);
            
        if (isMipMap == true)
        {
            glGenerateMipmap(mode);
			if (GLEW_VERSION_2_1)
            {
                glGenerateMipmap(mode);
            } else {
				gluBuild2DMipmaps(subMode,internalFormat,Width,Height,internalFormat2,GL_UNSIGNED_BYTE, srcPTR);
			}
		}
            
            // unbind
            glBindTexture(mode, 0);   
        }
    }
    
    void Texture::UpdateMipmap()
    {
        // bind
        glBindTexture(mode, ID);
        
        if (isMipMap == true)
        {
            glGenerateMipmap(mode);
			if (GLEW_VERSION_2_1)
            {
                glGenerateMipmap(mode);
            } else {
				gluBuild2DMipmaps(subMode,internalFormat,Width,Height,internalFormat2,GL_UNSIGNED_BYTE, (haveImage==false?NULL:Image.getPixelsPtr()));
			}
		}
        
        // unbind
        glBindTexture(mode, 0);           
    }
    
    void Texture::Bind()
    {
        glActiveTexture(GL_TEXTURE0 + UnitBinded);
        glBindTexture(mode, ID);
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
    
    unsigned Texture::GetLastBindedUnit()
    { 
        return LastUnitBinded;
    }
    
    void Texture::DeleteTexture()
    {
        if (ID!=-1)
        glDeleteTextures (1, (GLuint*)&ID);
    }
    
    unsigned Texture::GetID()
    {
        return ID;
    }
    
}
