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
		// Nothing to hand back once the device is gone. Its VAOs and pipelines
		// died with it, and Device() would not reach it anyway:
		// GetActiveRenderDevice() falls back to a lazily constructed *static*
		// GLRenderDevice when none is registered, so asking it to free a
		// Vulkan VAO builds a GL device with no context and dereferences its
		// null function table - a SIGSEGV on every clean exit, inside a
		// destructor, with a stack that blames the mesh rather than the order
		// it was destroyed in.
		//
		// The same rule the editor's shutdown ordering follows from the other
		// side (Editor::Shutdown tears previews down before the device): a
		// GPU-owning object must not outlive its device, and when it does
		// anyway it must not try to talk to one.
		if (!IsActiveRenderDeviceSet()) return;

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
			// Remove from Components vector. This one is a process-wide list,
			// not the scene's, so it happens whether or not there is a scene
			// to unregister from - leaving a destroyed component in it is a
			// dangling pointer every later Register() walks past.
			for (std::vector<IComponent*>::iterator i = Components.begin(); i != Components.end(); i++)
			{
				if ((*i) == this)
				{
					Components.erase(i);
					break;
				}
			}

			// Everything below is the SCENE's bookkeeping, and there may be
			// no scene: GameObject::Remove passes FindScene(), which returns
			// NULL for an object that has already been detached from the
			// graph - and tearing an editor document down does exactly that
			// before its components are removed. Dereferencing it there is a
			// null read that desktop happened to survive and a browser does
			// not: it came back as "Aborted(segmentation fault)" inside
			// GameObject::Remove with no further explanation.
			//
			// Nothing is leaked by skipping it. A scene that does not have
			// this component has nothing of it to erase.
			if (Scene == NULL)
			{
				Registered = false;
				this->Scene = NULL;
				return;
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

	void RenderingComponent::SetSpriteRig2D(const std::vector<SpritePart2D> &parts,
		const std::function<std::string(const std::string&)> &resolve)
	{
		spriteParts2D = parts;

		SpriteRig2DBuild built = BuildSpriteRig2D(parts, resolve);
		if (!built.renderable) return;
		spritePartHalfExtents = built.halfExtents;

		// Off the scene's render list FIRST. The list holds raw RenderingMesh*
		// and nothing else removes them, so deleting the meshes while still
		// registered leaves the renderer walking freed pointers every frame -
		// and the re-Register below would push `this` into the component lists
		// a second time. Re-authoring a character in the editor is exactly the
		// case that does this.
		const bool wasRegistered = Registered;
		SceneGraph* wasIn = Scene;
		if (wasRegistered && wasIn) Unregister(wasIn);

		// Out with the old meshes. Materials are shared_ptr and go with them;
		// the geometries belong to the renderable, which is replaced below.
		for (std::map<uint32, std::vector<RenderingMesh*> >::iterator i = Meshes.begin(); i != Meshes.end(); i++)
			for (std::vector<RenderingMesh*>::iterator k = (*i).second.begin(); k != (*i).second.end(); k++)
				delete (*k);
		Meshes.clear();

		renderable = built.renderable;

		for (uint32 i = 0; i < renderable->Geometries.size(); i++)
		{
			RenderingMesh* m = new RenderingMesh();
			m->Geometry = renderable->Geometries[i];
			m->Material = (i < built.materials.size()) ? built.materials[i] : std::shared_ptr<IMaterial>();
			m->renderingComponent = this;
			Meshes[0].push_back(m);
		}

		BoundingSphereRadius = renderable->GetBoundingSphereRadius();
		BoundingSphereCenter = renderable->GetBoundingSphereCenter();
		maxBounds = renderable->GetBoundingMaxValue();
		minBounds = renderable->GetBoundingMinValue();

		// Back on, with the new meshes, if it was on before. Without this a
		// re-authored character draws nothing until the scene is reloaded.
		if (wasRegistered && wasIn) Register(wasIn);

		RefreshSpriteParts2D();
	}

	// Each part follows its bone by way of its own mesh's Pivot, which the
	// renderer composes with the owner's world matrix
	// (ModelMatrix = ownerWorld * rmesh->Pivot). No child objects and no
	// transform writes: the character is one object whose pieces are drawn in
	// different places.
	void RenderingComponent::RefreshSpriteParts2D()
	{
		if (spriteParts2D.empty()) return;
		SkeletonAnimationInstance* inst =
			static_cast<SkeletonAnimationInstance*>(activeSkeletonAnimation);
		if (!inst) return;

		const std::vector<Bone> &bones = inst->GetSkeletonBones();
		std::vector<RenderingMesh*> &ms = GetMeshes(0);

		for (size_t i = 0; i < spriteParts2D.size() && i < ms.size(); i++)
		{
			const SpritePart2D &part = spriteParts2D[i];

			Matrix m;
			if (!part.bone.empty())
			{
				int32 id = -1;
				for (size_t b = 0; b < bones.size(); b++)
					if (bones[b].name == part.bone) { id = bones[b].self; break; }
				if (id >= 0) m = inst->GetBoneGlobalTransform(id);
			}

			// Offset then scale, in the bone's frame: the offset places the
			// artwork relative to the joint it turns about, and the scale must
			// not move it.
			Matrix off;
			off.Translate(Vec3(part.offset.x, part.offset.y, part.z));
			Matrix sc;
			sc.Scale(Vec3(part.scale.x, part.scale.y, 1.f));

			// The artwork's own pivot goes innermost, so it moves the quad
			// under everything else - that is what makes a limb turn about its
			// joint instead of about the middle of its texture.
			Matrix pv;
			if (i < spritePartHalfExtents.size())
			{
				const Vec2 &he = spritePartHalfExtents[i];
				const f32 lx = -he.x + part.pivot.x * (he.x * 2.f);
				const f32 ly = -he.y + part.pivot.y * (he.y * 2.f);
				pv.Translate(Vec3(-lx, -ly, 0.f));
			}

			ms[i]->Pivot = m * off * sc * pv;
		}
	}

	bool RenderingComponent::GetSpriteParts2DBounds(Vec2 &outMin, Vec2 &outMax) const
	{
		if (spriteParts2D.empty()) return false;

		// Read straight off the meshes: their Pivot is where each quad ends up
		// (RefreshSpriteParts2D wrote it), so this measures what is on screen
		// rather than re-deriving the placement and risking a second opinion.
		const std::vector<RenderingMesh*> &ms =
			const_cast<RenderingComponent*>(this)->GetMeshes(0);

		bool any = false;
		for (size_t i = 0; i < ms.size() && i < spritePartHalfExtents.size(); i++)
		{
			if (!ms[i]) continue;
			const Vec2 &he = spritePartHalfExtents[i];
			const Vec2 &sc = spriteParts2D[i].scale;
			// The quad's four corners, each through the part's placement. All
			// four, not just two: a bone's rotation turns the quad, so the
			// axis-aligned box of the corners is not the box of two of them.
			const f32 hx = he.x * sc.x, hy = he.y * sc.y;
			const Vec3 corners[4] = {
				Vec3(-hx, -hy, 0.f), Vec3(hx, -hy, 0.f),
				Vec3(hx,  hy, 0.f),  Vec3(-hx, hy, 0.f)
			};
			for (int c = 0; c < 4; c++)
			{
				const Vec3 p = ms[i]->Pivot * corners[c];
				if (!any) { outMin = outMax = Vec2(p.x, p.y); any = true; continue; }
				if (p.x < outMin.x) outMin.x = p.x;
				if (p.y < outMin.y) outMin.y = p.y;
				if (p.x > outMax.x) outMax.x = p.x;
				if (p.y > outMax.y) outMax.y = p.y;
			}
		}
		return any;
	}

	void RenderingComponent::Update(const f64 time)
	{
		RefreshSpriteParts2D();

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
