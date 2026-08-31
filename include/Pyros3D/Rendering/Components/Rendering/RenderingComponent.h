//============================================================================
// Name        : RenderingComponent
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Component For Rendering
//============================================================================

#ifndef RENDERINGCOMPONENT_H
#define	RENDERINGCOMPONENT_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Assets/Renderable/Renderables.h>
#include <Pyros3D/Assets/Renderable/Models/Model.h>
#include <Pyros3D/Materials/IMaterial.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <vector>
#include <memory>

namespace p3d {

	// Drawing Type
	namespace DrawingType
	{
		enum {
			Triangles = 0,
			Lines,
			Line_Loop,
			Line_Strip,
			Triangle_Fan,
			Triangle_Strip,
			Quads,
			Points,
			Polygons
		};
	}

	// Which pass a component belongs to. The renderer picks one layer and
	// draws only that, which is the piece tag filtering could never do:
	// GroupAndSortAssets()'s Tag filter is include-only (keep meshes that
	// HAVE the tag), so there was no way to say "everything except the UI"
	// and a screen-space canvas would otherwise be drawn a second time out
	// in the 3D world by the main pass.
	//
	// Deliberately a layer number on the component rather than a tag on the
	// GameObject: tags are user data that serialize into the scene file and
	// are matched by name, so borrowing one for engine bookkeeping would put
	// a reserved name into every project's tag list.
	namespace RenderLayer
	{
		enum {
			World = 0,
			UI = 1,
			// Nothing is ever assigned this. It exists so a renderer can be
			// pointed at an empty layer to draw nothing at all - which is
			// not the same as pointing it at another real layer, where it
			// would happily draw somebody else's meshes with its own
			// projection.
			None = 2
		};
	}

	// Circular Dependency
	class PYROS3D_API RenderingComponent;

	class PYROS3D_API RenderingMesh {

	public:

		RenderingMesh(const uint32 lod = 0) : drawingType(DrawingType::Triangles), CullingGeometry(0), Active(true), Clickable(true), hasTexcoordAttribute(-1), LodLevel(lod) {} // Triangles by Default

		// Not = default: releases the cached VAOs in VAOCache, which needs
		// glDeleteVertexArrays (defined out-of-line in RenderingComponent.cpp
		// so this widely-included header doesn't need a GL dependency).
		virtual ~RenderingMesh();

		uint32 GetDrawingType() { return drawingType; }

		// Pointer to Geometry
		IGeometry* Geometry;

		// Shaders Cache
		std::map<uint32, std::vector< std::vector<int32> > > ShadersAttributesCache;
		std::map<uint32, std::vector<int32> > ShadersGlobalCache;
		std::map<uint32, std::vector<int32> > ShadersModelCache;
		std::map<uint32, std::vector<int32> > ShadersUserCache;
		// GL Vertex Array Object cache, keyed by shader program. Bakes in
		// attribute enable/pointer state and the bound index buffer so
		// switching meshes at draw time is a single glBindVertexArray
		// instead of re-issuing glEnableVertexAttribArray/glVertexAttribPointer
		// per attribute on every switch. Keyed by shader because attribute
		// locations can differ across shader variants using this mesh.
		std::map<uint32, uint32> VAOCache;

		// Geometry->buffersRevision that VAOCache's entries were built
		// against. A geometry can hand out new GPU buffers under a mesh that
		// is already being drawn (Text::UpdateText() disposes and rebuilds
		// in place), which leaves every cached VAO referencing freed buffer
		// handles - BindMesh() compares this and throws the cache away when
		// it goes stale. See IGeometry::buffersRevision.
		uint32 VAOCacheRevision = 0;

		// Vulkan pipeline cache, keyed by (shader program, current render
		// target) packed into one uint64 - ((uint64)shader << 32) | targetFBO,
		// see IRenderDevice::GetCurrentRenderTarget(). Mirrors VAOCache's
		// shader-only keying (see RenderObject()/BindMesh() in IRenderer.cpp)
		// plus the render-target dimension a Vulkan pipeline needs, since
		// it bakes in a specific render pass's attachment shape at
		// creation time - the same mesh+shader drawn into two
		// differently-shaped targets (e.g. a color-only reflection FBO
		// versus the main color+depth swapchain pass) needs two separate
		// pipelines. Only ever populated on the Vulkan backend
		// (GLRenderDevice::CreatePipeline() exists too, but nothing calls
		// it outside this cache - GL still uses the individual
		// SetCullFaceMode/SetBlendingEnabled/etc calls directly, and
		// GetCurrentRenderTarget() always returns 0 there, collapsing
		// this back to shader-only keying, matching prior behavior
		// exactly since GL has no render-pass-shape concept to
		// disambiguate). Built from Material's blend/depth/cull/wireframe
		// state at the moment this (mesh, shader, target) triple is first
		// bound, not re-evaluated per object the way GL's own
		// dirty-tracked state is - correct for any Material whose
		// blend/depth/cull state doesn't change after the fact for a
		// given mesh/shader/target combination.
		std::map<uint64, uint32> PipelineCache;

