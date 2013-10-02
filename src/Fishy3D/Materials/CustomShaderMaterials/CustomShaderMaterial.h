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

#include <iostream>
#include <map>

namespace Fishy3D
{
 
    class CustomShaderMaterial : public IMaterial
    {
        
        public:
        
            CustomShaderMaterial(const std::string &vertexShaderFile, const std::string &fragmentShaderFile);
            virtual ~CustomShaderMaterial();
        private:
        
        protected:
            // Shaders List
            static std::map<unsigned, SuperSmartPointer<Shaders> > ShadersList;
            // Save Shader Location on Shaders List
            unsigned shaderID;
    };

}

#endif /* CUSTOMSHADERMATERIAL_H */
