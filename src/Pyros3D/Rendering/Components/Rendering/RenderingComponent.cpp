#include <cstdlib>
#include <cstdio>
//============================================================================
// Name        : RenderingComponent
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Component For Rendering
//============================================================================

#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <execinfo.h>
#include <dlfcn.h>
#endif
#include <map>
#include <string>
#include <algorithm>
#include <mutex>
#include <Pyros3D/Rendering/RenderState.h>
#include <Pyros3D/Assets/Renderable/Models/Model.h>
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
		SayLevelsChanged();
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
		lodRenderables.push_back(renderable);
	}

	void RenderingComponent::AddLOD(const std::shared_ptr<Renderable> &renderable, const f32 Distance, const uint32 MaterialOptions)
	{
		SayLevelsChanged();
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
		lodRenderables.push_back(renderable);
	}

	void RenderingComponent::AddLODOwnMaterials(const std::shared_ptr<Renderable> &renderable, const f32 Distance)
	{
		SayLevelsChanged();
		const std::vector<RenderingMesh*> &own = Meshes[0];
		if (!renderable || own.empty()) return;
		const uint32 LODLVL = (uint32)Meshes.size();
		for (uint32 i = 0; i < renderable->Geometries.size(); i++)
		{
			RenderingMesh* m = new RenderingMesh(LODLVL);
			m->Geometry = renderable->Geometries[i];
			if (renderable->Geometries[i]->materialProperties.haveBones)
			{
				m->MapBoneIDs = renderable->Geometries[i]->MapBoneIDs;
				m->BoneOffsetMatrix = renderable->Geometries[i]->BoneOffsetMatrix;
			}
			RenderingMesh* like = own[std::min((size_t)i, own.size() - 1)];
			m->standsFor = like;
			m->Material = like->Material;
			m->CullingGeometry = like->CullingGeometry;
			m->renderingComponent = this;
			Meshes[LODLVL].push_back(m);
		}
		LODDistances.push_back(Distance);
		LOD = true;
		lodRenderables.push_back(renderable);
		lodOwnMaterials.resize(LODLVL + 1, false);
		lodOwnMaterials[LODLVL] = true;
	}

	void RenderingComponent::AddHiddenLOD()
	{
		SayLevelsChanged();
		const uint32 LODLVL = (uint32)Meshes.size();
		Meshes[LODLVL];         // a level, and no meshes in it
		LODDistances.push_back(1e9f);
		LOD = true;
		lodRenderables.push_back(std::make_shared<Renderable>());
		lodOwnMaterials.resize(LODLVL + 1, false);
	}

	bool RenderingComponent::AddSimplifiedLOD(const f32 ratio, const f32 reach)
	{
		Model* model = dynamic_cast<Model*>(renderable.get());
		if (!model || model->GetPath().empty()) return false;
		std::shared_ptr<SimplifiedModel> simple = SimplifiedModel::LoadShared(model->GetPath(), ratio);
		if (!simple || simple->Geometries.empty()) return false;
		AddLODOwnMaterials(simple, reach);
		return true;
	}

	namespace { bool g_autoLod = true, g_autoLodInUse = true; uint32 g_autoLodChanges = 0; }
	void RenderingComponent::SetAutoLOD(const bool on) { g_autoLod = on; }
	bool RenderingComponent::GetAutoLOD() { return g_autoLod; }
	void RenderingComponent::SetAutoLODInUse(const bool on) { if (on != g_autoLodInUse) { g_autoLodInUse = on; g_autoLodChanges++; RenderState::Touch(); } }
	bool RenderingComponent::GetAutoLODInUse() { return g_autoLodInUse; }

	void RenderingComponent::TryAutomaticLODs()
	{
		if (autoLodTried) return;
		autoLodTried = true;
		if (!g_autoLod || autoLevels != 0 || Meshes.size() != 1 || IsInstanced() || !Owner || !renderable) return;
		const Vec3 &scale = Owner->GetScale();
		AddAutomaticLODs(renderable->GetBoundingSphereRadius() * std::max(fabs(scale.x), std::max(fabs(scale.y), fabs(scale.z))));
	}

	bool RenderingComponent::AddAutomaticLODs(const f32 worldRadius, const f32 given)
	{
		static const uint32 kLightest = 1500;         // triangles under which a model is left alone
		static const f32 kFrom[3] = { 12.f, 28.f, 60.f };
		static const f32 kRatio[3] = { 0.5f, 0.25f, 0.1f };
		if (!g_autoLod || autoLevels != 0 || worldRadius <= 0.f) return false;
		Model* model = dynamic_cast<Model*>(renderable.get());
		if (!model || model->GetPath().empty()) return false;
		uint32 triangles = 0;
		for (size_t i = 0; i < model->Geometries.size(); i++)
			triangles += (uint32)(model->Geometries[i]->GetIndexData().size() / 3);
		if (triangles < kLightest) return false;
		const f32 first = worldRadius * kFrom[0];
		if (given > 0.f)
		{
			// one level, between the model itself and the levels it was given -
			// where they leave room for one
			if (given < first * 1.4f || Meshes.size() != 1) return false;
			SetFirstLODDistance(first);
			if (!AddSimplifiedLOD(0.4f, given)) { SetFirstLODDistance(given); return false; }
			autoLevels = 1;
			return true;
		}
		if (Meshes.size() != 1) return false;
		SetFirstLODDistance(first);
		for (int l = 0; l < 3; l++)
		{
			const f32 reach = (l < 2) ? worldRadius * kFrom[l + 1] : 1e9f;
			if (AddSimplifiedLOD(kRatio[l], reach)) autoLevels++;
			else break;
		}
		if (autoLevels == 0) { SetFirstLODDistance(1e9f); return false; }
		// (the last level made reaches all the way out)
		LODDistances.back() = 1e9f;
		if (shadowDetail <= 0.f && triangles >= 3000) SetShadowDetail(0.25f);
		return true;
	}

	void RenderingComponent::ClearLODs()
	{
		RenderState::Touch();
		if (Meshes.size() <= 1) { LOD = false; if (LODDistances.size() > 1) LODDistances.resize(1); return; }
		// the scene is drawing one level's meshes: make that the nearest
		// before the others go
		if (LodInUse != 0)
		{
			if (Registered && Scene) UpdateLOD(0);
			else LodInUse = 0;
		}
		for (std::map<uint32, std::vector<RenderingMesh*> >::iterator i = Meshes.begin(); i != Meshes.end();)
		{
			if (i->first == 0) { ++i; continue; }
			for (size_t k = 0; k < i->second.size(); k++) delete i->second[k];
			i = Meshes.erase(i);
		}
		if (LODDistances.size() > 1) LODDistances.resize(1);
		// (GetLODRenderables() puts the nearest level's first by itself)
		lodRenderables.clear();
		lodOwnMaterials.clear();
		LOD = false;
	}

	void RenderingComponent::SetShadowRenderable(const std::shared_ptr<Renderable> &renderable)
	{
		SayDrawnChanged();        // (a kept list holds nothing of the shadow's mesh: only this component's entries are read again)
		for (size_t i = 0; i < shadowMeshes.size(); i++) delete shadowMeshes[i];
		shadowMeshes.clear();
		shadowRenderable.reset();
		if (!renderable || renderable->Geometries.empty()) return;
		const std::vector<RenderingMesh*> &own = Meshes[0];
		if (own.empty()) return;
		shadowRenderable = renderable;
		for (size_t i = 0; i < renderable->Geometries.size(); i++)
		{
			RenderingMesh* m = new RenderingMesh(0);
			m->Geometry = renderable->Geometries[i];
			// (a skinned one is posed with the model: SkeletonAnimationInstance
			// fills the matrices of every mesh the component has)
			if (renderable->Geometries[i]->materialProperties.haveBones)
			{
				m->MapBoneIDs = renderable->Geometries[i]->MapBoneIDs;
				m->BoneOffsetMatrix = renderable->Geometries[i]->BoneOffsetMatrix;
			}
			m->standsFor = own[std::min(i, own.size() - 1)];
			m->Material = own[std::min(i, own.size() - 1)]->Material;
			m->CullingGeometry = own[std::min(i, own.size() - 1)]->CullingGeometry;
			m->renderingComponent = this;
			shadowMeshes.push_back(m);
		}
	}

	bool RenderingComponent::SetShadowDetail(const f32 ratio)
	{
		shadowDetail = 0.f;
		if (ratio <= 0.f || ratio >= 0.999f) { SetShadowRenderable(std::shared_ptr<Renderable>()); return true; }
		Model* model = dynamic_cast<Model*>(renderable.get());
		if (!model || model->GetPath().empty()) { SetShadowRenderable(std::shared_ptr<Renderable>()); return false; }
		SetShadowRenderable(SimplifiedModel::LoadShared(model->GetPath(), ratio));
		if (shadowMeshes.empty()) return false;
		shadowDetail = ratio;
		return true;
	}

	const uint32 RenderingComponent::GetLODSize() const
	{
		return Meshes.size();
	}

	uint32 RenderingComponent::GetLODByDistance(const f32 Distance)
	{
		if (Distance != LastLodDistance || autoLodSeen != g_autoLodChanges)
		{
			LastLodDistance = Distance;
			autoLodSeen = g_autoLodChanges;
			for (size_t i = 0; i < LODDistances.size(); i++)
			{
				if (Distance < LODDistances[i] * LODDistances[i])
				{
					// (automatic levels not in use: the model itself in their place)
					if (autoLevels != 0 && !g_autoLodInUse && i >= 1 && i <= autoLevels) return 0;
					return i;
				}
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

			// Add Meshes to Rendering Meshes - and say so, mesh by mesh: whoever
			// keeps a list of what the scene draws puts them into it (it was
			// told only that something had changed, and made its list again)
			RenderState::NoteListed(RenderState::Listed::ComponentOn, Scene, this, Owner);
			for (std::vector<RenderingMesh*>::iterator k = Meshes[LodInUse].begin(); k != Meshes[LodInUse].end(); k++)
			{
				Scene->GetRenderingMeshes().push_back((*k));
				RenderState::NoteListed(RenderState::Listed::MeshOn, Scene, this, Owner, *k);
			}

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
			// The scene lists the meshes of the level in use, and only those:
			// they are taken out and the new level's put in - in the places the
			// old ones had, as far as there are as many. (Every mesh of EVERY
			// level was looked for through the scene's whole list, one at a
			// time, and each one found was closed up behind: with a few hundred
			// things changing level as the camera ran, that was most of a frame.)
			std::vector<RenderingMesh*> &listed = Scene->GetRenderingMeshes();
			const std::vector<RenderingMesh*> &was = Meshes[LodInUse];
			const std::vector<RenderingMesh*> &now = Meshes[lod];
			size_t put = 0;
			for (size_t m = 0; m < was.size(); m++)
			{
				// (looked for from the end: what was listed last is found first,
				// and a thing that has just come into the scene is at the end)
				for (size_t k = listed.size(); k-- > 0;)
				{
					if (listed[k] != was[m]) continue;
					if (put < now.size()) { listed[k] = now[put]; RenderState::NoteListed(RenderState::Listed::MeshSwap, Scene, this, Owner, was[m], now[put]); put++; }
					else { listed.erase(listed.begin() + k); RenderState::NoteListed(RenderState::Listed::MeshOff, Scene, this, Owner, was[m]); }
					break;
				}
			}
			for (; put < now.size(); put++) { listed.push_back(now[put]); RenderState::NoteListed(RenderState::Listed::MeshOn, Scene, this, Owner, now[put]); }
			LodInUse = lod;
		}
	}
	void RenderingComponent::Unregister(SceneGraph* Scene)
	{
		if (Registered)
		{
			// (always: a component with no mesh in the scene at the moment - one
			// past its last level of detail - is still in the renderers' kept
			// lists of components to watch. Said first: whoever reads the log
			// forgets the component before anything of it.)
			if (Scene != NULL) RenderState::NoteListed(RenderState::Listed::ComponentOff, Scene, this, Owner);
			else RenderState::Touch();
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
							RenderState::NoteListed(RenderState::Listed::MeshOff, Scene, this, Owner, *i1);
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
		const bool was = isCastingShadows;
		isCastingShadows = true;
		if (!was) SayDrawnChanged();
	}
	void RenderingComponent::DisableCastShadows()
	{
		const bool was = isCastingShadows;
		isCastingShadows = false;
		if (was) SayDrawnChanged();
	}
	// Switched on or off, casting or not, tested for culling or not: a few
	// bits of each of its meshes in a renderer's kept list, which are read
	// again where an object that has moved is (the moved log). One that is in
	// no scene yet is in no list - a game sets these on everything it makes,
	// before it is there to be drawn, and each one had every list made again.
	void RenderingComponent::SayDrawnChanged()
	{
		if (!Registered) return;
		if (Owner != NULL) RenderState::NoteMoved(Owner); else RenderState::Touch();
	}
	// Levels given to one already in a scene: said as the component coming
	// (whoever keeps the components that have levels takes it up).
	void RenderingComponent::SayLevelsChanged()
	{
		if (!Registered) return;
		if (Scene != NULL) RenderState::NoteListed(RenderState::Listed::ComponentOn, Scene, this, Owner); else RenderState::Touch();
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
		for (size_t i = 0; i < shadowMeshes.size(); i++) delete shadowMeshes[i];
		shadowMeshes.clear();
	}
};

namespace p3d {

	void RenderingComponent::StartAutoPlayOn(const std::vector<GameObject*> &all)
	{
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

	void RenderingComponent::StartAutoPlayInScene(SceneGraph* scene)
	{
		if (scene == NULL) return;
		std::vector<GameObject*> all;
		scene->CollectGameObjectsRecursive(all);
		StartAutoPlayOn(all);
	}

	void RenderingComponent::StartAutoPlayIn(GameObject* root)
	{
		if (root == NULL) return;
		std::vector<GameObject*> all(1, root);
		for (size_t i = 0; i < all.size(); i++)
		{
			const std::vector<std::shared_ptr<GameObject> > &kids = all[i]->GetChildren();
			for (size_t k = 0; k < kids.size(); k++) all.push_back(kids[k].get());
		}
		StartAutoPlayOn(all);
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

		AdoptGeneratedRenderable(built.renderable, built.materials);

		RefreshSpriteParts2D();
	}

	void RenderingComponent::AdoptGeneratedRenderable(
		const std::shared_ptr<Renderable> &built,
		const std::vector<std::shared_ptr<IMaterial> > &materials)
	{
		if (!built) return;

		// Off the scene's render list FIRST. The list holds raw RenderingMesh*
		// and nothing else removes them, so deleting the meshes while still
		// registered leaves the renderer walking freed pointers every frame -
		// and the re-Register below would push `this` into the component lists
		// a second time. Re-authoring a character in the editor, and repainting
		// a tilemap chunk into or out of existence, are the two cases that do
		// this.
		const bool wasRegistered = Registered;
		SceneGraph* wasIn = Scene;
		if (wasRegistered && wasIn) Unregister(wasIn);

		// Out with the old meshes. Materials are shared_ptr and go with them;
		// the geometries belong to the renderable, which is replaced below.
		for (std::map<uint32, std::vector<RenderingMesh*> >::iterator i = Meshes.begin(); i != Meshes.end(); i++)
			for (std::vector<RenderingMesh*>::iterator k = (*i).second.begin(); k != (*i).second.end(); k++)
				delete (*k);
		Meshes.clear();

		renderable = built;

		for (uint32 i = 0; i < renderable->Geometries.size(); i++)
		{
			RenderingMesh* m = new RenderingMesh();
			m->Geometry = renderable->Geometries[i];
			m->Material = (i < materials.size()) ? materials[i] : std::shared_ptr<IMaterial>();
			m->renderingComponent = this;
			// Carry the component's choice onto the new meshes. RenderingMesh
			// constructs with Sphere, so without this a geometry swap silently
			// reverts a component that had been set to Box - and the caller
			// cannot fix it by setting it first, because these meshes do not
			// exist yet at that point.
			m->CullingGeometry = CullingGeometry;
			Meshes[0].push_back(m);
		}

		BoundingSphereRadius = renderable->GetBoundingSphereRadius();
		BoundingSphereCenter = renderable->GetBoundingSphereCenter();
		maxBounds = renderable->GetBoundingMaxValue();
		minBounds = renderable->GetBoundingMinValue();

		// The owner aggregated our bounds when we were ADDED, from whatever
		// geometry we were constructed with - a placeholder, for anything
		// generated. CullingBoxTest tests the owner's box, so without this a
		// tilemap keeps the 2x2 box of the Plane it was built over and is
		// culled the moment the origin leaves the view.
		if (Owner) Owner->RefreshComponentBounds();

		// Back on, with the new meshes, if it was on before. Without this a
		// re-authored character draws nothing until the scene is reloaded.
		if (wasRegistered && wasIn) Register(wasIn);
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

	uint32 RenderingComponent::SeenEpoch = 0;

	std::atomic<uint32_t> RenderState::Version(1);
	std::atomic<uint32_t> RenderState::ReadEpoch(1);
	namespace {
		// The last so many objects moved, and how many there have ever been.
		const size_t kMovedKept = 1 << 16;
		std::vector<GameObject*> &MovedRing() { static std::vector<GameObject*> ring(kMovedKept, (GameObject*)NULL); return ring; }
		uint64_t g_movedCount = 0;
		std::mutex &MovedMutex() { static std::mutex m; return m; }
	}
	void RenderState::Touch()
	{
		Version.fetch_add(1, std::memory_order_relaxed);
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
		static const bool trace = std::getenv("PYROS_TRACE_TOUCH") != NULL;
		if (trace)
		{
			static std::mutex m;
			static std::map<std::string, uint64_t> by;
			static uint64_t calls = 0;
			std::lock_guard<std::mutex> lock(m);
			void* frames[6];
			const int n = backtrace(frames, 6);
			std::string who;
			for (int i = 1; i < n && i < 5; i++)
			{
				Dl_info info;
				if (dladdr(frames[i], &info) && info.dli_sname) { who += info.dli_sname; who += " < "; }
			}
			by[who]++;
			if (++calls % 3000 == 0)
			{
				fprintf(stderr, "[touch] %llu calls so far:\n", (unsigned long long)calls);
				std::vector<std::pair<uint64_t, std::string> > top;
				for (std::map<std::string, uint64_t>::iterator i = by.begin(); i != by.end(); ++i) top.push_back(std::make_pair(i->second, i->first));
				std::sort(top.rbegin(), top.rend());
				for (size_t i = 0; i < top.size() && i < 6; i++) fprintf(stderr, "[touch]   %8llu  %s\n", (unsigned long long)top[i].first, top[i].second.substr(0, 230).c_str());
				by.clear();
			}
		}
#endif
	}
	void RenderState::NoteMoved(GameObject* object)
	{
		std::lock_guard<std::mutex> lock(MovedMutex());
		MovedRing()[g_movedCount % kMovedKept] = object;
		g_movedCount++;
	}
	uint64_t RenderState::MovedCount()
	{
		std::lock_guard<std::mutex> lock(MovedMutex());
		return g_movedCount;
	}
	bool RenderState::MovedSince(const uint64_t seq, std::vector<GameObject*> &out)
	{
		std::lock_guard<std::mutex> lock(MovedMutex());
		out.clear();
		ReadEpoch.fetch_add(1, std::memory_order_relaxed);
		if (seq > g_movedCount || g_movedCount - seq > kMovedKept) return false;
		out.reserve((size_t)(g_movedCount - seq));
		for (uint64_t k = seq; k < g_movedCount; k++) out.push_back(MovedRing()[k % kMovedKept]);
		return true;
	}

	namespace {
		const uint64_t kListedKept = 16384;
		std::mutex &ListedMutex() { static std::mutex m; return m; }
		std::vector<RenderState::Listed> &ListedRing() { static std::vector<RenderState::Listed> r(kListedKept); return r; }
		uint64_t g_listedCount = 0;
	}
	void RenderState::NoteListed(const uint32_t what, SceneGraph* scene, RenderingComponent* component, GameObject* owner, RenderingMesh* mesh, RenderingMesh* other)
	{
		std::lock_guard<std::mutex> lock(ListedMutex());
		Listed &e = ListedRing()[g_listedCount % kListedKept];
		e.mesh = mesh; e.other = other; e.component = component; e.owner = owner; e.scene = scene; e.what = what;
		g_listedCount++;
	}
	uint64_t RenderState::ListedCount()
	{
		std::lock_guard<std::mutex> lock(ListedMutex());
		return g_listedCount;
	}
	bool RenderState::ListedSince(const uint64_t seq, std::vector<Listed> &out)
	{
		std::lock_guard<std::mutex> lock(ListedMutex());
		out.clear();
		if (seq > g_listedCount || g_listedCount - seq > kListedKept) return false;
		out.reserve((size_t)(g_listedCount - seq));
		for (uint64_t k = seq; k < g_listedCount; k++) out.push_back(ListedRing()[k % kListedKept]);
		return true;
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
			if (si->GetOwner() && (animateWhenUnseen || WasSeenRecently())) si->GetOwner()->UpdateInstance(si, (f32)time);
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