		// Materials - shared_ptr so many meshes/components can share one
		// IMaterial (and Lua/C++ share one refcount). BuildMaterials()
		// assigns a freshly-made material here; a caller-supplied material
		// is stored the same way.
		std::shared_ptr<IMaterial> Material;

		// Drawing Type
		uint32 drawingType;

		// Pointer to Owner
		RenderingComponent* renderingComponent;

		// Culling Method
		uint32 CullingGeometry;

		// Pivot
		Matrix Pivot;

		// Clickable
		bool Clickable, Active;

		// Map Bone ID's
		std::map<int32, int32> MapBoneIDs;
		// Bone Offset Matrix
		std::map<int32, Matrix> BoneOffsetMatrix;
		// Bones Matrix List
		std::vector<Matrix> SkinningBones;

		// Whether this mesh's geometry actually supplies aTexcoord, cached
		// because IRenderer::PickShadowMaterial() has to know it per draw.
		// A cutout caster only gets the alpha-test shadow material if its
		// geometry can feed that shader's texcoord attribute: Vulkan
		// requires every attribute a compiled shader declares to have a
		// matching vertex buffer attribute
		// (VUID-VkGraphicsPipelineCreateInfo-Input-07904), the same rule
		// BuildMaterials() masks ShaderUsage::Skinning off for.
		// -1 not yet determined, 0 no, 1 yes.
		int8 hasTexcoordAttribute;

		// LOD
		uint32 LodLevel;

		void BuildMaterials(const uint32 &MaterialOptions)
		{
			
			// From Properties
			uint32 options = 0;
			// Get Material Options
			if (Geometry->materialProperties.haveColor) options = options | ShaderUsage::Color;
			if (Geometry->materialProperties.haveSpecular) options = options | ShaderUsage::SpecularColor;
			if (Geometry->materialProperties.haveColorMap) options = options | ShaderUsage::Texture;
			if (Geometry->materialProperties.haveSpecularMap) options = options | ShaderUsage::SpecularMap;
			if (Geometry->materialProperties.haveNormalMap) options = options | ShaderUsage::BumpMapping;

			// Callers can pass ShaderUsage::Skinning explicitly (e.g. one
			// material applied uniformly across every submesh of an
			// animated Model, see SkeletonAnimationExample.cpp) even for a
			// submesh that has no per-vertex bone data at all (a rigid
			// prop submesh within an otherwise-skinned model, or any
			// geometry loaded without bone weights) - GL silently
			// tolerated this (the aBonesID/aBonesWeight attributes just
			// went unbound and were never sampled), but Vulkan requires
			// every attribute a compiled shader variant declares to have
			// a matching vertex buffer attribute
			// (VUID-VkGraphicsPipelineCreateInfo-Input-07904, found via a
			// live SkeletonAnimationExample crash). Mask it off here,
			// the same "derive from what the geometry actually has"
			// pattern already used for Color/Texture/etc above, so a
			// caller's blanket MaterialOptions can't request a shader
			// variant this specific submesh's vertex data can't satisfy.
			uint32 requestedOptions = MaterialOptions;
			if (!Geometry->materialProperties.haveBones) requestedOptions &= ~ShaderUsage::Skinning;

			GenericShaderMaterial* genMat = new GenericShaderMaterial(options | requestedOptions);

			// Material Properties
			if (Geometry->materialProperties.Twosided) genMat->SetCullFace(CullFace::DoubleSided);
			if (Geometry->materialProperties.haveColor) genMat->SetColor(Geometry->materialProperties.Color);
			if (Geometry->materialProperties.haveSpecular) genMat->SetSpecular(Geometry->materialProperties.Specular);
			if (Geometry->materialProperties.Opacity) genMat->SetOpacity(Geometry->materialProperties.Opacity);
			if (Geometry->materialProperties.haveColorMap)
			{
				std::shared_ptr<Texture> colorMap = std::make_shared<Texture>();
				colorMap->LoadTexture(Geometry->materialProperties.colorMap, TextureType::Texture);
				colorMap->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
				genMat->SetColorMap(colorMap);
			}
			if (Geometry->materialProperties.haveSpecularMap)
			{
				std::shared_ptr<Texture> specularMap = std::make_shared<Texture>();
				specularMap->LoadTexture(Geometry->materialProperties.specularMap, TextureType::Texture);
				specularMap->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
				genMat->SetSpecularMap(specularMap);
			}
			if (Geometry->materialProperties.haveNormalMap)
			{
				std::shared_ptr<Texture> normalMap = std::make_shared<Texture>();
				normalMap->LoadTexture(Geometry->materialProperties.normalMap, TextureType::Texture);
				normalMap->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
				genMat->SetNormalMap(normalMap);
			}
			Material.reset(genMat);
		}
	};

