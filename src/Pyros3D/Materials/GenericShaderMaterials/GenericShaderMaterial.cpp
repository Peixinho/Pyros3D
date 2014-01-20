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
        // Default
        colorMapID = specularMapID = normalMapID = envMapID = skyboxMapID = refractMapID = fontMapID = -1;
        
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

        // Back Face Culling
        cullFace = CullFace::BackFace;
        
        // Always used Uniforms
        AddUniform(Uniform::Uniform("uProjectionMatrix",Uniform::DataUsage::ProjectionMatrix));
        AddUniform(Uniform::Uniform("uViewMatrix",Uniform::DataUsage::ViewMatrix));
        AddUniform(Uniform::Uniform("uModelMatrix",Uniform::DataUsage::ModelMatrix));

        // Default Opacity
        f32 opacity = 1.0;
        AddUniform(Uniform::Uniform("uOpacity",Uniform::DataType::Float,&opacity));
        SetOpacity(opacity);

        // Default PCF Texel Size
        PCFTexelSize1 = 0.0001;
        PCFTexelSize2 = 0.0001;
        PCFTexelSize3 = 0.0001;
        PCFTexelSize4 = 0.0001;

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
            AddUniform(Uniform::Uniform("uPCFTexelSize1",Uniform::DataType::Float,&PCFTexelSize1));
            AddUniform(Uniform::Uniform("uPCFTexelSize2",Uniform::DataType::Float,&PCFTexelSize2));
            AddUniform(Uniform::Uniform("uPCFTexelSize3",Uniform::DataType::Float,&PCFTexelSize3));
            AddUniform(Uniform::Uniform("uPCFTexelSize4",Uniform::DataType::Float,&PCFTexelSize4));
            isCastingShadows = true;
        }
        
        if (options & ShaderUsage::PointShadow)
        {
            // Shadows
            AddUniform(Uniform::Uniform("uPointShadowMaps",Uniform::DataUsage::PointShadowMap));
            AddUniform(Uniform::Uniform("uPointDepthsMVP",Uniform::DataUsage::PointShadowMatrix));
            AddUniform(Uniform::Uniform("uNumberOfPointShadows",Uniform::DataUsage::NumberOfPointShadows));
            AddUniform(Uniform::Uniform("uPCFTexelSize",Uniform::DataType::Float,&PCFTexelSize1));
            isCastingShadows = true;
        }
        
        if (options & ShaderUsage::SpotShadow)
        {
            // Shadows
            AddUniform(Uniform::Uniform("uSpotShadowMaps",Uniform::DataUsage::SpotShadowMap));
            AddUniform(Uniform::Uniform("uSpotDepthsMVP",Uniform::DataUsage::SpotShadowMatrix));
            AddUniform(Uniform::Uniform("uNumberOfSpotShadows",Uniform::DataUsage::NumberOfSpotShadows));
            AddUniform(Uniform::Uniform("uPCFTexelSize",Uniform::DataType::Float,&PCFTexelSize1));
            isCastingShadows = true;
        }
        
        if (options & ShaderUsage::EnvMap)
        {
            AddUniform(Uniform::Uniform("uCameraPos",Uniform::DataUsage::CameraPosition));
            // Set Default Reflectivity
            Reflectivity = 1.0;
            AddUniform(Uniform::Uniform("uReflectivity",Uniform::DataType::Float,&Reflectivity));
        }

        if (options & ShaderUsage::Refraction)
        {
            AddUniform(Uniform::Uniform("uCameraPos",Uniform::DataUsage::CameraPosition));
            // Set Default Reflectivity
            Reflectivity = 1.0;
            AddUniform(Uniform::Uniform("uReflectivity",Uniform::DataType::Float,&Reflectivity));
        }        
        if (options & ShaderUsage::Skinning)
        {
            AddUniform(Uniform::Uniform("uBoneMatrix",Uniform::DataUsage::Skinning));
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
        
        // Delete Shaders
        if (ShadersList.find(shaderID)!=ShadersList.end())
        {
            ShadersList[shaderID]->currentMaterials--;
            if (ShadersList[shaderID]->currentMaterials==0)
                delete ShadersList[shaderID];
        }
        
        // Delete Textures
        for (std::map<uint32,Texture*>::iterator i=Textures.begin();i!=Textures.end();i++)
        {
            delete (*i).second;
        }
    }
    
    void GenericShaderMaterial::BindTextures()
    {
        for (std::map<uint32, Texture*>::iterator i = Textures.begin(); i!= Textures.end(); i++)
        {
            (*i).second->Bind();
        }
    }
    void GenericShaderMaterial::UnbindTextures()
    {
        for (std::map<uint32, Texture*>::reverse_iterator i = Textures.rbegin(); i!= Textures.rend(); i++)
        {
            (*i).second->Unbind();
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
    
    void GenericShaderMaterial::SetColorMap(Texture* colormap)
    {
        if (colorMapID==-1)
            colorMapID = Textures.size();
        
        // Save on Lirest
        Textures[colorMapID] = colormap;
        // Set Uniform
        AddUniform(Uniform::Uniform("uColormap",Uniform::DataType::Int,&colorMapID));
    }
    void GenericShaderMaterial::SetSpecularMap(Texture* specular)
    {
        if (specularMapID==-1)
            specularMapID = Textures.size();
        // Save on List
        Textures[specularMapID] = specular;
        // Set Uniform
        AddUniform(Uniform::Uniform("uSpecularmap",Uniform::DataType::Int,&specularMapID));
    }
    void GenericShaderMaterial::SetNormalMap(Texture* normalmap)
    {
        if (normalMapID==-1)
            normalMapID = Textures.size();
        // Save on List
        Textures[normalMapID] = normalmap;
        // Set Uniform
        AddUniform(Uniform::Uniform("uNormalmap",Uniform::DataType::Int,&normalMapID));
    }
    void GenericShaderMaterial::SetEnvMap(Texture* envmap)
    {
        if (envMapID==-1)
            envMapID = Textures.size();
        // Save on List
        Textures[envMapID] = envmap;
        // Set Uniform
        AddUniform(Uniform::Uniform("uEnvmap",Uniform::DataType::Int,&envMapID));
    }
    void GenericShaderMaterial::SetReflectivity(const f32& reflectivity)
    {
        Reflectivity = reflectivity;
        AddUniform(Uniform::Uniform("uReflectivity",Uniform::DataType::Float,&Reflectivity));
    }
    void GenericShaderMaterial::SetRefractMap(Texture* refractmap)
    {
        if (refractMapID==-1)
            refractMapID = Textures.size();
        // Save on List
        Textures[refractMapID] = refractmap;
        // Set Uniform
        AddUniform(Uniform::Uniform("uRefractmap",Uniform::DataType::Int,&refractMapID));
    }
    void GenericShaderMaterial::SetSkyboxMap(Texture* skyboxmap)
    {
        if (skyboxMapID==-1)
            skyboxMapID = Textures.size();
        // Save on List
        Textures[skyboxMapID] = skyboxmap;
        // Set Uniform
        AddUniform(Uniform::Uniform("uSkyboxmap",Uniform::DataType::Int,&skyboxMapID));
    }
    void GenericShaderMaterial::SetTextFont(Font* font)
    {
        if (fontMapID==-1)
            fontMapID = Textures.size();
        // Save on List
        Textures[fontMapID] = font->GetTexture();
        // Set Uniform
        AddUniform(Uniform::Uniform("uFontmap",Uniform::DataType::Int,&fontMapID));
    }
    void GenericShaderMaterial::PreRender()
    {
        BindTextures();
    }
    void GenericShaderMaterial::AfterRender()
    {
        UnbindTextures();
    }
    void GenericShaderMaterial::SetPCFTexelSize(const f32 &texel)
    {
        PCFTexelSize1 = texel;
    }
    void GenericShaderMaterial::SetPCFTexelCascadesSize(const f32 &texel1,const f32 &texel2,const f32 &texel3,const f32 &texel4)
    {
        PCFTexelSize1 = texel1;
        PCFTexelSize2 = texel2;
        PCFTexelSize3 = texel3;
        PCFTexelSize4 = texel4;
    }
}
