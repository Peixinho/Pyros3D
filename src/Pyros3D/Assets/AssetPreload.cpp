//============================================================================
// Name        : AssetPreload.cpp
// Description : Assets made ready before they are first wanted
//============================================================================

#include <Pyros3D/Assets/AssetPreload.h>
#include <Pyros3D/Assets/Renderable/Models/Model.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Materials/GenericShaderMaterials/ShaderLib.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <memory>

namespace p3d {

	namespace {
		struct HeldAsset
		{
			std::shared_ptr<Model> model;
			// (it is making something that draws the model that reads the
			// model's textures, not reading the model)
			std::shared_ptr<RenderingComponent> drawer;
			std::shared_ptr<Texture> texture;
		};
		std::map<std::string, HeldAsset> &HeldAssets() { static std::map<std::string, HeldAsset> m; return m; }
		std::vector<std::string> &HeldOrder() { static std::vector<std::string> v; return v; }

		std::string LowerExtension(const std::string &path)
		{
			const size_t dot = path.find_last_of('.');
			if (dot == std::string::npos) return std::string();
			std::string e = path.substr(dot + 1);
			std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return (char)std::tolower(c); });
			return e;
		}
	}

	bool AssetPreload::Add(const std::string &path)
	{
		if (path.empty()) return false;
		if (HeldAssets().find(path) != HeldAssets().end()) return true;
		{
			std::ifstream probe(path.c_str(), std::ios::binary);
			if (!probe) { echo("WARNING: preload - no such file: " + path); return false; }
		}
		const std::string ext = LowerExtension(path);
		HeldAsset held;
		if (ext == "p3dm")
		{
			held.model = Model::LoadShared(path, true);
			if (!held.model) return false;
			held.drawer = std::make_shared<RenderingComponent>(held.model, ShaderUsage::Diffuse | ShaderUsage::DirectionalShadow);
		}
		else if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "tga" || ext == "dds" || ext == "ktx")
		{
			held.texture = Texture::LoadShared(path);
			if (!held.texture) return false;
		}
		else
			return false;
		HeldAssets()[path] = held;
		HeldOrder().push_back(path);
		return true;
	}

	void AssetPreload::Add(const std::vector<std::string> &paths)
	{
		for (size_t i = 0; i < paths.size(); i++) Add(paths[i]);
	}

	void AssetPreload::Clear()
	{
		HeldAssets().clear();
		HeldOrder().clear();
	}

	uint32_t AssetPreload::Count() { return (uint32_t)HeldAssets().size(); }
	std::vector<std::string> AssetPreload::Held() { return HeldOrder(); }

}
