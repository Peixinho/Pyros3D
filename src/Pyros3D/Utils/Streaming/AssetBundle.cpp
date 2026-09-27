//============================================================================
// Name        : AssetBundle.cpp
// Author      : Duarte Peixinho
// Description : See AssetBundle.h.
//============================================================================

#include <Pyros3D/Utils/Streaming/AssetBundle.h>
#include <Pyros3D/Utils/Streaming/LoadStats.h>
#include <Pyros3D/Utils/ModelLoaders/MultiModelLoader/ModelLoader.h>
#include <Pyros3D/Utils/Jobs/JobSystem.h>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <functional>

namespace p3d {

	namespace {
		// Main thread only - see Use.
		AssetBundle* g_current = NULL;

		size_t ModelBytes(const ModelLoader &m)
		{
			size_t b = 0;
			for (size_t i = 0; i < m.subMeshes.size(); i++)
			{
				const SubMesh &s = m.subMeshes[i];
				b += s.tIndex.size() * sizeof(uint32);
				b += (s.tVertex.size() + s.tNormal.size() + s.tTangent.size() + s.tBitangent.size()) * sizeof(Vec3);
				b += s.tTexcoord.size() * sizeof(Vec2);
				b += (s.tVertexColor.size() + s.tBonesID.size() + s.tBonesWeight.size()) * sizeof(Vec4);
			}
			return b;
		}

		// fn(i) for every path, biggest file first, each lane pulling the
		// next index as it frees up. ParallelFor's even split does not suit
		// this: one 4k texture takes as long as twenty icons, and a lane
		// dealt three of them finishes long after the rest are idle.
		void ForEachLargestFirst(const std::vector<std::string> &in, const bool parallel, const std::function<void(const std::string &)> &fn)
		{
			std::vector<std::pair<uintmax_t, std::string> > sized;
			sized.reserve(in.size());
			for (size_t i = 0; i < in.size(); i++)
			{
				std::error_code ec;
				const uintmax_t bytes = std::filesystem::file_size(in[i], ec);
				sized.push_back(std::make_pair(ec ? 0 : bytes, in[i]));
			}
			std::sort(sized.begin(), sized.end(),
				[](const std::pair<uintmax_t, std::string> &a, const std::pair<uintmax_t, std::string> &b) { return a.first > b.first; });

			const uint32 count = (uint32)sized.size();
			if (!parallel)
			{
				for (uint32 i = 0; i < count; i++) fn(sized[i].second);
				return;
			}
			std::atomic<uint32> next(0);
			const uint32 lanes = std::min(count, JobSystem::Instance().WorkerCount() + 1);
			JobSystem::Instance().ParallelFor(lanes, 1, [&](uint32, uint32) {
				for (uint32 i = next.fetch_add(1); i < count; i = next.fetch_add(1))
					fn(sized[i].second);
			});
		}
	}

	AssetBundle::AssetBundle() : bytes(0) {}

	AssetBundle::~AssetBundle()
	{
		// A bundle dying while it is the open one would leave loads
		// reading freed memory; Use's destructor must have run first.
		if (g_current == this) g_current = NULL;
	}

	bool AssetBundle::AddImage(const std::string &path)
	{
		{
			std::lock_guard<std::mutex> lock(mutex);
			if (images.count(path)) return true;
		}
		Texture::DecodedImage image;
		if (!Texture::DecodeFile(path, image)) return false;
		std::lock_guard<std::mutex> lock(mutex);
		if (images.count(path)) return true;
		bytes += image.pixels.size();
		images[path] = std::move(image);
		return true;
	}

	bool AssetBundle::AddModel(const std::string &path, std::vector<std::string>* texturesOut)
	{
		std::unique_ptr<ModelLoader> loader(new ModelLoader());
		{
			LoadStats::Scope t(LoadStats::ModelParse);
			if (!loader->Load(path)) return false;
		}
		if (texturesOut)
			for (size_t i = 0; i < loader->materials.size(); i++)
			{
				const MaterialProperties &m = loader->materials[i];
				if (m.haveColorMap) texturesOut->push_back(m.colorMap);
				if (m.haveSpecularMap) texturesOut->push_back(m.specularMap);
				if (m.haveNormalMap) texturesOut->push_back(m.normalMap);
			}
		std::lock_guard<std::mutex> lock(mutex);
		// One copy per path: only one Model takes it either way.
		if (!models.count(path))
		{
			bytes += ModelBytes(*loader);
			models[path] = std::move(loader);
		}
		return true;
	}

