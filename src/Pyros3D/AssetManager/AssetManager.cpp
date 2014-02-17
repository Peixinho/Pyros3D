//============================================================================
// Name        : AssetManager.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : AssetManager
//============================================================================


#include "AssetManager.h"
#include "../Core/Logs/Log.h"
#include "Assets/Renderable/Primitives/Primitive.h"
#include "Assets/Renderable/Models/Model.h"
#include "Assets/Renderable/Primitives/Shapes/Cube.h"
#include "Assets/Renderable/Primitives/Shapes/TorusKnot.h"
#include "Assets/Renderable/Primitives/Shapes/Cone.h"
#include "Assets/Renderable/Primitives/Shapes/Cylinder.h"
#include "Assets/Renderable/Primitives/Shapes/Sphere.h"
#include "Assets/Renderable/Primitives/Shapes/Torus.h"
#include "Assets/Renderable/Primitives/Shapes/Capsule.h"
#include "Assets/Renderable/Primitives/Shapes/Plane.h"
#include "Assets/Renderable/Text/Text.h"
#include "Assets/Font/Font.h"

namespace p3d {
    
    // Textures
    std::map<uint32, Texture*> AssetManager::__TextureList;
    std::map<uint32, uint32> AssetManager::__TextureUsage;
    // Geometries
    std::map<uint32, Renderable*> AssetManager::__ModelList;
    std::map<uint32, uint32> AssetManager::__ModelUsage;
    // Fonts
    std::map<uint32, Font*> AssetManager::__FontList;
    std::map<uint32, uint32> AssetManager::__FontUsage;
    
    Renderable* AssetManager::LoadModel(const std::string& ModelPath, bool mergeMeshes, const uint32 &MaterialOptions)
    {
        uint32 ID = MakeStringID(ModelPath);
        
        if (__ModelList.find(ID)==__ModelList.end())
        {
            __ModelList[ID] = new Model(ModelPath,mergeMeshes,MaterialOptions);
            __ModelUsage[ID] = 0;
        }
        
        // Increment Usage
        __ModelUsage[ID]++;
        
        // Return PTR
        return __ModelList[ID];
    }
    Renderable* AssetManager::CreateCube(const f32& width, const f32& height, const f32& depth, bool smooth, bool flip)
    {
        std::stringstream str;
        str << "CUBE_" << width << "_" << height << "_" << depth << "_" << smooth << "_" << flip;
        uint32 ID = MakeStringID(str.str());
        
        if (__ModelList.find(ID)==__ModelList.end())
        {
            __ModelList[ID] = new Cube(width,height,depth,smooth,flip);
            __ModelUsage[ID] = 0;
        }

        // Increment Usage
        __ModelUsage[ID]++;
        
        // Return PTR
        return __ModelList[ID];
    }
    Renderable* AssetManager::CreateCapsule(const f32& radius, const f32& height, const uint32& numRings, const uint32& segmentsW, const uint32& segmentsH, bool smooth, bool flip)
    {
        std::stringstream str;
        str << "CAPSULE" << radius << "_" << height << "_" << numRings << "_" << segmentsW << "_" << segmentsH << "_" << smooth << "_" << flip;
        uint32 ID = MakeStringID(str.str());
        
        if (__ModelList.find(ID)==__ModelList.end())
        {
            __ModelList[ID] = new Capsule(radius,height,numRings,segmentsW,segmentsH,smooth,flip);
            __ModelUsage[ID] = 0;
        }

        // Increment Usage
        __ModelUsage[ID]++;
        
        // Return PTR
        return __ModelList[ID];
    }
    Renderable* AssetManager::CreateCone(const f32& radius, const f32& height, const uint32& segmentsW, const uint32& segmentsH, const bool& openEnded, bool smooth, bool flip)
    {
        std::stringstream str;
        str << "CONE" << radius << "_" << height << "_" << segmentsW << "_" << segmentsH << "_" << openEnded << "_" << smooth << "_" << flip;
        uint32 ID = MakeStringID(str.str());
        
        if (__ModelList.find(ID)==__ModelList.end())
        {
            __ModelList[ID] = new Cone(radius,height,segmentsW,segmentsH,openEnded,smooth,flip);
            __ModelUsage[ID] = 0;
        }

        // Increment Usage
        __ModelUsage[ID]++;
        
        // Return PTR
        return __ModelList[ID];
    }
    Renderable* AssetManager::CreateCylinder(const f32& radius, const f32& height, const uint32& segmentsW, const uint32& segmentsH, const bool& openEnded, bool smooth,bool flip)
    {
        std::stringstream str;
        str << "CYLINDER" << radius << "_" << height << "_" << segmentsW << "_" << segmentsH << "_" << openEnded << "_" << smooth << "_" << flip;
        uint32 ID = MakeStringID(str.str());
        
        if (__ModelList.find(ID)==__ModelList.end())
        {
            __ModelList[ID] = new Cylinder(radius,height,segmentsW,segmentsH,openEnded,smooth,flip);
            __ModelUsage[ID] = 0;
        }

        // Increment Usage
        __ModelUsage[ID]++;
        
        // Return PTR
        return __ModelList[ID];
    }
    Renderable* AssetManager::CreateSphere(const f32& radius, const uint32& segmentsW, const uint32& segmentsH, bool smooth, bool HalfSphere,bool flip)
    {
        std::stringstream str;
        str << "SPHERE" << radius << "_" << segmentsW << "_" << segmentsH << "_" << HalfSphere << "_" << smooth << "_" << flip;
        uint32 ID = MakeStringID(str.str());
        
        if (__ModelList.find(ID)==__ModelList.end())
        {
            __ModelList[ID] = new Sphere(radius,segmentsW,segmentsH,smooth,HalfSphere,flip);
            __ModelUsage[ID] = 0;
        }

        // Increment Usage
        __ModelUsage[ID]++;
        
        // Return PTR
        return __ModelList[ID];
    }
    Renderable* AssetManager::CreateTorus(const f32& radius, const f32& tube, const uint32& segmentsW, const uint32 segmentsH, bool smooth,bool flip)
    {
        std::stringstream str;
        str << "TORUS" << radius << "_" << tube << "_" << segmentsW << "_" << segmentsH << "_" << smooth << "_" << flip;
        uint32 ID = MakeStringID(str.str());
        
        if (__ModelList.find(ID)==__ModelList.end())
        {
            __ModelList[ID] = new Torus(radius,tube,segmentsW,segmentsH,smooth,flip);
            __ModelUsage[ID] = 0;
        }

        // Increment Usage
        __ModelUsage[ID]++;
        
        // Return PTR
        return __ModelList[ID];
    }
    Renderable* AssetManager::CreateTorusKnot(const f32& radius, const f32& tube, const uint32& segmentsW, const uint32& segmentsH, const f32& p, const f32& q, const uint32& heightscale, bool smooth,bool flip)
    {
        std::stringstream str;
        str << "TORUSNKOT" << radius << "_" << tube << "_" << segmentsW << "_" << segmentsH << "_" << p << "_" << q << "_" << heightscale << "_" << smooth << "_" << flip;
        uint32 ID = MakeStringID(str.str());
        
        if (__ModelList.find(ID)==__ModelList.end())
        {
            __ModelList[ID] = new TorusKnot(radius,tube,segmentsW,segmentsH,p,q,heightscale,smooth,flip);
            __ModelUsage[ID] = 0;
        }

        // Increment Usage
        __ModelUsage[ID]++;
        
        // Return PTR
        return __ModelList[ID];
    }
    Renderable* AssetManager::CreatePlane(const f32& width, const f32& height, bool smooth,bool flip)
    {
        std::stringstream str;
        str << "PLANE" << width << "_" << height << "_" << smooth << "_" << flip;
        uint32 ID = MakeStringID(str.str());
        
        if (__ModelList.find(ID)==__ModelList.end())
        {
            __ModelList[ID] = new Plane(width,height,smooth,flip);
            __ModelUsage[ID] = 0;
        }

        // Increment Usage
        __ModelUsage[ID]++;
        
        // Return PTR
        return __ModelList[ID];
    }
    void AssetManager::CreateCustom(const std::string &customName, Renderable* Custom)
    {
        CreateCustom(MakeStringID(customName), Custom);
    }
    
