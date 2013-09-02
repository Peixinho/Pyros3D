//============================================================================
// Name        : AssetManager.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : AssetManager
//============================================================================


#include "AssetManager.h"
#include "../Core/Logs/Log.h"
#include "GL/glew.h"
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
#include "Assets/Texture/Texture.h"
#include "Assets/Font/Font.h"

namespace p3d {
    
    // Assets List
    std::map<uint32, Asset*> AssetManager::AssetsList;
    
    // Assets Count
    uint32 AssetManager::AssetsCount = 0;
    
    uint32 AssetManager::LoadModel(const std::string& ModelPath, bool smooth)
    {
        AssetsCount++;
        AssetsList[AssetsCount] = new Asset();
        AssetsList[AssetsCount]->AssetPTR = new Model(ModelPath);
        AssetsList[AssetsCount]->Using = 0;

        return AssetsCount;
    }
    uint32 AssetManager::CreateCube(const f32& width, const f32& height, const f32& depth, bool smooth, bool flip)
    {
        AssetsCount++;
        AssetsList[AssetsCount] = new Asset();
        AssetsList[AssetsCount]->AssetPTR = new Cube(width,height,depth,smooth,flip);
        AssetsList[AssetsCount]->Using = 0;

        return AssetsCount;
    }
    uint32 AssetManager::CreateCapsule(const f32& radius, const f32& height, const uint32& numRings, const uint32& segmentsW, const uint32& segmentsH, bool smooth, bool flip)
    {
        AssetsCount++;
        AssetsList[AssetsCount] = new Asset();
        AssetsList[AssetsCount]->AssetPTR = new Capsule(radius,height,numRings,segmentsW,segmentsH,smooth,flip);
        AssetsList[AssetsCount]->Using = 0;

        return AssetsCount;
    }
    uint32 AssetManager::CreateCone(const f32& radius, const f32& height, const uint32& segmentsW, const uint32& segmentsH, const bool& openEnded, bool smooth, bool flip)
    {
        AssetsCount++;
        AssetsList[AssetsCount] = new Asset();
        AssetsList[AssetsCount]->AssetPTR = new Cone(radius,height,segmentsW,segmentsH,openEnded,smooth,flip);
        AssetsList[AssetsCount]->Using = 0;

        return AssetsCount;
    }
    uint32 AssetManager::CreateCylinder(const f32& radius, const f32& height, const uint32& segmentsW, const uint32& segmentsH, const bool& openEnded, bool smooth,bool flip)
    {
        AssetsCount++;
        AssetsList[AssetsCount] = new Asset();
        AssetsList[AssetsCount]->AssetPTR = new Cylinder(radius,height,segmentsW,segmentsH,openEnded,smooth,flip);
        AssetsList[AssetsCount]->Using = 0;

        return AssetsCount;
    }
    uint32 AssetManager::CreateSphere(const f32& radius, const uint32& segmentsW, const uint32& segmentsH, bool smooth, bool HalfSphere,bool flip)
    {
        AssetsCount++;
        AssetsList[AssetsCount] = new Asset();
        AssetsList[AssetsCount]->AssetPTR = new Sphere(radius,segmentsW,segmentsH,smooth,HalfSphere,flip);
        AssetsList[AssetsCount]->Using = 0;

        return AssetsCount;
    }
    uint32 AssetManager::CreateTorus(const f32& radius, const f32& tube, const uint32& segmentsW, const uint32 segmentsH, bool smooth,bool flip)
    {
        AssetsCount++;
        AssetsList[AssetsCount] = new Asset();
        AssetsList[AssetsCount]->AssetPTR = new Torus(radius,tube,segmentsW,segmentsH,smooth,flip);
        AssetsList[AssetsCount]->Using = 0;

        return AssetsCount;
    }
    uint32 AssetManager::CreateTorusKnot(const f32& radius, const f32& tube, const uint32& segmentsW, const uint32& segmentsH, const f32& p, const f32& q, const uint32& heightscale, bool smooth,bool flip)
    {
        AssetsCount++;
        AssetsList[AssetsCount] = new Asset();
        AssetsList[AssetsCount]->AssetPTR = new TorusKnot(radius,tube,segmentsW,segmentsH,p,q,heightscale,smooth,flip);
        AssetsList[AssetsCount]->Using = 0;

        return AssetsCount;
    }
    uint32 AssetManager::CreatePlane(const f32& width, const f32& height, bool smooth,bool flip)
    {
        AssetsCount++;
        AssetsList[AssetsCount] = new Asset();
        AssetsList[AssetsCount]->AssetPTR = new Plane(width,height,smooth,flip);
        AssetsList[AssetsCount]->Using = 0;

        return AssetsCount;
    }
    uint32 AssetManager::CreateCustom(Renderables::Renderable* Custom)
    {
        AssetsCount++;
        AssetsList[AssetsCount] = new Asset();
        AssetsList[AssetsCount]->AssetPTR = Custom;
        AssetsList[AssetsCount]->Using = 0;

        
        return AssetsCount;
    }
    
