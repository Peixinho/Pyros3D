//============================================================================
// Name        : GenericShaderMaterials.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Generic Shader Materials
//============================================================================

#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Assets/Font/Font.h>
#include <Pyros3D/Resources/Resources.h>

namespace p3d
{
    
    GenericShaderMaterial::GenericShaderMaterial(const uint32 options) : IMaterial()
    {
        // Default
        colorMapID = specularMapID = normalMapID = envMapID = skyboxMapID = refractMapID = fontMapID = -1;
        
        {
            shader = new Shader();
            //ShaderLib::BuildShader(options, ShadersList[options]);

#if defined(_DEBUG) || defined(USE_SHADER_FILE)
			ShadersList[options]->LoadShaderFile("PyrosShader.glsl");
#else
			shader->LoadShaderText(SHADER_CODE);
#endif
			std::string define;
			if (options & ShaderUsage::Color)
				define += std::string("#define COLOR\n");
			if (options & ShaderUsage::DebugRendering)
				define += std::string("#define DEBUGRENDERING\n");
			if (options & ShaderUsage::Texture)
				define += std::string("#define TEXTURE\n");
			if (options & ShaderUsage::TextRendering)
				define += std::string("#define TEXTRENDERING\n");
			if (options & ShaderUsage::DirectionalShadow)
				define += std::string("#define DIRECTIONALSHADOW\n");
			if (options & ShaderUsage::PointShadow)
				define += std::string("#define POINTSHADOW\n");
			if (options & ShaderUsage::SpotShadow)
				define += std::string("#define SPOTSHADOW\n");
			if (options & ShaderUsage::CastShadows)
				define += std::string("#define CASTSHADOWS\n");
			if (options & ShaderUsage::BumpMapping)
				define += std::string("#define BUMPMAPPING\n");
			if (options & ShaderUsage::Skinning)
				define += std::string("#define SKINNING\n");
			if (options & ShaderUsage::EnvMap)
				define += std::string("#define ENVMAP\n");
			if (options & ShaderUsage::Skybox)
				define += std::string("#define SKYBOX\n");
			if (options & ShaderUsage::Refraction)
				define += std::string("#define REFRACTION\n");
			if (options & ShaderUsage::SpecularColor)
				define += std::string("#define SPECULARCOLOR\n");
			if (options & ShaderUsage::SpecularMap)
				define += std::string("#define SPECULARMAP\n");
			if (options & ShaderUsage::Diffuse)
				define += std::string("#define DIFFUSE\n");
			if (options & ShaderUsage::CellShading)
				define += std::string("#define CELLSHADING\n");
			if (options & ShaderUsage::ClipPlane)
				define += std::string("#define CLIPSPACE\n");

#if defined(GLES2)
			define += std::string("#define GLES2\n");
#endif

			shader->CompileShader(ShaderType::VertexShader, (std::string("#define VERTEX\n") + define).c_str());
			shader->CompileShader(ShaderType::FragmentShader, (std::string("#define FRAGMENT\n") + define).c_str());

			shader->LinkProgram();
        }
        
        // Get Shader Program
        shaderProgram = shader->ShaderProgram();

        // Back Face Culling
        cullFace = CullFace::BackFace;
        
        // Always used Uniforms
        AddUniform(Uniform("uProjectionMatrix",DataUsage::ProjectionMatrix));
        AddUniform(Uniform("uViewMatrix",DataUsage::ViewMatrix));
        AddUniform(Uniform("uModelMatrix",DataUsage::ModelMatrix));

        // Default Opacity
        f32 opacity = 1.0;
        AddUniform(Uniform("uOpacity",DataType::Float,&opacity));
        SetOpacity(opacity);

        // Default PCF Texel Size
        PCFTexelSize1 = 0.0001;
        PCFTexelSize2 = 0.0001;
        PCFTexelSize3 = 0.0001;
        PCFTexelSize4 = 0.0001;

        if (options & ShaderUsage::Diffuse)
        {
            UseLights = 1.0;
			Shininess = 50.0;
            AddUniform(Uniform("uLights",DataUsage::Lights));
            AddUniform(Uniform("uNumberOfLights",DataUsage::NumberOfLights));
            AddUniform(Uniform("uAmbientLight",DataUsage::GlobalAmbientLight));
            AddUniform(Uniform("uUseLights",DataType::Float,&UseLights));
			AddUniform(Uniform("uCameraPos", DataUsage::CameraPosition));
			AddUniform(Uniform("uShininess", DataType::Float, &Shininess));
        }

		if (options & ShaderUsage::CellShading)
		{
			UseLights = 1.0;
			Shininess = 50.0;
			AddUniform(Uniform("uLights", DataUsage::Lights));
			AddUniform(Uniform("uNumberOfLights", DataUsage::NumberOfLights));
			AddUniform(Uniform("uAmbientLight", DataUsage::GlobalAmbientLight));
			AddUniform(Uniform("uUseLights", DataType::Float, &UseLights));
			AddUniform(Uniform("uCameraPos", DataUsage::CameraPosition));
			AddUniform(Uniform("uShininess", DataType::Float, &Shininess));
		}

        if (options & ShaderUsage::DirectionalShadow)
        {
            // Shadows
            AddUniform(Uniform("uDirectionalShadowMaps",DataUsage::DirectionalShadowMap));
            AddUniform(Uniform("uDirectionalDepthsMVP",DataUsage::DirectionalShadowMatrix));
            AddUniform(Uniform("uDirectionalShadowFar",DataUsage::DirectionalShadowFar));
            AddUniform(Uniform("uNumberOfDirectionalShadows",DataUsage::NumberOfDirectionalShadows));
            AddUniform(Uniform("uPCFTexelSize1",DataType::Float,&PCFTexelSize1));
            AddUniform(Uniform("uPCFTexelSize2",DataType::Float,&PCFTexelSize2));
            AddUniform(Uniform("uPCFTexelSize3",DataType::Float,&PCFTexelSize3));
            AddUniform(Uniform("uPCFTexelSize4",DataType::Float,&PCFTexelSize4));
            isCastingShadows = true;
        }
        
        if (options & ShaderUsage::PointShadow)
        {
            // Shadows
            AddUniform(Uniform("uPointShadowMaps",DataUsage::PointShadowMap));
            AddUniform(Uniform("uPointDepthsMVP",DataUsage::PointShadowMatrix));
            AddUniform(Uniform("uNumberOfPointShadows",DataUsage::NumberOfPointShadows));
            AddUniform(Uniform("uPCFTexelSize",DataType::Float,&PCFTexelSize1));
            isCastingShadows = true;
        }
        
        if (options & ShaderUsage::SpotShadow)
        {
            // Shadows
            AddUniform(Uniform("uSpotShadowMaps",DataUsage::SpotShadowMap));
            AddUniform(Uniform("uSpotDepthsMVP",DataUsage::SpotShadowMatrix));
            AddUniform(Uniform("uNumberOfSpotShadows",DataUsage::NumberOfSpotShadows));
            AddUniform(Uniform("uPCFTexelSize",DataType::Float,&PCFTexelSize1));
            isCastingShadows = true;
        }
        
        if (options & ShaderUsage::EnvMap)
        {
            AddUniform(Uniform("uCameraPos",DataUsage::CameraPosition));
            // Set Default Reflectivity
            Reflectivity = 1.0;
            AddUniform(Uniform("uReflectivity",DataType::Float,&Reflectivity));
        }

        if (options & ShaderUsage::Refraction)
        {
            AddUniform(Uniform("uCameraPos",DataUsage::CameraPosition));
            // Set Default Reflectivity
            Reflectivity = 1.0;
            AddUniform(Uniform("uReflectivity",DataType::Float,&Reflectivity));
        }        
        if (options & ShaderUsage::Skinning)
        {
            AddUniform(Uniform("uBoneMatrix",DataUsage::Skinning));
        }
		if (options & ShaderUsage::ClipPlane)
		{
			AddUniform(Uniform("uClipPlanes", DataUsage::ClipPlanes));
		}
		if (options & ShaderUsage::TextRendering)
		{
			SetTransparencyFlag(true);
		}
    }
    
