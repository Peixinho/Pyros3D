#include <chrono>
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
#include <Pyros3D/Assets/Renderable/Terrains/Heightfield.h>
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <execinfo.h>
#include <dlfcn.h>
#endif
#include <map>
#include <string>
#include <algorithm>
#include <Pyros3D/Physics/Cloth/ClothComponent.h>
#include <Pyros3D/Utils/Navigation/NavAgentComponent.h>
#include <Pyros3D/Physics/CharacterController.h>
#include <mutex>
#include <Pyros3D/Rendering/RenderState.h>
#include <Pyros3D/Assets/Renderable/Models/Model.h>
// In the .cpp only: the AnimationManager headers include this one, so pulling
// them into the header would be circular - which is why activeTextureAnimation
// is a void* in the first place.
#include <Pyros3D/AnimationManager/TextureAnimation.h>
#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <Pyros3D/AnimationManager/Components/AnimatorComponent.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>
#include <Pyros3D/Utils/Jobs/JobSystem.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Rendering/Components/UI/UIImage.h>

namespace p3d {

	// Initialize Rendering Components vector
	std::vector<IComponent*> RenderingComponent::Components;

	namespace {
		std::mutex g_bonesForDrawMutex;
		std::vector<RenderingMesh*> g_bonesForDraw;
	}
	void RenderingMesh::NoteBonesForDraw(RenderingMesh* mesh)
	{
		if (!GameObject::DrawCopies() || mesh == NULL) return;
		std::lock_guard<std::mutex> lock(g_bonesForDrawMutex);
		if (mesh->DrawBoneSlot >= 0) return;
		mesh->DrawBoneSlot = (int32)g_bonesForDraw.size();
		g_bonesForDraw.push_back(mesh);
	}
	void RenderingMesh::TakeDrawBones()
	{
		if (!GameObject::DrawCopies()) return;
		std::lock_guard<std::mutex> lock(g_bonesForDrawMutex);
		for (size_t i = 0; i < g_bonesForDraw.size(); i++)
		{
			RenderingMesh* m = g_bonesForDraw[i];
			if (m == NULL) continue;
			m->DrawSkinningBones = m->SkinningBones;
			m->DrawShadowSkinningBones = m->ShadowSkinningBones;
			m->DrawBonesTakenAt = GameObject::s_drawEpoch;
			m->DrawBoneSlot = -1;
		}
		g_bonesForDraw.clear();
	}

	RenderingMesh::~RenderingMesh()
	{
		if (DrawBoneSlot >= 0)
		{
			std::lock_guard<std::mutex> lock(g_bonesForDrawMutex);
			if ((size_t)DrawBoneSlot < g_bonesForDraw.size() && g_bonesForDraw[DrawBoneSlot] == this) g_bonesForDraw[DrawBoneSlot] = NULL;
		}
	}

	// (automatic levels are made on a thread of their own - SimplifiedModel::PrepareAutomatic: a build without threads, a
	// browser's, makes none - hundreds of models simplified on the page's one thread would be minutes of hitches)
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
	namespace { bool g_autoLod = false, g_autoLodInUse = true; uint32 g_autoLodChanges = 0; }
#else
	namespace { bool g_autoLod = !(std::getenv("PYROS_AUTO_LOD") && std::getenv("PYROS_AUTO_LOD")[0] == '0'), g_autoLodInUse = true; uint32 g_autoLodChanges = 0; }
#endif
	namespace {
		// PYROS_AUTO_LOD_AHEAD=0: levels made on the spot, the first time a model is listed, as they were (to compare)
		const bool g_autoLodAhead = !(std::getenv("PYROS_AUTO_LOD_AHEAD") && std::getenv("PYROS_AUTO_LOD_AHEAD")[0] == '0');
		std::vector<RenderingComponent*> g_autoLodWaiting;     // (drawing thread only)
	}

