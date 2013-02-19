//============================================================================
// Name        : ModelLoader.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Model Loader Interface
//============================================================================

#include "IModelLoader.h"

namespace Fishy3D {
        
        IModelLoader::IModelLoader() {}

        IModelLoader::~IModelLoader() {}
        
        std::string IModelLoader::LoadFile(const std::string& Filename)
        {
            std::ifstream t(Filename.c_str());
            std::string str;

            t.seekg(0, std::ios::end);   
            str.reserve(t.tellg());
            t.seekg(0, std::ios::beg);

            // copy file to string
            str.assign((std::istreambuf_iterator<char>(t)),std::istreambuf_iterator<char>());
                        
            return str;
        }
}