    void GenericShaderMaterial::SetShininess(const f32 shininess)
    {
        this->Shininess = shininess;
        SetUniformValue("uShininess",&this->Shininess);
    }
    GenericShaderMaterial::~GenericShaderMaterial()
    {
        
        // Delete Shaders
		delete shader;
    }
    
    void GenericShaderMaterial::AddTexture(const std::string &uniformName, Texture* texture)
    {
        uint32 id = Textures.size();
        
        // Save on Lirest
        Textures[id] = texture;
        // Set Uniform
        AddUniform(Uniform(uniformName.c_str(),DataType::Int,&id));
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
        AddUniform(Uniform("uColor",DataType::Vec4,&Color));
    }
    void GenericShaderMaterial::SetSpecular(const Vec4& specularColor)
    {
        Vec4 Specular = specularColor;
        AddUniform(Uniform("uSpecular",DataType::Vec4,&Specular));
    }
    
    void GenericShaderMaterial::SetColorMap(Texture* colormap)
    {
        if (colorMapID==-1)
            colorMapID = Textures.size();
        
        // Save on Lirest
        Textures[colorMapID] = colormap;
        // Set Uniform
        AddUniform(Uniform("uColormap",DataType::Int,&colorMapID));
    }
    void GenericShaderMaterial::SetSpecularMap(Texture* specular)
    {
        if (specularMapID==-1)
            specularMapID = Textures.size();
        // Save on List
        Textures[specularMapID] = specular;
        // Set Uniform
        AddUniform(Uniform("uSpecularmap",DataType::Int,&specularMapID));
    }
    void GenericShaderMaterial::SetNormalMap(Texture* normalmap)
    {
        if (normalMapID==-1)
            normalMapID = Textures.size();
        // Save on List
        Textures[normalMapID] = normalmap;
        // Set Uniform
        AddUniform(Uniform("uNormalmap",DataType::Int,&normalMapID));
    }
    void GenericShaderMaterial::SetEnvMap(Texture* envmap)
    {
        if (envMapID==-1)
            envMapID = Textures.size();
        // Save on List
        Textures[envMapID] = envmap;
        // Set Uniform
        AddUniform(Uniform("uEnvmap",DataType::Int,&envMapID));
    }
    void GenericShaderMaterial::SetReflectivity(const f32 reflectivity)
    {
        Reflectivity = reflectivity;
        AddUniform(Uniform("uReflectivity",DataType::Float,&Reflectivity));
    }
    void GenericShaderMaterial::SetRefractMap(Texture* refractmap)
    {
        if (refractMapID==-1)
            refractMapID = Textures.size();
        // Save on List
        Textures[refractMapID] = refractmap;
        // Set Uniform
        AddUniform(Uniform("uRefractmap",DataType::Int,&refractMapID));
    }
    void GenericShaderMaterial::SetSkyboxMap(Texture* skyboxmap)
    {
        if (skyboxMapID==-1)
            skyboxMapID = Textures.size();
        // Save on List
        Textures[skyboxMapID] = skyboxmap;
        // Set Uniform
        AddUniform(Uniform("uSkyboxmap",DataType::Int,&skyboxMapID));
    }
    void GenericShaderMaterial::SetTextFont(Font* font)
    {
        if (fontMapID==-1)
            fontMapID = Textures.size();
        // Save on List
        Textures[fontMapID] = font->GetTexture();
        // Set Uniform
        AddUniform(Uniform("uFontmap",DataType::Int,&fontMapID));
    }
    void GenericShaderMaterial::PreRender()
    {
        BindTextures();
    }
    void GenericShaderMaterial::AfterRender()
    {
        UnbindTextures();
    }
    void GenericShaderMaterial::SetPCFTexelSize(const f32 texel)
    {
        PCFTexelSize1 = texel;
    }
    void GenericShaderMaterial::SetPCFTexelCascadesSize(const f32 texel1,const f32 texel2,const f32 texel3,const f32 texel4)
    {
        PCFTexelSize1 = texel1;
        PCFTexelSize2 = texel2;
        PCFTexelSize3 = texel3;
        PCFTexelSize4 = texel4;
    }
}
