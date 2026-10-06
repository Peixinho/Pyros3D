//============================================================================
// Name        : Model.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Model Geometry
//============================================================================

#include <cstdint>
#include <cmath>
#include <unordered_map>
#include <filesystem>
#include <algorithm>
#include <map>
#include <mutex>
#include <Pyros3D/Assets/Renderable/Models/Model.h>
#include <Pyros3D/Utils/Streaming/LoadStats.h>
#include <Pyros3D/Utils/Streaming/AssetBundle.h>

namespace p3d {

	void ModelGeometry::CreateBuffers()
	{
		// Calculate SubMesh Bounding Box
		CalculateBounding();

		AttributeBuffer* Vertex = new AttributeBuffer(Buffer::Type::Attribute, Buffer::Draw::Static);
		if (tVertex.size() > 0) Vertex->AddAttribute("aPosition", Buffer::Attribute::Type::Vec3, &tVertex[0], tVertex.size());
		if (tNormal.size() > 0) Vertex->AddAttribute("aNormal", Buffer::Attribute::Type::Vec3, &tNormal[0], tNormal.size());
		if (tTexcoord.size() > 0) Vertex->AddAttribute("aTexcoord", Buffer::Attribute::Type::Vec2, &tTexcoord[0], tTexcoord.size());
		if (tTangent.size() > 0) Vertex->AddAttribute("aTangent", Buffer::Attribute::Type::Vec3, &tTangent[0], tTangent.size());
		if (tBitangent.size() > 0) Vertex->AddAttribute("aBitangent", Buffer::Attribute::Type::Vec3, &tBitangent[0], tBitangent.size());
		if (tBonesID.size() > 0) Vertex->AddAttribute("aBonesID", Buffer::Attribute::Type::Vec4, &tBonesID[0], tBonesID.size());
		if (tBonesWeight.size() > 0) Vertex->AddAttribute("aBonesWeight", Buffer::Attribute::Type::Vec4, &tBonesWeight[0], tBonesWeight.size());
		Attributes.push_back(Vertex);
	}

	void ModelGeometry::CalculateBounding()
	{
		// Bounding Box
		for (uint32 i = 0; i < tVertex.size(); i++)
		{
			if (i == 0) {
				minBounds = tVertex[i];
				maxBounds = tVertex[i];
			}
			else {
				if (tVertex[i].x < minBounds.x) minBounds.x = tVertex[i].x;
				if (tVertex[i].y < minBounds.y) minBounds.y = tVertex[i].y;
				if (tVertex[i].z < minBounds.z) minBounds.z = tVertex[i].z;
				if (tVertex[i].x > maxBounds.x) maxBounds.x = tVertex[i].x;
				if (tVertex[i].y > maxBounds.y) maxBounds.y = tVertex[i].y;
				if (tVertex[i].z > maxBounds.z) maxBounds.z = tVertex[i].z;
			}
		}
		// Bounding Sphere — center on the AABB, not the origin (offset meshes
		// used to get a huge radius = distance from 0 to the farthest corner).
		BoundingSphereCenter = (minBounds + maxBounds) * 0.5f;
		BoundingSphereRadius = maxBounds.distance(BoundingSphereCenter);
	}

	namespace {
		std::map<std::string, std::weak_ptr<Model> > &SharedModels() { static std::map<std::string, std::weak_ptr<Model> > m; return m; }
		std::vector<std::string> &SharedModelRequests() { static std::vector<std::string> v; return v; }
		std::mutex &SharedModelsMutex() { static std::mutex m; return m; }
	}

	std::shared_ptr<Model> Model::LoadShared(const std::string &ModelPath, bool mergeMeshes)
	{
		// (one spelling for a file, however it was written: "assets/x" and
		// "/the/game/assets/x" are the same model)
		std::error_code ec;
		std::string full = std::filesystem::absolute(std::filesystem::path(ModelPath), ec).lexically_normal().string();
		if (ec || full.empty()) full = ModelPath;
		const std::string key = full + (mergeMeshes ? "|m" : "|s");
		{
			std::lock_guard<std::mutex> lock(SharedModelsMutex());
			std::vector<std::string> &asked = SharedModelRequests();
			if (std::find(asked.begin(), asked.end(), full) == asked.end()) asked.push_back(full);
			std::map<std::string, std::weak_ptr<Model> >::iterator it = SharedModels().find(key);
			if (it != SharedModels().end())
			{
				if (std::shared_ptr<Model> hit = it->second.lock()) return hit;
				SharedModels().erase(it);
			}
		}
		std::shared_ptr<Model> made = std::make_shared<Model>(ModelPath, mergeMeshes);
		std::lock_guard<std::mutex> lock(SharedModelsMutex());
		SharedModels()[key] = made;
		return made;
	}