    void AssetManager::CreateCustom(const uint32 &customID, Renderable* Custom)
    {
        __ModelList[customID] = Custom;
        __ModelUsage[customID]=1;   
    }
    
    Font* AssetManager::CreateFont(const std::string& font, const f32& size)
    {
        std::stringstream str;
        str << font << "_" << size;
        uint32 ID = MakeStringID(str.str());
        
        if (__FontList.find(ID)==__FontList.end())
        {
            __FontList[ID] = new Font(font,size);
            __FontUsage[ID] = 0;
        }

        // Increment Usage
        __FontUsage[ID]++;
        
        // Return PTR
        return __FontList[ID];
    }
    
    Renderable* AssetManager::CreateText(Font* font, const std::string& text, const f32& charWidth, const f32& charHeight, const Vec4& color, bool DynamicText)
    {
        std::stringstream str;
        str  << "TEXT" << text << "_" << charWidth << "_" << charHeight;
        uint32 ID = MakeStringID(str.str());
        
        if (__ModelList.find(ID)==__ModelList.end())
        {
            __ModelList[ID] = new Text(font,text,charWidth,charHeight,color,DynamicText);
            __ModelUsage[ID] = 0;
        }

        // Increment Usage
        __ModelUsage[ID]++;
        
        // Return PTR
        return __ModelList[ID];
    }
    
    Renderable* AssetManager::CreateText(Font* font, const std::string& text, const f32& charWidth, const f32& charHeight, const std::vector<Vec4>& Colors, bool DynamicText)
    {
        std::stringstream str;
        str  << text << "_" << charWidth << "_" << charHeight;
        uint32 ID = MakeStringID(str.str());
        
        if (__ModelList.find(ID)==__ModelList.end())
        {
            __ModelList[ID] = new Text(font,text,charWidth,charHeight,Colors,DynamicText);
            __ModelUsage[ID] = 0;
        }

        // Increment Usage
        __ModelUsage[ID]++;
        
        // Return PTR
        return __ModelList[ID];
    }
    
