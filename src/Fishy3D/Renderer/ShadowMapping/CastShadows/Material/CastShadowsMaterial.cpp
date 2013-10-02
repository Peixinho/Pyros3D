//============================================================================
// Name        : CastShadowsMaterial.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Material to Render Depth to Texture
//============================================================================

#include "CastShadowsMaterial.h"

namespace Fishy3D {

    CastShadowsMaterial::CastShadowsMaterial() : GenericShaderMaterial(ShaderUsage::CastShadows)
    {
        // Default Uniforms
        AddUniform(Uniform::Uniform("uProjectionMatrix",Uniform::DataUsage::ProjectionMatrix));
        
        AddUniform(Uniform::Uniform("uViewMatrix",Uniform::DataUsage::ViewMatrix));
        
        AddUniform(Uniform::Uniform("uModelMatrix",Uniform::DataUsage::ModelMatrix));

		SetOpacity(1.f);
    } 
    
    CastShadowsMaterial::~CastShadowsMaterial() {}

    void CastShadowsMaterial::PreRender() {}
        
    void CastShadowsMaterial::AfterRender() {}
}