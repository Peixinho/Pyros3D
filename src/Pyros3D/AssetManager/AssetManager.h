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
#include "Assets/Font/Font.h"
#include <map>
#include <vector>
#include <list>
#include <SFML/Graphics/Image.hpp>

#undef CreateFont

namespace p3d {
    
    class AssetManager {
        
        friend class RenderingComponent;
        
        public:
            
            // Renderables
            static Renderable* LoadModel(const std::string &ModelPath, bool smooth = false, bool mergeMeshes = true);
            static Renderable* CreateCube(const f32 &width, const f32 &height, const f32 &depth, bool smooth = false, bool flip = false);
            static Renderable* CreateSphere(const f32 &radius, const uint32 &segmentsW = 16, const uint32 &segmentsH = 8, bool smooth = false, bool HalfSphere = false, bool flip = false);
            static Renderable* CreateCone(const f32 &radius, const f32 &height, const uint32 &segmentsW, const uint32 &segmentsH, const bool &openEnded, bool smooth = false, bool flip = false);
            static Renderable* CreateCylinder(const f32 &radius, const f32 &height, const uint32 &segmentsW, const uint32 &segmentsH, const bool &openEnded, bool smooth = false, bool flip = false);
            static Renderable* CreateCapsule(const f32 &radius, const f32 &height, const uint32 &numRings, const uint32 &segmentsW, const uint32 &segmentsH, bool smooth = false, bool flip = false);
            static Renderable* CreateTorus(const f32 &radius, const f32 &tube, const uint32 &segmentsW = 60, const uint32 segmentsH = 6, bool smooth = false, bool flip = false);
            static Renderable* CreateTorusKnot(const f32 &radius, const f32 &tube, const uint32 &segmentsW = 60, const uint32 &segmentsH = 6, const f32 &p = 2, const f32 &q = 3, const uint32 &heightscale = 1, bool smooth = false, bool flip = false);
            static Renderable* CreatePlane(const f32 &width, const f32 &height, bool smooth = false, bool flip = false);
            static void CreateCustom(const std::string &customName, Renderable* Custom);
            static void CreateCustom(const uint32 &custom, Renderable* Custom);
            
            // Font and Text Rendering
            static Font* CreateFont(const std::string &font,const f32 &size);
            static Renderable* CreateText(Font* font, const std::string &text, const f32 &charWidth, const f32 &charHeight, const Vec4 &color = Vec4(1,1,1,1), bool DynamicText = false);
            static Renderable* CreateText(Font* font, const std::string &text, const f32 &charWidth, const f32 &charHeight, const std::vector<Vec4> &Color, bool DynamicText = false);
            
            // Texture
            static Texture* LoadTexture(const std::string& FileName, const uint32 &Type, bool Mipmapping = true); 
            static void LoadAddTexture(const uint32 &Handle, const std::string& FileName, const uint32 &Type, bool Mipmapping = true); 
            static Texture* CreateTexture(const uint32 &Type, const uint32 &DataType, const int32&width = 0, const int32&height = 0, bool Mipmapping = true);
            static void AddTexture(const uint32 &Handle, const uint32 &Type, const uint32 &DataType, const int32&width = 0, const int32&height = 0, bool Mipmapping = true);
            static Texture* CreateTexture(bool Mipmapping = true);
            
            // Get and Delete Asset
            static Renderable* GetModel(const uint32 &Handle);
            static Font* GetFont(const uint32 &Handle);
            static Texture* GetTexture(const uint32 &Handle);
            
            // Delete
            static void DeleteModel(const uint32 &Handle);
            static void DeleteModel(Renderable* renderable);
            static void DeleteFont(const uint32 &Handle);
            static void DeleteFont(Font* font);
            static void DeleteTexture(const uint32 &Handle);
            static void DeleteTexture(Texture* texture);
        
            // Get Handles
            static uint32 GetModelHandle(Renderable* model);
            static uint32 GetFontHandle(Font* font);
            static uint32 GetTextureHandle(Texture* texture);
            
            // Destroy All Assets
            static void DestroyAssets();
            
        private:
            
            // Textures
            static std::map<uint32, Texture*> __TextureList;
            static std::map<uint32, uint32> __TextureUsage;
            // Geometries
            static std::map<uint32, Renderable*> __ModelList;
            static std::map<uint32, uint32> __ModelUsage;
            // Fonts
            static std::map<uint32, Font*> __FontList;
            static std::map<uint32, uint32> __FontUsage;
            
    };

}

#endif	/* ASSETMANAGER_H */
