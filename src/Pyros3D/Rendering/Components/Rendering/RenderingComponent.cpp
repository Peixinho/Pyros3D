//============================================================================
// Name        : RenderingComponent
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Component For Rendering
//============================================================================

#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Other/PyrosGL.h>

namespace p3d {

	// Initialize Rendering Components and Meshes vector
	std::vector<IComponent*> RenderingComponent::Components;
	std::map<SceneGraph*, std::vector<RenderingMesh*> > RenderingComponent::MeshesOnScene;
	std::map<SceneGraph*, std::vector<RenderingComponent*> > RenderingComponent::RenderingComponentsOnScene;

	RenderingComponent::RenderingComponent(Renderable* renderable, IMaterial* Material) : IComponent()
	{
		// Keep renderable pointer
		this->renderable = renderable;

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
			r_submesh->Material = renderable->Geometries[i]->Material;
			if (renderable->Geometries[i]->materialProperties.haveBones)
			{
				r_submesh->MapBoneIDs = renderable->Geometries[i]->MapBoneIDs;
				r_submesh->BoneOffsetMatrix = renderable->Geometries[i]->BoneOffsetMatrix;
			}
			if (Material != NULL)
			{
				r_submesh->Material = Material;
			}

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

		// LOD
		LOD = false;
		LodInUse = 0;
		LastLodDistance = 0.f;
	}

	void RenderingComponent::AddLOD(Renderable* renderable, const f32 Distance, IMaterial* Material)
	{

		uint32 LODLVL = Meshes.size();
		for (uint32 i = 0; i < renderable->Geometries.size(); i++)
		{
			// Rendering Mesh Instance
			RenderingMesh* r_submesh = new RenderingMesh(LODLVL);

			// Save Geometry Pointer
			r_submesh->Geometry = renderable->Geometries[i];
			// Get Geometry Specific Stuff
			r_submesh->Material = renderable->Geometries[i]->Material;
			if (renderable->Geometries[i]->materialProperties.haveBones)
			{
				r_submesh->MapBoneIDs = renderable->Geometries[i]->MapBoneIDs;
				r_submesh->BoneOffsetMatrix = renderable->Geometries[i]->BoneOffsetMatrix;
			}
			if (Material != NULL)
			{
				r_submesh->Material = Material;
			}

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
				MeshesOnScene[Scene].push_back((*k));

			Registered = true;
			this->Scene = Scene;
			RenderingComponentsOnScene[Scene].push_back(this);
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
					for (std::vector<RenderingMesh*>::iterator k = MeshesOnScene[Scene].begin(); k != MeshesOnScene[Scene].end(); k++)
					{
						if ((*k) == (*i1))
						{
							MeshesOnScene[Scene].erase(k);
							break;
						}
					}
				}

			LodInUse = lod;
			// Add to Scene
			for (std::vector<RenderingMesh*>::iterator i = GetMeshes(lod).begin(); i != GetMeshes(lod).end(); i++)
			{
				MeshesOnScene[Scene].push_back((*i));
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
					for (std::vector<RenderingMesh*>::iterator k = MeshesOnScene[Scene].begin(); k != MeshesOnScene[Scene].end(); k++)
					{
						if ((*k) == (*i1))
						{
							MeshesOnScene[Scene].erase(k);
							break;
						}
					}
				}

			// Remove Rendering Component From vector
			for (std::vector<RenderingComponent*>::iterator i = RenderingComponentsOnScene[Scene].begin(); i != RenderingComponentsOnScene[Scene].end();)
			{
				if ((*i) == this)
				{
					i = RenderingComponentsOnScene[Scene].erase(i);
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
		return RenderingComponentsOnScene[Scene];
	}

	std::vector<RenderingMesh*> &RenderingComponent::GetRenderingMeshes(SceneGraph* scene)
	{
		return MeshesOnScene[scene];
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
