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
#include "../../Other/Export.h"

#define MAX_LIGHTS 4

namespace p3d
{
 
    namespace ShaderUsage
    {
        enum {
            Color                               = 0x1,
            Texture                             = 0x2,
            EnvMap                              = 0x4,
            Skybox                              = 0x8,
            Refraction                          = 0x10,
            Skinning                            = 0x20,
            CellShading                         = 0x40,
            BumpMapping                         = 0x80,
            SpecularMap                         = 0x100,
            SpecularColor                       = 0x200,
            DirectionalShadow                   = 0x400,
            PointShadow                         = 0x800,
            SpotShadow                          = 0x1000,
            CastShadows                         = 0x2000,
            Diffuse                             = 0x4000,
            TextRendering                       = 0x8000,
            PhysicsDebug                        = 0x10000
        };
    };        
    
    class PYROS3D_API ShaderLib
    {
        friend class GenericShaderMaterial;
        
        protected:
            // Build Shaders
            static void BuildShader(const uint32 &option, Shader* shader);           
    };

}

#endif /* SHADERLIB_H */
