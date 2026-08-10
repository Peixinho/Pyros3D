//============================================================================
// Name        : IRenderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Renderer Interface
//============================================================================

#ifndef IRENDERER_H
#define IRENDERER_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/Materials/IMaterial.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingInstancedComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Lights/SpotLight/SpotLight.h>
#include <Pyros3D/Rendering/Components/Lights/PointLight/PointLight.h>
#include <Pyros3D/Rendering/Components/Lights/ILightComponent.h>
#include <Pyros3D/Rendering/Culling/FrustumCulling/FrustumCulling.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Other/Export.h>
#include <algorithm>
#include <memory>

namespace p3d {

	class PYROS3D_API IRenderer {

	public:

		IRenderer();
		// externalDevice, if non-NULL, is taken as this instance's device
		// instead of the default GLRenderDevice - lets a backend other than
		// GL (e.g. VulkanRenderDevice) be injected at construction time,
		// per the roadmap's "construction-time not compile-time backend
		// choice" design goal (VULKAN_ROADMAP.md). Also registered as the
		// process-wide active device (see SetActiveRenderDevice() in
		// IRenderDevice.h) so Shader/GeometryBuffer/RenderingComponent
		// resource creation elsewhere in the engine - constructed with no
		// IRenderer reference available - picks up the same backend.
		IRenderer(const uint32 Width, const uint32 Height, IRenderDevice* externalDevice = NULL);
		virtual ~IRenderer();
		void ClearBufferBit(const uint32 Option);

		// Depth Buffer
		void EnableClearDepthBuffer();
		void DisableClearDepthBuffer();
		void ClearDepthBuffer();

		// Clip Planes
		void EnableClipPlane(const uint32 &numberOfClipPlanes = 1);
		void DisableClipPlane();
		void SetClipPlane0(const Vec4 &clipPlane);
		void SetClipPlane1(const Vec4 &clipPlane);
		void SetClipPlane2(const Vec4 &clipPlane);
		void SetClipPlane3(const Vec4 &clipPlane);
		void SetClipPlane4(const Vec4 &clipPlane);
		void SetClipPlane5(const Vec4 &clipPlane);
		void SetClipPlane6(const Vec4 &clipPlane);
		void SetClipPlane7(const Vec4 &clipPlane);

		// Stencil
		void EnableStencil();
		void DisableStencil();
		void ClearStencilBuffer();
		void StencilFunction(const uint32 func, const uint32 ref = 0, const uint32 mask = 1);
		void StencilOperation(const uint32 sfail = StencilOp::Keep, const uint32 dpfail = StencilOp::Keep, const uint32 dppass = StencilOp::Keep);

		// Scissors Test
		void EnableScissorTest();
		void DisableScissorTest();
		void ScissorTestRect(const f32 x, const f32 y, const f32 width, const f32 height);

		// WireFrame
		void EnableWireFrame();
		void DisableWireFrame();

		// Color
		void ColorMask(const bool r, const bool g, const bool b, const bool a);

		// Sorting
		void EnableSorting();
		void DisableSorting();

		// LOD
		void EnableLOD() { lod = true; }
		void DisableLOD() { lod = false; }
		bool IsUsingLOD() { return lod; }

		void SetBackground(const Vec4 &Color);
		void UnsetBackground();
		void SetGlobalLight(const Vec4 &Light);
		void EnableDepthBias(const Vec2 &Bias);
		void DisableDepthBias();
		void SetViewPort(const uint32 initX, const uint32 initY, const uint32 endX, const uint32 endY);
		void ResetViewPort() { _viewPortStartX = _viewPortStartY = _viewPortEndX = _viewPortEndY = 0; } // Usefull for some shady stuff like rendering from different libs

		// Resize
		virtual void Resize(const uint32 &Width, const uint32 &Height);

		// culling
		void ActivateCulling(const uint32 cullingType);
		void DeactivateCulling();

		// Render Scene
		virtual void RenderScene(const p3d::Projection &projection, GameObject* Camera, SceneGraph* Scene);
		virtual void PreRender(GameObject* Camera, SceneGraph* Scene, const std::string &Tag);
		virtual void PreRender(GameObject* Camera, SceneGraph* Scene, const uint32 Tag);
		virtual void PreRender(GameObject* Camera, SceneGraph* Scene);

	protected:

