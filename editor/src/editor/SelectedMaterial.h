//============================================================================
// Name        : SelectedMaterial.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Selected Material
//============================================================================

#ifndef SELECTEDMATERIAL_H
#define	SELECTEDMATERIAL_H

#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>

using namespace p3d;

class SelectedMaterial : public CustomShaderMaterial {
    
        public:
            
			Uniform *uColor, *uColor2;

            SelectedMaterial() : CustomShaderMaterial("assets/selectedmaterial.glsl")
            {
				texture = new Texture();
				texture->LoadTexture("assets/selectedmaterial.png");
                AddUniform(Uniform("uProjectionMatrix",Uniforms::DataUsage::ProjectionMatrix));
                AddUniform(Uniform("uViewMatrix",Uniforms::DataUsage::ViewMatrix));
                AddUniform(Uniform("uModelMatrix",Uniforms::DataUsage::ModelMatrix));
                AddUniform(Uniform("uTime",Uniforms::DataUsage::Timer)); 
				int32 id = 0;
				AddUniform(Uniform("uTexture", DataType::Int, &id));
				uColor = AddUniform(Uniform("uColor", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
				uColor2 = AddUniform(Uniform("uColor2", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
			}

			virtual void PreRender()
			{
				Vec4 color = Vec4(1, 1, 0, 1);
				Vec4 color2 = Vec4(1, 0, 1, 1);
				uColor->SetValue(&color);
				uColor2->SetValue(&color2);
				texture->Bind();
            }
            virtual void Render() {}
            virtual void AfterRender() 
			{
				texture->Unbind();
			}

			Texture* texture;
            
};

#endif	/* SELECTEDMATERIAL_H */

