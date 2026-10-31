//============================================================================
// Name        : CustomShaderMaterials.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Custom Shader Materials
//=================================================================================

#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#include <fstream>
#include <iterator>

namespace p3d
{

	void CustomShaderMaterial::LoadSurfaceBeside(const std::string &shaderFile)
	{
		if (shaderFile.empty()) return;
		std::ifstream surface((shaderFile + ".surface").c_str(), std::ios::binary);
		if (surface) SetSurfaceSource(std::string((std::istreambuf_iterator<char>(surface)), std::istreambuf_iterator<char>()));
	}

	CustomShaderMaterial::CustomShaderMaterial(const std::string& ShaderFile) : IMaterial()
	{
		ShaderFilePath = ShaderFile;
		LoadSurfaceBeside(ShaderFile);
		SetOpacity(1.0);
	}

	CustomShaderMaterial::~CustomShaderMaterial() {}

	void CustomShaderMaterial::DeclareParameter(const std::string &name, bool isVector, const Vec4 &defaultValue)
	{
		Parameter &p = parameters[name];
		p.isVector = isVector;
		p.value = defaultValue;
	}

	void CustomShaderMaterial::SetParameter(const std::string &name, const Vec4 &value)
	{
		std::map<std::string, Parameter>::iterator it = parameters.find(name);
		if (it == parameters.end()) { DeclareParameter(name, true, value); return; }
		it->second.value = value;
	}

	void CustomShaderMaterial::SetParameter(const std::string &name, f32 value)
	{
		std::map<std::string, Parameter>::iterator it = parameters.find(name);
		if (it == parameters.end()) { DeclareParameter(name, false, Vec4(value, value, value, value)); return; }
		SetParameter(name, it->second.isVector ? Vec4(value, value, value, value) : Vec4(value, 0.f, 0.f, 0.f));
	}

	Vec4 CustomShaderMaterial::GetParameter(const std::string &name) const
	{
		std::map<std::string, Parameter>::const_iterator it = parameters.find(name);
		return it == parameters.end() ? Vec4() : it->second.value;
	}

	bool CustomShaderMaterial::IsVectorParameter(const std::string &name) const
	{
		std::map<std::string, Parameter>::const_iterator it = parameters.find(name);
		return it != parameters.end() && it->second.isVector;
	}

	std::vector<std::string> CustomShaderMaterial::GetParameterNames() const
	{
		std::vector<std::string> names;
		for (std::map<std::string, Parameter>::const_iterator it = parameters.begin(); it != parameters.end(); ++it)
			names.push_back(it->first);
		return names;
	}

	void CustomShaderMaterial::RemoveParameter(const std::string &name)
	{
		parameters.erase(name);
	}

	void CustomShaderMaterial::AddSampler(const std::string &uniformName, const std::shared_ptr<Texture> &tex)
	{
		textures.push_back(tex);
		samplerNames.push_back(uniformName);
	}

	void CustomShaderMaterial::ClearSamplers()
	{
		textures.clear();
		samplerNames.clear();
	}
}