	void AssetBundle::Fill(const std::vector<std::string> &modelPaths, const std::vector<std::string> &imagePaths, bool parallel,
		std::vector<std::string>* modelTexturesOut)
	{
		// Missing files are left for the real load to report, once.
		std::vector<std::string> existingModels;
		for (size_t i = 0; i < modelPaths.size(); i++)
		{
			std::error_code ec;
			if (std::filesystem::is_regular_file(modelPaths[i], ec)) existingModels.push_back(modelPaths[i]);
		}
		std::mutex texturesMutex;
		std::vector<std::string> modelTextures;
		ForEachLargestFirst(existingModels, parallel, [&](const std::string &path) {
			std::vector<std::string> t;
			if (!AddModel(path, &t)) return;
			std::lock_guard<std::mutex> lock(texturesMutex);
			modelTextures.insert(modelTextures.end(), t.begin(), t.end());
		});

		if (modelTexturesOut) modelTexturesOut->insert(modelTexturesOut->end(), modelTextures.begin(), modelTextures.end());

		// Model textures and material maps load through Texture::LoadShared,
		// so one already alive will not be decoded by the load - skip it.
		// Any other image with the same path decodes on demand instead,
		// which is rare. "*0" names a texture embedded in a model file.
		std::vector<std::string> all;
		std::vector<std::string> candidates(imagePaths);
		candidates.insert(candidates.end(), modelTextures.begin(), modelTextures.end());
		for (size_t i = 0; i < candidates.size(); i++)
		{
			const std::string &p = candidates[i];
			if (!p.empty() && p[0] != '*' && !Texture::IsSharedLoaded(p))
				all.push_back(p);
		}
		std::sort(all.begin(), all.end());
		all.erase(std::unique(all.begin(), all.end()), all.end());
		std::vector<std::string> existing;
		for (size_t i = 0; i < all.size(); i++)
		{
			std::error_code ec;
			if (std::filesystem::is_regular_file(all[i], ec)) existing.push_back(all[i]);
		}
		ForEachLargestFirst(existing, parallel, [this](const std::string &path) { AddImage(path); });
	}

	namespace {
		size_t HeightfieldBytes(const PreparedHeightfield &p)
		{
			size_t b = p.data ? p.data->heights.size() * sizeof(f32) : 0;
			for (size_t i = 0; i < p.meshes.size(); i++)
			{
				const HeightfieldMesh &m = p.meshes[i];
				b += (m.vertex.size() + m.normal.size() + m.tangent.size() + m.bitangent.size()) * sizeof(Vec3)
					+ m.texcoord.size() * sizeof(Vec2) + m.index.size() * sizeof(uint32);
			}
			return b;
		}
	}

	void AssetBundle::AddHeightfield(const std::string &key, const std::shared_ptr<PreparedHeightfield> &prepared)
	{
		if (!prepared) return;
		std::lock_guard<std::mutex> lock(mutex);
		if (heightfields.count(key)) return;
		bytes += HeightfieldBytes(*prepared);
		heightfields[key] = prepared;
	}

	std::shared_ptr<PreparedHeightfield> AssetBundle::TakeHeightfield(const std::string &key)
	{
		AssetBundle* b = g_current;
		if (!b) return std::shared_ptr<PreparedHeightfield>();
		std::lock_guard<std::mutex> lock(b->mutex);
		std::map<std::string, std::shared_ptr<PreparedHeightfield> >::iterator it = b->heightfields.find(key);
		if (it == b->heightfields.end()) return std::shared_ptr<PreparedHeightfield>();
		std::shared_ptr<PreparedHeightfield> p = it->second;
		b->bytes -= HeightfieldBytes(*p);
		b->heightfields.erase(it);
		return p;
	}

	void AssetBundle::AddFoliage(const std::string &key, const std::shared_ptr<PreparedFoliageLayer> &prepared)
	{
		if (!prepared) return;
		std::lock_guard<std::mutex> lock(mutex);
		if (foliage.count(key)) return;
		for (size_t i = 0; i < prepared->blocks.size(); i++)
			bytes += prepared->blocks[i].transforms.size() * (sizeof(Matrix) + sizeof(Vec4));
		foliage[key] = prepared;
	}

	std::shared_ptr<PreparedFoliageLayer> AssetBundle::TakeFoliage(const std::string &key)
	{
		AssetBundle* b = g_current;
		if (!b) return std::shared_ptr<PreparedFoliageLayer>();
		std::lock_guard<std::mutex> lock(b->mutex);
		std::map<std::string, std::shared_ptr<PreparedFoliageLayer> >::iterator it = b->foliage.find(key);
		if (it == b->foliage.end()) return std::shared_ptr<PreparedFoliageLayer>();
		std::shared_ptr<PreparedFoliageLayer> p = it->second;
		for (size_t i = 0; i < p->blocks.size(); i++)
			b->bytes -= p->blocks[i].transforms.size() * (sizeof(Matrix) + sizeof(Vec4));
		b->foliage.erase(it);
		return p;
	}

	size_t AssetBundle::ImageCount() const { std::lock_guard<std::mutex> lock(mutex); return images.size(); }
	size_t AssetBundle::ModelCount() const { std::lock_guard<std::mutex> lock(mutex); return models.size(); }
	size_t AssetBundle::ByteSize() const { std::lock_guard<std::mutex> lock(mutex); return bytes; }

	AssetBundle::Use::Use(AssetBundle &bundle) : previous(g_current) { g_current = &bundle; }
	AssetBundle::Use::~Use() { g_current = previous; }

	bool AssetBundle::TakeImage(const std::string &path, Texture::DecodedImage &out)
	{
		AssetBundle* b = g_current;
		if (!b) return false;
		std::lock_guard<std::mutex> lock(b->mutex);
		std::map<std::string, Texture::DecodedImage>::iterator it = b->images.find(path);
		if (it == b->images.end()) return false;
		b->bytes -= it->second.pixels.size();
		out = std::move(it->second);
		b->images.erase(it);
		return true;
	}

	ModelLoader* AssetBundle::TakeModel(const std::string &path)
	{
		AssetBundle* b = g_current;
		if (!b) return NULL;
		std::lock_guard<std::mutex> lock(b->mutex);
		std::map<std::string, std::unique_ptr<ModelLoader> >::iterator it = b->models.find(path);
		if (it == b->models.end()) return NULL;
		b->bytes -= ModelBytes(*it->second);
		ModelLoader* m = it->second.release();
		b->models.erase(it);
		return m;
	}

}
