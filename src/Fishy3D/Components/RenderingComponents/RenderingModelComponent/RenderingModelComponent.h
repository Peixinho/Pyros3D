//============================================================================
// Name        : RenderingModelComponent.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering Model Component
//============================================================================

#include "../../../Utils/ModelLoaders/MultiModelLoader/ModelLoader.h"
#include "../IRenderingComponent.h"

#ifndef RENDERINGMODELCOMPONENT_H
#define	RENDERINGMODELCOMPONENT_H

namespace Fishy3D {

    // SubMesh
    class RenderingSubMeshComponent : public IRenderingComponent {
        
        friend class RenderingModelComponent;
        
        public:
            RenderingSubMeshComponent();
            RenderingSubMeshComponent(const std::string &Name, SuperSmartPointer<IMaterial> material, bool SNormals = false);
            
            virtual void Start() {};
            virtual void Update() {};
            virtual void Shutdown() {};                        
            
            // Adds Bones List
            void SetSkinningBones(const std::vector<Matrix> &Bones);
            
            virtual void Build();
            
            // Vectors
            std::vector<vec3> tVertex, tNormal, tTangent, tBitangent;
            std::vector<vec2> tTexcoord;
            // Bones
            std::vector<vec4> tBonesID, tBonesWeight;
            
            // Map Bone ID's
            std::map<short int, short int> MapBoneIDs;
            // Bone Offset Matrix
            std::map<short int, Matrix> BoneOffsetMatrix;
            // Bones Matrix List
            std::vector<Matrix> SkinningBones;            
            
        protected:

            // Submesh is not clickable
            virtual void CalculateBounding();
            
            virtual void SmoothNormals();
            
    };
    
    class RenderingModelComponent : public IRenderingComponent {
        public:
        
            RenderingModelComponent(const std::string &Name, const std::string &ModelPath, SuperSmartPointer<IMaterial> material, bool SNormals = false, bool MaterialsFromFile = false);
            virtual ~RenderingModelComponent();
            
            virtual void Start();
            virtual void Update();
            virtual void Shutdown();
            virtual void Register(void* ptr);
            virtual void UnRegister(void* ptr);
            
            virtual void Build();
            
            // Culling
            virtual void ActivateCulling();
            virtual void DeactivateCulling();
            
            // Get Model Skeleton
            const std::map<StringID, Bone> &GetSkeleton() const;
            
			// Materials List
			std::vector< SuperSmartPointer<IMaterial> > Materials;
        protected:
            
            virtual void CalculateBounding();
            
        private:
            
            // Model loader, skeleton and animation
            SuperSmartPointer<ModelLoader> mesh;
            std::map<StringID, Bone> skeleton;
            
            // Vectors
            std::vector<vec3> tVertex, tNormal, tTangent, tBitangent;
            std::vector<vec2> tTexcoord;
            
            // skeleton
            std::vector<vec4> tBonesID, tBonesWeight;
            
            // Materials
            bool materialsFromFile;
    };
}

#endif	/* RENDERINGMODELCOMPONENT_H */

