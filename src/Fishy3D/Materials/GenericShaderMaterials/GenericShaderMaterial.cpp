//============================================================================
// Name        : GenericShaderMaterials.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Generic Shader Materials
//============================================================================

#include "GenericShaderMaterial.h"

namespace Fishy3D
{
    // Shaders List
    std::map<unsigned, SuperSmartPointer<Shaders> > GenericShaderMaterial::ShadersList;
    
    GenericShaderMaterial::GenericShaderMaterial(const unsigned& options) : IMaterial()
    {
        // Find if Shader exists, if not, creates a new one
        if (ShadersList.find(options)==ShadersList.end())
        {
            ShadersList[options] = SuperSmartPointer<Shaders> (new Shaders());
            ShaderLib::BuildShader(options, ShadersList[options].Get());
        }
        
        // Save Shader Location
        shaderID = options;
        
        // Add Counter
        ShadersList[options]->currentMaterials++;
        
        // Get Shader Program
        shaderProgram = ShadersList[options]->shaderProgram;

		// Always used Uniforms
		AddUniform(Uniform::Uniform("uProjectionMatrix",Uniform::DataUsage::ProjectionMatrix));
        AddUniform(Uniform::Uniform("uViewMatrix",Uniform::DataUsage::ViewMatrix));
        AddUniform(Uniform::Uniform("uModelMatrix",Uniform::DataUsage::ModelMatrix));
        AddUniform(Uniform::Uniform("uNormalMatrix",Uniform::DataUsage::NormalMatrix));

		// Default Opacity
		SetOpacity(1.0);

		if (options & ShaderUsage::Diffuse)
		{
			// Default Lighting Values
			Ke = vec4(0.2,0.2,0.2,0.2);
			Ka = vec4(0.2,0.2,0.2,0.2);
			Kd = vec4(1.0f,1.0f,1.0f,1.0f);
			Ks = vec4(1.f,1.f,1.f,1.f);
			Shininess = 50.f;
			UseLights = 1.0;
                
			// Lighting Uniforms
			AddUniform(Uniform::Uniform("uKe",Uniform::DataType::Vec4,&Ke));
			AddUniform(Uniform::Uniform("uKa",Uniform::DataType::Vec4,&Ka));
			AddUniform(Uniform::Uniform("uKd",Uniform::DataType::Vec4,&Kd));
			AddUniform(Uniform::Uniform("uKs",Uniform::DataType::Vec4,&Ks));
			AddUniform(Uniform::Uniform("uShininess",Uniform::DataType::Float,&Shininess));
			AddUniform(Uniform::Uniform("uLights",Uniform::DataUsage::Lights));
			AddUniform(Uniform::Uniform("uNumberOfLights",Uniform::DataUsage::NumberOfLights));
			AddUniform(Uniform::Uniform("uAmbientLight",Uniform::DataUsage::GlobalAmbientLight));
			AddUniform(Uniform::Uniform("uUseLights",Uniform::DataType::Float,&UseLights));
		}
			
		if (options & ShaderUsage::ShadowMaterial)
		{
			// Shadows
			AddUniform(Uniform::Uniform("uShadowmaps",Uniform::DataUsage::ShadowMap));
			AddUniform(Uniform::Uniform("uDepthsMVP",Uniform::DataUsage::ShadowMatrix));
			AddUniform(Uniform::Uniform("uShadowFar",Uniform::DataUsage::ShadowFar));
		};

    }
    
	void GenericShaderMaterial::ChangeLightingProperties(const vec4 &Ke, const vec4 &Ka, const vec4 &Kd, const vec4 &Ks, const float &shininess)
    {
		this->Ke = Ke;
		this->Ka = Ka;
		this->Kd = Kd;
		this->Ks = Ks;
		SetUniformValue("uKe",&this->Ke);
		SetUniformValue("uKe",&this->Ka);
		SetUniformValue("uKe",&this->Kd);
		SetUniformValue("uKe",&this->Ks);
		SetUniformValue("uShininess",&this->Shininess);
    }
    GenericShaderMaterial::~GenericShaderMaterial()
    {
        if (ShadersList.find(shaderID)!=ShadersList.end())
        {
            ShadersList[shaderID]->currentMaterials--;
            if (ShadersList[shaderID]->currentMaterials==0)
                ShadersList[shaderID].Dispose();
        }
    }
    
    void GenericShaderMaterial::BindTextures()
    {
        for (std::map<unsigned, Texture>::iterator i = Textures.begin(); i!= Textures.end(); i++)
        {
            (*i).second.Bind();
        }
    }
    void GenericShaderMaterial::UnbindTextures()
    {
        for (std::map<unsigned, Texture>::reverse_iterator i = Textures.rbegin(); i!= Textures.rend(); i++)
        {
            (*i).second.Unbind();
        }
    }
    
    void GenericShaderMaterial::SetColor(const vec4& color)
    {
        vec4 Color = color;
        AddUniform(Uniform::Uniform("uColor",Uniform::DataType::Vec4,&Color));
    }
    void GenericShaderMaterial::SetSpecular(const vec4& specularColor)
    {
        vec4 Specular = specularColor;
        AddUniform(Uniform::Uniform("uSpecular",Uniform::DataType::Vec4,&Specular));
    }
    
    void GenericShaderMaterial::SetColorMap(const Texture &colormap)
    {
        unsigned id = Textures.size();
        // Save on List
        Textures[id] = colormap;
        // Set Uniform
        AddUniform(Uniform::Uniform("uColormap",Uniform::DataType::Int,&id));
    }
    void GenericShaderMaterial::SetSpecularMap(const Texture &specular)
    {
        unsigned id = Textures.size();
        // Save on List
        Textures[id] = specular;
        // Set Uniform
        AddUniform(Uniform::Uniform("uSpecularmap",Uniform::DataType::Int,&id));
    }
    void GenericShaderMaterial::SetNormalMap(const Texture &normalmap)
    {
        unsigned id = Textures.size();
        // Save on List
        Textures[id] = normalmap;
        // Set Uniform
        AddUniform(Uniform::Uniform("uNormalmap",Uniform::DataType::Int,&id));
    }
    void GenericShaderMaterial::SetEnvMap(const Texture &envmap)
    {
        unsigned id = Textures.size();
        // Save on List
        Textures[id] = envmap;
        // Set Uniform
        AddUniform(Uniform::Uniform("uEnvmap",Uniform::DataType::Int,&id));
    }
    void GenericShaderMaterial::SetRefractMap(const Texture &refractmap)
    {
        unsigned id = Textures.size();
        // Save on List
        Textures[id] = refractmap;
        // Set Uniform
        AddUniform(Uniform::Uniform("uRefractmap",Uniform::DataType::Int,&id));
    }
    void GenericShaderMaterial::SetSkyboxMap(const Texture& skyboxmap)
    {
        unsigned id = Textures.size();
        // Save on List
        Textures[id] = skyboxmap;
        // Set Uniform
        AddUniform(Uniform::Uniform("uSkybox",Uniform::DataType::Int,&id));
    }
    void GenericShaderMaterial::PreRender()
    {
        BindTextures();
    }
    void GenericShaderMaterial::AfterRender()
    {
        UnbindTextures();
    }
}