	class PYROS3D_API RenderingComponent : public IComponent {

		friend class IRenderer;

	public:

		RenderingComponent(const std::shared_ptr<Renderable> &renderable, const std::shared_ptr<IMaterial> &Material, const f32 Distance = 0.0f);
		RenderingComponent(const std::shared_ptr<Renderable> &renderable, const uint32 MaterialOptions = 0, const f32 Distance = 0.0f);
		void AddLOD(const std::shared_ptr<Renderable> &renderable, const f32 Distance, const std::shared_ptr<IMaterial> &Material);
		void AddLOD(const std::shared_ptr<Renderable> &renderable, const f32 Distance, const uint32 MaterialOptions = 0);

		virtual ~RenderingComponent();

		virtual void Register(SceneGraph* Scene);
		virtual void Init() {}
		// Advances the active texture animation and puts its current frame on
		// the material. Both halves were missing: TextureAnimation::Update()
		// existed but nothing called it, and nothing applied the resulting
		// frame anywhere - so a sprite animation held correct playback state
		// and never actually animated. The timer is absolute (the scene's,
		// not a delta), which is what TextureAnimation::Update() expects and
		// is what makes calling it from several components idempotent.
		virtual void Update(const f64 time = 0);
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene);

		virtual uint32 GetComponentType() const { return ComponentType::RenderingComponent; }

		// See RenderLayer above. World unless something moves it.
		void SetRenderLayer(const uint32 layer) { renderLayer = layer; }
		uint32 GetRenderLayer() const { return renderLayer; }

		void SetCullingGeometry(const uint32 Geometry);
		void EnableCullTest() { cullTest = true; }
		void DisableCullTest() { cullTest = false; }
		bool IsCullTesting() { return cullTest; }

		void EnableCastShadows();
		void DisableCastShadows();
		bool IsCastingShadows();

		Renderable* GetRenderable() { return renderable.get(); }
		// The owning handle, for callers that need to build another
		// component over the same geometry (the editor's selection
		// highlight draws a second RenderingComponent on the same
		// Renderable) - GetRenderable() alone can't do that any more now
		// that the constructors take a shared_ptr.
		const std::shared_ptr<Renderable> &GetRenderableShared() const { return renderable; }

		// Attribute buffers belonging to this component rather than to the
		// geometry it draws - the per-instance transform stream, particle
		// streams, whatever AddBuffer() is handed. Deliberately not stored
		// on Renderable's IGeometry::Attributes: a Renderable is shared, so
		// two instanced components drawing the same mesh (a grass field cut
		// into one component per chunk, say) would each append their own
		// buffer to that one shared list, and every component would then
		// bind all of them to the same attribute location - last write
		// wins, and every chunk ends up drawing the last chunk's instances.
		// BindMesh() walks the geometry's attributes and then these.
		std::vector<AttributeBuffer*> ownAttributeBuffers;

		// Get Model Skeleton
		const std::map<StringID, Bone> &GetSkeleton() const { return skeleton; }

