//============================================================================
// Name        : RenderingComponent
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Component For Rendering
//============================================================================

#ifndef RENDERINGCOMPONENT_H
#define	RENDERINGCOMPONENT_H

#include "../../../Components/IComponent.h"
#include "../../../AssetManager/Assets/Renderable/Renderables.h"
#include "../../../Materials/IMaterial.h"
#include "../../../AssetManager/AssetManager.h"
#include "../../../Materials/GenericShaderMaterials/GenericShaderMaterial.h"
#include "../../../SceneGraph/SceneGraph.h"
#include <vector>

namespace p3d {
    
    // Drawing Type
    namespace DrawingType
    {
        enum {
            Triangles = 0,
            Lines,
            Triangles_Fan,
            Triangles_Strip,
            Quads,
            Points,
            Polygons
        };
    }
    
    // Circular Dependency
    class RenderingComponent;
    
    class RenderingMesh {
        
        public:
            
            RenderingMesh() : drawingType(DrawingType::Triangles), CullingGeometry(0) {} // Triangles by Default

            virtual ~RenderingMesh() { if (OwnMaterial) delete Material; }
            uint32 GetDrawingType() { return drawingType; }
            // Pointer to Geometry
            Renderables::IGeometry* Geometry;
            
            // Shaders Cache
            std::map<uint32, std::vector< std::vector<int32> > > ShadersAttributesCache;
            std::map<uint32, std::vector<int32> > ShadersGlobalCache;
            std::map<uint32, std::vector<int32> > ShadersModelCache;
            std::map<uint32, std::vector<int32> > ShadersUserCache;
            
            // Materials
            IMaterial* Material;
            bool OwnMaterial;
            
            // Drawing Type
            uint32 drawingType;
            
            // Pointer to Owner
            RenderingComponent* renderingComponent;
            
            // Culling Method
            uint32 CullingGeometry;
            
    };
    
    class RenderingComponent : public IComponent {
        
        public:
            
            RenderingComponent(const uint32 &AssetID, IMaterial* Material = NULL);
            virtual ~RenderingComponent() {}
            
            virtual void Register(SceneGraph* Scene);
            virtual void Init() {}
            virtual void Update() {}
            virtual void Destroy();
            virtual void Unregister(SceneGraph* Scene);
            
            void SetCullingGeometry(const uint32 &Geometry);
            
            void EnableCastShadows();
            void DisableCastShadows();
            bool IsCastingShadows();
            
            // Get Model's Meshes
            std::vector<RenderingMesh*> GetMeshes();
            
            // Get Rendering Components
            static std::vector<IComponent*> GetComponents();
            // Get Global Meshes
            static std::vector<RenderingMesh*> GetRenderingMeshes(SceneGraph* scene);
            
        protected:

            // Keep Asset ID in use
            uint32 AssetID;

            // Casting Shadows
            bool isCastingShadows;
            
            std::vector<RenderingMesh*> Meshes;
            
            // Culling Geometry
            uint32 CullingGeometry;
            
            // INTERNAL - Components of this Type
            static std::vector<IComponent*> Components;
            // INTERNAL - Renderables on the Scene
            static std::map<SceneGraph*, std::vector<RenderingMesh*> > MeshesOnScene;
            
    };
    
};

#endif /* RENDERINGCOMPONENT_H */