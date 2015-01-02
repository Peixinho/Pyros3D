//============================================================================
// Name        : Texture.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Texture
//============================================================================

#ifndef TEXTURE_H
#define TEXTURE_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Core/File/File.h>
#include <map>
#include <vector>

namespace p3d {
    
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

    namespace TextureDataType {
        enum {
            RGBA = 0,
            BGR,
            BGRA,
            DepthComponent,
            DepthComponent16,
            DepthComponent24,
            DepthComponent32,
            R,
            R16F,
            R32F,
            R16I,
            R32I,
            RG,
            RG16F,
            RG32F,
            RG16I,
            RG32I,
            RGB,
            RGB16F,
            RGB32F,
            RGB16I,
            RGB32I,
            RGBA16F,
            RGBA32F,
            RGBA16I,
            RGBA32I,
            ALPHA
        };
    }
    
    namespace TextureType {
        enum {
            CubemapPositive_X = 0,
            CubemapNegative_X,
            CubemapPositive_Y,
            CubemapNegative_Y,
            CubemapPositive_Z,
            CubemapNegative_Z,
            Texture
        };
    }
    
    
    struct PYROS3D_API __Texture {
        
        uint32 TextureID;
        uint32 Using;
        uint32 Type;
        uint32 DataType;
        uint32 Width;
        uint32 Height;
        std::string Filename;

        std::vector<uchar> Image;
        const uchar* GetPixels() { return &Image[0]; }

        __Texture() : Using(0), Type(TextureType::Texture), DataType(TextureDataType::RGBA) {}
        
    };

    class PYROS3D_API Texture {
        
        private:
            
            // Internal ID for GL
            int32 GL_ID;
            uint32 Type;
            uint32 DataType;
            uint32 Width;
            uint32 Height;
            uint32 TextureInternalID;
            bool haveImage;
            bool isMipMap;
            // Image Data
            std::vector<uchar> pixels;
            bool pixelsRetrieved;

            // GL Properties
            uint32 Transparency;
            uint32 MinFilter;
            uint32 MagFilter;
            uint32 SRepeat;
            uint32 TRepeat;
            uint32 RRepeat;
            uint32 mode, GLMode;
            uint32 subMode, GLSubMode;
            uint32 internalFormat, internalFormat2, internalFormat3;
            uint32 Anysotropic;
        
            // List of textures
            static std::map<uint32, __Texture> __Textures;
            
        
        public:
            // Constructor
            Texture();
            
            // Texture
            bool LoadTexture(const std::string& Filename, const uint32 &Type = TextureType::Texture, bool Mipmapping = true);
            bool LoadTextureFromMemory(std::vector<uchar> data, const uint32 &length, const uint32 &Type = TextureType::Texture, bool Mipmapping = true);
            bool CreateTexture(const uint32 &Type, const uint32 &DataType, const int32 &width = 0, const int32 &height = 0, bool Mipmapping = true);
            bool CreateTexture(bool Mipmapping = true);
            void SetMinMagFilter(const uint32 &MinFilter,const uint32 &MagFilter);
            void SetRepeat(const uint32 &WrapS,const uint32 &WrapT, const int32 &WrapR = -1);
            void EnableCompareMode();
            void SetAnysotropy(const uint32 &Anysotropic);
            void SetTransparency(const f32 &Transparency);
            void Resize(const uint32 &Width, const uint32 &Height);
            void UpdateData(void* srcPTR);
            void UpdateMipmap();
            void SetTextureByteAlignment(const uint32 &Value);
            const uint32 GetBindID() const;
            const uint32 GetWidth() const;
            const uint32 GetHeight() const;
            // Use Asset
            void Bind();
            void Unbind();
            void DeleteTexture();  
            
            // Get Texture Data
            std::vector<uchar> GetTextureData();
            
            // Get Last Binded Texture
            static uint32 GetLastBindedUnit();
            
            // Destructor
            virtual ~Texture();
            
            // Keep Unit Binded
            static uint32 LastUnitBinded;
            static uint32 UnitBinded;
        
    };
    
};

#endif /* TEXTURE_H */
