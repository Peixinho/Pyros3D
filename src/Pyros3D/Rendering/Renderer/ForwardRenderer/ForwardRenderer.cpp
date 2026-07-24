//============================================================================
// Name        : ForwardRenderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Forward Renderer
//============================================================================

#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Other/PyrosGL.h>
namespace p3d {

	ForwardRenderer::ForwardRenderer(const uint32 Width, const uint32 Height, IRenderDevice* externalDevice) : IRenderer(Width, Height, externalDevice)
	{
		echo("SUCCESS: Forward Renderer Created");

		ActivateCulling(CullingMode::FrustumCulling);

		// Default View Port Init Values
		viewPortStartX = viewPortStartY = 0;
		viewPortEndX = viewPortEndY = 0;
	}

	ForwardRenderer::~ForwardRenderer() = default;

	void ForwardRenderer::RenderScene(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene)
	{
		InitRender();

		// Prepare and Pack Lights to Send to Shaders
		std::vector<Matrix> _Lights;

		if (lcomps.size() > 0)
		{
			uint32 pointCounter = 0;
			uint32 spotCounter = 0;
			for (std::vector<IComponent*>::iterator i = lcomps.begin(); i != lcomps.end(); i++)
			{
				switch (((ILightComponent*)(*i))->GetLightType())
				{
				case LIGHT_TYPE::DIRECTIONAL:
				{
					DirectionalLight* d = ((DirectionalLight*)(*i));

					// Directional Lights
					Vec4 color = d->GetLightColor();
					Vec3 position;
					Vec3 direction = (d->GetOwner()->GetWorldTransformation() * Vec4(d->GetLightDirection(), 0.f)).xyz().normalize();
					f32 attenuation = 1.f;
					Vec2 cones;
					int32 type = 1;

					Matrix directionalLight = Matrix();
					directionalLight.m[0] = color.x;         directionalLight.m[1] = color.y;             directionalLight.m[2] = color.z;             directionalLight.m[3] = color.w;
					directionalLight.m[4] = position.x;      directionalLight.m[5] = position.y;          directionalLight.m[6] = position.z;
					directionalLight.m[7] = direction.x;     directionalLight.m[8] = direction.y;         directionalLight.m[9] = direction.z;
					directionalLight.m[10] = 0.0f;			 directionalLight.m[11] = 0.0f;				  directionalLight.m[12] = 0.0f;
					directionalLight.m[13] = (f32)type;	  	 directionalLight.m[14] = d->GetShadowPCFTexelSize();  directionalLight.m[15] = (d->IsCastingShadows() ? 1.f : -1.f);

					_Lights.push_back(directionalLight);

					if (d->IsCastingShadows())
					{
						// Increase Number of Shadows
						NumberOfDirectionalShadows++;
					}
				}
				break;
				case LIGHT_TYPE::POINT:
				{
					PointLight* p = ((PointLight*)(*i));

					// Point Lights
					Vec4 color = p->GetLightColor();
					Vec3 position = (p->GetOwner()->GetWorldPosition());
					Vec3 direction;
					f32 attenuation = p->GetLightRadius();
					Vec2 cones;
					int32 type = 2;

					Matrix pointLight = Matrix();
					pointLight.m[0] = color.x;       pointLight.m[1] = color.y;           pointLight.m[2] = color.z;           pointLight.m[3] = color.w;
					pointLight.m[4] = position.x;    pointLight.m[5] = position.y;        pointLight.m[6] = position.z;
					pointLight.m[7] = direction.x;   pointLight.m[8] = direction.y;       pointLight.m[9] = direction.z;
					pointLight.m[10] = attenuation;  pointLight.m[11] = 0.f;			  pointLight.m[12] = 0.f;
					pointLight.m[13] = (f32)type;	 pointLight.m[14] = p->GetShadowPCFTexelSize(); pointLight.m[15] = -1.f;

					if (p->IsCastingShadows())
					{
						pointLight.m[14] = p->GetShadowPCFTexelSize();
						pointLight.m[15] = (f32)pointCounter++;
						NumberOfPointShadows++;
					}

					_Lights.push_back(pointLight);
				}
				break;
				case LIGHT_TYPE::SPOT:
				{
					SpotLight* s = ((SpotLight*)(*i));

					// Spot Lights
					Vec4 color = s->GetLightColor();
					Vec3 position = s->GetOwner()->GetWorldPosition();
					Vec3 direction = (s->GetOwner()->GetWorldTransformation() * Vec4(s->GetLightDirection(), 0.f)).xyz().normalize();
					f32 attenuation = s->GetLightRadius();
					Vec2 cones = Vec2(s->GetLightCosInnerCone(), s->GetLightCosOutterCone());
					int32 type = 3;

					Matrix spotLight = Matrix();
					spotLight.m[0] = color.x;        spotLight.m[1] = color.y;            spotLight.m[2] = color.z;            spotLight.m[3] = color.w;
					spotLight.m[4] = position.x;     spotLight.m[5] = position.y;         spotLight.m[6] = position.z;
					spotLight.m[7] = direction.x;    spotLight.m[8] = direction.y;        spotLight.m[9] = direction.z;
					spotLight.m[10] = attenuation;	 spotLight.m[11] = cones.x;			  spotLight.m[12] = cones.y;
					spotLight.m[13] = (f32)type;	 spotLight.m[15] = -1.f;

					if (s->IsCastingShadows())
					{
						spotLight.m[14] = s->GetShadowPCFTexelSize();
						spotLight.m[15] = (f32)spotCounter++;
						NumberOfSpotShadows++;
					}

					_Lights.push_back(spotLight);

				};

				// Universal Cache
				ProjectionMatrix = projection.m;
				NearFarPlane = Vec2(projection.Near, projection.Far);

				}
			}
		}

		// Save Time
		Timer = Scene->GetTime();

		// Save Values for Cache
		// Saves Scene
		this->Scene = Scene;

		// Saves Camera
		this->Camera = Camera;

		// Saves Projection
		this->projection = projection;

		// Universal Cache
		PrvProjectionMatrix = ProjectionMatrix;
		ProjectionMatrix = projection.m;
		NearFarPlane = Vec2(projection.Near, projection.Far);

		// View Matrix and Position
		ViewMatrix = Camera->GetWorldTransformation().Inverse();
		CameraPosition = Camera->GetWorldPosition();

		// Update Culling
		UpdateCulling(ProjectionMatrix*ViewMatrix);

		// Flags
		ViewMatrixInverseIsDirty = true;
		ProjectionMatrixInverseIsDirty = true;
		ViewProjectionMatrixIsDirty = true;

		// Set ViewPort
		if (viewPortEndX == 0 || viewPortEndY == 0)
		{
			viewPortEndX = Width;
			viewPortEndY = Height;
		}

		_SetViewPort(viewPortStartX, viewPortStartY, viewPortEndX, viewPortEndY);
		ClearDepthBuffer();

		// Scissor Test
		StartScissorTest();

		EndClippingPlanes();

		// Draw Background
		DrawBackground();

		// Real per-frame boundary - see the comment on
		// IRenderDevice::BeginFrame()/EndFrame(). No-op on GL; on Vulkan
		// this is the actual swapchain acquire + render pass begin (which
		// bakes in the clear DrawBackground() just configured via
		// SetClearColor() - must run after it, not before), paired with
		// EndFrame()'s submit + present at the end of this function.
		// Deliberately placed here (this function, called once per frame
		// by every example) and not inside the shared
		// IRenderer::EndRender()/PreRender(), which also run for a shadow
		// sub-pass that has no swapchain-framebuffer target at all - see
		// the interface comment for why that's out of scope for now.
		//
		// Gated on GetCurrentRenderTarget()==0 (the swapchain, not some
		// caller-bound offscreen FBO) - some examples (IslandDemo's water
		// reflection/refraction) call RenderScene() *multiple* times per
		// real frame, each wrapped in their own FrameBuffer::Bind()/UnBind()
		// around an offscreen render target. Calling BeginFrame()
		// unconditionally here (as before this check existed) would
		// acquire/begin the *swapchain*'s own frame on every one of those
		// calls too, hijacking activeCommandBuffer away from the offscreen
		// command buffer FrameBuffer::Bind() had just correctly set up -
		// found via a live regression (VUID-vkCmdDrawIndexed-renderPass-02684:
		// a pipeline correctly built against the reflection FBO's render
		// pass, but the actual draw landing in the swapchain's) once
		// color-attachment FBOs started working at all, exposing a call
		// pattern that was never reachable before.
		bool isMainSwapchainPass = device->GetCurrentRenderTarget() == 0;
		if (isMainSwapchainPass)
			device->BeginFrame();

		// Clear Screen - a no-op on Vulkan (BeginFrame() above already
		// cleared via the render pass's LOAD_OP_CLEAR).
		ClearScreen();

		// Render Scene with Objects Material
		for (std::vector<RenderingMesh*>::iterator i = rmesh.begin(); i != rmesh.end(); i++)
		{

			Lights.clear();
			if ((*i)->renderingComponent->GetOwner() != NULL)
			{
				// Culling Test
				bool cullingTest = false;
				switch ((*i)->CullingGeometry)
				{
				case CullingGeometry::Box:
					cullingTest = CullingBoxTest((*i), (*i)->renderingComponent->GetOwner());
					break;
				case CullingGeometry::Sphere:
				default:
					cullingTest = CullingSphereTest((*i), (*i)->renderingComponent->GetOwner());
					break;
				}
				if (!(*i)->renderingComponent->IsCullTesting()) cullingTest = true;
				if (cullingTest && (*i)->renderingComponent->IsActive() && (*i)->Active == true)
				{
					Vec3 objectPosition = (*i)->renderingComponent->GetOwner()->GetWorldPosition();
					for (std::vector<Matrix>::iterator _l = _Lights.begin(); _l != _Lights.end(); _l++)
					{
						if ((*_l).m[13] == 1) Lights.push_back(*_l);
						else if ((*_l).m[13] == 2 || (*_l).m[13] == 3)
						{
							Vec3 _lPos = Vec3((*_l).m[4], (*_l).m[5], (*_l).m[6]);
							if ((_lPos.distance(objectPosition) - ((*i)->renderingComponent->GetOwner()->GetBoundingSphereRadiusWorldSpace())) < (*_l).m[10])
								Lights.push_back(*_l);
						}
					}

					// PyrosShader.glsl only has room for MAX_LIGHTS (4); when
					// more lights than that are relevant to this object,
					// whatever's past the cap gets silently dropped further
					// down (SendGlobalUniforms's UBO upload clamps to
					// PYROS_MAX_LIGHTS). Sort nearest-first so it's the
					// *farthest* lights that get dropped, not an arbitrary
					// subset in scene-registration order. Directional lights
					// have no meaningful position/distance and are always
					// prioritized first.
					std::stable_sort(Lights.begin(), Lights.end(), [&objectPosition](const Matrix &a, const Matrix &b) {
						bool aDirectional = (a.m[13] == 1);
						bool bDirectional = (b.m[13] == 1);
						if (aDirectional != bDirectional) return aDirectional;
						if (aDirectional) return false;
						f32 aDistSQR = Vec3(a.m[4], a.m[5], a.m[6]).distanceSQR(objectPosition);
						f32 bDistSQR = Vec3(b.m[4], b.m[5], b.m[6]).distanceSQR(objectPosition);
						return aDistSQR < bDistSQR;
					});

					NumberOfLights = Lights.size();
					RenderObject((*i), (*i)->renderingComponent->GetOwner(), (*i)->Material);
				}
			}
		}

		// Disable Scissor Test
		EndScissorTest();

		// End Clipping Planes
		EndClippingPlanes();

		// End Rendering
		EndRender();

		// See the comment on BeginFrame() above.
		if (isMainSwapchainPass)
			device->EndFrame();
	}
};
