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

            // Vectors
            std::vector<Vec3> tVertex, tNormal, tTangent, tBitangent;
            std::vector<Vec2> tTexcoord;

            // Bones
            std::vector<Vec4> tBonesID, tBonesWeight;
            
            void CreateBuffers();

            virtual std::vector<uint32> &GetIndexData() { return index; }
            virtual std::vector<Vec3> &GetVertexData() { return tVertex; }

        protected:
            virtual void CalculateBounding();
    };

    class Model : public Renderable {

        public:

            Model(const std::string ModelPath, bool mergeMeshes = true, const uint32 &MaterialOptions = 0);

            virtual ~Model() {}

            // Model loader, skeleton and animation
            IModelLoader* mesh;

            void Build();
        
        protected:
        
            uint32 MaterialOptions;

    };
};

 #endif /* MODEL_H */