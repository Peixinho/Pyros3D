//============================================================================
// Name        : AssimPmodelimporter.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Loads  model formats based on Assimp
//============================================================================

#ifndef ASSIMPMODELIMPORTER_H
#define	ASSIMPMODELIMPORTER_H

#include <map>
#include <vector>
#include "Pyros3D/Utils/ModelLoaders/IModelLoader.h"
#include "Pyros3D/Core/Math/Math.h"
#include "Pyros3D/Ext/StringIDs/StringID.hpp"
#include "Pyros3D/Assets/Renderable/Renderables.h"
#include "Pyros3D/Utils/Binary/BinaryFile.h"

// Assimp Lib
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/material.h>
#include <assimp/Importer.hpp> 


namespace p3d {

    class AssimpModelImporter : public IModelLoader {

        public:

            AssimpModelImporter();

            virtual ~AssimpModelImporter();

            virtual bool Load(const std::string &Filename);
            
            bool ConvertToPyrosFormat(const std::string &Filename);
            
        private:
            
            // assimp model
            const aiScene* assimp_model;
           
            // bone count
            uint32 boneCount;       

            // aux function to construct skeleton            
            void GetBone(aiNode *bone, const int32 &parentID = -1); 
    };
}

#endif	/* ASSIMPMODELIMPORTER_H */