	void Model::ForgetShared(const std::string &ModelPath)
	{
		std::lock_guard<std::mutex> lock(SharedModelsMutex());
		std::error_code ec;
		std::string full = std::filesystem::absolute(std::filesystem::path(ModelPath), ec).lexically_normal().string();
		if (ec || full.empty()) full = ModelPath;
		SharedModels().erase(full + "|m");
		SharedModels().erase(full + "|s");
	}

	std::vector<std::string> Model::SharedRequests(const bool clear)
	{
		std::lock_guard<std::mutex> lock(SharedModelsMutex());
		std::vector<std::string> out = SharedModelRequests();
		if (clear) SharedModelRequests().clear();
		return out;
	}

	Model::Model(const std::string ModelPath, bool mergeMeshes)
	{
		Path = ModelPath;
		MergeMeshes = mergeMeshes;

		bool loaded = true;
		if (ModelLoader* parked = AssetBundle::TakeModel(ModelPath))
			mesh = parked;
		else
		{
			mesh = new ModelLoader();
			LoadStats::Scope t(LoadStats::ModelParse);
			loaded = mesh->Load(ModelPath);
		}
		LoadStats::Scope build(LoadStats::ModelBuild);
		if (!loaded)
		{
			echo(std::string("ERROR: Model - failed to load ") + ModelPath);
			delete mesh;
			mesh = NULL;
			return;
		}

		std::map<uint32, ModelGeometry*> meshes;

		// List of Material Properties
		std::vector<MaterialProperties> materialProperties;
		// Build Materials
		for (uint32 i = 0; i < mesh->materials.size(); i++)
		{
			materialProperties.push_back(mesh->materials[i]);
		}

		for (uint32 i = 0; i < mesh->subMeshes.size(); i++)
		{
			if (mesh->subMeshes[i].tIndex.size()>0)
			{
				if (mergeMeshes)
				{
					if (meshes.find(mesh->subMeshes[i].materialID) == meshes.end())
					{

						ModelGeometry* c_submesh = new ModelGeometry();

						// Set Material From ID
						c_submesh->materialProperties = materialProperties[mesh->subMeshes[i].materialID];

						c_submesh->index = mesh->subMeshes[i].tIndex;

						if (mesh->subMeshes[i].hasVertex == true)
							c_submesh->tVertex = std::move(mesh->subMeshes[i].tVertex);
						if (mesh->subMeshes[i].hasNormal == true)
							c_submesh->tNormal = std::move(mesh->subMeshes[i].tNormal);
						if (mesh->subMeshes[i].hasTexcoord == true)
							c_submesh->tTexcoord = std::move(mesh->subMeshes[i].tTexcoord);
						if (mesh->subMeshes[i].hasTangentBitangent == true)
						{
							c_submesh->tTangent = std::move(mesh->subMeshes[i].tTangent);
							c_submesh->tBitangent = std::move(mesh->subMeshes[i].tBitangent);
						}
						if (mesh->subMeshes[i].hasBones == true)
						{
							c_submesh->tBonesID = std::move(mesh->subMeshes[i].tBonesID);
							c_submesh->tBonesWeight = std::move(mesh->subMeshes[i].tBonesWeight);

							// Save SubMesh Map
							c_submesh->MapBoneIDs = mesh->subMeshes[i].MapBoneIDs;
							// Save SubMesh Bones Offset Matrix
							c_submesh->BoneOffsetMatrix = mesh->subMeshes[i].BoneOffsetMatrix;
							// Set Skinning Flag on
							c_submesh->materialProperties.haveBones = true;
						}

						meshes[mesh->subMeshes[i].materialID] = c_submesh;

					}
					else {

						// JOIN MESHES

						// Set SubMesh From Material ID
						ModelGeometry* c_submesh = meshes[mesh->subMeshes[i].materialID];

						uint32 offset = (uint32)c_submesh->tVertex.size();
						const size_t addIdx = mesh->subMeshes[i].tIndex.size();
						c_submesh->index.reserve(c_submesh->index.size() + addIdx);
						for (uint32 k = 0; k < addIdx; k++)
							c_submesh->index.push_back(mesh->subMeshes[i].tIndex[k] + offset);

						if (mesh->subMeshes[i].hasVertex == true)
						{
							c_submesh->tVertex.resize(offset + mesh->subMeshes[i].tVertex.size());
							memcpy(&c_submesh->tVertex[offset], &mesh->subMeshes[i].tVertex[0], mesh->subMeshes[i].tVertex.size()*sizeof(Vec3));
						}
						if (mesh->subMeshes[i].hasNormal == true)
						{
							c_submesh->tNormal.resize(offset + mesh->subMeshes[i].tNormal.size());
							memcpy(&c_submesh->tNormal[offset], &mesh->subMeshes[i].tNormal[0], mesh->subMeshes[i].tNormal.size()*sizeof(Vec3));
						}
						if (mesh->subMeshes[i].hasTexcoord == true)
						{
							c_submesh->tTexcoord.resize(offset + mesh->subMeshes[i].tTexcoord.size());
							memcpy(&c_submesh->tTexcoord[offset], &mesh->subMeshes[i].tTexcoord[0], mesh->subMeshes[i].tTexcoord.size()*sizeof(Vec2));
						}
						if (mesh->subMeshes[i].hasTangentBitangent == true)
						{
							c_submesh->tTangent.resize(offset + mesh->subMeshes[i].tTangent.size());
							memcpy(&c_submesh->tTangent[offset], &mesh->subMeshes[i].tTangent[0], mesh->subMeshes[i].tTangent.size()*sizeof(Vec3));

							c_submesh->tBitangent.resize(offset + mesh->subMeshes[i].tBitangent.size());
							memcpy(&c_submesh->tBitangent[offset], &mesh->subMeshes[i].tBitangent[0], mesh->subMeshes[i].tBitangent.size()*sizeof(Vec3));
						}
						if (mesh->subMeshes[i].hasBones == true)
						{
							c_submesh->tBonesID.resize(mesh->subMeshes[i].tVertex.size());
							memcpy(&c_submesh->tBonesID[0], &mesh->subMeshes[i].tBonesID[0], mesh->subMeshes[i].tVertex.size()*sizeof(Vec4));

							c_submesh->tBonesWeight.resize(mesh->subMeshes[i].tVertex.size());
							memcpy(&c_submesh->tBonesWeight[0], &mesh->subMeshes[i].tBonesWeight[0], mesh->subMeshes[i].tVertex.size()*sizeof(Vec4));

							// Save SubMesh Map
							c_submesh->MapBoneIDs = mesh->subMeshes[i].MapBoneIDs;
							// Save SubMesh Bones Offset Matrix
							c_submesh->BoneOffsetMatrix = mesh->subMeshes[i].BoneOffsetMatrix;
							// Set Skinning Flag on
							c_submesh->materialProperties.haveBones = true;
						}
					}
				}
				else {

					// Not Merging
					ModelGeometry* c_submesh = new ModelGeometry();

					// Set Material From ID
					c_submesh->materialProperties = materialProperties[mesh->subMeshes[i].materialID];

					// Fix passing for short if needed
					for (uint32 indexx = 0; indexx < mesh->subMeshes[i].tIndex.size(); indexx++)
					{
						c_submesh->index.push_back(mesh->subMeshes[i].tIndex[indexx]);
					}

					if (mesh->subMeshes[i].hasVertex == true)
					{
						c_submesh->tVertex.resize(mesh->subMeshes[i].tVertex.size());
						memcpy(&c_submesh->tVertex[0], &mesh->subMeshes[i].tVertex[0], mesh->subMeshes[i].tVertex.size()*sizeof(Vec3));
					}

					if (mesh->subMeshes[i].hasNormal == true)
					{
						c_submesh->tNormal.resize(mesh->subMeshes[i].tNormal.size());
						memcpy(&c_submesh->tNormal[0], &mesh->subMeshes[i].tNormal[0], mesh->subMeshes[i].tNormal.size()*sizeof(Vec3));
					}

					if (mesh->subMeshes[i].hasTexcoord == true)
					{
						c_submesh->tTexcoord.resize(mesh->subMeshes[i].tTexcoord.size());
						memcpy(&c_submesh->tTexcoord[0], &mesh->subMeshes[i].tTexcoord[0], mesh->subMeshes[i].tTexcoord.size()*sizeof(Vec2));
					}

					if (mesh->subMeshes[i].hasTangentBitangent == true)
					{
						c_submesh->tTangent.resize(mesh->subMeshes[i].tTangent.size());
						memcpy(&c_submesh->tTangent[0], &mesh->subMeshes[i].tTangent[0], mesh->subMeshes[i].tTangent.size()*sizeof(Vec3));

						c_submesh->tBitangent.resize(mesh->subMeshes[i].tBitangent.size());
						memcpy(&c_submesh->tBitangent[0], &mesh->subMeshes[i].tBitangent[0], mesh->subMeshes[i].tBitangent.size()*sizeof(Vec3));
					}

					if (mesh->subMeshes[i].hasBones == true)
					{
						c_submesh->tBonesID.resize(mesh->subMeshes[i].tVertex.size());
						memcpy(&c_submesh->tBonesID[0], &mesh->subMeshes[i].tBonesID[0], mesh->subMeshes[i].tBonesID.size()*sizeof(Vec4));

						c_submesh->tBonesWeight.resize(mesh->subMeshes[i].tVertex.size());
						memcpy(&c_submesh->tBonesWeight[0], &mesh->subMeshes[i].tBonesWeight[0], mesh->subMeshes[i].tBonesWeight.size()*sizeof(Vec4));

						// Save SubMesh Map
						c_submesh->MapBoneIDs = mesh->subMeshes[i].MapBoneIDs;
						// Save SubMesh Bones Offset Matrix
						c_submesh->BoneOffsetMatrix = mesh->subMeshes[i].BoneOffsetMatrix;
						// Set Skinning Flag on
						c_submesh->materialProperties.haveBones = true;
					}

					Geometries.push_back(c_submesh);

				}
			}
		}

		// Add Merged Meshes
		if (mergeMeshes)
			for (std::map<uint32, ModelGeometry*>::iterator i = meshes.begin(); i != meshes.end(); i++)
			{
				Geometries.push_back((*i).second);
			}

		// Save Skeleton
		skeleton = mesh->skeleton;

		// Build Meshes
		Build();

		// Delete Model Loader - its data has already been copied into
		// Geometries above; NULL it so the public mesh member can't be
		// mistaken for a live pointer afterward.
		delete mesh;
		mesh = NULL;
	}