    Texture* AssetManager::LoadTexture(const std::string& FileName, const uint32& Type, bool Mipmapping)
    {        
        uint32 AssetsCount = __TextureList.size();
        __TextureList[AssetsCount] = new Texture();
        __TextureList[AssetsCount]->LoadTexture(FileName,Type,Mipmapping);
        
        return __TextureList[AssetsCount];
    }
    
    void AssetManager::LoadAddTexture(const uint32 &Handle, const std::string& FileName, const uint32& Type, bool Mipmapping)
    {
        __TextureList[Handle]->LoadTexture(FileName,Type,Mipmapping);
    }
    
    Texture* AssetManager::CreateTexture(const uint32& Type, const uint32& DataType, const int32& width, const int32& height, bool Mipmapping)
    {
        
        uint32 AssetsCount = __TextureList.size();
        __TextureList[AssetsCount] = new Texture();
        __TextureList[AssetsCount]->CreateTexture(Type,DataType,width,height,Mipmapping);
        
        return __TextureList[AssetsCount];
    }
    Texture* AssetManager::CreateTexture(bool Mipmapping)
    {
        
        uint32 AssetsCount = __TextureList.size();
        __TextureList[AssetsCount] = new Texture();
        
        return __TextureList[AssetsCount];
    }
    void AssetManager::AddTexture(const uint32 &Handle, const uint32& Type, const uint32& DataType, const int32& width, const int32& height, bool Mipmapping)
    {
        __TextureList[Handle]->CreateTexture(Type,DataType,width,height,Mipmapping);
    }
    
    void AssetManager::DeleteModel(const uint32& Handle)
    {
        if (__ModelList.find(Handle)!=__ModelList.end())
            __ModelList.erase(Handle);
    }
    void AssetManager::DeleteModel(Renderable* renderable)
    {
        for (std::map<uint32, Renderable*>::iterator i=__ModelList.begin();i!=__ModelList.end();i++)
        {
            if ((*i).second==renderable)
            {
                __ModelList.erase(i);
                break;
            }
        }
    }
    
    void AssetManager::DeleteFont(const uint32& Handle)
    {
        if (__FontList.find(Handle)!=__FontList.end())
            __FontList.erase(Handle);
    }
    void AssetManager::DeleteFont(Font* font)
    {
        for (std::map<uint32, Font*>::iterator i=__FontList.begin();i!=__FontList.end();i++)
        {
            if ((*i).second==font)
            {
                __FontList.erase(i);
                break;
            }
        }
    }
    
    void AssetManager::DeleteTexture(const uint32& Handle)
    {
        if (__TextureList.find(Handle)!=__TextureList.end())
            __TextureList.erase(Handle);
    }
    void AssetManager::DeleteTexture(Texture* texture)
    {
        for (std::map<uint32, Texture*>::iterator i=__TextureList.begin();i!=__TextureList.end();i++)
        {
            if ((*i).second==texture)
            {
                __TextureList.erase(i);
                break;
            }
        }
    }
    
    uint32 AssetManager::GetModelHandle(Renderable* model) 
    {
        for (std::map<uint32,Renderable*>::iterator i=__ModelList.begin();i!=__ModelList.end();i++)
        {
            if ((*i).second == model)
            {
                return (*i).first;
            }
        }
    }
    uint32 AssetManager::GetFontHandle(Font* font)
    {
        for (std::map<uint32,Font*>::iterator i=__FontList.begin();i!=__FontList.end();i++)
        {
            if ((*i).second == font)
            {
                return (*i).first;
            }
        }
    }
    uint32 AssetManager::GetTextureHandle(Texture* texture)
    {
        for (std::map<uint32,Texture*>::iterator i=__TextureList.begin();i!=__TextureList.end();i++)
        {
            if ((*i).second == texture)
            {
                return (*i).first;
            }
        }
    }
    
    void AssetManager::DestroyAssets()
    {
        for (std::map<uint32, Renderable*>::iterator i=__ModelList.begin();i!=__ModelList.end();i++)
        {
            delete (*i).second;
        }
        
        for (std::map<uint32, Texture*>::iterator i=__TextureList.begin();i!=__TextureList.end();i++)
        {
            delete (*i).second;
        }
        
        for (std::map<uint32, Font*>::iterator i=__FontList.begin();i!=__FontList.end();i++)
        {
            delete (*i).second;
        }
        
        __ModelList.clear();
        __FontList.clear();
        __TextureList.clear();
    }
    
    Renderable* AssetManager::GetModel(const uint32& Handle)
    {
        return __ModelList[Handle];
    }
    
    Font* AssetManager::GetFont(const uint32& Handle)
    {
        return __FontList[Handle];
    }
    
    Texture* AssetManager::GetTexture(const uint32& Handle)
    {
        return __TextureList[Handle];
    }
};
