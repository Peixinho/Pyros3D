//============================================================================
// Name        : DeferredRenderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Deferred Renderer
//============================================================================

#include <Pyros3D/Rendering/Renderer/DeferredRenderer/DeferredRenderer.h>
#include <Pyros3D/Other/PyrosGL.h>

namespace p3d {

	f32 f(f32 r)
	{
		return r * (2.f * (tanf((f32)PI / 4.f)));
	}
	f32 g(f32 a)
	{
		return a / (2.f*sinf((f32)PI / 4.f));
	}

	DeferredRenderer::DeferredRenderer(const uint32 Width, const uint32 Height, FrameBuffer* fbo) : IRenderer(Width, Height)
	{

		echo("SUCCESS: Deferred Renderer Created");

		ActivateCulling(CullingMode::FrustumCulling);

		shadowMaterial = new GenericShaderMaterial(ShaderUsage::CastShadows);
		shadowMaterial->SetCullFace(CullFace::DoubleSided);
		shadowSkinnedMaterial = new GenericShaderMaterial(ShaderUsage::CastShadows | ShaderUsage::Skinning);
		shadowSkinnedMaterial->SetCullFace(CullFace::DoubleSided);

		// Default View Port Init Values
		viewPortStartX = viewPortStartY = 0;

		// Save FrameBuffer
		FBO = fbo;

		// Create Second Pass Specifics        
		deferredMaterialAmbient= new CustomShaderMaterial("../examples/DeferredRendering/assets/shaders/secondpassAmbient.glsl");
		deferredMaterialDirectional = new CustomShaderMaterial("../examples/DeferredRendering/assets/shaders/secondpassDirectional.glsl");
		deferredMaterialPoint = new CustomShaderMaterial("../examples/DeferredRendering/assets/shaders/secondpassPoint.glsl");
		deferredMaterialSpot = new CustomShaderMaterial("../examples/DeferredRendering/assets/shaders/secondpassSpot.glsl");

		uint32 texID = 0;
		deferredMaterialAmbient->AddUniform(Uniform("tDepth", Uniforms::DataType::Int, &texID));
		deferredMaterialDirectional->AddUniform(Uniform("tDepth", Uniforms::DataType::Int, &texID));
		deferredMaterialPoint->AddUniform(Uniform("tDepth", Uniforms::DataType::Int, &texID));
		deferredMaterialSpot->AddUniform(Uniform("tDepth", Uniforms::DataType::Int, &texID));
		texID = 1;
		deferredMaterialAmbient->AddUniform(Uniform("tDiffuse", Uniforms::DataType::Int, &texID));
		deferredMaterialDirectional->AddUniform(Uniform("tDiffuse", Uniforms::DataType::Int, &texID));
		deferredMaterialPoint->AddUniform(Uniform("tDiffuse", Uniforms::DataType::Int, &texID));
		deferredMaterialSpot->AddUniform(Uniform("tDiffuse", Uniforms::DataType::Int, &texID));
		texID = 2;
		deferredMaterialAmbient->AddUniform(Uniform("tSpecular", Uniforms::DataType::Int, &texID));
		deferredMaterialDirectional->AddUniform(Uniform("tSpecular", Uniforms::DataType::Int, &texID));
		deferredMaterialPoint->AddUniform(Uniform("tDiffuse", Uniforms::DataType::Int, &texID));
		deferredMaterialSpot->AddUniform(Uniform("tDiffuse", Uniforms::DataType::Int, &texID));
		texID = 3;
		deferredMaterialAmbient->AddUniform(Uniform("tNormal", Uniforms::DataType::Int, &texID));
		deferredMaterialDirectional->AddUniform(Uniform("tNormal", Uniforms::DataType::Int, &texID));
		deferredMaterialPoint->AddUniform(Uniform("tNormal", Uniforms::DataType::Int, &texID));
		deferredMaterialSpot->AddUniform(Uniform("tNormal", Uniforms::DataType::Int, &texID));

		deferredMaterialAmbient->AddUniform(Uniform("uScreenDimensions", Uniforms::DataUsage::ScreenDimensions));
		deferredMaterialAmbient->AddUniform(Uniform("uMatProj", Uniforms::DataUsage::ProjectionMatrix));

		deferredMaterialDirectional->AddUniform(Uniform("uScreenDimensions", Uniforms::DataUsage::ScreenDimensions));
		dirDirHandle = deferredMaterialDirectional->AddUniform(Uniform("uLightDirection", Uniforms::DataUsage::Other, Uniforms::DataType::Vec3));
		dirColorHandle = deferredMaterialDirectional->AddUniform(Uniform("uLightColor", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
		dirShadowHandle = deferredMaterialDirectional->AddUniform(Uniform("uShadowMap", Uniforms::DataUsage::Other, Uniforms::DataType::Int));
		dirShadowPCFTexelHandle = deferredMaterialDirectional->AddUniform(Uniform("uPCFTexelSize", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
		dirShadowDepthsMVPHandle = deferredMaterialDirectional->AddUniform(Uniform("uDirectionalDepthsMVP", Uniforms::DataUsage::Other, Uniforms::DataType::Matrix));
		dirShadowFarHandle = deferredMaterialDirectional->AddUniform(Uniform("uDirectionalShadowFar", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
		dirHaveShadowHandle = deferredMaterialDirectional->AddUniform(Uniform("uHaveShadowmap", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		deferredMaterialDirectional->AddUniform(Uniform("uMatProj", Uniforms::DataUsage::ProjectionMatrix));
		deferredMaterialDirectional->AddUniform(Uniform("uNearFar", Uniforms::DataUsage::NearFarPlane));
		deferredMaterialDirectional->DisableDepthTest();
		deferredMaterialDirectional->DisableDepthWrite();
		deferredMaterialDirectional->EnableBlending();
		deferredMaterialDirectional->BlendingEquation(BlendEq::Add);
		deferredMaterialDirectional->BlendingFunction(BlendFunc::One, BlendFunc::One);

		deferredMaterialPoint->AddUniform(Uniform("uScreenDimensions", Uniforms::DataUsage::ScreenDimensions));
		pointPosHandle = deferredMaterialPoint->AddUniform(Uniform("uLightPosition", Uniforms::DataUsage::Other, Uniforms::DataType::Vec3));
		pointRadiusHandle = deferredMaterialPoint->AddUniform(Uniform("uLightRadius", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		pointColorHandle = deferredMaterialPoint->AddUniform(Uniform("uLightColor", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
		pointShadowHandle = deferredMaterialPoint->AddUniform(Uniform("uShadowMap", Uniforms::DataUsage::Other, Uniforms::DataType::Int));
		pointShadowPCFTexelHandle = deferredMaterialPoint->AddUniform(Uniform("uPCFTexelSize", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
		pointShadowDepthsMVPHandle = deferredMaterialPoint->AddUniform(Uniform("uPointDepthsMVP", Uniforms::DataUsage::Other, Uniforms::DataType::Matrix));
		pointHaveShadowHandle = deferredMaterialPoint->AddUniform(Uniform("uHaveShadowmap", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		deferredMaterialPoint->AddUniform(Uniform("uModelMatrix", Uniforms::DataUsage::ModelMatrix));
		deferredMaterialPoint->AddUniform(Uniform("uViewMatrix", Uniforms::DataUsage::ViewMatrix));
		deferredMaterialPoint->AddUniform(Uniform("uProjectionMatrix", Uniforms::DataUsage::ProjectionMatrix));
		deferredMaterialPoint->AddUniform(Uniform("uMatProj", Uniforms::DataUsage::ProjectionMatrix));
		deferredMaterialPoint->AddUniform(Uniform("uNearFar", Uniforms::DataUsage::NearFarPlane));
		deferredMaterialPoint->SetCullFace(CullFace::FrontFace);
		deferredMaterialPoint->DisableDepthTest();
		deferredMaterialPoint->DisableDepthWrite();
		deferredMaterialPoint->EnableBlending();
		deferredMaterialPoint->BlendingEquation(BlendEq::Add);
		deferredMaterialPoint->BlendingFunction(BlendFunc::One, BlendFunc::One);

		spotPosHandle = deferredMaterialSpot->AddUniform(Uniform("uLightPosition", Uniforms::DataUsage::Other, Uniforms::DataType::Vec3));
		spotDirHandle = deferredMaterialSpot->AddUniform(Uniform("uLightDirection", Uniforms::DataUsage::Other, Uniforms::DataType::Vec3));
		spotRadiusHandle = deferredMaterialSpot->AddUniform(Uniform("uLightRadius", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		spotOutterHandle = deferredMaterialSpot->AddUniform(Uniform("uOutterCone", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		spotInnerHandle = deferredMaterialSpot->AddUniform(Uniform("uInnerCone", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		spotColorHandle = deferredMaterialSpot->AddUniform(Uniform("uLightColor", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
		spotShadowHandle = deferredMaterialSpot->AddUniform(Uniform("uShadowMap", Uniforms::DataUsage::Other, Uniforms::DataType::Int));
		spotShadowPCFTexelHandle = deferredMaterialSpot->AddUniform(Uniform("uPCFTexelSize", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
		spotShadowDepthsMVPHandle = deferredMaterialSpot->AddUniform(Uniform("uSpotDepthsMVP", Uniforms::DataUsage::Other, Uniforms::DataType::Matrix));
		spotHaveShadowHandle = deferredMaterialSpot->AddUniform(Uniform("uHaveShadowmap", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		deferredMaterialSpot->AddUniform(Uniform("uScreenDimensions", Uniforms::DataUsage::ScreenDimensions));
		deferredMaterialSpot->AddUniform(Uniform("uModelMatrix", Uniforms::DataUsage::ModelMatrix));
		deferredMaterialSpot->AddUniform(Uniform("uViewMatrix", Uniforms::DataUsage::ViewMatrix));
		deferredMaterialSpot->AddUniform(Uniform("uProjectionMatrix", Uniforms::DataUsage::ProjectionMatrix));
		deferredMaterialSpot->AddUniform(Uniform("uMatProj", Uniforms::DataUsage::ProjectionMatrix));
		deferredMaterialSpot->AddUniform(Uniform("uNearFar", Uniforms::DataUsage::NearFarPlane));
		deferredMaterialSpot->SetCullFace(CullFace::FrontFace);
		deferredMaterialSpot->DisableDepthTest();
		deferredMaterialSpot->DisableDepthWrite();
		deferredMaterialSpot->EnableBlending();
		deferredMaterialSpot->BlendingEquation(BlendEq::Add);
		deferredMaterialSpot->BlendingFunction(BlendFunc::One, BlendFunc::One);

		// Light Volume
		quadHandle = new Plane(1, 1);
		directionalLight = new RenderingComponent(quadHandle);

		sphereHandle = new Sphere(1, 6, 4);
		pointLight = new RenderingComponent(sphereHandle);
		pointLight->GetMeshes()[0]->Material->SetCullFace(CullFace::FrontFace);
	}

	void DeferredRenderer::Resize(const uint32 Width, const uint32 Height)
	{
		IRenderer::Resize(Width, Height);
	}

	DeferredRenderer::~DeferredRenderer()
	{
		if (IsCulling)
		{
			delete culling;
		}

		delete shadowMaterial;
		delete sphereHandle;
		delete deferredMaterialAmbient;
		delete deferredMaterialDirectional;
		delete deferredMaterialPoint;
		delete deferredMaterialSpot;
	}

	void DeferredRenderer::RenderScene(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene, const uint32 BufferOptions)
	{

		// Initialize Renderer
		InitRender();

		// Group and Sort Meshes
		std::vector<RenderingMesh*> rmesh = GroupAndSortAssets(Scene, Camera);

		// Get Lights List
		std::vector<IComponent*> lcomps = ILightComponent::GetLightsOnScene(Scene);

		// Save Time
		Timer = Scene->GetTime();

		// First Pass

		// Save Values for Cache
		// Saves Scene
		this->Scene = Scene;

		// Saves Camera
		this->Camera = Camera;
		this->CameraPosition = this->Camera->GetWorldPosition();

		// Saves Projection
		this->projection = projection;

		// Universal Cache
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

		// Bind Frame Buffer
		FBO->Bind();

		// Set ViewPort
		viewPortEndX = Width;
		viewPortEndY = Height;
		// Set Viewport
		_SetViewPort(viewPortStartX, viewPortStartY, viewPortEndX, viewPortEndY);
		ClearBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth);
		ClearDepthBuffer();
		ClearScreen();

		// Draw Background
		DrawBackground();

		// Render Scene with Objects Material
		for (std::vector<RenderingMesh*>::iterator i = rmesh.begin(); i != rmesh.end(); i++)
		{

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
				if (cullingTest && (*i)->renderingComponent->IsActive() && (*i)->Active == true)
					RenderObject((*i), (*i)->renderingComponent->GetOwner(), (*i)->Material);
			}
		}

		// End Rendering
		EndRender();

		// Unbind FrameBuffer
		FBO->UnBind();

		// Initialize Rendering
		InitRender();

		ClearBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth);
		ClearScreen();

		// Bind FBO Textures
		for (int i = 0;i<FBO->GetAttachments().size();i++)
			FBO->GetAttachments()[i]->TexturePTR->Bind();

		// Ambient
		{
			// Directional Lights
			GameObject go = GameObject();
			RenderObject(directionalLight->GetMeshes()[0], &go, deferredMaterialAmbient);
		}
		// End Ambient


		uint32 numberDir = 0, numberPoint = 0, numberSpot = 0;
		// Render Point Lights
		for (std::vector<IComponent*>::iterator i = lcomps.begin(); i != lcomps.end(); i++)
		{

			if ((*i)->GetOwner() != NULL)
			{
				switch (((ILightComponent*)(*i))->GetLightType())
				{
				case LIGHT_TYPE::POINT:
				{
					PointLight* p = (PointLight*)(*i);
					// Point Lights
					Vec3 pos = (ViewMatrix * Vec4(p->GetOwner()->GetWorldPosition(), 1.f)).xyz();					
					pointPosHandle->SetValue(&pos);
					pointRadiusHandle->SetValue((void*)&p->GetLightRadius());
					pointColorHandle->SetValue((void*)&p->GetLightColor());
					int i = -1;
					float haveShadow = 0.f;
					if (p->IsCastingShadows())
					{
						Vec4 txl;
						txl.x = 0.0001f;
						Matrix mvp[2];
						mvp[0] = PointShadowMatrix[numberPoint];
						mvp[1] = PointShadowMatrix[numberPoint+1];
						pointShadowPCFTexelHandle->SetValue(&txl);
						pointShadowDepthsMVPHandle->SetValue(&mvp, 2);
						p->GetShadowMapTexture()->Bind();
						i = Texture::GetLastBindedUnit();
						haveShadow = 1.f;
					}
					pointShadowHandle->SetValue(&i);
					pointHaveShadowHandle->SetValue(&haveShadow);

					// Set Scale
					f32 sc = g(f(p->GetLightRadius()));
					Matrix m; m.Scale(sc, sc, sc);
					pointLight->GetMeshes()[0]->Pivot = m;
					RenderObject(pointLight->GetMeshes()[0], p->GetOwner(), deferredMaterialPoint);

					if (p->IsCastingShadows())
					{
						p->GetShadowMapTexture()->Unbind();
						numberPoint++;
					}
				}
				break;
				case LIGHT_TYPE::SPOT:
				{
					SpotLight* s = (SpotLight*)(*i);
					// Spot Lights
					Vec3 pos = (ViewMatrix * Vec4(s->GetOwner()->GetWorldPosition(), 1.f)).xyz();
					Vec3 dir = (ViewMatrix * (s->GetOwner()->GetWorldTransformation() * Vec4(s->GetLightDirection(), 0.f))).xyz();
					spotPosHandle->SetValue(&pos);
					spotDirHandle->SetValue(&dir);
					spotRadiusHandle->SetValue((void*)&s->GetLightRadius());
					spotOutterHandle->SetValue((void*)&s->GetLightCosOutterCone());
					spotInnerHandle->SetValue((void*)&s->GetLightCosInnerCone());
					spotColorHandle->SetValue((void*)&s->GetLightColor());

					int i = -1;
					float haveShadow = 0.f;
					if (s->IsCastingShadows())
					{
						Vec4 txl;
						txl.x = 0.0001f;
						Matrix mvp = SpotShadowMatrix[numberDir];
						spotShadowPCFTexelHandle->SetValue(&txl);
						spotShadowDepthsMVPHandle->SetValue(&mvp);
						s->GetShadowMapTexture()->Bind();
						i = Texture::GetLastBindedUnit();
						haveShadow = 1.f;
					}
					spotShadowHandle->SetValue(&i);
					spotHaveShadowHandle->SetValue(&haveShadow);

					// Set Scale
					f32 sc = g(f(s->GetLightRadius()));
					Matrix m; m.Scale(sc, sc, sc);
					pointLight->GetMeshes()[0]->Pivot = m;
					RenderObject(pointLight->GetMeshes()[0], s->GetOwner(), deferredMaterialSpot);

					if (s->IsCastingShadows())
					{
						s->GetShadowMapTexture()->Unbind();
						numberSpot++;
					}
				}
				break;
				case LIGHT_TYPE::DIRECTIONAL:
				{
					DirectionalLight* d = (DirectionalLight*)(*i);
					// Directional Lights
					Vec3 dir = (ViewMatrix * (d->GetOwner()->GetWorldTransformation() * Vec4(d->GetLightDirection(), 0.f))).xyz().normalize();
					dirDirHandle->SetValue(&dir);
					dirColorHandle->SetValue((void*)&d->GetLightColor());
					int i = -1;
					float haveShadow = 0.f;
					if (d->IsCastingShadows())
					{
						Vec4 txl;
						txl.x = 0.0001f;
						txl.y = 0.0001f;
						txl.z = 0.0001f;
						txl.w = 0.0001f;

						Vec4 _ShadowFar;
						if (d->GetNumberCascades() > 0) _ShadowFar.x = d->GetCascade(0).Far;
						if (d->GetNumberCascades() > 1) _ShadowFar.y = d->GetCascade(1).Far;
						if (d->GetNumberCascades() > 2) _ShadowFar.z = d->GetCascade(2).Far;
						if (d->GetNumberCascades() > 3) _ShadowFar.w = d->GetCascade(3).Far;

						Vec4 ShadowFar;
						ShadowFar.x = 0.5f*(-_ShadowFar.x*projection.m.m[10] + projection.m.m[14]) / _ShadowFar.x + 0.5f;
						ShadowFar.y = 0.5f*(-_ShadowFar.y*projection.m.m[10] + projection.m.m[14]) / _ShadowFar.y + 0.5f;
						ShadowFar.z = 0.5f*(-_ShadowFar.z*projection.m.m[10] + projection.m.m[14]) / _ShadowFar.z + 0.5f;
						ShadowFar.w = 0.5f*(-_ShadowFar.w*projection.m.m[10] + projection.m.m[14]) / _ShadowFar.w + 0.5f;

						std::vector<Matrix> mvp;
						for (int j = 0; j < d->GetNumberCascades(); j++)
							mvp.push_back(DirectionalShadowMatrix[numberDir+j]);

						dirShadowPCFTexelHandle->SetValue(&txl);
						dirShadowDepthsMVPHandle->SetValue(&mvp[0], d->GetNumberCascades());
						dirShadowFarHandle->SetValue(&ShadowFar);

						d->GetShadowMapTexture()->Bind();
						i = Texture::GetLastBindedUnit();
						haveShadow = 1.f;
					}
					dirShadowHandle->SetValue(&i);
					dirHaveShadowHandle->SetValue(&haveShadow);

					RenderObject(directionalLight->GetMeshes()[0], d->GetOwner(), deferredMaterialDirectional);

					if (d->IsCastingShadows())
					{
						d->GetShadowMapTexture()->Unbind();
						numberDir++;
					}
				}
				break;
				};
			}
		}

		for (int i = FBO->GetAttachments().size()-1; i>=0; i--)
			FBO->GetAttachments()[i]->TexturePTR->Unbind();

		// End Render
		EndRender();
	}

	void DeferredRenderer::SetFBO(FrameBuffer* fbo)
	{
		// Save FBO
		FBO = fbo;
	}

};
