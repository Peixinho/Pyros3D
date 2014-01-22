//============================================================================
// Name        : AssimPimporter.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Loads  model formats based on Assimp
//============================================================================

#ifndef ASSIMPIMPORTER_H
#define	ASSIMPIMPORTER_H

#include <map>
#include <vector>
#include "../IModelLoader.h"
#include "../../../Core/Math/Math.h"
#include "../../../Ext/StringIDs/StringID.hpp"
#include "../../../AssetManager/Assets/Renderable/Renderables.h"
#include "../../../Utils/Binary/BinaryFile.h"

// Assimp Lib
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/material.h>
#include <assimp/Importer.hpp> 


namespace p3d {

    class AssimpImporter : public IModelLoader {

        public:

            AssimpImporter();

            virtual ~AssimpImporter();

            virtual void Load(const std::string &Filename);
            
            void ConvertToPyrosFormat();
            
        private:
            
            // assimp model
            const aiScene* assimp_model;
           
            // bone count
            uint32 boneCount;       

            // aux function to construct skeleton            
            void GetBone(aiNode *bone, const int32 &parentID = -1); 
    };
}

#endif	/* ASSIMPIMPORTER_H */