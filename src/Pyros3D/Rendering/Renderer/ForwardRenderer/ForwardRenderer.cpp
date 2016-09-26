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

	namespace Sort {

		GameObject* _Camera;
		bool sortRenderingMeshes(const void* a, const void* b)
		{
			f32 a2 = _Camera->GetPosition().distanceSQR(((RenderingMesh*)a)->renderingComponent->GetOwner()->GetWorldPosition());
			f32 b2 = _Camera->GetPosition().distanceSQR(((RenderingMesh*)b)->renderingComponent->GetOwner()->GetWorldPosition());
			return (a2 < b2);
		}
	}

	ForwardRenderer::ForwardRenderer(const uint32 Width, const uint32 Height) : IRenderer(Width, Height)
	{
		echo("SUCCESS: Forward Renderer Created");

		ActivateCulling(CullingMode::FrustumCulling);

		shadowMaterial = new GenericShaderMaterial(ShaderUsage::CastShadows);
		shadowMaterial->SetCullFace(CullFace::DoubleSided);
		shadowSkinnedMaterial = new GenericShaderMaterial(ShaderUsage::CastShadows | ShaderUsage::Skinning);
		shadowSkinnedMaterial->SetCullFace(CullFace::DoubleSided);

		// Default View Port Init Values
		viewPortStartX = viewPortStartY = 0;
		viewPortEndX = viewPortEndY = 0;
	}

	ForwardRenderer::~ForwardRenderer()
	{
		if (IsCulling)
		{
			delete culling;
		}

		delete shadowMaterial;
		delete shadowSkinnedMaterial;
	}

	std::vector<RenderingMesh*> ForwardRenderer::GroupAndSortAssets(SceneGraph* Scene, GameObject* Camera, const uint32 Tag)
	{

		// Sort and Group Objects From Scene
		std::vector<RenderingMesh*> _OpaqueMeshes;
		std::vector<RenderingMesh*> _TranslucidMeshes;

		// LOD
		if (lod)
		{
			std::vector<RenderingComponent*> comps(RenderingComponent::GetRenderingComponents(Scene));
			for (std::vector<RenderingComponent*>::iterator i = comps.begin(); i != comps.end(); i++)
			{
				f32 distance = (Camera->GetWorldPosition().distanceSQR((*i)->GetOwner()->GetWorldPosition() + ((*i)->GetBoundingSphereCenter() - (*i)->GetBoundingSphereRadius())*(*i)->GetOwner()->GetScale()));
				(*i)->UpdateLOD((*i)->GetLODByDistance(fabs(distance)));
			}
		}
		// Get Meshes
		std::vector<RenderingMesh*> rmeshes(RenderingComponent::GetRenderingMeshes(Scene));

		if (Tag != 0)
		{
			for (std::vector<RenderingMesh*>::iterator k = rmeshes.begin(); k != rmeshes.end();)
			{
				if (!(*k)->renderingComponent->GetOwner()->HaveTag(Tag))
				{
					k = rmeshes.erase(k);
				}
				else ++k;
			}
		}

		for (std::vector<RenderingMesh*>::iterator k = rmeshes.begin(); k != rmeshes.end(); k++)
		{
			if ((*k)->Material->IsTransparent() && sorting)
			{
				_TranslucidMeshes.push_back((*k));
			}
			else _OpaqueMeshes.push_back((*k));
		}

		// sorting translucid
		Sort::_Camera = Camera;
		sort(_TranslucidMeshes.begin(), _TranslucidMeshes.end(), Sort::sortRenderingMeshes);

		// final list
		for (std::vector<RenderingMesh*>::iterator i = _TranslucidMeshes.begin(); i != _TranslucidMeshes.end(); i++)
		{
			_OpaqueMeshes.push_back((*i));
		}

		return _OpaqueMeshes;
	}

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
						directionalLight.m[13] = (f32)type;		 directionalLight.m[14] = (d->IsCastingShadows() ? 1.f : 0.f);

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
						pointLight.m[13] = (f32)type;

						if (p->IsCastingShadows())
						{
							pointLight.m[14] = 1.f;
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
							spotLight.m[14] = 1.f;
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

		// Clear Screen
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
	}

	void ForwardRenderer::PreRender(GameObject* Camera, SceneGraph* Scene)
	{
		PreRender(Camera, Scene, 0);
	}

	void ForwardRenderer::PreRender(GameObject* Camera, SceneGraph* Scene, const std::string &Tag = "")
	{
		PreRender(Camera, Scene, MakeStringID(Tag));
	}

	void ForwardRenderer::PreRender(GameObject* Camera, SceneGraph* Scene, const uint32 Tag)
	{


		// Group and Sort Meshes
		rmesh = GroupAndSortAssets(Scene, Camera, Tag);

		// Get Lights List
		lcomps = ILightComponent::GetLightsOnScene(Scene);

		if (rmesh.size() > 0 && lcomps.size() > 0)
		{
			// Initialize Renderer
			InitRender();

			// Prepare and Pack Lights to Send to Shaders
			std::vector<Matrix> _Lights;

			Lights.clear();

			// Keep user settings
			uint32 _bufferOptions = bufferOptions;
			uint32 _glBufferOptions = glBufferOptions;
			bool _clearDepthBuffer = clearDepthBuffer;

			// ShadowMaps
			DirectionalShadowMapsTextures.clear();
			DirectionalShadowMatrix.clear();
			NumberOfDirectionalShadows = 0;

			PointShadowMapsTextures.clear();
			PointShadowMatrix.clear();
			NumberOfPointShadows = 0;

			SpotShadowMapsTextures.clear();
			SpotShadowMatrix.clear();
			NumberOfSpotShadows = 0;

			ViewMatrix = Camera->GetWorldTransformation().Inverse();
			uint32 pointCounter = 0;
			uint32 spotCounter = 0;
			for (std::vector<IComponent*>::iterator i = lcomps.begin(); i != lcomps.end(); i++)
			{
				switch (((ILightComponent*)(*i))->GetLightType())
				{
				case LIGHT_TYPE::DIRECTIONAL:
				{
					DirectionalLight* d = ((DirectionalLight*)(*i));

					Vec3 direction = (d->GetOwner()->GetWorldTransformation() * Vec4(d->GetLightDirection(), 0.f)).xyz().normalize();

					// Shadows
					if (d->IsCastingShadows())
					{
						// Increase Number of Shadows
						NumberOfDirectionalShadows++;

						// Bind FBO
						d->GetShadowFBO()->Bind();

#if defined(GLES2) || defined(GL_LEGACY)
						ClearBufferBit(Buffer_Bit::Depth | Buffer_Bit::Color);
#else
						ClearBufferBit(Buffer_Bit::Depth);
#endif
						EnableClearDepthBuffer();
						ClearDepthBuffer();
						ClearScreen();

						StartClippingPlanes();

						// Enable Depth Bias
						shadowMaterial->EnableDethBias(d->GetShadowBiasFactor(), d->GetShadowBiasUnits()); // enable polygon offset fill to combat "z-fighting"

						ViewMatrix.identity();
						ViewMatrix.LookAt(Vec3::ZERO, direction, Vec3(0.f, 0.f, -1.f));

						// Get Lights Shadow Map Texture
						for (uint32 i = 0; i < d->GetNumberCascades(); i++)
						{
							d->UpdateCascadeFrustumPoints(i, Camera->GetWorldPosition(), Camera->GetDirection());
							ProjectionMatrix = d->GetLightProjection(ViewMatrix, i, rmesh);

							// Set Viewport
							_SetViewPort((uint32)((float)(i % 2) * d->GetShadowWidth()), (uint32)((i <= (uint32)1 ? 0.0f : 1.f) * d->GetShadowHeight()), d->GetShadowWidth(), d->GetShadowHeight());

							// Update Culling
							UpdateCulling(d->GetCascade(i).ortho.GetProjectionMatrix()*ViewMatrix);

							// Render Scene with Objects Material
							for (std::vector<RenderingMesh*>::iterator k = rmesh.begin(); k != rmesh.end(); k++)
							{

								if ((*k)->renderingComponent->GetOwner() != NULL)
								{
									if ((*k)->renderingComponent->IsCastingShadows() && (*k)->renderingComponent->IsActive())
										RenderObject((*k), (*k)->renderingComponent->GetOwner(), ((*k)->SkinningBones.size() > 0 ? shadowSkinnedMaterial : shadowMaterial));
								}
							}

							DirectionalShadowMatrix.push_back((Matrix::BIAS * (ProjectionMatrix * ViewMatrix)));

						}

						EndClippingPlanes();

						// Get Texture (only 1)
						DirectionalShadowMapsTextures.push_back(d->GetShadowMapTexture());

						// Set Shadow Far
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
						DirectionalShadowFar = ShadowFar;

						// Disable Depth Bias
						DisableDepthBias();

						// Unbind FBO
						d->GetShadowFBO()->UnBind();

					}
				}
				break;
				case LIGHT_TYPE::POINT:
				{
					PointLight* p = ((PointLight*)(*i));

					// Shadows
					if (p->IsCastingShadows())
					{
						// Increase Number of Shadows
						NumberOfPointShadows++;

						// Bind FBO
						p->GetShadowFBO()->Bind();

						// Create Projection Matrix
						// Get Light Projection
						Projection ShadowProjection;
						ShadowProjection.Perspective(90.f, 1.f, p->GetShadowNear(), p->GetShadowFar());
						ProjectionMatrix = ShadowProjection.m;

						// Get Lights Shadow Map Texture
						for (int32 i = 0; i < 6; i++)
						{
							// Clean View Matrix
							ViewMatrix.identity();

							// Create Light View Matrix For Rendering Each Face of the Cubemap
							if (i == 0)
								ViewMatrix.LookAt(p->GetOwner()->GetWorldPosition(), p->GetOwner()->GetWorldPosition() + Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)); // +X
							if (i == 1)
								ViewMatrix.LookAt(p->GetOwner()->GetWorldPosition(), p->GetOwner()->GetWorldPosition() + Vec3(-1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)); // -X
							if (i == 2)
								ViewMatrix.LookAt(p->GetOwner()->GetWorldPosition(), p->GetOwner()->GetWorldPosition() + Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)); // +Y
							if (i == 3)
								ViewMatrix.LookAt(p->GetOwner()->GetWorldPosition(), p->GetOwner()->GetWorldPosition() + Vec3(0.0f, -1.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f)); // -Y
							if (i == 4)
								ViewMatrix.LookAt(p->GetOwner()->GetWorldPosition(), p->GetOwner()->GetWorldPosition() + Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, -1.0f, 0.0f)); // +Z
							if (i == 5)
								ViewMatrix.LookAt(p->GetOwner()->GetWorldPosition(), p->GetOwner()->GetWorldPosition() + Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, -1.0f, 0.0f)); // -Z

																																											// Update Culling
							UpdateCulling(ShadowProjection.m*ViewMatrix);

#if defined(GLES2) || defined(GL_LEGACY)
							// Regular Shadows
							p->GetShadowFBO()->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::CubemapPositive_X + i, p->GetShadowMapTexture());
#else
							// GPU Shadows
							p->GetShadowFBO()->AddAttach(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::CubemapPositive_X + i, p->GetShadowMapTexture());
#endif

#if defined(GLES2) || defined(GL_LEGACY)
							ClearBufferBit(Buffer_Bit::Depth | Buffer_Bit::Color);
#else
							ClearBufferBit(Buffer_Bit::Depth);
#endif
							EnableClearDepthBuffer();
							ClearDepthBuffer();
							ClearScreen();

							StartClippingPlanes();

							// Enable Depth Bias
							shadowMaterial->EnableDethBias(p->GetShadowBiasFactor(), p->GetShadowBiasUnits()); // enable polygon offset fill to combat "z-fighting"

																												// Set Viewport
							_SetViewPort(0, 0, p->GetShadowWidth(), p->GetShadowHeight());

							// Render Scene with Objects Material
							for (std::vector<RenderingMesh*>::iterator k = rmesh.begin(); k != rmesh.end(); k++)
							{

								if ((*k)->renderingComponent->GetOwner() != NULL)
								{
									// Culling Test
									/*bool cullingTest = false;
									switch ((*k)->CullingGeometry)
									{
									case CullingGeometry::Box:
									cullingTest = CullingBoxTest((*k), (*k)->renderingComponent->GetOwner());
									break;
									case CullingGeometry::Sphere:
									default:
									cullingTest = CullingSphereTest((*k), (*k)->renderingComponent->GetOwner());
									break;
									}
									if (!(*k)->renderingComponent->IsCullTesting()) cullingTest = true;*/
									if (/*cullingTest && */!(*k)->Material->IsTransparent())
									{
										if ((*k)->renderingComponent->IsCastingShadows() && (*k)->renderingComponent->IsActive())
											RenderObject((*k), (*k)->renderingComponent->GetOwner(), ((*k)->SkinningBones.size() > 0 ? shadowSkinnedMaterial : shadowMaterial));
									}
								}
							}

							EndClippingPlanes();

						}

						// Set Light Projection
						PointShadowMatrix.push_back(ShadowProjection.m);
						// Set Light View Matrix
						Matrix m;
						m.Translate(p->GetOwner()->GetWorldPosition().negate());
						PointShadowMatrix.push_back(m);

						// Get Texture (only 1)
						PointShadowMapsTextures.push_back(p->GetShadowMapTexture());

						// Disable Depth Bias
						DisableDepthBias();

						// Unbind FBO
						p->GetShadowFBO()->UnBind();

					}
				}
				break;
				case LIGHT_TYPE::SPOT:
				{
					SpotLight* s = ((SpotLight*)(*i));

					// Shadows
					if (s->IsCastingShadows())
					{

						Vec3 direction = (s->GetOwner()->GetWorldTransformation() * Vec4(s->GetLightDirection(), 0.f)).xyz().normalize();

						// Increase Number of Shadows
						NumberOfSpotShadows++;

						// Bind FBO
						s->GetShadowFBO()->Bind();

						// Get Light Projection
						Projection ShadowProjection;
						ShadowProjection.Perspective(2 * s->GetLightOutterCone(), 1.0, s->GetShadowNear(), s->GetShadowFar());
						ProjectionMatrix = ShadowProjection.m;

						// Clean View Matrix
						ViewMatrix.identity();

						// Create Light View Matrix For Rendering the ShadowMap
						ViewMatrix.LookAt(s->GetOwner()->GetWorldPosition(), (s->GetOwner()->GetWorldPosition() + direction));

						// Update Culling
						UpdateCulling(ShadowProjection.m*ViewMatrix);

#if defined(GLES2) || defined(GL_LEGACY)
						ClearBufferBit(Buffer_Bit::Depth | Buffer_Bit::Color);
#else
						ClearBufferBit(Buffer_Bit::Depth);
#endif
						EnableClearDepthBuffer();
						ClearDepthBuffer();
						ClearScreen();

						StartClippingPlanes();

						// Enable Depth Bias
						shadowMaterial->EnableDethBias(s->GetShadowBiasFactor(), s->GetShadowBiasUnits()); // enable polygon offset fill to combat "z-fighting"

																											// Set Viewport
						_SetViewPort(0, 0, s->GetShadowWidth(), s->GetShadowHeight());

						// Render Scene with Objects Material
						for (std::vector<RenderingMesh*>::iterator k = rmesh.begin(); k != rmesh.end(); k++)
						{

							if ((*k)->renderingComponent->GetOwner() != NULL)
							{
								// Culling Test
								/*bool cullingTest = false;
								switch ((*k)->CullingGeometry)
								{
								case CullingGeometry::Box:
								cullingTest = CullingBoxTest((*k), (*k)->renderingComponent->GetOwner());
								break;
								case CullingGeometry::Sphere:
								default:
								cullingTest = CullingSphereTest((*k), (*k)->renderingComponent->GetOwner());
								break;
								}
								if (!(*k)->renderingComponent->IsCullTesting()) cullingTest = true;*/
								if (/*cullingTest && */!(*k)->Material->IsTransparent())
								{
									if ((*k)->renderingComponent->IsCastingShadows() && (*k)->renderingComponent->IsActive())
										RenderObject((*k), (*k)->renderingComponent->GetOwner(), ((*k)->SkinningBones.size() > 0 ? shadowSkinnedMaterial : shadowMaterial));
								}
							}
						}

						EndClippingPlanes();

						// Disable Depth Bias
						DisableDepthBias();

						// Unbind FBO
						s->GetShadowFBO()->UnBind();

						// Set Light Matrix
						SpotShadowMatrix.push_back((Matrix::BIAS * (ProjectionMatrix * ViewMatrix)));

						// Get Texture (only 1)
						SpotShadowMapsTextures.push_back(s->GetShadowMapTexture());
					}
				};
				}
			}

			// Reset User Defined for Depth Buffer
			bufferOptions = _bufferOptions;
			glBufferOptions = _glBufferOptions;
			clearDepthBuffer = _clearDepthBuffer;
			EndRender();
		}

	}
};