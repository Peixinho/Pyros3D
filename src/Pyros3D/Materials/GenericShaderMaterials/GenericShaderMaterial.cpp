//============================================================================
// Name        : GenericShaderMaterials.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Generic Shader Materials
//============================================================================

#include "GenericShaderMaterial.h"
#include "../../AssetManager/Assets/Font/Font.h"
#include "../../AssetManager/AssetManager.h"

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

        // Default Opacity
        f32 opacity = 1.0;
        AddUniform(Uniform::Uniform("uOpacity",Uniform::DataType::Float,&opacity));
        SetOpacity(opacity);

        if (options & ShaderUsage::Diffuse)
        {
            // Default Lighting Values
            Ke = Vec4(0.2f,0.2f,0.2f,1.0f);
            Ka = Vec4(0.2f,0.2f,0.2f,1.0f);
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

        if (options & ShaderUsage::DirectionalShadow)
        {
            // Shadows
            AddUniform(Uniform::Uniform("uDirectionalShadowMaps",Uniform::DataUsage::DirectionalShadowMap));
            AddUniform(Uniform::Uniform("uDirectionalDepthsMVP",Uniform::DataUsage::DirectionalShadowMatrix));
            AddUniform(Uniform::Uniform("uDirectionalShadowFar",Uniform::DataUsage::DirectionalShadowFar));
            AddUniform(Uniform::Uniform("uNumberOfDirectionalShadows",Uniform::DataUsage::NumberOfDirectionalShadows));
            
            isCastingShadows = true;
        }
        
        if (options & ShaderUsage::PointShadow)
        {
            // Shadows
            AddUniform(Uniform::Uniform("uPointShadowMaps",Uniform::DataUsage::PointShadowMap));
            AddUniform(Uniform::Uniform("uPointDepthsMVP",Uniform::DataUsage::PointShadowMatrix));
            AddUniform(Uniform::Uniform("uNumberOfPointShadows",Uniform::DataUsage::NumberOfPointShadows));

            isCastingShadows = true;
        }
        
        if (options & ShaderUsage::SpotShadow)
        {
            // Shadows
            AddUniform(Uniform::Uniform("uSpotShadowMaps",Uniform::DataUsage::SpotShadowMap));
            AddUniform(Uniform::Uniform("uSpotDepthsMVP",Uniform::DataUsage::SpotShadowMatrix));
            AddUniform(Uniform::Uniform("uNumberOfSpotShadows",Uniform::DataUsage::NumberOfSpotShadows));

            isCastingShadows = true;
        }
        
        if (options & ShaderUsage::EnvMap)
        {
            AddUniform(Uniform::Uniform("uCameraPos",Uniform::DataUsage::CameraPosition));
            // Set Default Reflectivity
            Reflectivity = 1.0;
            AddUniform(Uniform::Uniform("uReflectivity",Uniform::DataType::Float,&Reflectivity));
        }

    }
    
    void GenericShaderMaterial::SetLightingProperties(const Vec4 &Ke, const Vec4 &Ka, const Vec4 &Kd, const Vec4 &Ks, const f32 &shininess)
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
    void GenericShaderMaterial::SetKe(const Vec4 &Ke)
    {
        this->Ke = Ke;
        SetUniformValue("uKe",&this->Ke);
    }
    void GenericShaderMaterial::SetKa(const Vec4 &Ka)
    {
        this->Ka = Ka;
        SetUniformValue("uKa",&this->Ka);
    }
    void GenericShaderMaterial::SetKd(const Vec4 &Kd)
    {
        this->Kd = Kd;
        SetUniformValue("uKd",&this->Kd);
    }
    void GenericShaderMaterial::SetKs(const Vec4 &Ks)
    {
        this->Ks = Ks;
        SetUniformValue("uKs",&this->Ks);
    }
    void GenericShaderMaterial::SetShininess(const f32 &shininess)
    {
        this->Shininess = shininess;
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
    void GenericShaderMaterial::SetReflectivity(const f32& reflectivity)
    {
        Reflectivity = reflectivity;
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
        AddUniform(Uniform::Uniform("uSkybox",Uniform::DataType::Int,id));
        // Cullface
//        cullFace = CullFace::FrontFace;
    }
    void GenericShaderMaterial::SetTextFont(const uint32& FontHandle)
    {
        p3d::Font* font = (p3d::Font*)AssetManager::GetAsset(FontHandle)->AssetPTR;
        uint32 id = Textures.size();
        Textures[id] = font->GetTexture();
        AddUniform(Uniform::Uniform("uColormap",Uniform::DataType::Int,id));
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