    uint32 AssetManager::CreateFont(const std::string& font, const f32& size)
    {
        AssetsCount++;
        AssetsList[AssetsCount] = new Asset();
        AssetsList[AssetsCount]->AssetPTR = new Font(font,size);
        AssetsList[AssetsCount]->Using = 0;
        
        return AssetsCount;
    }
    
    uint32 AssetManager::CreateText(const uint32& Handle, const std::string& text, const f32 &charWidth, const f32 &charHeight)
    {
        AssetsCount++;
        AssetsList[AssetsCount] = new Asset();
        AssetsList[AssetsCount]->AssetPTR = new Text(Handle,text,charWidth,charHeight);
        AssetsList[AssetsCount]->Using = 0;
        return AssetsCount;
    }
    
    Asset* AssetManager::UseAsset(const uint32 &Handle)
    {
        try {
            if (AssetsList.find(Handle)!=AssetsList.end())
            {
                AssetsList[Handle]->Using++;
                return AssetsList[Handle];
            }
            throw "ERROR: Asset Not Found";
        } catch(const std::string &msg)
        {
            echo(msg);
        }
    }
    void AssetManager::UnUseAsset(const uint32 &Handle)
    {
        try {
            if (AssetsList.find(Handle)!=AssetsList.end())
            {
                AssetsList[Handle]->Using--;
            }
            throw "ERROR: Asset Not Found";
        } catch(const std::string &msg)
        {
            echo(msg);
        }
    }
    Asset* AssetManager::GetAsset(const uint32 &Handle)
    {
        try {
            if (AssetsList.find(Handle)!=AssetsList.end())
            {
                return AssetsList[Handle];
            }
            throw "ERROR: Asset Not Found";
        } catch(const std::string &msg)
        {
            echo(msg);
        }
    }    
    void AssetManager::DeleteAsset(const uint32 &Handle)
    {
        try {
            if (AssetsList.find(Handle)!=AssetsList.end())
            {
                try {
                    if (AssetsList[Handle]->Using==0)
                    {
                        // Dipose Asset
                        AssetsList[Handle]->AssetPTR->Dispose();
                        // Delete Pointer
                        delete AssetsList[Handle]->AssetPTR;
                        delete AssetsList[Handle];
                        // Remove From Assets List
                        AssetsList.erase(Handle);
                        echo("SUCCESS: Asset Deleted");
                        
                    } else {
                        throw "ERROR: Asset is in Use";
                    }
                }
                catch (const std::string &msg)
                {
                    echo(msg);
                }
            } else {
                throw "ERROR: Asset Not Found";
            }
        } catch(const std::string &msg)
        {
            echo(msg);
        }
    }
    
    uint32 AssetManager::LoadTexture(const std::string& FileName, const uint32& Type, bool Mipmapping)
    {
        Texture* texture = new Texture();
        if (texture->LoadTexture(FileName,Type,Mipmapping)) AssetsCount++;
        AssetsList[AssetsCount]=new Asset();
        AssetsList[AssetsCount]->AssetPTR = texture;
        AssetsList[AssetsCount]->Using = 0;
        
        return AssetsCount;
    }
    
    void AssetManager::LoadAddTexture(const uint32 &Handle, const std::string& FileName, const uint32& Type, bool Mipmapping)
    {
        if (AssetsList.find(Handle)!=AssetsList.end())
        {
            Texture* texture = static_cast<Texture*>(AssetsList[Handle]->AssetPTR);
            texture->LoadTexture(FileName,Type,Mipmapping);
        }
    }
    
