//============================================================================
// Name        : PainterPick.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Painter Pick Class
//============================================================================

#ifndef PAINTERPICK_H
#define PAINTERPICK_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/Assets/Renderable/Renderables.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>

namespace p3d {
    
    class PYROS3D_API PainterPick : public IRenderer {
    public:
        
        // Constructor
        PainterPick(const uint32 Width, const uint32 Height);
        
        // Destructor
        virtual ~PainterPick();
        
        // Resize Window
        virtual void Resize(const uint32 Width, const uint32 Height);
        
        // Returns Rendering Mesh Clicked
        RenderingMesh* PickObject(const f32 mouseX, const f32 mouseY, Projection projection, GameObject* Camera, SceneGraph* Scene);
        
    private:
        
        // Should Select Only Clickable Objects
        virtual std::vector<RenderingMesh*> GroupAndSortAssets(SceneGraph* Scene, GameObject* Camera, const uint32 Tag = 0) { std::vector<RenderingMesh*> o; return o; }
        
        // Render Scene
        virtual void RenderScene(const p3d::Projection &projection, GameObject* Camera, SceneGraph* Scene);
        
        // Material for Rendering
        GenericShaderMaterial* material;
        
        // Frame Buffer
        FrameBuffer* fbo;
        
        // Texture
        Texture* texture;
        f32 mouseX, mouseY;

        // Colors
        uint32 colors;
        
        // List
        std::map<uint32, RenderingMesh*> MeshPickingList;
        
        // Aux Methods
        Vec4 Rgba8ToVec4(const uint32 val)
        {
            return Vec4((f32)((val & 0xFF000000) >> 24), (f32)((val & 0xFF0000) >> 16), (f32)((val & 0xFF00) >> 8), (f32)(val & 0xFF)) / 255.f;
        }

        uint32 Vec4ToRgba8(Vec4 val)
        {
            val *= 255.f;
            return ((uint32(val.x) & 0xFF) << 24) | ((uint32(val.y) & 0xFF) << 16) | ((uint32(val.z) & 0xFF) << 8) | (uint32(val.w) & 0xFF);
        }

    };
}

#endif  /* PAINTERPICK */
