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

	// Shaders List
	std::map<uint32, Shader* > GenericShaderMaterial::ShadersList;

	GenericShaderMaterial::GenericShaderMaterial(const uint32 options) : IMaterial()
	{
		// Default
		colorMapID = specularMapID = normalMapID = envMapID = skyboxMapID = refractMapID = fontMapID = -1;

		uColor = uSpecular = uReflectivity = NULL;

		// Find if Shader exists, if not, creates a new one
		if (ShadersList.find(options) == ShadersList.end())
		{
			ShadersList[options] = new Shader();
			ShadersList[options]->currentMaterials = 0;
			//ShaderLib::BuildShader(options, ShadersList[options]);

#if defined(_DEBUG) || defined(USE_SHADER_FILE)
			ShadersList[options]->LoadShaderFile("PyrosShader.glsl");
#else
			ShadersList[options]->LoadShaderText(SHADER_CODE);
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
#if defined(GLLEGACY)
			define += std::string("#define GLLEGACY\n");
#endif
#if defined(EMSCRIPTEN)
			define += std::string("#define EMSCRIPTEN\n");
#endif

			ShadersList[options]->CompileShader(ShaderType::VertexShader, (std::string("#define VERTEX\n") + define).c_str());
			ShadersList[options]->CompileShader(ShaderType::FragmentShader, (std::string("#define FRAGMENT\n") + define).c_str());

			ShadersList[options]->LinkProgram();
		}

		// Save Shader Location
		shaderID = options;

		// Add Counter
		ShadersList[options]->currentMaterials++;

		// Get Shader Program
		shaderProgram = ShadersList[options]->ShaderProgram();

		// Back Face Culling
		cullFace = CullFace::BackFace;

		// Always used Uniforms
		AddUniform(Uniform("uProjectionMatrix", Uniforms::DataUsage::ProjectionMatrix));
		AddUniform(Uniform("uViewMatrix", Uniforms::DataUsage::ViewMatrix));
		AddUniform(Uniform("uModelMatrix", Uniforms::DataUsage::ModelMatrix));

		// Default Opacity
		SetOpacity(1.0);

		// Default PCF Texel Size
		PCFTexelSize1 = 0.0001f;
		PCFTexelSize2 = 0.0001f;
		PCFTexelSize3 = 0.0001f;
		PCFTexelSize4 = 0.0001f;

		if (options & ShaderUsage::Diffuse)
		{
			UseLights = 1.0;
			Shininess = 50.0;
			AddUniform(Uniform("uLights", Uniforms::DataUsage::Lights));
			AddUniform(Uniform("uNumberOfLights", Uniforms::DataUsage::NumberOfLights));
			AddUniform(Uniform("uAmbientLight", Uniforms::DataUsage::GlobalAmbientLight));
			uUseLights = AddUniform(Uniform("uUseLights", Uniforms::DataType::Float, &UseLights));
			AddUniform(Uniform("uCameraPos", Uniforms::DataUsage::CameraPosition));
			uShininess = AddUniform(Uniform("uShininess", Uniforms::DataType::Float, &Shininess));
		}

		if (options & ShaderUsage::CellShading)
		{
			UseLights = 1.0;
			Shininess = 50.0;
			AddUniform(Uniform("uLights", Uniforms::DataUsage::Lights));
			AddUniform(Uniform("uNumberOfLights", Uniforms::DataUsage::NumberOfLights));
			AddUniform(Uniform("uAmbientLight", Uniforms::DataUsage::GlobalAmbientLight));
			uUseLights = AddUniform(Uniform("uUseLights", Uniforms::DataType::Float, &UseLights));
			AddUniform(Uniform("uCameraPos", Uniforms::DataUsage::CameraPosition));
			uShininess = AddUniform(Uniform("uShininess", Uniforms::DataType::Float, &Shininess));
		}

		if (options & ShaderUsage::DirectionalShadow)
		{
			// Shadows
			AddUniform(Uniform("uDirectionalShadowMaps", Uniforms::DataUsage::DirectionalShadowMap));
			AddUniform(Uniform("uDirectionalDepthsMVP", Uniforms::DataUsage::DirectionalShadowMatrix));
			AddUniform(Uniform("uDirectionalShadowFar", Uniforms::DataUsage::DirectionalShadowFar));
			AddUniform(Uniform("uNumberOfDirectionalShadows", Uniforms::DataUsage::NumberOfDirectionalShadows));
			uPCFTexelSize1 = AddUniform(Uniform("uPCFTexelSize1", Uniforms::DataType::Float, &PCFTexelSize1));
			uPCFTexelSize2 = AddUniform(Uniform("uPCFTexelSize2", Uniforms::DataType::Float, &PCFTexelSize2));
			uPCFTexelSize3 = AddUniform(Uniform("uPCFTexelSize3", Uniforms::DataType::Float, &PCFTexelSize3));
			uPCFTexelSize4 = AddUniform(Uniform("uPCFTexelSize4", Uniforms::DataType::Float, &PCFTexelSize4));
			isCastingShadows = true;
		}

		if (options & ShaderUsage::PointShadow)
		{
			// Shadows
			AddUniform(Uniform("uPointShadowMaps", Uniforms::DataUsage::PointShadowMap));
			AddUniform(Uniform("uPointDepthsMVP", Uniforms::DataUsage::PointShadowMatrix));
			AddUniform(Uniform("uNumberOfPointShadows", Uniforms::DataUsage::NumberOfPointShadows));
			uPCFTexelSize1 = AddUniform(Uniform("uPCFTexelSize", Uniforms::DataType::Float, &PCFTexelSize1));
			isCastingShadows = true;
		}

		if (options & ShaderUsage::SpotShadow)
		{
			// Shadows
			AddUniform(Uniform("uSpotShadowMaps", Uniforms::DataUsage::SpotShadowMap));
			AddUniform(Uniform("uSpotDepthsMVP", Uniforms::DataUsage::SpotShadowMatrix));
			AddUniform(Uniform("uNumberOfSpotShadows", Uniforms::DataUsage::NumberOfSpotShadows));
			uPCFTexelSize1 = AddUniform(Uniform("uPCFTexelSize", Uniforms::DataType::Float, &PCFTexelSize1));
			isCastingShadows = true;
		}

		if (options & ShaderUsage::EnvMap)
		{
			AddUniform(Uniform("uCameraPos", Uniforms::DataUsage::CameraPosition));
			// Set Default Reflectivity
			Reflectivity = 1.0;
			uReflectivity = AddUniform(Uniform("uReflectivity", Uniforms::DataType::Float, &Reflectivity));
		}

		if (options & ShaderUsage::Refraction)
		{
			AddUniform(Uniform("uCameraPos", Uniforms::DataUsage::CameraPosition));
			// Set Default Reflectivity
			Reflectivity = 1.0;
			uReflectivity = AddUniform(Uniform("uReflectivity", Uniforms::DataType::Float, &Reflectivity));
		}
		if (options & ShaderUsage::Skinning)
		{
			AddUniform(Uniform("uBoneMatrix", Uniforms::DataUsage::Skinning));
		}
		if (options & ShaderUsage::ClipPlane)
		{
			AddUniform(Uniform("uClipPlanes", Uniforms::DataUsage::ClipPlanes));
		}
		if (options & ShaderUsage::TextRendering)
		{
			SetTransparencyFlag(true);
		}
	}

	void GenericShaderMaterial::SetShininess(const f32 shininess)
	{
		this->Shininess = shininess;
		//SetUniformValue("uShininess", &this->Shininess);
	}
	GenericShaderMaterial::~GenericShaderMaterial()
	{
		// Delete Shaders
		if (ShadersList.find(shaderID) != ShadersList.end())
		{
			ShadersList[shaderID]->currentMaterials--;
			if (ShadersList[shaderID]->currentMaterials == 0)
			{
				delete ShadersList[shaderID];
				ShadersList.erase(ShadersList.find(shaderID));
			}
		}
	}

	void GenericShaderMaterial::AddTexture(const std::string &uniformName, Texture* texture)
	{
		uint32 id = Textures.size();

		// Save on Lirest
		Textures.push_back(texture);
		// Set Uniform
		AddUniform(Uniform(uniformName.c_str(), Uniforms::DataType::Int, &id));
	}

	void GenericShaderMaterial::BindTextures()
	{
		for (std::vector<Texture*>::iterator i = Textures.begin(); i != Textures.end(); i++)
		{
			(*i)->Bind();
		}
	}
	void GenericShaderMaterial::UnbindTextures()
	{
		for (std::vector<Texture*>::reverse_iterator i = Textures.rbegin(); i != Textures.rend(); i++)
		{
			(*i)->Unbind();
		}
	}

	void GenericShaderMaterial::SetColor(const Vec4& color)
	{
		Vec4 Color = color;
		if (!uColor)
			uColor = AddUniform(Uniform("uColor", Uniforms::DataType::Vec4, &Color));
		else
			uColor->SetValue(&Color);
	}
	void GenericShaderMaterial::SetSpecular(const Vec4& specularColor)
	{
		Vec4 Specular = specularColor;
		if (!uSpecular)
			uSpecular = AddUniform(Uniform("uSpecular", Uniforms::DataType::Vec4, &Specular));
		else uSpecular->SetValue(&Specular);
	}

	void GenericShaderMaterial::SetColorMap(Texture* colormap)
	{
		if (colorMapID == -1)
			colorMapID = Textures.size();
		else {
			Textures[colorMapID] = colormap;
			return;
		}
		// Save on List
		Textures.push_back(colormap);
		// Set Uniform
		AddUniform(Uniform("uColormap", Uniforms::DataType::Int, &colorMapID));
	}
	void GenericShaderMaterial::SetSpecularMap(Texture* specular)
	{
		if (specularMapID == -1)
			specularMapID = Textures.size();
		else {
			Textures[specularMapID] = specular;
			return;
		}
		// Save on List
		Textures.push_back(specular);
		// Set Uniform
		AddUniform(Uniform("uSpecularmap", Uniforms::DataType::Int, &specularMapID));
	}
	void GenericShaderMaterial::SetNormalMap(Texture* normalmap)
	{
		if (normalMapID == -1)
			normalMapID = Textures.size();
		else {
			Textures[normalMapID] = normalmap;
			return;
		}
		// Save on List
		Textures.push_back(normalmap);
		// Set Uniform
		AddUniform(Uniform("uNormalmap", Uniforms::DataType::Int, &normalMapID));
	}
	void GenericShaderMaterial::SetEnvMap(Texture* envmap)
	{
		if (envMapID == -1)
			envMapID = Textures.size();
		else {
			Textures[envMapID] = envmap;
			return;
		}
		// Save on List
		Textures.push_back(envmap);
		// Set Uniform
		AddUniform(Uniform("uEnvmap", Uniforms::DataType::Int, &envMapID));
	}
	void GenericShaderMaterial::SetReflectivity(const f32 reflectivity)
	{
		Reflectivity = reflectivity;
		if (!uReflectivity)
			AddUniform(Uniform("uReflectivity", Uniforms::DataType::Float, &Reflectivity));
		else
			uReflectivity->SetValue(&Reflectivity);
	}
	void GenericShaderMaterial::SetRefractMap(Texture* refractmap)
	{
		if (refractMapID == -1)
			refractMapID = Textures.size();
		else {
			Textures[refractMapID] = refractmap;
			return;
		}
		// Save on List
		Textures.push_back(refractmap);
		// Set Uniform
		AddUniform(Uniform("uRefractmap", Uniforms::DataType::Int, &refractMapID));
	}
	void GenericShaderMaterial::SetSkyboxMap(Texture* skyboxmap)
	{
		if (skyboxMapID == -1)
			skyboxMapID = Textures.size();
		else {
			Textures[skyboxMapID] = skyboxmap;
			return;
		}
		// Save on List
		Textures.push_back(skyboxmap);
		// Set Uniform
		AddUniform(Uniform("uSkyboxmap", Uniforms::DataType::Int, &skyboxMapID));
	}
	void GenericShaderMaterial::SetTextFont(Font* font)
	{
		if (fontMapID == -1)
			fontMapID = Textures.size();
		else {
			Textures[fontMapID] = font->GetTexture();
			return;
		}
		// Save on List
		Textures.push_back(font->GetTexture());
		// Set Uniform
		AddUniform(Uniform("uFontmap", Uniforms::DataType::Int, &fontMapID));
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
		uPCFTexelSize1->SetValue(&PCFTexelSize1);
	}
	void GenericShaderMaterial::SetPCFTexelCascadesSize(const f32 texel1, const f32 texel2, const f32 texel3, const f32 texel4)
	{
		PCFTexelSize1 = texel1;
		PCFTexelSize2 = texel2;
		PCFTexelSize3 = texel3;
		PCFTexelSize4 = texel4;

		uPCFTexelSize1->SetValue(&PCFTexelSize1);
		uPCFTexelSize2->SetValue(&PCFTexelSize2);
		uPCFTexelSize3->SetValue(&PCFTexelSize3);
		uPCFTexelSize4->SetValue(&PCFTexelSize4);
	}
}