    uint32 AssetManager::CreateTexture(const uint32& Type, const uint32& DataType, const int32& width, const int32& height, bool Mipmapping)
    {
        AssetsCount++;
        Texture* texture = new Texture();
        texture->CreateTexture(Type,DataType,width,height,Mipmapping);
        AssetsList[AssetsCount]=new Asset();
        AssetsList[AssetsCount]->AssetPTR = texture;
        AssetsList[AssetsCount]->Using = 0;
        
        return AssetsCount;
    }
    void AssetManager::AddTexture(const uint32 &Handle, const uint32& Type, const uint32& DataType, const int32& width, const int32& height, bool Mipmapping)
    {
        if (AssetsList.find(Handle)!=AssetsList.end())
        {
            Texture* texture = static_cast<Texture*>(AssetsList[Handle]->AssetPTR);
            texture->CreateTexture(Type,DataType,width,height,Mipmapping);        
        } else echo("ERROR: Texture Not Found");
    }
    
    void AssetManager::SetAnysotropy(const uint32 &Handle, const f32& Anysotropic)
    {
        Texture* texture = static_cast<Texture*> (AssetsList[Handle]->AssetPTR);
        texture->SetAnysotropy(Anysotropic);
    }
    
    void AssetManager::SetRepeat(const uint32 &Handle,const uint32& WrapS, const uint32& WrapT)
    {
        Texture* texture = static_cast<Texture*> (AssetsList[Handle]->AssetPTR);
        texture->SetRepeat(WrapS,WrapT);
    }
    
    void AssetManager::SetMinMagFilter(const uint32 &Handle,const uint32& MinFilter, const uint32& MagFilter)
    {
        Texture* texture = static_cast<Texture*> (AssetsList[Handle]->AssetPTR);
        texture->SetMinMagFilter(MinFilter,MagFilter);
    }
    
    void AssetManager::EnableCompareMode(const uint32& Handle)
    {
        Texture* texture = static_cast<Texture*> (AssetsList[Handle]->AssetPTR);
        texture->EnableCompareMode();
    }
    
    void AssetManager::SetTextureByteAlignment(const uint32& Handle, const uint32& Value)
    {
        Texture* texture = static_cast<Texture*> (AssetsList[Handle]->AssetPTR);
        texture->SetTextureByteAlignment(Value);
    }
    
    void AssetManager::SetTransparency(const uint32 &Handle,const f32& Transparency)
    {
        Texture* texture = static_cast<Texture*> (AssetsList[Handle]->AssetPTR);
        texture->SetTransparency(Transparency);        
    }
    
    void AssetManager::Resize(const uint32 &Handle,const uint32& Width, const uint32& Height)
    {
        Texture* texture = static_cast<Texture*> (AssetsList[Handle]->AssetPTR);
        texture->Resize(Width,Height);
    }
    
    void AssetManager::UpdateData(const uint32 &Handle,void* srcPTR)
    {
        Texture* texture = static_cast<Texture*> (AssetsList[Handle]->AssetPTR);
        texture->UpdateData(srcPTR);
    }
    
    void AssetManager::UpdateMipmap(const uint32 &Handle)
    {
        Texture* texture = static_cast<Texture*> (AssetsList[Handle]->AssetPTR);
        texture->UpdateMipmap();          
    }
    void AssetManager::BindTexture(const uint32 &Handle)
    {
        Texture* texture = static_cast<Texture*> (AssetsList[Handle]->AssetPTR);
        texture->Bind();
    }
    void AssetManager::UnbindTexture(const uint32 &Handle)
    {
        Texture* texture = static_cast<Texture*> (AssetsList[Handle]->AssetPTR);
        texture->Unbind();
    }
    
    // Get Last Binded Texture
    uint32 AssetManager::GetLastTextureBindedUnit()
    {
        return Texture::LastUnitBinded;
    }
    
    int32 AssetManager::GetAssetType(const uint32& Handle)
    {
        try {
            if (AssetsList.find(Handle)!=AssetsList.end())
                return AssetsList[Handle]->Type;
            else {
                throw "ERROR: Asset Not Found";
            }
        } catch (const std::string &msg)
        {
            echo(msg);
            return -1;
        }
    }
};