		// Group by:
		//  -Asset and Material
		//  -Asset
		//  -Material
		//  -Owner
		// to avoid state changes - This should be done once
		// the rendering list is changed.
		// e.g. Add/Remove Models
		// Sort Assets (mostly the Translucent ones)
		virtual std::vector<RenderingMesh*> GroupAndSortAssets(SceneGraph* Scene, GameObject* Camera, const uint32 Tag = 0);

		// Reset
		void InitRender();

		// Render Object
		void RenderObject(RenderingMesh* rmesh, GameObject* owner, IMaterial* Material);

		// End Rendering
		void EndRender();

		// Depth Buffer
		void DepthTest(const uint32 test = DepthTest::Less);
		void DepthWrite();
		void ClearDepth();

		// Blending
		void EnableBlending();
		void DisableBlending();
		void BlendingFunction(const uint32 sfactor, const uint32 dfactor);
		void BlendingEquation(const uint32 mode);

		// Internal Function to Clear Buffers
		void ClearScreen();

		// Send Uniforms
		void SendGlobalUniforms(RenderingMesh* rmesh, IMaterial* Material);
		void SendUserUniforms(RenderingMesh* rmesh, IMaterial* Material);
		void SendModelUniforms(RenderingMesh* rmesh, IMaterial* Material);
		// See IMaterial.h's comment on extraUniformsBinding - a material
		// opting into this (any CustomShaderMaterial whose shader wraps its
		// own loose uniforms in a UBO, e.g. DeferredRenderer's second-pass
		// lighting materials) gets its current uniform values packed into
		// extraUniformsScratch and uploaded here, once per RenderObject()
		// call. No-op for every other material (extraUniformsBinding stays
		// 0 by default).
		void SendExtraUniforms(RenderingMesh* rmesh, IMaterial* Material);
		// Helper for SendExtraUniforms() - resolves one Uniform's current
		// value (the same per-DataUsage sources SendGlobalUniforms()/
		// SendModelUniforms() use, plus the generic Uniform::Value path)
		// and copies it into Material->extraUniformsScratch if its name is
		// registered in Material->extraUniformOffsets.
		void CaptureExtraUniform(IMaterial* Material, const Uniform &u);

		void StartClippingPlanes();
		void EndClippingPlanes();

		void StartScissorTest();
		void EndScissorTest();

		// Reset
		void Reset();

		// Flags
		uint32 bufferOptions, glBufferOptions;
		bool
			depthWritting;
		bool
			depthTesting;
		int32
			depthTestMode;
		bool
			clearDepthBuffer;
		bool
			sorting;

		// Background
		void DrawBackground();
		bool BackgroundColorSet;

		// Properties
		bool
			blending;

		int32
			sfactor,
			dfactor,
			mode;

		bool
			ClipPlane;
		uint32
			ClipPlaneNumber;
		Vec4
			ClipPlanes[8];

		Vec4
			BackgroundColor;
		Vec4
			GlobalLight;

		bool
			scissorTest;
		f32
			scissorTestX,
			scissorTestY,
			scissorTestWidth,
			scissorTestHeight;

		// Face Culling
		int32
			cullFace;
		bool
			cullFaceChanged;

		// Dimensions
		uint32
			Width,
			Height;

		// Scene
		SceneGraph*
			Scene;

		// Camera Matrix
		GameObject*
			Camera;

		// Projection Matrix
		Projection
			projection;

		// Depth Bias
		Vec2
			DepthBias;

		bool
			IsUsingDepthBias;

		uint32
			DrawType, InternalDrawType;

		// Culling
		bool CullingSphereTest(RenderingMesh* rmesh, GameObject* owner);
		bool CullingPointTest(RenderingMesh* rmesh, GameObject* owner);
		bool CullingBoxTest(RenderingMesh* rmesh, GameObject* owner);
		void UpdateCulling(const Matrix &ViewProjectionMatrix);
		bool
			IsCulling;
		// Owned whenever IsCulling is true; ActivateCulling()/DeactivateCulling()
		// always go through .reset() so re-activating or repeated deactivation
		// can't leak or double-free the previous FrustumCulling.
		std::unique_ptr<FrustumCulling>
			culling;

		// Backend seam for the state/bind/draw calls this class used to
		// issue directly against OpenGL - see IRenderDevice.h. One per
		// instance (GLRenderDevice itself holds no state, so this is cheap);
		// only the shared UBO handles below are refcounted/static.
		// MaybeOwningDevicePtr (not a plain unique_ptr<IRenderDevice>) so
		// this can sometimes *borrow* an already-active device instead of
		// always owning one - see IRenderDevice.h's comment on
		// MaybeOwningDeviceDeleter/IsActiveRenderDeviceSet().
		MaybeOwningDevicePtr
			device;

