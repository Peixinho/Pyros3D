//============================================================================
// Name        : Texture.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Texture
//============================================================================

#ifndef TEXTURE_H
#define TEXTURE_H

#include "../Utils/StringIDs/StringID.hpp"
#include "../Utils/Pointers/SuperSmartPointer.h"
#include <map>
#include <SFML/Graphics/Image.hpp>

namespace Fishy3D {
    
    namespace TextureType {
        enum {
            Texture = 0,            
            Cubemap
        };
    }

    namespace TextureTransparency {
        enum {
            Opaque = 0,
            Transparent
        };
    }

    namespace TextureFilter {
        enum {
            Nearest = 0,
            Linear,
            LinearMipmapLinear,
            LinearMipmapNearest,
            NearestMipmapNearest,
            NearestMipmapLinear
        };
    }

    namespace TextureRepeat {
        enum {
            Clamp = 0,
            ClampToBorder,
            ClampToEdge,
            Repeat
        };
    }

    namespace TextureSubType {
        enum {
            CubemapPositive_X = 0,            
            CubemapNegative_X,        
            CubemapPositive_Y,
            CubemapNegative_Y,
            CubemapPositive_Z,
            CubemapNegative_Z,
            NormalTexture,
            DepthComponent,
            FloatingPointTexture16F,
            FloatingPointTexture32F
        };
    }
    
    class Texture {
        
    private:
        
        int ID;
        std::string FileName;
        unsigned Type;
        unsigned SubType;
        int Width;
        int Height;
        sf::Image Image;
        bool haveImage;
        bool isMipMap;
        
        // GL Properties
        unsigned int Transparency;
        unsigned int MinFilter;
        unsigned int MagFilter;
        unsigned int SRepeat;
        unsigned int TRepeat;
        unsigned int mode;
        unsigned int subMode;
        unsigned int internalFormat, internalFormat2;
        float Anysotropic;        
        
        
        public:
            // Constructor
            Texture();
            
            // Texture
            bool LoadTexture(const std::string& FileName, const unsigned &Type,const unsigned &SubType, bool Mipmapping = true); 
            bool CreateTexture(const unsigned &Type,const unsigned &SubType, const int &width = 0, const int &height = 0, bool Mipmapping = true);
            bool CreateTexture(bool Mipmapping = true);
            void SetMinMagFilter(const unsigned int &MinFilter,const unsigned int &MagFilter);
            void SetRepeat(const unsigned int &WrapS,const unsigned int &WrapT);
            void SetAnysotropy(const float &Anysotropic);
            void SetTransparency(const float &Transparency);
            void Resize(const unsigned &Width, const unsigned &Height);
            void UpdateData(void* srcPTR);
            void UpdateMipmap();
            
            unsigned GetID();
            
            // Use Asset
            void Bind();
            void Unbind();
            void DeleteTexture();            
            
            // Get Last Binded Texture
            static unsigned GetLastBindedUnit();
                        
            virtual ~Texture();
                
        private:
            
            // List of Assets
            static unsigned long ListSize;                        

            // Keep Unit Binded
            static unsigned LastUnitBinded, UnitBinded;
            
    };

}

#endif	/* ASSETMANAGER_H */
