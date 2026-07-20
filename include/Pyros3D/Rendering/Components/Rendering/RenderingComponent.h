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

	// Circular Dependency
	class PYROS3D_API RenderingComponent;

	class PYROS3D_API RenderingMesh {

	protected:
		// Owned only when built internally by BuildMaterials() - null (and
		// therefore a no-op to destroy) when Material points at a caller-
		// owned material instead.
		std::unique_ptr<IMaterial> InternalMaterial;

	public:

		RenderingMesh(const uint32 lod = 0) : drawingType(DrawingType::Triangles), CullingGeometry(0), Active(true), Clickable(true), LodLevel(lod) {} // Triangles by Default

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

		// Vulkan pipeline cache, keyed by shader program - mirrors VAOCache
		// exactly (see RenderObject()/BindMesh() in IRenderer.cpp). Only
		// ever populated on the Vulkan backend (GLRenderDevice::CreatePipeline()
		// exists too, but nothing calls it outside this cache - GL still
		// uses the individual SetCullFaceMode/SetBlendingEnabled/etc calls
		// directly). Built from Material's blend/depth/cull/wireframe state
		// at the moment this (mesh, shader) pair is first bound, not
		// re-evaluated per object the way GL's own dirty-tracked state is -
		// correct for any Material whose blend/depth/cull state doesn't
		// change after the fact for a given mesh/shader pairing (true for
		// RotatingCube, this backend's only validated target), not a
		// general solution.
		std::map<uint32, uint32> PipelineCache;

		// Materials
		IMaterial* Material;

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

		// Owned only for textures created internally by BuildMaterials().
		std::vector<std::unique_ptr<Texture>> Texturesvector;

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

			GenericShaderMaterial* genMat = new GenericShaderMaterial(options | MaterialOptions);

			// Material Properties
			if (Geometry->materialProperties.Twosided) genMat->SetCullFace(CullFace::DoubleSided);
			if (Geometry->materialProperties.haveColor) genMat->SetColor(Geometry->materialProperties.Color);
			if (Geometry->materialProperties.haveSpecular) genMat->SetSpecular(Geometry->materialProperties.Specular);
			if (Geometry->materialProperties.Opacity) genMat->SetOpacity(Geometry->materialProperties.Opacity);
			if (Geometry->materialProperties.haveColorMap)
			{
				Texture* colorMap = new Texture();
				colorMap->LoadTexture(Geometry->materialProperties.colorMap, TextureType::Texture);
				colorMap->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
				genMat->SetColorMap(colorMap);
				Texturesvector.emplace_back(colorMap);
			}
			if (Geometry->materialProperties.haveSpecularMap)
			{
				Texture* specularMap = new Texture();
				specularMap->LoadTexture(Geometry->materialProperties.specularMap, TextureType::Texture);
				specularMap->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
				genMat->SetSpecularMap(specularMap);
				Texturesvector.emplace_back(specularMap);
			}
			if (Geometry->materialProperties.haveNormalMap)
			{
				Texture* normalMap = new Texture();
				normalMap->LoadTexture(Geometry->materialProperties.normalMap, TextureType::Texture);
				normalMap->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
				genMat->SetNormalMap(normalMap);
				Texturesvector.emplace_back(normalMap);
			}
			InternalMaterial.reset(genMat);
			Material = genMat;
		}
	};

	class PYROS3D_API RenderingComponent : public IComponent {

		friend class IRenderer;

	public:

		RenderingComponent(Renderable* renderable, IMaterial* Material, const f32 Distance = 0.0f);
		RenderingComponent(Renderable* renderable, const uint32 MaterialOptions = 0, const f32 Distance = 0.0f);
		void AddLOD(Renderable* renderable, const f32 Distance, IMaterial* Material);
		void AddLOD(Renderable* renderable, const f32 Distance, const uint32 MaterialOptions = 0);

		virtual ~RenderingComponent();

		virtual void Register(SceneGraph* Scene);
		virtual void Init() {}
		virtual void Update(const f64 time = 0) {}
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene);

		void SetCullingGeometry(const uint32 Geometry);
		void EnableCullTest() { cullTest = true; }
		void DisableCullTest() { cullTest = false; }
		bool IsCullTesting() { return cullTest; }

		void EnableCastShadows();
		void DisableCastShadows();
		bool IsCastingShadows();

		Renderable* GetRenderable() { return renderable; }

		// Get Model Skeleton
		const std::map<StringID, Bone> &GetSkeleton() const { return skeleton; }
		bool HasBones() { return hasBones; }

		// Get Model's Meshes
		std::vector<RenderingMesh*> &GetMeshes(const uint32 LODLevel = 0);

		// Get LOD Number
		const uint32 GetLODSize() const;

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
	protected:

		// Save Renderable Pointer
		Renderable* renderable;

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
	};

};

#endif /* RENDERINGCOMPONENT_H */