		// True only for instances built via IRenderer(Width, Height) - the
		// no-arg IRenderer() used by DebugRenderer never touches the shared
		// UBOs below, so it must not increment SharedUBORefCount either;
		// this flag is what tells ~IRenderer() whether it owes a decrement.
		bool UsesSharedUBOs;

		// GL binding points (0-4) are context-global, not per-object, so
		// these UBOs - and the dirty-tracking cache of what's currently
		// uploaded to each - are shared/static across every IRenderer
		// instance rather than per-instance. Per-instance UBOs each calling
		// glBindBufferBase(..., 0, ownUBO) in their own constructor would
		// mean the most-recently-constructed instance silently steals
		// binding point 0 (etc.) from every earlier one - which happens for
		// real whenever two renderers coexist, e.g. PickingPainterMethod's
		// ForwardRenderer + PainterPick. Reference-counted: created when
		// the first instance is constructed, destroyed when the last one
		// (of any IRenderer-derived type) is destroyed.
		static uint32 SharedUBORefCount;

		// GL Uniform Buffer Object holding uProjectionMatrix + uViewMatrix
		// (std140: 2 mat4, 128 bytes), bound once to binding point 0 and
		// re-uploaded via glBufferSubData in SendGlobalUniforms() instead of
		// resending both as individual glUniform calls on every mesh/material
		// switch.
		static uint32 GlobalMatricesUBO;

		// Same idea for PyrosShader.glsl's uLights[MAX_LIGHTS] array (bound
		// to binding point 1). Sized/uploaded for up to PYROS_MAX_LIGHTS
		// entries - keep in sync with PyrosShader.glsl's own MAX_LIGHTS.
		static uint32 LightsUBO;

		// Same idea for the shadow-casting matrix arrays (DirectionalShadowBlock/
		// PointShadowBlock/SpotShadowBlock, binding points 2/3/4). Each is
		// sized to match its shader-side array declaration exactly - see
		// the PYROS_MAX_*_SHADOWS #defines in IRenderer.cpp.
		static uint32 DirectionalShadowUBO;
		static uint32 PointShadowUBO;
		static uint32 SpotShadowUBO;

		// UBOs for what used to be "loose" (non-block) uniforms in
		// PyrosShader.glsl - uModelMatrix/uCameraPos/uOpacity/etc - moved
		// into fixed-binding blocks (binding points 16-23, see the BIND_*
		// macros in that file) so the shader also compiles for Vulkan/SPIR-V,
		// which rejects non-opaque uniforms outside a block outright. Only
		// used for materials where Material->SupportsUniformBlocks() is
		// true (GenericShaderMaterial - see IMaterial.h); other materials'
		// shaders (CustomShaderMaterial) keep declaring these as plain
		// uniforms and keep receiving them via the pre-existing individual
		// Shader::SendUniform() path, unaffected by any of this. Same
		// refcounted/shared lifecycle as the UBOs above.
		static uint32 VertexFrameUniformsUBO;
		static uint32 VelocityFrameUniformsUBO;
		static uint32 ObjectMatrixUniformsUBO;
		static uint32 BoneMatricesUBO;
		static uint32 VelocityObjectUniformsUBO;
		static uint32 AmbientLightUniformsUBO;
		static uint32 MaterialUniformsUBO;
		// Split out of MaterialUniformsUBO: uNumberOfLights/etc are
		// per-object (each object gets its own nearby-lights count from
		// the renderer's light-culling loop), not per-material, so they
		// can't be gated on material-change the way the rest of
		// MaterialUniforms now is - see SendModelUniforms()/SendUserUniforms().
		static uint32 ObjectLightCountsUBO;