	RenderingComponent::RenderingComponent(const std::shared_ptr<Renderable> &renderable, const std::shared_ptr<IMaterial> &Material, const f32 Distance) : IComponent()
	{
		// Keep renderable pointer
		this->renderable = renderable;
		// (its levels of detail begun now, on their own thread: ready by the time it is drawn)
		if (g_autoLod && g_autoLodAhead) SimplifiedModel::PrepareAutomatic(std::dynamic_pointer_cast<Model>(renderable));
		isHeightfield = dynamic_cast<Heightfield*>(this->renderable.get()) != NULL;

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

	RenderingComponent::RenderingComponent(const std::shared_ptr<Renderable> &renderable, const ShaderOptions MaterialOptions, const f32 Distance) : IComponent()
	{

		isInstanced = false;

		renderLayer = RenderLayer::World;

		this->renderable = renderable;
		isHeightfield = dynamic_cast<Heightfield*>(this->renderable.get()) != NULL;

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

	void RenderingComponent::AddLOD(const std::shared_ptr<Renderable> &renderable, const f32 Distance, const ShaderOptions MaterialOptions)
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

	// PYROS_AUTO_LOD=0: no automatic levels are made - for telling whether something seen is theirs.
	void RenderingComponent::SetAutoLOD(const bool on) { g_autoLod = on; }
	bool RenderingComponent::GetAutoLOD() { return g_autoLod; }
	void RenderingComponent::SetAutoLODInUse(const bool on) { if (on != g_autoLodInUse) { g_autoLodInUse = on; g_autoLodChanges++; RenderState::Touch(); } }
	bool RenderingComponent::GetAutoLODInUse() { return g_autoLodInUse; }
	// How far each level of detail reaches, as a multiple of what the scene says: the
	// application's quality setting for the machine it is on (1: as authored; 2: every
	// level kept to twice the distance; 0.5: half).
	namespace {
		f32 g_lodScale = 1.f;
		f32 g_lodFade = 0.25f;
		f64 LodClock() { return std::chrono::duration<f64>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
	}
	void RenderingComponent::SetLODCrossfade(const f32 seconds) { g_lodFade = seconds > 0.f ? seconds : 0.f; }
	f32 RenderingComponent::GetLODCrossfade() { return g_lodFade; }
	void RenderingComponent::SetLODDistanceScale(const f32 scale) { const f32 s = std::max(0.05f, scale); if (s != g_lodScale) { g_lodScale = s; g_autoLodChanges++; RenderState::Touch(); } }
	f32 RenderingComponent::GetLODDistanceScale() { return g_lodScale; }

	// The first time it is listed, a model of no levels gets its own - made ahead (SimplifiedModel::
	// PrepareAutomatic, from the moment the component was made: at load, mostly), so here they are only
	// taken. One still being made is drawn as it is meanwhile and asked again every frame.
	void RenderingComponent::TryAutomaticLODs()
	{
		if (autoLodTried && !autoLodWaiting) return;
		autoLodTried = true;
		if (!g_autoLod || autoLevels != 0 || Meshes.size() != 1 || IsInstanced() || !Owner || !renderable) { autoLodWaiting = false; return; }
		std::shared_ptr<Model> model = std::dynamic_pointer_cast<Model>(renderable);
		if (g_autoLodAhead && model && !model->GetPath().empty())
		{
			SimplifiedModel::PrepareAutomatic(model);        // (one made since loading: begun now)
			if (!SimplifiedModel::AutomaticReady(model->GetPath(), false))
			{
				if (!autoLodWaiting) { autoLodWaiting = true; g_autoLodWaiting.push_back(this); }
				return;
			}
		}
		if (autoLodWaiting) { autoLodWaiting = false; g_autoLodWaiting.erase(std::remove(g_autoLodWaiting.begin(), g_autoLodWaiting.end(), this), g_autoLodWaiting.end()); }
		const Vec3 &scale = Owner->GetScale();
		AddAutomaticLODs(renderable->GetBoundingSphereRadius() * std::max(fabs(scale.x), std::max(fabs(scale.y), fabs(scale.z))));
	}

	void RenderingComponent::PollAutomaticLODs()
	{
		if (g_autoLodWaiting.empty()) return;
		const std::vector<RenderingComponent*> waiting = g_autoLodWaiting;
		for (size_t i = 0; i < waiting.size(); i++) waiting[i]->TryAutomaticLODs();
	}

	bool RenderingComponent::AddAutomaticLODs(const f32 worldRadius, const f32 given)
	{
		const uint32 kLightest = SimplifiedModel::AutoLightest;
		static const f32 kFrom[3] = { 12.f, 28.f, 60.f };
		const f32* kRatio = SimplifiedModel::AutoRatios;
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
		autoLodRadius = worldRadius;
		// (a quarter of the model for its shadow - unless it was given a shadow mesh of its own, which
		// this used to throw away: a scene's "shadowRenderable" never cast on any model with levels)
		if (shadowDetail <= 0.f && !shadowRenderable && triangles >= 3000) SetShadowDetail(0.25f);
		return true;
	}

	void RenderingComponent::RescaleAutomaticLODs(const f32 worldRadius)
	{
		if (autoLevels == 0 || autoLodRadius <= 0.f || worldRadius <= 0.f) return;
		const f32 k = worldRadius / autoLodRadius;
		if (k > 0.8f && k < 1.25f) return;           // (not for an arm's swing: for a size that was wrong)
		for (size_t i = 0; i < LODDistances.size(); i++)
			if (LODDistances[i] < 1e8f) LODDistances[i] *= k;
		autoLodRadius = worldRadius;
		LastLodDistance = -1.f;                         // (asked again next time, at this size)
		RenderState::Touch();
	}

	void RenderingComponent::ClearLODs()
	{
		RenderState::Touch();
		FinishLODFade();
		if (Meshes.size() <= 1) { LOD = false; if (LODDistances.size() > 1) LODDistances.resize(1); return; }
		// the scene is drawing one level's meshes: make that the nearest
		// before the others go
		if (LodInUse != 0)
		{
			if (Registered && Scene) UpdateLOD(0, true);
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

	void RenderingComponent::SetShadowRenderable(const std::shared_ptr<Renderable> &renderable, const f32 from)
	{
		SayDrawnChanged();        // (a kept list holds nothing of the shadow's mesh: only this component's entries are read again)
		for (size_t i = 0; i < shadowMeshes.size(); i++) delete shadowMeshes[i];
		shadowMeshes.clear();
		shadowRenderable.reset();
		if (!renderable || renderable->Geometries.empty()) return;
		const std::vector<RenderingMesh*> &own = Meshes[0];
		if (own.empty()) return;
		shadowRenderable = renderable;
		{
			f32 size = this->renderable ? this->renderable->GetBoundingSphereRadius() : 0.f;
			if (Owner) { const Vec3 &sc = Owner->GetScale(); size *= std::max(fabs(sc.x), std::max(fabs(sc.y), fabs(sc.z))); }
			shadowMeshFromSQR = (3.f * size) * (3.f * size);
			if (from >= 0.f) shadowMeshFromSQR = from * from;
		}
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
			// (a stand-in with fewer parts than the model borrows the material of a part that casts:
			// the Polo's first is its glass, and a see-through caster casts nothing)
			if (renderable->Geometries.size() < own.size() && m->Material && m->Material->IsTransparent())
				for (size_t k = 0; k < own.size(); k++)
					if (own[k]->Material && !own[k]->Material->IsTransparent() && own[k]->Material->IsCastingShadows())
					{ m->standsFor = NULL; m->Material = own[k]->Material; break; }
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

	uint32 RenderingComponent::LevelAtDistanceSQR(const f32 Distance) const
	{
		for (size_t i = 0; i < LODDistances.size(); i++)
			if (Distance < LODDistances[i] * LODDistances[i] * g_lodScale * g_lodScale)
				return (autoLevels != 0 && !g_autoLodInUse && i >= 1 && i <= autoLevels) ? 0 : (uint32)i;
		return (uint32)LODDistances.size();
	}
	f32 RenderingComponent::LODDistanceSQR(const Vec3 &from, const Vec3 &ownerPosition, const Vec3 &ownerScale)
	{
		const f32 maxScale = std::max(fabs(ownerScale.x), std::max(fabs(ownerScale.y), fabs(ownerScale.z)));
		Vec3 sphere; f32 sphereRadius;
		GetLODSphere(sphere, sphereRadius);
		const f32 d = std::max(0.f, from.distance(ownerPosition + sphere * ownerScale) - sphereRadius * maxScale);
		return d * d;
	}
	void RenderingComponent::GetLODSphere(Vec3 &centre, f32 &radius)
	{
		const size_t parts = GetOwner() ? GetOwner()->GetComponents().size() : 0;
		if (lodSphereRadius < 0.f || parts != lodSphereParts)
		{
			lodSphereParts = parts;
			lodSphereCentre = BoundingSphereCenter;
			lodSphereRadius = BoundingSphereRadius;
			if (GetOwner())
			{
				const std::vector<std::shared_ptr<IComponent> > &comps = GetOwner()->GetComponents();
				for (size_t i = 0; i < comps.size(); i++)
				{
					IComponent* c = comps[i].get();
					if (!c || c == this) continue;
					const uint32 t = c->GetComponentType();
					if (t != ComponentType::RenderingComponent && t != ComponentType::RenderingInstancedComponent) continue;
					RenderingComponent* o = static_cast<RenderingComponent*>(c);
					if (!o->HasLOD()) continue;
					// grow the sphere to hold the other part's (Ritter's step)
					const Vec3 oc = o->GetBoundingSphereCenter();
					const f32 orad = o->GetBoundingSphereRadius();
					const f32 d = (oc - lodSphereCentre).magnitude();
					if (d + orad <= lodSphereRadius) continue;
					if (d + lodSphereRadius <= orad) { lodSphereCentre = oc; lodSphereRadius = orad; continue; }
					const f32 r = (lodSphereRadius + d + orad) * 0.5f;
					lodSphereCentre = lodSphereCentre + (oc - lodSphereCentre) * ((r - lodSphereRadius) / d);
					lodSphereRadius = r;
				}
			}
		}
		centre = lodSphereCentre;
		radius = lodSphereRadius;
	}

	uint32 RenderingComponent::GetLODByDistance(const f32 Distance)
	{
		if (Distance != LastLodDistance || autoLodSeen != g_autoLodChanges)
		{
			LastLodDistance = Distance;
			autoLodSeen = g_autoLodChanges;
			for (size_t i = 0; i < LODDistances.size(); i++)
			{
				if (Distance < LODDistances[i] * LODDistances[i] * g_lodScale * g_lodScale)
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
	void RenderingComponent::UpdateLOD(const uint32 lod, const bool instant)
	{
		// Check if LOD Level is Different
		if (LodInUse != lod && lod < GetLODSize())
		{
			// (one change at a time: a level still going goes now)
			if (lodFadeFrom >= 0) FinishLODFade();
			std::vector<RenderingMesh*> &listed = Scene->GetRenderingMeshes();
			const std::vector<RenderingMesh*> &was = Meshes[LodInUse];
			const std::vector<RenderingMesh*> &now = Meshes[lod];
			// Crossfading: the new level's meshes are listed beside the old one's, which stay until it
			// has dissolved into them (FinishLODFade). A level's pop, as the camera came closer, was
			// the most visible thing about a big world.
			if (!instant && g_lodFade > 0.f)
			{
				for (size_t m = 0; m < now.size(); m++) { listed.push_back(now[m]); RenderState::NoteListed(RenderState::Listed::MeshOn, Scene, this, Owner, now[m]); now[m]->LodFade = 1e-3f; }
				for (size_t m = 0; m < was.size(); m++) was[m]->LodFade = -1.f + 1e-3f;
				lodFadeFrom = (int32)LodInUse;
				lodFadeStart = LodClock();
				LodInUse = lod;
				return;
			}
			// The scene lists the meshes of the level in use, and only those:
			// they are taken out and the new level's put in - in the places the
			// old ones had, as far as there are as many. (Every mesh of EVERY
			// level was looked for through the scene's whole list, one at a
			// time, and each one found was closed up behind: with a few hundred
			// things changing level as the camera ran, that was most of a frame.)
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
	void RenderingComponent::StepLODFade()
	{
		if (lodFadeFrom < 0) return;
		const f32 p = g_lodFade > 0.f ? (f32)((LodClock() - lodFadeStart) / (f64)g_lodFade) : 1.f;
		if (p >= 1.f) { FinishLODFade(); return; }
		const f32 in = std::max(p, 1e-3f), out = -1.f + std::max(p, 1e-3f);
		std::vector<RenderingMesh*> &now = Meshes[LodInUse], &was = Meshes[(uint32)lodFadeFrom];
		for (size_t m = 0; m < now.size(); m++) now[m]->LodFade = in;
		for (size_t m = 0; m < was.size(); m++) was[m]->LodFade = out;
	}
	void RenderingComponent::FinishLODFade()
	{
		if (lodFadeFrom < 0) return;
		const std::vector<RenderingMesh*> &was = Meshes[(uint32)lodFadeFrom];
		const std::vector<RenderingMesh*> &now = Meshes[LodInUse];
		if (Scene != NULL)
		{
			std::vector<RenderingMesh*> &listed = Scene->GetRenderingMeshes();
			for (size_t m = 0; m < was.size(); m++)
				for (size_t k = listed.size(); k-- > 0;)
					if (listed[k] == was[m]) { listed.erase(listed.begin() + k); RenderState::NoteListed(RenderState::Listed::MeshOff, Scene, this, Owner, was[m]); break; }
		}
		for (size_t m = 0; m < was.size(); m++) was[m]->LodFade = 0.f;
		for (size_t m = 0; m < now.size(); m++) now[m]->LodFade = 0.f;
		lodFadeFrom = -1;
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

			// (a level that was going: its meshes are listed too, and go with the rest below)
			if (lodFadeFrom >= 0)
			{
				for (std::map<uint32, std::vector<RenderingMesh*> >::iterator i = Meshes.begin(); i != Meshes.end(); i++)
					for (size_t m = 0; m < i->second.size(); m++) i->second[m]->LodFade = 0.f;
				lodFadeFrom = -1;
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

	ModelGeometry* RenderingComponent::OwnGeometry(RenderingMesh* m)
	{
		if (!m || !m->Geometry) return NULL;
		for (size_t k = 0; k < ownGeometries.size(); k++) if (ownGeometries[k].get() == m->Geometry) return ownGeometries[k].get();
		ModelGeometry* g = dynamic_cast<ModelGeometry*>(m->Geometry);
		std::unique_ptr<ModelGeometry> copy(new ModelGeometry());
		if (!g)
		{
			// (a primitive's: the same attributes under the same names, but for vertex colours,
			// which a model's geometry has no place for)
			PrimitiveGeometry* pg = dynamic_cast<PrimitiveGeometry*>(m->Geometry);
			if (!pg || !pg->tColor.empty() || pg->tVertex.empty()) return NULL;
			copy->tVertex = pg->tVertex; copy->tNormal = pg->tNormal; copy->tTexcoord = pg->tTexcoord;
			copy->tTangent = pg->tTangent; copy->tBitangent = pg->tBitangent;
			copy->index = pg->index;
			copy->CreateBuffers();
			copy->SendBuffers();
			ModelGeometry* own = copy.get();
			ownOriginals.push_back(std::make_pair(m, m->Geometry));
			m->Geometry = own;
			ownGeometries.push_back(std::move(copy));
			ownBuffersRevision++;
			RenderState::Touch();
			return own;
		}
		copy->tVertex = g->tVertex; copy->tNormal = g->tNormal; copy->tTexcoord = g->tTexcoord;
		copy->tTangent = g->tTangent; copy->tBitangent = g->tBitangent;
		copy->tBonesID = g->tBonesID; copy->tBonesWeight = g->tBonesWeight;
		copy->MapBoneIDs = g->MapBoneIDs; copy->BoneOffsetMatrix = g->BoneOffsetMatrix;
		copy->morphTargets = g->morphTargets; copy->morphBaseVertex = g->morphBaseVertex; copy->morphBaseNormal = g->morphBaseNormal;
		copy->index = g->index; copy->materialProperties = g->materialProperties;
		copy->CreateBuffers();
		copy->SendBuffers();
		if (getenv("PYROS_DENT_TRACE")) fprintf(stderr, "[dent] %s: own copy of geometry %p -> %p\n", GetOwner() ? GetOwner()->GetName().c_str() : "?", (void*)g, (void*)copy.get());
		ModelGeometry* own = copy.get();
		ownOriginals.push_back(std::make_pair(m, m->Geometry));
		m->Geometry = own;
		ownGeometries.push_back(std::move(copy));
		ownBuffersRevision++;              // (cached bindings were of the shared buffers)
		RenderState::Touch();              // (the kept draw list batched it with the others)
		return own;
	}

	void RenderingComponent::RestoreOwnGeometry()
	{
		// (the last generation goes now: by the next Stop its frames are long done)
		retiredGeometries.clear();
		if (ownOriginals.empty() && ownGeometries.empty() && morphWeights.empty()) return;
		for (size_t k = 0; k < ownOriginals.size(); k++)
			if (ownOriginals[k].first) ownOriginals[k].first->Geometry = ownOriginals[k].second;
		ownOriginals.clear();
		// (kept a generation, not freed: a Stop clicked in the editor runs while a frame that
		// draws them is being recorded)
		for (size_t k = 0; k < ownGeometries.size(); k++) retiredGeometries.push_back(std::move(ownGeometries[k]));
		ownGeometries.clear();
		morphWeights.clear();
		if (renderable)
		{
			minBounds = renderable->GetBoundingMinValue(); maxBounds = renderable->GetBoundingMaxValue();
			BoundingSphereRadius = renderable->GetBoundingSphereRadius();
			BoundingSphereCenter = renderable->GetBoundingSphereCenter();
			if (Owner) Owner->RefreshComponentBounds();
		}
		ownBuffersRevision++;
		RenderState::Touch();
	}

	std::vector<std::string> RenderingComponent::GetMorphTargetNames()
	{
		std::vector<std::string> names;
		for (RenderingMesh* m : GetMeshes(0))
		{
			ModelGeometry* g = m ? dynamic_cast<ModelGeometry*>(m->Geometry) : NULL;
			if (!g) continue;
			for (const MorphTarget &t : g->morphTargets)
				if (std::find(names.begin(), names.end(), t.name) == names.end()) names.push_back(t.name);
		}
		return names;
	}

	f32 RenderingComponent::GetMorphWeight(const std::string &name) const
	{
		std::map<std::string, f32>::const_iterator it = morphWeights.find(name);
		return it == morphWeights.end() ? 0.f : it->second;
	}

	void RenderingComponent::SetMorphWeight(const std::string &name, const f32 weight)
	{
		if (GetMorphWeight(name) == weight && morphWeights.count(name)) return;
		morphWeights[name] = weight;
		for (RenderingMesh* m : GetMeshes(0))
		{
			ModelGeometry* g = m ? dynamic_cast<ModelGeometry*>(m->Geometry) : NULL;
			if (!g || g->morphTargets.empty()) continue;
			bool has = false;
			for (const MorphTarget &t : g->morphTargets) if (t.name == name) { has = true; break; }
			if (has) ApplyMorphs(m);
		}
	}

	void RenderingComponent::ApplyMorphs(RenderingMesh* m)
	{
		ModelGeometry* g = OwnGeometry(m);
		if (!g) return;
		if (g->morphBaseVertex.size() != g->tVertex.size()) { g->morphBaseVertex = g->tVertex; g->morphBaseNormal = g->tNormal; }
		const size_t n = g->tVertex.size();
		g->tVertex = g->morphBaseVertex;
		std::vector<Vec3> nrm = g->morphBaseNormal;
		bool normals = nrm.size() == n;
		for (const MorphTarget &t : g->morphTargets)
		{
			const f32 w = GetMorphWeight(t.name);
			if (w == 0.f || t.dPos.size() != n) continue;
			for (size_t v = 0; v < n; v++) g->tVertex[v] = g->tVertex[v] + t.dPos[v] * w;
			if (normals && t.dNrm.size() == n) for (size_t v = 0; v < n; v++) nrm[v] = nrm[v] + t.dNrm[v] * w;
		}
		if (normals) { for (size_t v = 0; v < n; v++) { const f32 l = nrm[v].magnitude(); if (l > 1e-8f) nrm[v] = nrm[v] / l; } g->tNormal = nrm; }
		g->RecalculateBounding();
		g->ResendVertices();
	}

	uint32 RenderingComponent::Dent(const Vec3 &worldPoint, const f32 radius, const Vec3 &worldPush)
	{
		if (!GetOwner() || radius <= 0.f) return 0;
		const Matrix world = GetOwner()->GetWorldTransformation();
		const Matrix inv = world.Inverse();
		const Vec3 c = inv * worldPoint;
		// (the push in the object's own space: through the inverse without its translation)
		const Vec3 push = inv * (worldPoint + worldPush) - c;
		// (the radius in its own units: an object scaled 0.01 has a centimetre model)
		const Vec3 r1 = inv * (worldPoint + Vec3(radius, 0.f, 0.f)) - c;
		const f32 r = std::max(1e-5f, r1.magnitude());
		uint32 moved = 0;
		std::vector<RenderingMesh*> &meshes = GetMeshes(0);
		for (RenderingMesh* m : meshes)
		{
			ModelGeometry* g = m ? dynamic_cast<ModelGeometry*>(m->Geometry) : NULL;
			if (!g || !g->tBonesID.empty() || g->tVertex.empty()) continue;
			// (nothing of it touched: left as it is, shared)
			{
				bool touched = false;
				for (size_t v = 0; v < g->tVertex.size() && !touched; v++) touched = g->tVertex[v].distance(c) < r;
				if (!touched) continue;
			}
			// (its own copy, the first time: the model file's geometry is every user's)
			g = OwnGeometry(m);
			std::vector<uint8> touched(g->tVertex.size(), 0);
			bool any = false;
			for (size_t v = 0; v < g->tVertex.size(); v++)
			{
				const f32 d = g->tVertex[v].distance(c);
				if (d >= r) continue;
				f32 w = 1.f - d / r; w = w * w * (3.f - 2.f * w);
				g->tVertex[v] = g->tVertex[v] + push * w;
				// (and the shape morph weights are applied over, or the next weight set undid the dent)
				if (g->morphBaseVertex.size() == g->tVertex.size()) g->morphBaseVertex[v] = g->morphBaseVertex[v] + push * w;
				touched[v] = 1; any = true; moved++;
			}
			if (!any) continue;
			// the normals of what moved, from the triangles that hold it (area-weighted)
			if (g->tNormal.size() == g->tVertex.size())
			{
				for (size_t v = 0; v < touched.size(); v++) if (touched[v]) g->tNormal[v] = Vec3(0.f, 0.f, 0.f);
				const std::vector<__INDEX_C_TYPE__> &ix = g->index;
				for (size_t t = 0; t + 2 < ix.size(); t += 3)
				{
					const uint32 a = ix[t], b = ix[t + 1], cc = ix[t + 2];
					if (!(touched[a] || touched[b] || touched[cc])) continue;
					const Vec3 fn = (g->tVertex[b] - g->tVertex[a]).cross(g->tVertex[cc] - g->tVertex[a]);
					if (touched[a]) g->tNormal[a] = g->tNormal[a] + fn;
					if (touched[b]) g->tNormal[b] = g->tNormal[b] + fn;
					if (touched[cc]) g->tNormal[cc] = g->tNormal[cc] + fn;
				}
				for (size_t v = 0; v < touched.size(); v++)
					if (touched[v]) { const f32 l = g->tNormal[v].magnitude(); g->tNormal[v] = l > 1e-12f ? g->tNormal[v] / l : Vec3(0.f, 1.f, 0.f); }
				// (the base's normals take the dent's: exact with no weight on, close with one)
				if (g->morphBaseNormal.size() == g->tNormal.size())
					for (size_t v = 0; v < touched.size(); v++) if (touched[v]) g->morphBaseNormal[v] = g->tNormal[v];
			}
			g->RecalculateBounding();
			g->ResendVertices();
		}
		return moved;
	}

	std::vector<RenderingMesh*> &RenderingComponent::GetMeshes(const uint32 LODLevel)
	{
		// Never index past the levels that exist. With no meshes at all (a 2D
		// character with bones and no sprites yet), the old fallback indexed
		// Meshes[GetLODSize() - 1] = Meshes[UINT_MAX], which INSERTED a level;
		// every later Meshes[l] inserted another, so any loop bounded by
		// GetLODSize() - UploadSkinning's - never ended. An empty component
		// gets one empty level 0 instead, and the count stops growing.
		if (Meshes.empty()) return Meshes[0];
		std::map<uint32, std::vector<RenderingMesh*> >::iterator it = Meshes.find(LODLevel);
		if (it != Meshes.end()) return it->second;
		return Meshes.rbegin()->second;
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
		if (autoLodWaiting) g_autoLodWaiting.erase(std::remove(g_autoLodWaiting.begin(), g_autoLodWaiting.end(), this), g_autoLodWaiting.end());
		if (animatedSlot >= 0) SetActiveSkeletonAnimation(NULL);
		for (std::map<uint32, std::vector<RenderingMesh*> >::iterator i = Meshes.begin(); i != Meshes.end(); i++)
		{
			for (std::vector<RenderingMesh*>::iterator k = (*i).second.begin(); k != (*i).second.end(); k++)
				// Delete Mesh
				delete (*k);
		}
		// Clear Meshes List
		Meshes.clear();
		ownOriginals.clear();              // (the meshes those were of are gone)
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
				// A state machine starts with the game too, not while the scene
				// is being authored (it would drive the rig being posed).
				if (comps[c] && comps[c]->GetComponentType() == ComponentType::Animator)
					static_cast<AnimatorComponent*>(comps[c].get())->SetRunning(true);
				// (cloth too: in the editor the mesh stays as made)
				if (comps[c] && comps[c]->GetComponentType() == ComponentType::Cloth)
					static_cast<ClothComponent*>(comps[c].get())->SetRunning(true);
				if (comps[c] && comps[c]->GetComponentType() == ComponentType::NavAgent)
					static_cast<NavAgentComponent*>(comps[c].get())->SetRunning(true);
				if (comps[c] && comps[c]->GetComponentType() == ComponentType::CharacterController)
					static_cast<CharacterController*>(comps[c].get())->SetRunning(true);
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
		spriteResolve2D = resolve;
		spritePartShownTexture.clear();
		spritePartShownColor.clear();
		for (size_t i = 0; i < parts.size(); i++)
		{
			spritePartShownTexture.push_back(parts[i].texture);
			spritePartShownColor.push_back(Vec4(1.f, 1.f, 1.f, 1.f));
		}

		// The skeleton in id order, for parts that bend across two bones.
		std::vector<Bone> bones(skeleton.size());
		for (std::map<StringID, Bone>::const_iterator b = skeleton.begin(); b != skeleton.end(); ++b)
			if (b->second.self >= 0 && (size_t)b->second.self < bones.size()) bones[b->second.self] = b->second;
		SpriteRig2DBuild built = BuildSpriteRig2D(parts, resolve, bones.empty() ? NULL : &bones);
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
		isHeightfield = dynamic_cast<Heightfield*>(this->renderable.get()) != NULL;

		for (uint32 i = 0; i < renderable->Geometries.size(); i++)
		{
			RenderingMesh* m = new RenderingMesh();
			m->Geometry = renderable->Geometries[i];
			m->Material = (i < materials.size()) ? materials[i] : std::shared_ptr<IMaterial>();
			m->renderingComponent = this;
			// A skinned piece (a 2D part that bends across two bones) needs
			// its bone map, as the constructor copies it for a model.
			if (m->Geometry && m->Geometry->materialProperties.haveBones)
			{
				m->MapBoneIDs = m->Geometry->MapBoneIDs;
				m->BoneOffsetMatrix = m->Geometry->BoneOffsetMatrix;
			}
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

			// A clip's property tracks for this part, else its own rest values.
			SkeletonAnimationInstance::TrackValue tv;
			const bool tracks = inst->HasTrackValues();
			f32 z = part.z;
			if (tracks && inst->GetTrackValue(part.name, "z", tv) && tv.hasValue) z = tv.value.x;
			bool visible = true;
			if (tracks && inst->GetTrackValue(part.name, "visible", tv) && tv.hasValue) visible = tv.value.x > 0.5f;
			ms[i]->Active = visible;
			Vec4 color(1.f, 1.f, 1.f, 1.f);
			if (tracks && inst->GetTrackValue(part.name, "color", tv) && tv.hasValue) color = tv.value;
			std::string texture = part.texture;
			if (tracks && inst->GetTrackValue(part.name, "sprite", tv) && tv.hasText) texture = tv.text;
			GenericShaderMaterial* gm = (i < spritePartShownColor.size() && ms[i]->Material)
				? dynamic_cast<GenericShaderMaterial*>(ms[i]->Material.get()) : NULL;
			if (gm)
			{
				const Vec4 &was = spritePartShownColor[i];
				if (was.x != color.x || was.y != color.y || was.z != color.z || was.w != color.w)
				{
					gm->SetColor(color);
					spritePartShownColor[i] = color;
				}
				if (texture != spritePartShownTexture[i])
				{
					// Shared: a swap cycle (mouth shapes, a hand opening) reuses
					// the same few textures every time round.
					const std::string path = spriteResolve2D ? spriteResolve2D(texture) : texture;
					std::shared_ptr<Texture> t = texture.empty() ? std::shared_ptr<Texture>()
						: Texture::LoadShared(path, TextureType::Texture, true, true);
					gm->SetColorMap(t ? t : UIImage::WhiteTexture());
					spritePartShownTexture[i] = texture;
				}
			}

			// A bent part is skinned: its grid is already in rest space and the
			// bones move it, so its Pivot only carries an animated draw order.
			if (!part.bendBone.empty() && !ms[i]->MapBoneIDs.empty())
			{
				Matrix zOnly;
				zOnly.Translate(Vec3(0.f, 0.f, z - part.z));
				ms[i]->Pivot = zOnly;
				continue;
			}

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
			off.Translate(Vec3(part.offset.x, part.offset.y, z));
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
			// A bent part is a skinned grid in rest space with an identity
			// Pivot, so its corners mean nothing here - its geometry's own box
			// (the rest pose, which is what framing is for) does.
			if (!spriteParts2D[i].bendBone.empty() && !ms[i]->MapBoneIDs.empty() && ms[i]->Geometry)
			{
				const Vec3 mn = ms[i]->Geometry->GetBoundingMinValue(), mx = ms[i]->Geometry->GetBoundingMaxValue();
				if (!any) { outMin = Vec2(mn.x, mn.y); outMax = Vec2(mx.x, mx.y); any = true; }
				else
				{
					outMin.x = std::min(outMin.x, mn.x); outMin.y = std::min(outMin.y, mn.y);
					outMax.x = std::max(outMax.x, mx.x); outMax.y = std::max(outMax.y, mx.y);
				}
				continue;
			}
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

	std::atomic<uint32> RenderingComponent::SeenEpoch(0);

	std::atomic<uint32_t> RenderState::Version(1);
	std::atomic<uint64_t> RenderState::Structure(1);
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

	namespace {
		std::vector<RenderingComponent*> &Animated() { static std::vector<RenderingComponent*> v; return v; }
		bool g_parallelAnimation = true;
	}
	void RenderingComponent::SetParallelAnimation(const bool on) { g_parallelAnimation = on; }
	bool RenderingComponent::GetParallelAnimation() { return g_parallelAnimation; }
	void RenderingComponent::SetActiveSkeletonAnimation(void* instance)
	{
		activeSkeletonAnimation = instance;
		std::vector<RenderingComponent*> &all = Animated();
		if (instance != NULL && animatedSlot < 0) { animatedSlot = (int32)all.size(); all.push_back(this); }
		else if (instance == NULL && animatedSlot >= 0)
		{
			all[animatedSlot] = all.back(); all[animatedSlot]->animatedSlot = animatedSlot;
			all.pop_back(); animatedSlot = -1;
			animatedReached = animatedAhead = false;
		}
		WakeOwner();
	}
	void RenderingComponent::AnimateAhead(SceneGraph* scene, const f64 time)
	{
		std::vector<RenderingComponent*> &all = Animated();
		std::vector<RenderingComponent*> now;
		for (size_t i = 0; i < all.size(); i++)
		{
			RenderingComponent* c = all[i];
			if (!c->Registered || c->Scene != scene) continue;
			// (posed ahead of the last walk and never come to: no longer walked)
			const bool reached = c->animatedReached && !c->animatedAhead;
			c->animatedReached = false; c->animatedAhead = false;
			if (!reached || !g_parallelAnimation) continue;
			SkeletonAnimationInstance* si = static_cast<SkeletonAnimationInstance*>(c->activeSkeletonAnimation);
			if (si->GetOwner() == NULL || si->HasPoseModifiers()) continue;
			if (!(c->animateWhenUnseen || c->WasSeenRecently())) continue;
			now.push_back(c);
		}
		if (now.size() < 2) return;       // (one alone is as well posed by the walk)
		PYROS_PROFILE_SCOPE("Scene.AnimateAhead");
		const f32 at = (f32)time;
		JobSystem::Instance().ParallelFor((uint32)now.size(), 1, [&now, at](uint32 begin, uint32 end) {
			for (uint32 i = begin; i < end; i++)
			{
				SkeletonAnimationInstance* si = static_cast<SkeletonAnimationInstance*>(now[i]->activeSkeletonAnimation);
				si->GetOwner()->UpdateInstance(si, at);
			}
		});
		for (size_t i = 0; i < now.size(); i++) now[i]->animatedAhead = true;
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
			animatedReached = true;
			// (posed already, ahead of this walk: AnimateAhead)
			if (animatedAhead) animatedAhead = false;
			else
			{
				SkeletonAnimationInstance* si =
					static_cast<SkeletonAnimationInstance*>(activeSkeletonAnimation);
				if (si->GetOwner() && (animateWhenUnseen || WasSeenRecently())) si->GetOwner()->UpdateInstance(si, (f32)time);
			}
			// Back on the main thread: events to scripts, root motion to the
			// owner. The update itself may have run on a worker (AnimateAhead).
			static_cast<SkeletonAnimationInstance*>(activeSkeletonAnimation)->DeliverPending();
		}

		// After the pose for this frame, not before it: placing the parts
		// first drew every 2D character one frame behind its own skeleton.
		RefreshSpriteParts2D();

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