	void Model::Build()
	{
		for (std::vector<IGeometry*>::iterator i = Geometries.begin(); i != Geometries.end(); i++)
		{
			ModelGeometry *geometry = (ModelGeometry*)(*i);
			// Create Attributes Buffers
			geometry->CreateBuffers();
			// Send Index and Attributes Buffers
			geometry->SendBuffers();
		}

		// Calculate Model's Bounding Box
		CalculateBounding();
	}

	namespace {
		// The cell of a grid a point falls in, as one number.
		inline uint64_t CellOf(const Vec3 &p, const Vec3 &lo, const f32 inv)
		{
			const uint64_t x = (uint64_t)std::max(0.f, (p.x - lo.x) * inv) & 0x1FFFFF;
			const uint64_t y = (uint64_t)std::max(0.f, (p.y - lo.y) * inv) & 0x1FFFFF;
			const uint64_t z = (uint64_t)std::max(0.f, (p.z - lo.z) * inv) & 0x1FFFFF;
			return (x << 42) | (y << 21) | z;
		}
		// How many triangles are left with cells this big (three corners in
		// three different cells).
		uint32 TrianglesLeft(const ModelGeometry &g, const Vec3 &lo, const f32 cell, std::vector<uint64_t> &cells)
		{
			const f32 inv = 1.f / cell;
			cells.resize(g.tVertex.size());
			for (size_t v = 0; v < g.tVertex.size(); v++) cells[v] = CellOf(g.tVertex[v], lo, inv);
			uint32 left = 0;
			for (size_t t = 0; t + 2 < g.index.size(); t += 3)
			{
				const uint64_t a = cells[g.index[t]], b = cells[g.index[t + 1]], c = cells[g.index[t + 2]];
				if (a != b && b != c && a != c) left++;
			}
			return left;
		}
	}

