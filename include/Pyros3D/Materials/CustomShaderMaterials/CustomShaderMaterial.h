//============================================================================
// Name        : CustomShaderMaterials.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Custom Shader Materials
//============================================================================

#ifndef CUSTOMSHADERMATERIAL_H
#define CUSTOMSHADERMATERIAL_H

#include"../IMaterial.h"
#include "../../Other/Export.h"
#include <iostream>
#include <map>

namespace p3d
{
 
    class PYROS3D_API CustomShaderMaterial : public IMaterial
    {
        
        public:
        
            CustomShaderMaterial(const std::string &vertexShaderFile, const std::string &fragmentShaderFile);
            virtual ~CustomShaderMaterial();

        protected:
            // Shaders List
            static std::map<uint32, Shader*> ShadersList;
            // Save Shader Location on Shaders List
            uint32 shaderID;
    };

}

#endif /* CUSTOMSHADERMATERIAL_H */