		// Last-uploaded contents of each UBO above, compared byte-for-byte
		// in SendGlobalUniforms() to skip the glBufferSubData call (and the
		// GPU upload/driver round-trip it costs) when the source data
		// hasn't actually changed since the last mesh/material switch -
		// which for the shadow matrices in particular (computed once per
		// RenderScene() call, not per object) is most of the time. Static
		// for the same reason the UBOs themselves are: this cache describes
		// the one shared buffer's actual GPU-side contents, not anything
		// instance-local, so a per-instance cache could wrongly believe its
		// last upload is still current when a different renderer instance
		// has since overwritten the shared buffer with different data. The
		// *Valid flags start false to force the first upload unconditionally,
		// since there's nothing meaningful to compare against yet.
		static bool GlobalMatricesUBOValid;
		static Matrix CachedProjectionMatrix, CachedViewMatrix;
		// Part of the same dirty-check as CachedProjectionMatrix/
		// CachedViewMatrix above - RenderingPointShadowFace changes which
		// *translation* of an otherwise-possibly-identical ProjectionMatrix
		// gets uploaded (see SendGlobalUniforms()), so it has to be part of
		// what "unchanged" means too, not just the untranslated source matrices.
		static bool CachedRenderingPointShadowFace;
		static bool LightsUBOValid;
		static std::vector<Matrix> CachedLights;
		static bool DirectionalShadowUBOValid;
		static std::vector<Matrix> CachedDirectionalShadowMatrix;
		static Vec4 CachedDirectionalShadowFar;
		static bool PointShadowUBOValid;
		static std::vector<Matrix> CachedPointShadowMatrix;
		static bool SpotShadowUBOValid;
		static std::vector<Matrix> CachedSpotShadowMatrix;

		// Dirty-tracking for the per-frame UBOs above (VertexFrameUniforms/
		// AmbientLightUniforms/VelocityFrameUniforms) - same
		// skip-if-unchanged reasoning as GlobalMatricesUBOValid. The
		// per-object ones (ObjectMatrixUniforms/BoneMatrices/
		// VelocityObjectUniforms/MaterialUniforms) aren't cached here: their
		// source data (ModelMatrix, skinning bones, material scalars)
		// changes on essentially every RenderObject() call anyway - same as
		// how SendModelUniforms()/SendUserUniforms() already resend their
		// individual uniforms unconditionally every call, no dirty-check.
		static bool VertexFrameUniformsUBOValid;
		static Vec3 CachedCameraPosition;
		static bool CachedClipPlaneEnabled;
		static Vec4 CachedClipPlane0;
		static bool AmbientLightUniformsUBOValid;
		static Vec4 CachedGlobalLight;
		static bool VelocityFrameUniformsUBOValid;
		static Matrix CachedPrvProjectionMatrix, CachedPrvViewMatrix;

		// Universal Uniforms Cache
		Matrix
			ProjectionMatrix,
			ViewMatrix,
			ViewProjectionMatrix,
			ProjectionMatrixInverse,
			ViewMatrixInverse,
			PrvProjectionMatrix,
			PrvViewMatrix;
		bool
			ProjectionMatrixInverseIsDirty,
			ViewMatrixInverseIsDirty,
			ViewProjectionMatrixIsDirty;

		// Set for the duration of a point light's 6-face shadow cubemap
		// render loop (PreRender()), read by SendGlobalUniforms() to skip
		// device->TranslateProjectionMatrix()'s Y-flip for those draws
		// only - see the comment on that method in IRenderDevice.h for
		// why a cubemap face specifically must not get it. False the rest
		// of the time (every other pass - main camera, directional/spot
		// shadows - needs the normal flip).
		bool RenderingPointShadowFace;

		Vec3
			CameraPosition;

		Vec2
			NearFarPlane;

		std::vector<Matrix>
			Lights;

		uint32
			NumberOfLights;

		f64
			Timer;

		// Model Specific Cache
		Matrix
			ModelMatrix,
			PrvModelMatrix,
			NormalMatrix,
			ModelViewMatrix,
			ModelViewProjectionMatrix,
			ViewProjectionMatrixInverse,
			ModelViewProjectionMatrixInverse,
			ModelMatrixInverse,
			ModelViewMatrixInverse,
			ModelMatrixInverseTranspose;
		bool
			NormalMatrixIsDirty,
			ModelViewMatrixIsDirty,
			ModelViewProjectionMatrixIsDirty,
			ModelMatrixInverseIsDirty,
			ModelViewMatrixInverseIsDirty,
			ModelMatrixInverseTransposeIsDirty,
			ModelViewProjectionMatrixInverseIsDirty,
			ViewProjectionMatrixInverseIsDirty;
		bool
			ProjectionMatrixDirty,
			ViewMatrixDirty;

		// Shadow Casting
		std::vector<Texture*>
			DirectionalShadowMapsTextures, 
			PointShadowMapsTextures, 
			SpotShadowMapsTextures;
		std::vector<uint32>
			DirectionalShadowMapsUnits, 
			PointShadowMapsUnits, 
			SpotShadowMapsUnits;
		std::vector<Matrix>
			DirectionalShadowMatrix, 
			PointShadowMatrix, 
			SpotShadowMatrix;
		Vec4
			DirectionalShadowFar;
		uint32
			NumberOfDirectionalShadows, 
			NumberOfPointShadows, 
			NumberOfSpotShadows;