	SimplifiedModel::SimplifiedModel(const std::shared_ptr<Model> &source, const f32 ratio)
		: Ratio(std::max(0.01f, std::min(1.f, ratio))), Triangles(0), SourceTriangles(0)
	{
		if (!source) return;
		Path = source->GetPath();
		skeleton = source->GetSkeleton();
		for (size_t gi = 0; gi < source->Geometries.size(); gi++)
		{
			const ModelGeometry &g = *static_cast<ModelGeometry*>(source->Geometries[gi]);
			const uint32 tris = (uint32)(g.index.size() / 3);
			SourceTriangles += tris;
			ModelGeometry* out = new ModelGeometry();
			out->materialProperties = g.materialProperties;
			out->MapBoneIDs = g.MapBoneIDs;
			out->BoneOffsetMatrix = g.BoneOffsetMatrix;

			const uint32 want = std::max(4u, (uint32)((f32)tris * Ratio));
			if (tris <= 16 || want >= tris || g.tVertex.empty())
			{
				// nothing to gain: as it is
				out->index = g.index;
				out->tVertex = g.tVertex; out->tNormal = g.tNormal; out->tTexcoord = g.tTexcoord;
				out->tTangent = g.tTangent; out->tBitangent = g.tBitangent;
				out->tBonesID = g.tBonesID; out->tBonesWeight = g.tBonesWeight;
			}
			else
			{
				Vec3 lo = g.tVertex[0], hi = g.tVertex[0];
				for (size_t v = 1; v < g.tVertex.size(); v++)
				{
					const Vec3 &p = g.tVertex[v];
					lo.x = std::min(lo.x, p.x); lo.y = std::min(lo.y, p.y); lo.z = std::min(lo.z, p.z);
					hi.x = std::max(hi.x, p.x); hi.y = std::max(hi.y, p.y); hi.z = std::max(hi.z, p.z);
				}
				const f32 diagonal = std::max(1e-5f, hi.distance(lo));
				// The smallest cell that leaves no more than was asked for:
				// halved in on, between a thousandth of the model and half of it.
				std::vector<uint64_t> cells;
				f32 small = diagonal / 1024.f, big = diagonal * 0.5f;
				for (int step = 0; step < 14; step++)
				{
					const f32 mid = sqrtf(small * big);
					if (TrianglesLeft(g, lo, mid, cells) > want) small = mid; else big = mid;
				}
				TrianglesLeft(g, lo, big, cells);

				std::unordered_map<uint64_t, uint32> vertexOf;
				std::vector<uint32> members;      // how many of the model's vertices each new one stands for
				std::vector<uint32> newIndex(g.tVertex.size());
				const bool n = g.tNormal.size() == g.tVertex.size(), uv = g.tTexcoord.size() == g.tVertex.size();
				const bool tb = g.tTangent.size() == g.tVertex.size() && g.tBitangent.size() == g.tVertex.size();
				const bool bones = g.tBonesID.size() == g.tVertex.size() && g.tBonesWeight.size() == g.tVertex.size();
				for (size_t v = 0; v < g.tVertex.size(); v++)
				{
					std::unordered_map<uint64_t, uint32>::iterator it = vertexOf.find(cells[v]);
					if (it == vertexOf.end())
					{
						const uint32 id = (uint32)out->tVertex.size();
						vertexOf[cells[v]] = id;
						newIndex[v] = id;
						out->tVertex.push_back(g.tVertex[v]);
						members.push_back(1);
						// what cannot be averaged is the first one's: where on the
						// texture, which bones
						if (n) out->tNormal.push_back(g.tNormal[v]);
						if (uv) out->tTexcoord.push_back(g.tTexcoord[v]);
						if (tb) { out->tTangent.push_back(g.tTangent[v]); out->tBitangent.push_back(g.tBitangent[v]); }
						if (bones) { out->tBonesID.push_back(g.tBonesID[v]); out->tBonesWeight.push_back(g.tBonesWeight[v]); }
					}
					else
					{
						newIndex[v] = it->second;
						out->tVertex[it->second] += g.tVertex[v];
						members[it->second]++;
						if (n) out->tNormal[it->second] += g.tNormal[v];
					}
				}
				for (size_t v = 0; v < out->tVertex.size(); v++)
				{
					out->tVertex[v] = out->tVertex[v] / (f32)members[v];
					if (n)
					{
						const Vec3 &nn = out->tNormal[v];
						out->tNormal[v] = (nn.x * nn.x + nn.y * nn.y + nn.z * nn.z > 1e-12f) ? nn.normalize() : Vec3(0.f, 1.f, 0.f);
					}
				}
				for (size_t t = 0; t + 2 < g.index.size(); t += 3)
				{
					const uint32 a = newIndex[g.index[t]], b = newIndex[g.index[t + 1]], c = newIndex[g.index[t + 2]];
					if (a == b || b == c || a == c) continue;
					out->index.push_back((__INDEX_C_TYPE__)a); out->index.push_back((__INDEX_C_TYPE__)b); out->index.push_back((__INDEX_C_TYPE__)c);
				}
			}
			Triangles += (uint32)(out->index.size() / 3);
			out->CreateBuffers();
			out->SendBuffers();
			Geometries.push_back(out);
		}
		CalculateBounding();
	}

