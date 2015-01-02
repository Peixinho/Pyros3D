//============================================================================
// Name        : RenderingComponent
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Component For Rendering
//============================================================================

#ifndef RENDERINGCOMPONENT_H
#define	RENDERINGCOMPONENT_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Assets/Renderable/Renderables.h>
#include <Pyros3D/Assets/Renderable/Models/Model.h>
#include <Pyros3D/Materials/IMaterial.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <vector>

namespace p3d {
    
    // Drawing Type
    namespace DrawingType
    {
        enum {
            Triangles = 0,
            Lines,
            Line_Loop,
            Line_Strip,
            Triangle_Fan,
            Triangle_Strip,
            Quads,
            Points,
            Polygons
        };
    }
    
    // Circular Dependency
    class PYROS3D_API RenderingComponent;
    
    class PYROS3D_API RenderingMesh {
        
        public:
            
            RenderingMesh(const uint32 &lod = 0) : drawingType(DrawingType::Triangles), CullingGeometry(0), Active(true), Clickable(true), LodLevel(lod) {} // Triangles by Default

            virtual ~RenderingMesh() {}
            
            uint32 GetDrawingType() { return drawingType; }
            
            // Pointer to Geometry
            IGeometry* Geometry;
            
            // Shaders Cache
            std::map<uint32, std::vector< std::vector<int32> > > ShadersAttributesCache;
            std::map<uint32, std::vector<int32> > ShadersGlobalCache;
            std::map<uint32, std::vector<int32> > ShadersModelCache;
            std::map<uint32, std::vector<int32> > ShadersUserCache;
            
            // Materials
            IMaterial* Material;
            
            // Drawing Type
            uint32 drawingType;
            
            // Pointer to Owner
            RenderingComponent* renderingComponent;
            
            // Culling Method
            uint32 CullingGeometry;
            
            // Pivot
            Matrix Pivot;
            
            // Clickable
            bool Clickable, Active;

            // Map Bone ID's
            std::map<int32, int32> MapBoneIDs;
            // Bone Offset Matrix
            std::map<int32, Matrix> BoneOffsetMatrix;
            // Bones Matrix List
            std::vector<Matrix> SkinningBones;

            // LOD
            uint32 LodLevel;

    };
    
    class PYROS3D_API RenderingComponent : public IComponent {
        
        public:
            
            RenderingComponent(Renderable* renderable, IMaterial* Material = NULL);
            void AddLOD(Renderable* renderable, f32 Distance, IMaterial* Material = NULL);
            virtual ~RenderingComponent();
        
            virtual void Register(SceneGraph* Scene);
            virtual void Init() {}
            virtual void Update() {}
            virtual void Destroy() {}
            virtual void Unregister(SceneGraph* Scene);
            
            void SetCullingGeometry(const uint32 &Geometry);
            
            void EnableCastShadows();
            void DisableCastShadows();
            bool IsCastingShadows();
            
            // Get Model Skeleton
            const std::map<StringID, Bone> &GetSkeleton() const { return skeleton; }
            bool HasBones() { return hasBones; }

            // Get Model's Meshes
            std::vector<RenderingMesh*> &GetMeshes(const uint32 &LODLevel = 0);
            
            // Get LOD Number
            const uint32 GetLODSize() const;
            
            // Returns LOD level based on distance
            const uint32 GetLODByDistance(const f32 &Distance) const;

            // Update Rendering Meshes Based on LOD
            void UpdateLOD(const uint32 &lod);

            // Get Bounding Properties
            const f32 &GetBoundingSphereRadius() const { return BoundingSphereRadius; }
            const Vec3 &GetBoundingSphereCenter() const { return BoundingSphereCenter; }
            const Vec3 &GetBoundingMinValue() const { return minBounds; }
            const Vec3 &GetBoundingMaxValue() const { return maxBounds; }

            // Get Rendering Components
            static std::vector<IComponent*> &GetComponents();

            // Get Global Meshes
            static std::vector<RenderingMesh*> &GetRenderingMeshes(SceneGraph* scene);
            
            // Get Global Meshes
            static std::vector<RenderingComponent*> &GetRenderingComponents(SceneGraph* scene);

        protected:

            // Casting Shadows
            bool isCastingShadows;
        
            // List of Meshes of this Model
            std::map< uint32, std::vector<RenderingMesh*> > Meshes;

            // Skeleton
            std::map<StringID, Bone> skeleton;

            // Bones Flag
            bool hasBones;

            // Culling Geometry
            uint32 CullingGeometry;
            
            // Scene
            SceneGraph* Scene;

            // Bounds of the Whole Model
            f32 BoundingSphereRadius;
            Vec3 BoundingSphereCenter;
            Vec3 maxBounds, minBounds;

            // LOD
            bool LOD;

            // LOD Rendered
            uint32 LodInUse;

            //LOD Distance
            std::vector<f32> LODDistances;

            // INTERNAL - Components of this Type
            static std::vector<IComponent*> Components;
            
            // INTERNAL - Renderables on the Scene
            static std::map<SceneGraph*, std::vector<RenderingMesh*> > MeshesOnScene;
            static std::map<SceneGraph*, std::vector<RenderingComponent*> > RenderingComponentsOnScene;
            
    };
    
};

#endif /* RENDERINGCOMPONENT_H */