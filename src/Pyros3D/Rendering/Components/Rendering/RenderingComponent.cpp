//============================================================================
// Name        : RenderingComponent
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Component For Rendering
//============================================================================

#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Device/GLRenderDevice.h>

namespace p3d {

	// Every RenderingMesh shares whichever backend is currently active
	// (see GetActiveRenderDevice() in IRenderDevice.h) - same pattern as
	// GeometryBuffer.cpp/Shaders.cpp. Texture.cpp/FrameBuffer.cpp still
	// hardcode GLRenderDevice directly (not part of RotatingCube's
	// Vulkan-validation path - see VULKAN_ROADMAP.md Phase 5 Step D).
	static IRenderDevice& Device()
	{
		return GetActiveRenderDevice();
	}

	// Initialize Rendering Components vector
	std::vector<IComponent*> RenderingComponent::Components;

	RenderingMesh::~RenderingMesh()
	{
		for (std::map<uint32, uint32>::iterator i = VAOCache.begin(); i != VAOCache.end(); i++)
		{
			Device().DeleteVertexArray(i->second);
		}
		for (std::map<uint64, uint32>::iterator i = PipelineCache.begin(); i != PipelineCache.end(); i++)
		{
			Device().DestroyPipeline(i->second);
		}
	}

	RenderingComponent::RenderingComponent(const std::shared_ptr<Renderable> &renderable, const std::shared_ptr<IMaterial> &Material, const f32 Distance) : IComponent()
	{
		// Keep renderable pointer
		this->renderable = renderable;

		isInstanced = false;

		// By Default Is Casting Shadows
		isCastingShadows = true;

		// By Default is Cull Testing
		cullTest = true;

		for (uint32 i = 0; i < renderable->Geometries.size(); i++)
		{
			// Rendering Mesh Instance
			RenderingMesh* r_submesh = new RenderingMesh();

			// Save Geometry Pointer
			r_submesh->Geometry = renderable->Geometries[i];
			// Get Geometry Specific Stuff
			if (renderable->Geometries[i]->materialProperties.haveBones)
			{
				r_submesh->MapBoneIDs = renderable->Geometries[i]->MapBoneIDs;
				r_submesh->BoneOffsetMatrix = renderable->Geometries[i]->BoneOffsetMatrix;
			}
			r_submesh->Material = Material;

			// Own this Mothafuckah!
			r_submesh->renderingComponent = this;

			// Push Mesh
			Meshes[0].push_back(r_submesh);
		}

		// Keep Skeleton
		skeleton = renderable->GetSkeleton();
		hasBones = (skeleton.size() > 0 ? true : false);

		// Bounding
		BoundingSphereRadius = renderable->GetBoundingSphereRadius();
		BoundingSphereCenter = renderable->GetBoundingSphereCenter();
		maxBounds = renderable->GetBoundingMaxValue();
		minBounds = renderable->GetBoundingMinValue();

		if (Distance > 0.f)
		{
			LODDistances.push_back(Distance);
			LOD = true;
		}
		else {
			// LOD
			LOD = false;
			LodInUse = 0;
			LastLodDistance = 0.f;
		}

	}

	RenderingComponent::RenderingComponent(const std::shared_ptr<Renderable> &renderable, const uint32 MaterialOptions, const f32 Distance) : IComponent()
	{

		isInstanced = false;

		this->renderable = renderable;

		uint32 LODLVL = Meshes.size();
		for (uint32 i = 0; i < renderable->Geometries.size(); i++)
		{
			// Rendering Mesh Instance
			RenderingMesh* r_submesh = new RenderingMesh(LODLVL);

			// Save Geometry Pointer
			r_submesh->Geometry = renderable->Geometries[i];
			// Get Geometry Specific Stuff
			r_submesh->BuildMaterials(MaterialOptions);
			if (renderable->Geometries[i]->materialProperties.haveBones)
			{
				r_submesh->MapBoneIDs = renderable->Geometries[i]->MapBoneIDs;
				r_submesh->BoneOffsetMatrix = renderable->Geometries[i]->BoneOffsetMatrix;
			}

			// Own this Mothafuckah!
			r_submesh->renderingComponent = this;

			// Push Mesh
			Meshes[LODLVL].push_back(r_submesh);
		}

		// Keep Skeleton
		skeleton = renderable->GetSkeleton();
		hasBones = (skeleton.size() > 0 ? true : false);

		// Bounding
		BoundingSphereRadius = renderable->GetBoundingSphereRadius();
		BoundingSphereCenter = renderable->GetBoundingSphereCenter();
		maxBounds = renderable->GetBoundingMaxValue();
		minBounds = renderable->GetBoundingMinValue();

		if (Distance > 0.f)
		{
			LODDistances.push_back(Distance);
			LOD = true;
		} else {
			// LOD
			LOD = false;
			LodInUse = 0;
			LastLodDistance = 0.f;
		}
		
	}

