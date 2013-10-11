//============================================================================
// Name        : Model
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Model Geometry
//============================================================================

#ifndef MODEL_H
#define MODEL_H

#include "../Renderables.h"
#include "../../../../Utils/ModelLoaders/MultiModelLoader/ModelLoader.h"

namespace p3d {

    class ModelGeometry : public IGeometry
    {
        public:

            ModelGeometry() : IGeometry(GeometryType::BUFFER) {}

            // Adds Bones List
            void SetSkinningBones(const std::vector<Matrix> &Bones) {}

            // Vectors
            std::vector<Vec3> tVertex, tNormal, tTangent, tBitangent;
            std::vector<Vec2> tTexcoord;
            // Bones
            std::vector<Vec4> tBonesID, tBonesWeight;

            // Map Bone ID's
            std::map<int32, int32> MapBoneIDs;
            // Bone Offset Matrix
            std::map<int32, Matrix> BoneOffsetMatrix;
            // Bones Matrix List
            std::vector<Matrix> SkinningBones;

            void CreateBuffers();

            virtual std::vector<uint32> &GetIndexData() { return index; }
            virtual std::vector<Vec3> &GetVertexData() { return tVertex; }

        protected:
            virtual void CalculateBounding();
    };

    class Model : public Renderable {

        public:

            Model(const std::string ModelPath, bool mergeMeshes = true);            

            virtual ~Model() {}

            // Model loader, skeleton and animation
            ModelLoader* mesh;
            std::map<StringID, Bone> skeleton;

            void Build();

    };
};

 #endif /* MODEL_H */