		// Builds a skeleton on this component directly, instead of inheriting
		// one from the Renderable at construction. That was the only way a rig
		// could exist, so a skeleton required an imported rigged model - which
		// makes authoring a 2D rig in the editor impossible, since there is no
		// model to import. Everything downstream reads
		// SkeletonAnimationInstance's copy of this map, so poses, clips,
		// layers, blending and the IK solver all work from an authored
		// skeleton exactly as from a loaded one. Bones with no vertex weights
		// simply skin nothing, which is what a cutout 2D rig wants.
		//
		// `bones` must be in id order (bones[i].self == i), matching what the
		// instance assumes when it indexes by Bone::self.
		void SetSkeleton(const std::vector<Bone> &bones);
		bool HasBones() { return hasBones; }

		// Get Model's Meshes
		std::vector<RenderingMesh*> &GetMeshes(const uint32 LODLevel = 0);

		// Get LOD Number
		const uint32 GetLODSize() const;

		// Per-LOD switch distances, parallel to Meshes' LOD keys.
		const std::vector<f32> &GetLODDistances() const { return LODDistances; }

		// Returns LOD level based on distance
		uint32 GetLODByDistance(const f32 Distance);

		// Update Rendering Meshes Based on LOD
		void UpdateLOD(const uint32 lod);

		// Get Rendering Components
		static std::vector<IComponent*> &GetComponents();

		// Get Global Meshes
		static std::vector<RenderingMesh*> &GetRenderingMeshes(SceneGraph* scene);

		// Get Sorted Global Meshes
		static std::vector<RenderingMesh*> &GetRenderingMeshesSorted(SceneGraph* scene);

		// Get Global Meshes
		static std::vector<RenderingComponent*> &GetRenderingComponents(SceneGraph* scene);

		bool IsInstanced() { return isInstanced;  }

		// Real back-reference to whichever SkeletonAnimationInstance is
		// currently driving this component's skeleton - didn't exist
		// before (SkeletonAnimationInstance's constructor reads a
		// RenderingComponent's skeleton/meshes but never wrote anything
		// back), so nothing could ever ask "what animation is this mesh
		// playing" starting from the component itself; every example
		// instead keeps the instance as a sibling variable in user code.
		// Set automatically by SkeletonAnimationInstance's constructor
		// (SkeletonAnimation.cpp) - real, minimal, automatic association,
		// not a serializer-side shadow map. NULL if no skeleton animation
		// has ever been created against this component.
		void SetActiveSkeletonAnimation(void* instance) { activeSkeletonAnimation = instance; }
		void* GetActiveSkeletonAnimation() const { return activeSkeletonAnimation; }

		// Opt-in equivalent for texture animation - unlike skeleton
		// animation there is no constructor-time link anywhere in the
		// engine between a TextureAnimationInstance and any component/
		// material (every example manually pushes the current frame into
		// a material each Update()), so this is deliberately NOT set
		// automatically - a caller calls this once after creating the
		// instance if they want its playback state (not behavior -
		// nothing here auto-drives a material) to be capturable.
		void SetActiveTextureAnimation(void* instance) { activeTextureAnimation = instance; }
		void* GetActiveTextureAnimation() const { return activeTextureAnimation; }

	protected:

		// void* to avoid a circular #include (AnimationManager headers
		// already include this one) - callers cast back to
		// SkeletonAnimationInstance*/TextureAnimationInstance*.
		void* activeSkeletonAnimation = NULL;
		void* activeTextureAnimation = NULL;

		// Last frame index pushed onto the material, so the colormap is only
		// swapped when the frame actually changes rather than every tick.
		int32 lastAppliedTextureFrame = -1;

		// Save Renderable Pointer
		std::shared_ptr<Renderable> renderable;

		// Casting Shadows
		bool isCastingShadows;

		// List of Meshes of this Model
		std::map< uint32, std::vector<RenderingMesh*> > Meshes;

		// Skeleton
		std::map<StringID, Bone> skeleton;

		// Bones Flag
		bool hasBones;

		// Culling Geometry
		uint32 CullingGeometry;

		// Scene
		SceneGraph* Scene;

		// LOD
		bool LOD;

		// LOD Rendered
		uint32 LodInUse;
		float LastLodDistance;

		//LOD Distance
		std::vector<f32> LODDistances;

		// Culling
		bool cullTest;

		// INTERNAL - Components of this Type
		static std::vector<IComponent*> Components;

		// Instacing Flag
		bool isInstanced;

		// Render layer, see RenderLayer above.
		uint32 renderLayer;
	};

};

#endif /* RENDERINGCOMPONENT_H */
