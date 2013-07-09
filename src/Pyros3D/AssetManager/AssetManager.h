//============================================================================
// Name        : AssetManager.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : AssetManager - It will provide loaders for Textures, Models, 
//               Sounds and Animations
//               It Upload Textures and Geometry to GPU (VBOS)
//============================================================================

#ifndef ASSETMANAGER_H
#define ASSETMANAGER_H

#include "../Ext/StringIDs/StringID.hpp"
#include "../Core/Buffers/GeometryBuffer.h"
#include "../AssetManager/Assets/Renderable/Renderables.h"
#include <map>
#include <vector>
#include <list>
#include <SFML/Graphics/Image.hpp>

namespace p3d {
    
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
    
    // Asset Types
    namespace AssetType {
        enum {
            Texture = 0,
            Model
        };
    };
    
    struct Asset {
        uint32 ID;
        uint32 Type;
        uint32 Using;
        IAsset* AssetPTR;
    };

    class AssetManager {
        
        friend class RenderingComponent;
        
        public:
            
            // Renderables
            static uint32 LoadModel(const std::string &ModelPath, bool smooth = false);
            static uint32 CreateCube(const f32 &width, const f32 &height, const f32 &depth, bool smooth = false);
            static uint32 CreateSphere(const f32 &radius, const uint32 &segmentsW, const uint32 &segmentsH, bool smooth = false, bool HalfSphere = false);
            static uint32 CreateCone(const f32 &radius, const f32 &height, const uint32 &segmentsW, const uint32 &segmentsH, const bool &openEnded, bool smooth = false);
            static uint32 CreateCylinder(const f32 &radius, const f32 &height, const uint32 &segmentsW, const uint32 &segmentsH, const bool &openEnded, bool smooth = false);
            static uint32 CreateCapsule(const f32 &radius, const f32 &height, const uint32 &numRings, const uint32 &segmentsW, const uint32 &segmentsH, bool smooth = false);
            static uint32 CreateTorus(const f32 &radius, const f32 &tube, const uint32 &segmentsW = 60, const uint32 segmentsH = 6, bool smooth = false);
            static uint32 CreateTorusKnot(const f32 &radius, const f32 &tube, const uint32 &segmentsW = 60, const uint32 &segmentsH = 6, const f32 &p = 2, const f32 &q = 3, const uint32 &heightscale = 1, bool smooth = false);
            static uint32 CreatePlane(const f32 &width, const f32 &height, bool smooth = false);
            static uint32 CreateCustom(Renderables::Renderable* Custom);
            static Asset* GetAsset(const uint32 &Handle);
            static void DeleteAsset(const uint32 &Handle);

            // Texture
            static uint32 LoadTexture(const std::string& FileName, const uint32 &Type,const uint32 &SubType, bool Mipmapping = true); 
            static uint32 CreateTexture(const uint32 &Type,const uint32 &SubType, const int32&width = 0, const int32&height = 0, bool Mipmapping = true);
            static uint32 CreateTexture(bool Mipmapping = true);
            static void SetMinMagFilter(const uint32 &Handle, const uint32 &MinFilter,const uint32 &MagFilter);
            static void SetRepeat(const uint32 &Handle, const uint32 &WrapS,const uint32 &WrapT);
            static void SetAnysotropy(const uint32 &Handle, const f32 &Anysotropic);
            static void SetTransparency(const uint32 &Handle, const f32 &Transparency);
            static void Resize(const uint32 &Handle, const uint32 &Width, const uint32 &Height);
            static void UpdateData(const uint32 &Handle, void* srcPTR);
            static void UpdateMipmap(const uint32 &Handle);
            static void EnableCompareMode(const uint32 &Handle);
            // Use Asset
            static void BindTexture(const uint32 &Handle);
            static void UnbindTexture(const uint32 &Handle);
            
            // Get Last Binded Texture
            static uint32 GetLastTextureBindedUnit();
        
            // Get Asset Type
            static int32 GetAssetType(const uint32 &Handle);
            
        protected:
            
            // Asset Usage
            static Asset* UseAsset(const uint32 &Handle);
            static void UnUseAsset(const uint32 &Handle);
            
        private:
            
            // List Of Assets
            static std::map<uint32, Asset*> AssetsList;
            static std::list<uint32> ModelsInUse; // For Fast Access
            static std::list<uint32> TexturesInUse; // For Fast Access
            static uint32 AssetsCount;        

            // Texture Unit Binded
            static uint32 LastUnitBinded, UnitBinded;
            
    };

}

#endif	/* ASSETMANAGER_H */
