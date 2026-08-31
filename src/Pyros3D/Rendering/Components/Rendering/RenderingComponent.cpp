//============================================================================
// Name        : RenderingComponent
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Component For Rendering
//============================================================================

#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Device/GLRenderDevice.h>
// In the .cpp only: the AnimationManager headers include this one, so pulling
// them into the header would be circular - which is why activeTextureAnimation
// is a void* in the first place.
#include <Pyros3D/AnimationManager/TextureAnimation.h>
#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>

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

		renderLayer = RenderLayer::World;

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

		renderLayer = RenderLayer::World;

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

	// Neither overload sets `isInstanced = false` any more. It used to, which
	// silently turned an instanced component into a non-instanced one the
	// moment it was given an LOD level - the draw stopped being a
	// DrawElementsInstanced and every chunk collapsed to a single item at the
	// component's own model matrix, with no diagnostic.
	//
	// It was defensible once: the per-instance transform buffer used to live
	// on the base Renderable's geometries, so an LOD level built from a
	// *different* renderable would never have received it. That is no longer
	// where it lives - see RenderingComponent::ownAttributeBuffers - and
	// BindMesh() appends the component's own buffers to every mesh it owns,
	// LOD levels included. Verified by logging the VAO build: each LOD mesh
	// binds its own component's transform buffer and draws the full instance
	// count, and a field of instanced chunks switches to its LOD mesh with
	// every instance still in place.
	void RenderingComponent::AddLOD(const std::shared_ptr<Renderable> &renderable, const f32 Distance, const std::shared_ptr<IMaterial> &Material)
	{
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

namespace p3d {

	void RenderingComponent::StartAutoPlayInScene(SceneGraph* scene)
	{
		if (scene == NULL) return;
		std::vector<GameObject*> all;
		scene->CollectGameObjectsRecursive(all);
		for (size_t i = 0; i < all.size(); i++)
		{
			if (!all[i]) continue;
			const std::vector<std::shared_ptr<IComponent> > &comps = all[i]->GetComponents();
			for (size_t c = 0; c < comps.size(); c++)
			{
				if (!comps[c] || comps[c]->GetComponentType() != ComponentType::RenderingComponent) continue;
				RenderingComponent* rc = static_cast<RenderingComponent*>(comps[c].get());
				if (rc->autoPlayClip.empty()) continue;
				SkeletonAnimationInstance* inst =
					static_cast<SkeletonAnimationInstance*>(rc->GetActiveSkeletonAnimation());
				if (!inst || !inst->GetOwner()) continue;
				const std::vector<Animation> clips = inst->GetOwner()->GetAnimations();
				for (size_t k = 0; k < clips.size(); k++)
					if (clips[k].AnimationName == rc->autoPlayClip)
					{
						// -1 is the loop-forever sentinel; 0 would read as
						// "no repetitions left" and stop on the final pose.
						inst->Play((uint32)k, 0.f, rc->autoPlayLoop ? -1.f : 1.f);
						break;
					}
			}
		}
	}

	void RenderingComponent::SetSkeleton(const std::vector<Bone> &bones)
	{
		skeleton.clear();
		for (size_t i = 0; i < bones.size(); i++)
			skeleton[MakeStringID(bones[i].name)] = bones[i];
		hasBones = !skeleton.empty();
	}

	void RenderingComponent::Update(const f64 time)
	{
		// Skeleton animation. Nothing outside the editor's own animation
		// preview ever called SkeletonAnimation::Update(), so a clip playing
		// in a running game never advanced a frame - the same gap texture
		// animation had. Safe to run with nothing playing: Update() leaves
		// the pose alone when the playing list is empty, which is what lets
		// the editor's posing and the IK solver hold.
		if (activeSkeletonAnimation != NULL)
		{
			SkeletonAnimationInstance* si =
				static_cast<SkeletonAnimationInstance*>(activeSkeletonAnimation);
			if (si->GetOwner()) si->GetOwner()->Update((f32)time);
		}

		if (activeTextureAnimation == NULL) return;

		TextureAnimationInstance* inst = static_cast<TextureAnimationInstance*>(activeTextureAnimation);
		TextureAnimation* owner = inst->GetOwner();
		if (owner == NULL || owner->GetNumberFrames() == 0) return;

		// Absolute time, not a delta: Update() derives the frame from
		// (timer - timeStart), so feeding it the same value twice in one
		// frame lands on the same frame rather than double-advancing.
		owner->Update((f32)time);

		const int32 frame = (int32)inst->GetFrame();
		if (frame == lastAppliedTextureFrame) return;
		lastAppliedTextureFrame = frame;

		const std::shared_ptr<Texture> tex = inst->GetTextureShared();
		if (!tex) return;
		// Every LOD, not just LOD 0: a sprite has one, but a model that
		// carries an animated texture would otherwise stop animating the
		// moment it switched LOD.
		for (std::map<uint32, std::vector<RenderingMesh*> >::iterator lod = Meshes.begin(); lod != Meshes.end(); ++lod)
		{
			std::vector<RenderingMesh*> &list = lod->second;
			for (size_t i = 0; i < list.size(); i++)
			{
				if (!list[i] || !list[i]->Material) continue;
				GenericShaderMaterial* gm = dynamic_cast<GenericShaderMaterial*>(list[i]->Material.get());
				if (gm) gm->SetColorMap(tex);
			}
		}
	}

};
