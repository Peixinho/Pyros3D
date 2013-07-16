//============================================================================
// Name        : GenericShaderMaterials.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Generic Shader Materials
//============================================================================

#include "GenericShaderMaterial.h"

namespace p3d
{
    // Shaders List
    std::map<uint32, Shaders* > GenericShaderMaterial::ShadersList;
    
    GenericShaderMaterial::GenericShaderMaterial(const uint32& options) : IMaterial()
    {
        // Find if Shader exists, if not, creates a new one
        if (ShadersList.find(options)==ShadersList.end())
        {
            ShadersList[options] = new Shaders();
            ShadersList[options]->currentMaterials = 0;
            ShaderLib::BuildShader(options, ShadersList[options]);
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
        f32 opacity = 1.0;
        AddUniform(Uniform::Uniform("uOpacity",Uniform::DataType::Float,&opacity));
        SetOpacity(opacity);

        if (options & ShaderUsage::Diffuse)
        {
            // Default Lighting Values
            Ke = Vec4(0.2f,0.2f,0.2f,0.2f);
            Ka = Vec4(0.2f,0.2f,0.2f,0.2f);
            Kd = Vec4(1.0f,1.0f,1.0f,1.0f);
            Ks = Vec4(1.0f,1.0f,1.0f,1.0f);
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
        }
        
        if (options & ShaderUsage::ShadowMaterialGPU)
        {
            // Shadows
            AddUniform(Uniform::Uniform("uShadowmaps",Uniform::DataUsage::ShadowMap));
            AddUniform(Uniform::Uniform("uDepthsMVP",Uniform::DataUsage::ShadowMatrix));
            AddUniform(Uniform::Uniform("uShadowFar",Uniform::DataUsage::ShadowFar));
        };

    }
    
    void GenericShaderMaterial::ChangeLightingProperties(const Vec4 &Ke, const Vec4 &Ka, const Vec4 &Kd, const Vec4 &Ks, const f32 &shininess)
    {
        this->Ke = Ke;
        this->Ka = Ka;
        this->Kd = Kd;
        this->Ks = Ks;
		this->Shininess = shininess;
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
                delete ShadersList[shaderID];
        }
    }
    
    void GenericShaderMaterial::BindTextures()
    {
        for (std::map<uint32, Texture>::iterator i = Textures.begin(); i!= Textures.end(); i++)
        {
            (*i).second.Bind();
        }
    }
    void GenericShaderMaterial::UnbindTextures()
    {
        for (std::map<uint32, Texture>::reverse_iterator i = Textures.rbegin(); i!= Textures.rend(); i++)
        {
            (*i).second.Unbind();
        }
    }
    
    void GenericShaderMaterial::SetColor(const Vec4& color)
    {
        Vec4 Color = color;
        AddUniform(Uniform::Uniform("uColor",Uniform::DataType::Vec4,&Color));
    }
    void GenericShaderMaterial::SetSpecular(const Vec4& specularColor)
    {
        Vec4 Specular = specularColor;
        AddUniform(Uniform::Uniform("uSpecular",Uniform::DataType::Vec4,&Specular));
    }
    
    void GenericShaderMaterial::SetColorMap(const Texture &colormap)
    {
        uint32 id = Textures.size();
        // Save on List
        Textures[id] = colormap;
        // Set Uniform
        AddUniform(Uniform::Uniform("uColormap",Uniform::DataType::Int,&id));
    }
    void GenericShaderMaterial::SetSpecularMap(const Texture &specular)
    {
        uint32 id = Textures.size();
        // Save on List
        Textures[id] = specular;
        // Set Uniform
        AddUniform(Uniform::Uniform("uSpecularmap",Uniform::DataType::Int,&id));
    }
    void GenericShaderMaterial::SetNormalMap(const Texture &normalmap)
    {
        uint32 id = Textures.size();
        // Save on List
        Textures[id] = normalmap;
        // Set Uniform
        AddUniform(Uniform::Uniform("uNormalmap",Uniform::DataType::Int,&id));
    }
    void GenericShaderMaterial::SetEnvMap(const Texture &envmap)
    {
        uint32 id = Textures.size();
        // Save on List
        Textures[id] = envmap;
        // Set Uniform
        AddUniform(Uniform::Uniform("uEnvmap",Uniform::DataType::Int,&id));
    }
    void GenericShaderMaterial::SetRefractMap(const Texture &refractmap)
    {
        uint32 id = Textures.size();
        // Save on List
        Textures[id] = refractmap;
        // Set Uniform
        AddUniform(Uniform::Uniform("uRefractmap",Uniform::DataType::Int,&id));
    }
    void GenericShaderMaterial::SetSkyboxMap(const Texture& skyboxmap)
    {
        uint32 id = Textures.size();
        // Save on List
        Textures[id] = skyboxmap;
        // Set Uniform
        AddUniform(Uniform::Uniform("uSkybox",Uniform::DataType::Int,&id));
        // Cullface
        cullFace = CullFace::FrontFace;
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