		// ViewPort Size
		uint32              
			viewPortStartX,
			viewPortStartY,
			viewPortEndX,
			viewPortEndY;
		bool
			customViewPort;

		bool
			lod;

		// Internal ViewPort Dimension
		void _SetViewPort(const uint32 initX, const uint32 initY, const uint32 endX, const uint32 endY);

		std::vector<RenderingMesh*>
			rmesh;
		std::vector<IComponent*>
			lcomps;

	private:

		void BindMesh(RenderingMesh* rmesh, IMaterial* material);
		void BindShadowMaps(IMaterial* material);
		void UnbindShadowMaps(IMaterial* material);

		// The shadow pass overrides every caster's own material with one of
		// these, so each vertex-shader variant a caster can need has to
		// exist here - the override is picked purely from the mesh, by
		// PickShadowMaterial() below.
		//
		// shadowInstancedMaterial is why that's now a function rather than
		// the inline `SkinningBones.size() > 0 ? ... : ...` ternary it used
		// to be at all three call sites. RenderObject() issues a real
		// DrawElementsInstanced() for any mesh whose component
		// IsInstanced(), whatever material is bound - but shadowMaterial
		// has no ShaderUsage::InstancedRendering, so its shader never
		// declares aInstancedTransform, BindMesh()'s attribute lookup
		// returned -1 for it, and the divisor setup was skipped: every
		// instance drew at the component's own uModelMatrix. An instanced
		// mesh cast N identical shadows stacked in one spot instead of one
		// per instance.
		//
		// There is deliberately no skinned+instanced variant: per-instance
		// bones would need a per-instance bones UBO, which this engine
		// doesn't have, so that combination can't be rendered correctly in
		// the main pass either. PickShadowMaterial() takes instanced first
		// on the grounds that a wrong-but-placed shadow beats N shadows in
		// a heap.
		//
		// The AlphaTest pair exists because a cutout caster is only opaque
		// where its colormap's alpha says so, and a shadow material that
		// doesn't sample that map casts the caster's whole quad. Grass
		// blades shadowed as solid rectangles is the visible version of
		// that. These carry ShaderUsage::Texture too, since the discard
		// needs the map; PickShadowMaterial() lends it the caster's own
		// colormap and cutoff per draw, which is what lets one shared
		// override material stand in for casters that don't share a
		// texture. PyrosShader.glsl already runs its ALPHATEST discard
		// ahead of the CASTSHADOWS depth write, so no shader change was
		// needed for this - only a variant that reaches it.
		GenericShaderMaterial
			*shadowMaterial,
			*shadowSkinnedMaterial,
			*shadowInstancedMaterial,
			*shadowAlphaTestMaterial,
			*shadowInstancedAlphaTestMaterial;

		// Picks the shadow-pass override material for one caster.
		GenericShaderMaterial* PickShadowMaterial(RenderingMesh* mesh);

		// True for exactly the materials PickShadowMaterial() can return.
		// Single source of truth for "is this draw part of a shadow pass",
		// which RenderObject() needs for PipelineDesc::isShadowPass - that
		// flag decides which render pass the pipeline is built against on
		// Vulkan/Metal, so a shadow material missing from this list builds
		// against the wrong one.
		bool IsShadowMaterial(IMaterial* material) const;

		// Applies this light's shadow depth bias to *every* shadow material,
		// not just shadowMaterial. Only shadowMaterial was getting it
		// before, so a skinned caster shadowed with no polygon offset at
		// all - the z-fighting/acne the bias exists to prevent, on exactly
		// the animated characters most likely to show it. Adding
		// shadowInstancedMaterial to the same blind spot is what turned
		// this from someone else's bug into one this change would own.
		void SetShadowDepthBias(const f32 factor, const f32 units);

		// Last Mesh Rendered
		int32
			LastMeshRendered;
		RenderingMesh*
			LastMeshRenderedPTR;

		// Last Material Used
		int32
			LastMaterialUsed,
			LastProgramUsed;
		IMaterial*
			LastMaterialPTR;

		// Internal ViewPort Dimension
		static uint32
			_viewPortStartX;
		static uint32
			_viewPortStartY;
		static uint32
			_viewPortEndX;
		static uint32
			_viewPortEndY;
	};

};

#endif /* IRENDERER_H */