	std::shared_ptr<SimplifiedModel> SimplifiedModel::LoadShared(const std::string &ModelPath, const f32 ratio)
	{
		static std::map<std::string, std::weak_ptr<SimplifiedModel> > cache;
		static std::mutex guard;
		std::error_code ec;
		std::string full = std::filesystem::absolute(std::filesystem::path(ModelPath), ec).lexically_normal().string();
		if (ec || full.empty()) full = ModelPath;
		const std::string key = full + "|" + std::to_string((int)(std::max(0.01f, std::min(1.f, ratio)) * 1000.f + 0.5f));
		{
			std::lock_guard<std::mutex> lock(guard);
			std::map<std::string, std::weak_ptr<SimplifiedModel> >::iterator it = cache.find(key);
			if (it != cache.end()) { if (std::shared_ptr<SimplifiedModel> hit = it->second.lock()) return hit; cache.erase(it); }
		}
		std::shared_ptr<Model> source = Model::LoadShared(ModelPath, true);
		if (!source || source->Geometries.empty()) return std::shared_ptr<SimplifiedModel>();
		std::shared_ptr<SimplifiedModel> made = std::make_shared<SimplifiedModel>(source, ratio);
		std::lock_guard<std::mutex> lock(guard);
		cache[key] = made;
		return made;
	}

	// Debug Skeleton
	void Model::DebugSkeleton()
	{
		// show skeleton
		for (std::map<StringID, Bone>::iterator i = skeleton.begin(); i != skeleton.end(); i++)
		{
			if ((*i).second.self == 0)
			{
				std::cout << "ID: " << (*i).second.self << " Name: " << (*i).second.name << std::endl;
				GetBoneChilds(skeleton, 0, 0);
			}
		}
	}

	void Model::GetBoneChilds(std::map<StringID, Bone> Skeleton, const int32 id, const uint32 iterations)
	{
		for (std::map<StringID, Bone>::iterator i = Skeleton.begin(); i != Skeleton.end(); i++)
		{
			if ((*i).second.parent == id)
			{
				for (uint32 j = 0; j < iterations + 1; j++) if (j == iterations) std::cout << " |_"; else std::cout << "     ";
				std::cout << "___" << "ID: " << (*i).second.self << " Name: " << (*i).second.name << std::endl;
				GetBoneChilds(Skeleton, (*i).second.self, iterations + 1);
			}
		}
	}
};