	// Both overloads used to open with `isInstanced = false`, which silently
	// turned an instanced component into a non-instanced one the moment it
	// was given an LOD level: the draw stopped being a
	// DrawElementsInstanced and every chunk collapsed to a single item at
	// the component's own model matrix, with no diagnostic.
	//
	// Refusing outright is not the fix, it is the honest failure. The
	// buffer plumbing is fine - since ownAttributeBuffers moved the
	// per-instance transform onto the component, BindMesh() does append it
	// to every LOD level's mesh, and those meshes do issue
	// DrawElementsInstanced with the full instance count (verified). The
	// result on screen is still wrong - roughly one small clump per
	// component instead of a full field - for a reason not yet found. Until
	// it is, a loud no-op beats either silently dropping instancing or
	// rendering something incorrect.
	void RenderingComponent::AddLOD(const std::shared_ptr<Renderable> &renderable, const f32 Distance, const std::shared_ptr<IMaterial> &Material)
	{
		if (isInstanced)
		{
			echo("ERROR: RenderingComponent::AddLOD - LOD levels are not supported on instanced components; ignoring. Use per-chunk instance counts (SetNumberInstances) for distance detail instead.");
			return;
		}

		uint32 LODLVL = Meshes.size();
		for (uint32 i = 0; i < renderable->Geometries.size(); i++)
		{
			// Rendering Mesh Instance
			RenderingMesh* r_submesh = new RenderingMesh(LODLVL);

			// Save Geometry Pointer
			r_submesh->Geometry = renderable->Geometries[i];
			// Get Geometry Specific Stuff
			if (renderable->Geometries[i]->materialProperties.haveBones)
			{
				r_submesh->MapBoneIDs = renderable->Geometries[i]->MapBoneIDs;
				r_submesh->BoneOffsetMatrix = renderable->Geometries[i]->BoneOffsetMatrix;
			}
			r_submesh->Material = Material;

			// Own this Mothafuckah!
			r_submesh->renderingComponent = this;

			// Push Mesh
			Meshes[LODLVL].push_back(r_submesh);
		}

		LODDistances.push_back(Distance);
		LOD = true;
	}

	void RenderingComponent::AddLOD(const std::shared_ptr<Renderable> &renderable, const f32 Distance, const uint32 MaterialOptions)
	{
		if (isInstanced)
		{
			echo("ERROR: RenderingComponent::AddLOD - LOD levels are not supported on instanced components; ignoring. Use per-chunk instance counts (SetNumberInstances) for distance detail instead.");
			return;
		}

		uint32 LODLVL = Meshes.size();
		for (uint32 i = 0; i < renderable->Geometries.size(); i++)
		{
			// Rendering Mesh Instance
			RenderingMesh* r_submesh = new RenderingMesh(LODLVL);

			// Save Geometry Pointer
			r_submesh->Geometry = renderable->Geometries[i];
			// Get Geometry Specific Stuff
			if (renderable->Geometries[i]->materialProperties.haveBones)
			{
				r_submesh->MapBoneIDs = renderable->Geometries[i]->MapBoneIDs;
				r_submesh->BoneOffsetMatrix = renderable->Geometries[i]->BoneOffsetMatrix;
			}
			r_submesh->BuildMaterials(MaterialOptions);

			// Own this Mothafuckah!
			r_submesh->renderingComponent = this;

			// Push Mesh
			Meshes[LODLVL].push_back(r_submesh);
		}

		LODDistances.push_back(Distance);
		LOD = true;
	}

	const uint32 RenderingComponent::GetLODSize() const
	{
		return Meshes.size();
	}

	uint32 RenderingComponent::GetLODByDistance(const f32 Distance)
	{
		if (Distance != LastLodDistance)
		{
			LastLodDistance = Distance;
			for (size_t i = 0; i < LODDistances.size(); i++)
			{
				if (Distance < LODDistances[i] * LODDistances[i]) return i;
			}
			return LODDistances.size();
		}
		else return LodInUse;
	}

