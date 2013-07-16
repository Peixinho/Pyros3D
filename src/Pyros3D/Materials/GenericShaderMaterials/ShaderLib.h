//============================================================================
// Name        : ShaderLib.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : ShaderLib
//============================================================================

#ifndef SHADERLIB_H
#define SHADERLIB_H
#include <iostream>
#include <map>

#include "../../Ext/StringIDs/StringID.hpp"
#include "../Shaders/Shaders.h"

#define MAX_LIGHTS 4

namespace p3d
{
 
    namespace ShaderUsage
    {
        enum {
            Color               = 0x1,
            Texture             = 0x2,
            EnvMap              = 0x4,
            Skybox              = 0x8,
            Refraction          = 0x10,
            Skinning            = 0x20,
            CellShading         = 0x40,
            BumpMapping         = 0x80,
            SpecularMap         = 0x100,
            SpecularColor       = 0x200,
            ShadowMaterial      = 0x400,
            CastShadows         = 0x800,
            Diffuse             = 0x1000,
            ShadowMaterialGPU   = 0x2000
        };
    };        
    
    class ShaderLib
    {
        friend class GenericShaderMaterial;
        
        protected:
            // Build Shaders
            static void BuildShader(const uint32 &option, Shaders* shader);
        
            virtual ~ShaderLib();                
    };

}

#endif /* SHADERLIB_H */
