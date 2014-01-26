//============================================================================
// Name        : ModelLoader.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Loads Pyros3D Own Model Format
//============================================================================

#ifndef MODELLOADER_H
#define	MODELLOADER_H

#include <map>
#include <vector>
#include "../IModelLoader.h"
#include "../../../Core/Math/Math.h"
#include "../../../Ext/StringIDs/StringID.hpp"
#include "../../../AssetManager/Assets/Renderable/Renderables.h"
#include "../../../Utils/Binary/BinaryFile.h"


namespace p3d {

    class ModelLoader : public IModelLoader {
        public:

            ModelLoader();

            virtual ~ModelLoader();

            virtual bool Load(const std::string &Filename);
    };
}

#endif	/* MODELLOADER_H */