	void RenderingComponent::Register(SceneGraph* Scene)
	{
		if (!Registered)
		{
			// Add Self to Components vector
			Components.push_back(this);

			// Add Meshes to Rendering Meshes
			for (std::vector<RenderingMesh*>::iterator k = Meshes[0].begin(); k != Meshes[0].end(); k++)
				// Add Mesh
				Scene->GetRenderingMeshes().push_back((*k));

			Registered = true;
			this->Scene = Scene;
			Scene->GetRenderingComponents().push_back(this);
		}
	}
	void RenderingComponent::UpdateLOD(const uint32 lod)
	{
		// Check if LOD Level is Different
		if (LodInUse != lod && lod < GetLODSize())
		{
			// Unregister Meshes On Scene
			for (std::map<uint32, std::vector<RenderingMesh*> >::iterator i = Meshes.begin(); i != Meshes.end(); i++)
				for (std::vector<RenderingMesh*>::iterator i1 = (*i).second.begin(); i1 != (*i).second.end(); i1++)
				{
					for (std::vector<RenderingMesh*>::iterator k = Scene->GetRenderingMeshes().begin(); k != Scene->GetRenderingMeshes().end(); k++)
					{
						if ((*k) == (*i1))
						{
							Scene->GetRenderingMeshes().erase(k);
							break;
						}
					}
				}

			LodInUse = lod;
			// Add to Scene
			for (std::vector<RenderingMesh*>::iterator i = GetMeshes(lod).begin(); i != GetMeshes(lod).end(); i++)
			{
				Scene->GetRenderingMeshes().push_back((*i));
			}
		}
	}
	void RenderingComponent::Unregister(SceneGraph* Scene)
	{
		if (Registered)
		{
			// Remove from Components vector
			for (std::vector<IComponent*>::iterator i = Components.begin(); i != Components.end(); i++)
			{
				if ((*i) == this)
				{
					Components.erase(i);
					break;
				}
			}
			// Remove from Meshes vector
			for (std::map<uint32, std::vector<RenderingMesh*> >::iterator i = Meshes.begin(); i != Meshes.end(); i++)
				for (std::vector<RenderingMesh*>::iterator i1 = (*i).second.begin(); i1 != (*i).second.end(); i1++)
				{
					for (std::vector<RenderingMesh*>::iterator k = Scene->GetRenderingMeshes().begin(); k != Scene->GetRenderingMeshes().end(); k++)
					{
						if ((*k) == (*i1))
						{
							Scene->GetRenderingMeshes().erase(k);
							break;
						}
					}
				}

			// Remove Rendering Component From vector
			for (std::vector<RenderingComponent*>::iterator i = Scene->GetRenderingComponents().begin(); i != Scene->GetRenderingComponents().end();)
			{
				if ((*i) == this)
				{
					i = Scene->GetRenderingComponents().erase(i);
				}
				else i++;
			}

			Registered = false;
			this->Scene = NULL;
		}
	}

	std::vector<IComponent*> &RenderingComponent::GetComponents()
	{
		return Components;
	}

	std::vector<RenderingComponent*> &RenderingComponent::GetRenderingComponents(SceneGraph* Scene)
	{
		return Scene->GetRenderingComponents();
	}

	std::vector<RenderingMesh*> &RenderingComponent::GetRenderingMeshes(SceneGraph* scene)
	{
		return scene->GetRenderingMeshes();
	}

	std::vector<RenderingMesh*> &RenderingComponent::GetRenderingMeshesSorted(SceneGraph* scene)
	{
		return scene->GetRenderingMeshesSorted().size()>0 ? scene->GetRenderingMeshesSorted() : scene->GetRenderingMeshes();
	}

	std::vector<RenderingMesh*> &RenderingComponent::GetMeshes(const uint32 LODLevel)
	{
		if (LODLevel < GetLODSize())
			return Meshes[LODLevel];
		else return Meshes[GetLODSize() - 1];
	}

	void RenderingComponent::SetCullingGeometry(const uint32 Geometry)
	{
		// Set Culling Geometry to all  Meshes
		CullingGeometry = Geometry;
		for (std::map<uint32, std::vector<RenderingMesh*> >::iterator i = Meshes.begin(); i != Meshes.end(); i++)
		{
			for (std::vector<RenderingMesh*>::iterator k = (*i).second.begin(); k != (*i).second.end(); k++)
				(*k)->CullingGeometry = Geometry;
		}
	}

	void RenderingComponent::EnableCastShadows()
	{
		isCastingShadows = true;
	}
	void RenderingComponent::DisableCastShadows()
	{
		isCastingShadows = false;
	}
	bool RenderingComponent::IsCastingShadows()
	{
		return isCastingShadows;
	}
	RenderingComponent::~RenderingComponent()
	{
		for (std::map<uint32, std::vector<RenderingMesh*> >::iterator i = Meshes.begin(); i != Meshes.end(); i++)
		{
			for (std::vector<RenderingMesh*>::iterator k = (*i).second.begin(); k != (*i).second.end(); k++)
				// Delete Mesh
				delete (*k);
		}
		// Clear Meshes List
		Meshes.clear();
	}
};
