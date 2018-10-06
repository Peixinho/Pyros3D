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

		colorTexture = new Texture(); colorTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height);
		colorTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

		lastPassFBO = new FrameBuffer();
		lastPassFBO->Init(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, colorTexture);
		lastPassFBO->AddAttach(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, fbo->GetAttachments()[0]->TexturePTR);


		// Default View Port Init Values
		viewPortStartX = viewPortStartY = 0;

		// Save FrameBuffer
		FBO = fbo;

		// Create Second Pass Specifics
		deferredLastPass= new CustomShaderMaterial("shaders/lastPass.glsl");
		deferredMaterialAmbient= new CustomShaderMaterial("shaders/secondpassAmbient.glsl");
		deferredMaterialDirectional = new CustomShaderMaterial("shaders/secondpassDirectional.glsl");
		deferredMaterialPoint = new CustomShaderMaterial("shaders/secondpassPoint.glsl");
		deferredMaterialSpot = new CustomShaderMaterial("shaders/secondpassSpot.glsl");

		uint32 colorID = 0;
		deferredLastPass->AddUniform(Uniform("tColor", Uniforms::DataType::Int, &colorID));
		deferredLastPass->AddUniform(Uniform("uScreenDimensions", Uniforms::DataUsage::ScreenDimensions));

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
		deferredMaterialPoint->AddUniform(Uniform("tSpecular", Uniforms::DataType::Int, &texID));
		deferredMaterialSpot->AddUniform(Uniform("tSpecular", Uniforms::DataType::Int, &texID));
		texID = 3;
		deferredMaterialAmbient->AddUniform(Uniform("tNormal", Uniforms::DataType::Int, &texID));
		deferredMaterialDirectional->AddUniform(Uniform("tNormal", Uniforms::DataType::Int, &texID));
		deferredMaterialPoint->AddUniform(Uniform("tNormal", Uniforms::DataType::Int, &texID));
		deferredMaterialSpot->AddUniform(Uniform("tNormal", Uniforms::DataType::Int, &texID));

		deferredMaterialAmbient->AddUniform(Uniform("uScreenDimensions", Uniforms::DataUsage::ScreenDimensions));
		deferredMaterialAmbient->AddUniform(Uniform("uMatProj", Uniforms::DataUsage::ProjectionMatrix));
		deferredMaterialAmbient->DisableDepthTest();
		deferredMaterialAmbient->DisableDepthWrite();
		deferredMaterialAmbient->EnableBlending();
		deferredMaterialAmbient->BlendingEquation(BlendEq::Add);
		deferredMaterialAmbient->BlendingFunction(BlendFunc::One, BlendFunc::One);

		deferredMaterialDirectional->AddUniform(Uniform("uScreenDimensions", Uniforms::DataUsage::ScreenDimensions));
		dirDirHandle = deferredMaterialDirectional->AddUniform(Uniform("uLightDirection", Uniforms::DataUsage::Other, Uniforms::DataType::Vec3));
		dirColorHandle = deferredMaterialDirectional->AddUniform(Uniform("uLightColor", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
		dirShadowHandle = deferredMaterialDirectional->AddUniform(Uniform("uShadowMap", Uniforms::DataUsage::Other, Uniforms::DataType::Int));
		dirShadowPCFTexelHandle = deferredMaterialDirectional->AddUniform(Uniform("uPCFTexelSize", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
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
		pointShadowPCFTexelHandle = deferredMaterialPoint->AddUniform(Uniform("uPCFTexelSize", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		pointShadowDepthsMVPHandle = deferredMaterialPoint->AddUniform(Uniform("uPointDepthsMVP", Uniforms::DataUsage::Other, Uniforms::DataType::Matrix));
		pointHaveShadowHandle = deferredMaterialPoint->AddUniform(Uniform("uHaveShadowmap", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		deferredMaterialPoint->AddUniform(Uniform("uModelMatrix", Uniforms::DataUsage::ModelMatrix));
		deferredMaterialPoint->AddUniform(Uniform("uViewMatrix", Uniforms::DataUsage::ViewMatrix));
		deferredMaterialPoint->AddUniform(Uniform("uProjectionMatrix", Uniforms::DataUsage::ProjectionMatrix));
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
		spotShadowPCFTexelHandle = deferredMaterialSpot->AddUniform(Uniform("uPCFTexelSize", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		spotShadowDepthsMVPHandle = deferredMaterialSpot->AddUniform(Uniform("uSpotDepthsMVP", Uniforms::DataUsage::Other, Uniforms::DataType::Matrix));
		spotHaveShadowHandle = deferredMaterialSpot->AddUniform(Uniform("uHaveShadowmap", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		deferredMaterialSpot->AddUniform(Uniform("uScreenDimensions", Uniforms::DataUsage::ScreenDimensions));
		deferredMaterialSpot->AddUniform(Uniform("uModelMatrix", Uniforms::DataUsage::ModelMatrix));
		deferredMaterialSpot->AddUniform(Uniform("uViewMatrix", Uniforms::DataUsage::ViewMatrix));
		deferredMaterialSpot->AddUniform(Uniform("uProjectionMatrix", Uniforms::DataUsage::ProjectionMatrix));
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
		lastPassFBO->Resize(Width, Height);
	}

	DeferredRenderer::~DeferredRenderer()
	{
		if (IsCulling)
		{
			delete culling;
		}
		delete lastPassFBO;
		delete colorTexture;
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
		_SetViewPort(viewPortStartX, viewPortStartY, viewPortEndX, viewPortEndY);

		ClearBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth);
		ClearDepthBuffer();
		ClearScreen();

		// Draw Background
		DrawBackground();

		// Render Scene with Objects Material
		for (std::vector<RenderingMesh*>::iterator j = rmesh.begin(); j != rmesh.end(); j++)
		{

			if (!(*j)->Material->IsTransparent())
			{
				if ((*j)->renderingComponent->GetOwner() != NULL)
				{
					// Culling Test
					bool cullingTest = false;
					switch ((*j)->CullingGeometry)
					{
					case CullingGeometry::Box:
						cullingTest = CullingBoxTest((*j), (*j)->renderingComponent->GetOwner());
						break;
					case CullingGeometry::Sphere:
					default:
						cullingTest = CullingSphereTest((*j), (*j)->renderingComponent->GetOwner());
						break;
					}

					if (cullingTest && (*j)->renderingComponent->IsActive() && (*j)->Active == true)
						RenderObject((*j), (*j)->renderingComponent->GetOwner(), (*j)->Material);

				}
				else {
					break;
				}
			}
		}

		// End Rendering
		EndRender();

		// Unbind FrameBuffer
		FBO->UnBind();

		lastPassFBO->Bind();
		ClearBufferBit(Buffer_Bit::Color);
		ClearScreen();

		// Initialize Rendering
		InitRender();

		// Bind FBO Textures
		for (int i = 0;i<(int)FBO->GetAttachments().size();i++)
			FBO->GetAttachments()[i]->TexturePTR->Bind();

		// Ambient
		{
			GameObject go = GameObject();
			RenderObject(directionalLight->GetMeshes()[0], &go, deferredMaterialAmbient);
		}
		// End Ambient

		uint32 numberDir = 0, numberPoint = 0, numberSpot = 0;

		// Render Lights
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
					int shadowUnit = 0;
					float haveShadow = 0.f;
					if (p->IsCastingShadows())
					{
						f32 txl = p->GetShadowPCFTexelSize();
						Matrix mvp[2];
						mvp[0] = PointShadowMatrix[numberPoint];
						mvp[1] = PointShadowMatrix[numberPoint+1];
						pointShadowPCFTexelHandle->SetValue((void*)&txl);
						pointShadowDepthsMVPHandle->SetValue(&mvp, 2);
						p->GetShadowMapTexture()->Bind();
						shadowUnit = Texture::GetLastBindedUnit();
						haveShadow = 1.f;
					}
					pointShadowHandle->SetValue(&shadowUnit);
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

					int shadowUnit = 0;
					float haveShadow = 0.f;
					if (s->IsCastingShadows())
					{
						f32 txl = s->GetShadowPCFTexelSize();
						Matrix mvp = SpotShadowMatrix[numberDir];
						spotShadowPCFTexelHandle->SetValue(&txl);
						spotShadowDepthsMVPHandle->SetValue(&mvp);
						s->GetShadowMapTexture()->Bind();
						shadowUnit = Texture::GetLastBindedUnit();
						haveShadow = 1.f;
					}
					spotShadowHandle->SetValue(&shadowUnit);
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
					int shadowUnit = 0;
					float haveShadow = 0.f;
					if (d->IsCastingShadows())
					{
						f32 txl = d->GetShadowPCFTexelSize();

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
						for (int j = 0; j < (int)d->GetNumberCascades(); j++)
							mvp.push_back(DirectionalShadowMatrix[numberDir+j]);

						dirShadowPCFTexelHandle->SetValue(&txl);
						dirShadowDepthsMVPHandle->SetValue(&mvp[0], d->GetNumberCascades());
						dirShadowFarHandle->SetValue(&ShadowFar);

						d->GetShadowMapTexture()->Bind();
						shadowUnit = Texture::GetLastBindedUnit();
						haveShadow = 1.f;
					}
					dirShadowHandle->SetValue(&shadowUnit);
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
						directionalLight.m[13] = (f32)type;	  	 directionalLight.m[14] = d->GetShadowPCFTexelSize();  directionalLight.m[15] = (d->IsCastingShadows() ? 1.f : 0.f);

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
						pointLight.m[10] = attenuation;  pointLight.m[11] = 0.f;				  pointLight.m[12] = 0.f;
						pointLight.m[13] = (f32)type;	 pointLight.m[14] = p->GetShadowPCFTexelSize();

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
						spotLight.m[13] = (f32)type;

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

		// Scissor Test
		StartScissorTest();

		EndClippingPlanes();

		// Render Translucid Meshes
		for (std::vector<RenderingMesh*>::iterator i = rmesh.begin(); i != rmesh.end(); i++)
		{

			Lights.clear();
			if ((*i)->Material->IsTransparent() && (*i)->renderingComponent->GetOwner() != NULL)
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
					for (std::vector<Matrix>::iterator _l = _Lights.begin(); _l != _Lights.end(); _l++)
					{
						if ((*_l).m[13] == 1) Lights.push_back(*_l);
						else if ((*_l).m[13] == 2 || (*_l).m[13] == 3)
						{
							Vec3 _lPos = Vec3((*_l).m[4], (*_l).m[5], (*_l).m[6]);
							if ((_lPos.distance((*i)->renderingComponent->GetOwner()->GetWorldPosition()) - ((*i)->renderingComponent->GetOwner()->GetBoundingSphereRadiusWorldSpace())) < (*_l).m[10])
								Lights.push_back(*_l);
						}
					}
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

		for (int i = FBO->GetAttachments().size()-1; i>=0; i--)
			FBO->GetAttachments()[i]->TexturePTR->Unbind();

		lastPassFBO->UnBind();

		ClearBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth);
		ClearDepthBuffer();
		ClearScreen();

		InitRender();

		colorTexture->Bind();
		// Render to Screen
		{
			GameObject go = GameObject();
			RenderObject(directionalLight->GetMeshes()[0], &go, deferredLastPass);
		}
		colorTexture->Unbind();
		
		EndRender();	
	}

	void DeferredRenderer::SetFBO(FrameBuffer* fbo)
	{
		// Save FBO
		FBO = fbo;
	}

};
