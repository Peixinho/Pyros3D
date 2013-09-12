//============================================================================
// Name        : Texture.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Asset Interface
//============================================================================

#ifndef TEXTURE_H
#define TEXTURE_H

#include "../IAsset.h"
#include "../../../Core/Logs/Log.h"
#include <SFML/Graphics.hpp>
namespace p3d {
    
    class Texture : public IAsset {
        
        private:
            
            // Internal ID for GL
            int32 GL_ID;
            // FileName
            std::string FileName;
            uint32 Type;
            uint32 DataType;
            int32 Width;
            int32 Height;
            sf::Image Image;
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
            f32 Anysotropic;
        
        
        public:
            // Constructor
            Texture();
            
            // Texture
            bool LoadTexture(const std::string& FileName, const uint32 &Type,bool Mipmapping = true); 
            bool CreateTexture(const uint32 &Type, const uint32 &DataType, const int32 &width = 0, const int32 &height = 0, bool Mipmapping = true);
            bool CreateTexture(bool Mipmapping = true);
            void SetMinMagFilter(const uint32 &MinFilter,const uint32 &MagFilter);
            void SetRepeat(const uint32 &WrapS,const uint32 &WrapT, const int32 &WrapR = -1);
            void EnableCompareMode();
            void SetAnysotropy(const f32 &Anysotropic);
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
                        
            virtual ~Texture();
            
            virtual void Dispose();
            
            // Keep Unit Binded
            static uint32 LastUnitBinded, UnitBinded;
        
    };
    
};

#endif /* TEXTURE_H */