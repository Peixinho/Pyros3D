//============================================================================
// Name        : IRenderer.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Renderer Interface
//============================================================================

#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/Other/PyrosGL.h>

namespace p3d {

	// ViewPort Dimension
	uint32 IRenderer::_viewPortStartX = 0;
	uint32 IRenderer::_viewPortStartY = 0;
	uint32 IRenderer::_viewPortEndX = 0;
	uint32 IRenderer::_viewPortEndY = 0;

	namespace Sort {

		GameObject* _Camera;
		bool sortRenderingMeshes(const void* a, const void* b)
		{
			f32 a2 = _Camera->GetPosition().distanceSQR(((RenderingMesh*)a)->renderingComponent->GetOwner()->GetWorldPosition());
			f32 b2 = _Camera->GetPosition().distanceSQR(((RenderingMesh*)b)->renderingComponent->GetOwner()->GetWorldPosition());
			return (a2 < b2);
		}
	}

	std::vector<RenderingMesh*> IRenderer::GroupAndSortAssets(SceneGraph* Scene, GameObject* Camera, const uint32 Tag)
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
		for (std::vector<RenderingMesh*>::reverse_iterator i = _TranslucidMeshes.rbegin(); i != _TranslucidMeshes.rend(); i++)
		{
			_OpaqueMeshes.push_back((*i));
		}

		RenderingComponent::MeshesOnSceneSorted[Scene] = _OpaqueMeshes;

		return _OpaqueMeshes;
	}

	IRenderer::IRenderer() {}

	IRenderer::IRenderer(const uint32 Width, const uint32 Height)
	{
		// Background Unset by Default
		BackgroundColorSet = false;

		// Set Global Light Default Color
		GlobalLight = Vec4(0.2f, 0.2f, 0.2f, 0.2f);

		// Save Dimensions
		this->Width = Width;
		this->Height = Height;

		// Depth Bias
		IsUsingDepthBias = false;

		// Custom ViewPort
		customViewPort = false;

		// Blending Flag
		blending = false;

		// Defaults
		ClearBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth);
		depthWritting = depthTesting = false;
		clearDepthBuffer = true;
		sorting = true;
		scissorTest = false;
		scissorTestX = 0;
		scissorTestY = 0;
		scissorTestWidth = (f32)Width;
		scissorTestHeight = (f32)Height;
		lod = false;
		ClipPlane = false;

		// Shadows materials
		shadowMaterial = new GenericShaderMaterial(ShaderUsage::CastShadows);
		shadowMaterial->SetCullFace(CullFace::DoubleSided);
		shadowSkinnedMaterial = new GenericShaderMaterial(ShaderUsage::CastShadows | ShaderUsage::Skinning);
		shadowSkinnedMaterial->SetCullFace(CullFace::DoubleSided);
	}

	void IRenderer::Reset()
	{
		// Defaults
		depthWritting = depthTesting = false;
		clearDepthBuffer = true;
		depthTestMode = -1;
	}

	void IRenderer::Resize(const uint32 Width, const uint32 Height)
	{
		// Save Dimensions
		this->Width = Width;
		this->Height = Height;

		if (!customViewPort)
		{
			viewPortEndX = Width;
			viewPortEndY = Height;
		}

		// Reset States
		Reset();
	}

	void IRenderer::SetViewPort(const uint32 initX, const uint32 initY, const uint32 endX, const uint32 endY)
	{
		viewPortStartX = initX;
		viewPortStartY = initY;
		viewPortEndX = endX;
		viewPortEndY = endY;
		customViewPort = true;
	}

	void IRenderer::_SetViewPort(const uint32 initX, const uint32 initY, const uint32 endX, const uint32 endY)
	{
		if (initX != _viewPortStartX || initY != _viewPortStartY || endX != _viewPortEndX || endY != _viewPortEndY)
		{
			_viewPortStartX = initX;
			_viewPortStartY = initY;
			_viewPortEndX = endX;
			_viewPortEndY = endY;
			GLCHECKER(glViewport(initX, initY, endX, endY));
		}
	}

	IRenderer::~IRenderer()
	{
		delete shadowMaterial;
		delete shadowSkinnedMaterial;
	}

	void IRenderer::RenderScene(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene) {

	}

	void IRenderer::PreRender(GameObject* Camera, SceneGraph* Scene)
	{
		PreRender(Camera, Scene, 0);
	}

	void IRenderer::PreRender(GameObject* Camera, SceneGraph* Scene, const std::string &Tag = "")
	{
		PreRender(Camera, Scene, MakeStringID(Tag));
	}

	void IRenderer::PreRender(GameObject* Camera, SceneGraph* Scene, const uint32 Tag)
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

#if defined(GLES2) || defined(GLLEGACY)
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

								if ((*k)->renderingComponent->GetOwner() != NULL && !(*k)->Material->IsTransparent())
								{
									if ((*k)->renderingComponent->IsCastingShadows() && (*k)->renderingComponent->IsActive())
										RenderObject((*k), (*k)->renderingComponent->GetOwner(), ((*k)->SkinningBones.size() > 0 ? shadowSkinnedMaterial : shadowMaterial));
								}
							}

							DirectionalShadowMatrix.push_back((Matrix::BIAS * (ProjectionMatrix * ViewMatrix * Camera->GetWorldTransformation())));

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

#if defined(GLES2) || defined(GLLEGACY)
							// Regular Shadows
							p->GetShadowFBO()->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::CubemapPositive_X + i, p->GetShadowMapTexture());
#else
							// GPU Shadows
							p->GetShadowFBO()->AddAttach(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::CubemapPositive_X + i, p->GetShadowMapTexture());
#endif

#if defined(GLES2) || defined(GLLEGACY)
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
									if (/*cullingTest && */ (*k)->renderingComponent->GetOwner() != NULL && !(*k)->Material->IsTransparent() && !(*k)->Material->IsTransparent())
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
						PointShadowMatrix.push_back(m * Camera->GetWorldTransformation());

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

#if defined(GLES2) || defined(GLLEGACY)
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
								if (/*cullingTest && */ (*k)->renderingComponent->GetOwner() != NULL && !(*k)->Material->IsTransparent() && !(*k)->Material->IsTransparent())
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
						SpotShadowMatrix.push_back((Matrix::BIAS * (ProjectionMatrix * ViewMatrix * Camera->GetWorldTransformation())));

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

	void IRenderer::InitRender()
	{
		LastProgramUsed = -1;
		LastMaterialUsed = -1;
		LastMeshRendered = -1;
		InternalDrawType = -1;
		LastMaterialPTR = NULL;
		LastMeshRenderedPTR = NULL;
		cullFace = -1;
	}

	void IRenderer::EndRender()
	{
		if (LastMeshRenderedPTR != NULL && LastMaterialPTR != NULL)
		{
			// Unbind Index Buffer
			if (LastMeshRenderedPTR->Geometry->GetGeometryType() == GeometryType::BUFFER)
				GLCHECKER(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
			// Unbind Vertex Attributes
			UnbindMesh(LastMeshRenderedPTR, LastMaterialPTR);
			// Unbind Shadow Maps
			UnbindShadowMaps(LastMaterialPTR);
			// Material After Render
			LastMaterialPTR->AfterRender();
		}

#if !defined(GLES2)
		// Set Default Polygon Mode
		GLCHECKER(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
#endif
		// Disable Cull Face
		GLCHECKER(glDisable(GL_CULL_FACE));

		// Unbind Shader Program
		GLCHECKER(glUseProgram(0));
		// Unset Pointers
		LastMaterialPTR = NULL;
		LastMeshRenderedPTR = NULL;
		LastProgramUsed = -1;
		LastMaterialUsed = -1;
		LastMeshRendered = -1;

		DisableBlending();
	}

	void IRenderer::RenderObject(RenderingMesh* rmesh, GameObject* owner, IMaterial* Material)
	{
		// model cache
		ModelMatrix = owner->GetWorldTransformation() * rmesh->Pivot;

		NormalMatrixIsDirty = true;
		ModelViewMatrixIsDirty = true;
		ModelViewProjectionMatrixIsDirty = true;
		ModelMatrixInverseIsDirty = true;
		ModelViewMatrixInverseIsDirty = true;
		ModelMatrixInverseTransposeIsDirty = true;
		ModelViewProjectionMatrixInverseIsDirty = true;
		ViewProjectionMatrixInverseIsDirty = true;

		if ((LastMeshRenderedPTR != rmesh || LastMaterialPTR != Material || LastMeshRenderedPTR->Geometry->GetGeometryType() == GeometryType::ARRAY) && LastProgramUsed != -1)
		{
			// Unbind Index Buffer
			if (LastMeshRenderedPTR->Geometry->GetGeometryType() == GeometryType::BUFFER)
				GLCHECKER(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
			// Unbind Mesh
			UnbindMesh(LastMeshRenderedPTR, LastMaterialPTR);
			// Material Stuff After Render
			UnbindShadowMaps(LastMaterialPTR);
			// After Render
			LastMaterialPTR->AfterRender();
		}
		if (LastProgramUsed != Material->GetShader()) GLCHECKER(glUseProgram(Material->GetShader()));

		if (LastMeshRenderedPTR != rmesh || LastMaterialPTR != Material || LastMeshRenderedPTR->Geometry->GetGeometryType() == GeometryType::ARRAY)
		{
			// Bind Mesh
			BindMesh(rmesh, Material);

			// Material Stuff Pre Render
			Material->PreRender();

			// Bind Shadow Maps
			BindShadowMaps(Material);

			// Send Global Uniforms
			SendGlobalUniforms(rmesh, Material);

			// Send Vertex Attributes
			SendAttributes(rmesh, Material);

			if (Material->depthBias)
				EnableDepthBias(Vec2(Material->depthFactor, Material->depthUnits));

			// Bind Index Buffer
			if (rmesh->Geometry->GetGeometryType() == GeometryType::BUFFER)
				GLCHECKER(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rmesh->Geometry->IndexBuffer->ID));
		}

		// Check double sided
		if (rmesh->Material != Material)
		{
			if (rmesh->Material->GetCullFace() != Material->GetCullFace())
			{
				Material->SetCullFace(rmesh->Material->GetCullFace());
				cullFaceChanged = true;
			}
		}
		if (LastMaterialPTR != Material || cullFaceChanged)
		{
			// Check if Material is DoubleSided
			if (Material->GetCullFace() != cullFace)
			{
				switch (Material->GetCullFace())
				{
				case CullFace::FrontFace:
					GLCHECKER(glEnable(GL_CULL_FACE));
					GLCHECKER(glCullFace(GL_FRONT));
					break;
				case CullFace::DoubleSided:
					GLCHECKER(glDisable(GL_CULL_FACE));
					break;
				case CullFace::BackFace:
				default:
					GLCHECKER(glEnable(GL_CULL_FACE));
					GLCHECKER(glCullFace(GL_BACK));
					break;
				}
				cullFace = Material->GetCullFace();
				cullFaceChanged = false;
			}

			// Check if Material is WireFrame
			(Material->IsWireFrame() ? EnableWireFrame() : DisableWireFrame());

			// Material Render Method
			Material->Render();
		}

		if (LastMeshRenderedPTR != rmesh && (InternalDrawType == -1 || InternalDrawType != rmesh->GetDrawingType()))
		{
			// getting material drawing type
			switch (rmesh->GetDrawingType())
			{
			case DrawingType::Lines:
				DrawType = GL_LINES;
				break;
#if !defined(GLES2)
			case DrawingType::Polygons:
				DrawType = GL_POLYGON;
				break;
			case DrawingType::Quads:
				DrawType = GL_QUADS;
				break;
#endif
			case DrawingType::Points:
				DrawType = GL_POINTS;
				break;
			case DrawingType::Line_Loop:
				DrawType = GL_LINE_LOOP;
				break;
			case DrawingType::Line_Strip:
				DrawType = GL_LINE_STRIP;
				break;
			case DrawingType::Triangle_Fan:
				DrawType = GL_TRIANGLE_FAN;
				break;
			case DrawingType::Triangle_Strip:
				DrawType = GL_TRIANGLE_STRIP;
				break;
			case DrawingType::Triangles:
			default:
				DrawType = GL_TRIANGLES;
				break;
			}
			InternalDrawType = rmesh->GetDrawingType();
		}

		// Send User Uniforms
		SendUserUniforms(rmesh, Material);

		// Send Model Specific Uniforms
		SendModelUniforms(rmesh, Material);

		// Depth Write
		if (Material->IsDepthWritting() != depthWritting)
		{
			depthWritting = Material->IsDepthWritting();
			DepthWrite();
		}

		// Depth Test
		if (Material->IsDepthTesting() != depthTesting || Material->depthTestMode != depthTestMode)
		{
			depthTesting = Material->IsDepthTesting();
			DepthTest(Material->depthTestMode);
		}

		// Enable / Disable Blending
		if (Material->blending || Material->IsTransparent())
		{
			// Default for Transparency
			uint32 s = BlendFunc::Src_Alpha;
			uint32 d = BlendFunc::One_Minus_Src_Alpha;
			uint32 m = BlendEq::Add;

			// Override for transparency
			if (Material->blending)
			{
				s = Material->sfactor;
				d = Material->dfactor;
				m = Material->mode;
			}

			if (!blending || s != sfactor || d != dfactor || m != mode)
			{
				EnableBlending();
				BlendingEquation(m);
				BlendingFunction(s, d);
			}
		}
		else if (blending && (!Material->IsTransparent() || !Material->blending)) DisableBlending();

		// Draw
		if (rmesh->Geometry->GetGeometryType() == GeometryType::BUFFER)
		{
			GLCHECKER(glDrawElements(DrawType, rmesh->Geometry->GetIndexData().size(), __INDEX_TYPE__, BUFFER_OFFSET(0)));
		}
		else {
			GLCHECKER(glDrawElements(DrawType, rmesh->Geometry->GetIndexData().size(), __INDEX_TYPE__, &rmesh->Geometry->index[0]));
		}

		// Save Last Material and Mesh
		LastProgramUsed = Material->GetShader();
		LastMaterialPTR = Material;
		LastMaterialUsed = Material->GetInternalID();
		LastMeshRendered = rmesh->Geometry->GetInternalID();
		LastMeshRenderedPTR = rmesh;

		if (Material->depthBias)
			DisableDepthBias();
	}

	void IRenderer::EnableSorting()
	{
		sorting = true;
	}

	void IRenderer::DisableSorting()
	{
		sorting = false;
	}

	void IRenderer::ClearBufferBit(const uint32 Option)
	{
		glBufferOptions = 0;
		if (Option & Buffer_Bit::Color) glBufferOptions |= GL_COLOR_BUFFER_BIT;
		if (Option & Buffer_Bit::Depth) glBufferOptions |= GL_DEPTH_BUFFER_BIT;
		if (Option & Buffer_Bit::Stencil) glBufferOptions |= GL_STENCIL_BUFFER_BIT;

		bufferOptions = Option;
	}

	void IRenderer::DrawBackground()
	{
		if (BackgroundColorSet)
			GLCHECKER(glClearColor(BackgroundColor.x, BackgroundColor.y, BackgroundColor.z, BackgroundColor.w));
	}

	void IRenderer::DepthTest(const uint32 test)
	{
		depthTestMode = test;

		if (depthTesting)
		{
			GLCHECKER(glEnable(GL_DEPTH_TEST));

			switch (test)
			{
			case DepthTest::Always:
				GLCHECKER(glDepthFunc(GL_ALWAYS));
				break;
			case DepthTest::Equal:
				GLCHECKER(glDepthFunc(GL_EQUAL));
				break;
			case DepthTest::GEqual:
				GLCHECKER(glDepthFunc(GL_GEQUAL));
				break;
			case DepthTest::Greater:
				GLCHECKER(glDepthFunc(GL_GREATER));
				break;
			case DepthTest::LEqual:
				GLCHECKER(glDepthFunc(GL_LEQUAL));
				break;
			case DepthTest::Never:
				GLCHECKER(glDepthFunc(GL_NEVER));
				break;
			case DepthTest::NotEqual:
				GLCHECKER(glDepthFunc(GL_NOTEQUAL));
				break;
			case DepthTest::Less:
			default:
				GLCHECKER(glDepthFunc(GL_LESS));
				break;
			}
		}
		else {
			GLCHECKER(glDisable(GL_DEPTH_TEST));
		}
	}

	void IRenderer::DepthWrite()
	{
		if (depthWritting)
		{
			GLCHECKER(glDepthMask(GL_TRUE));
		}
		else {
			GLCHECKER(glDepthMask(GL_FALSE));
		}
	}

	void IRenderer::EnableClearDepthBuffer()
	{
		clearDepthBuffer = true;
	}

	void IRenderer::DisableClearDepthBuffer()
	{
		clearDepthBuffer = false;
	}

	void IRenderer::ClearDepthBuffer()
	{
#if !defined(GLES2)
		if (clearDepthBuffer) {
			GLCHECKER(glDepthMask(GL_TRUE));
			GLCHECKER(glClearDepth(1.f));
		}
#endif
	}

	void IRenderer::EnableStencil()
	{
		GLCHECKER(glEnable(GL_STENCIL_TEST));
	}

	void IRenderer::DisableStencil()
	{
		GLCHECKER(glDisable(GL_STENCIL_TEST));
	}

	void IRenderer::ClearStencilBuffer()
	{
		GLCHECKER(glClearStencil(0));
	}

	void IRenderer::StencilFunction(const uint32 func, const uint32 ref, const uint32 mask)
	{
		uint32 Func = GL_ALWAYS;
		switch (func)
		{
		case StencilFunc::Never:
			Func = GL_NEVER;
			break;
		case StencilFunc::Less:
			Func = GL_LESS;
			break;
		case StencilFunc::LEqual:
			Func = GL_LEQUAL;
			break;
		case StencilFunc::Greater:
			Func = GL_GREATER;
			break;
		case StencilFunc::GEqual:
			Func = GL_GEQUAL;
			break;
		case StencilFunc::Equal:
			Func = GL_EQUAL;
			break;
		case StencilFunc::Notequal:
			Func = GL_NOTEQUAL;
			break;
		default:
		case StencilFunc::Always:
			Func = GL_ALWAYS;
			break;
		}
		GLCHECKER(glStencilFunc(Func, ref, mask));
	}

	void IRenderer::StencilOperation(const uint32 sfail, const uint32 dpfail, const uint32 dppass)
	{
		uint32 Sfail = GL_KEEP;
		switch (sfail)
		{
		case StencilOp::Zero:
			Sfail = GL_KEEP;
			break;
		case StencilOp::Replace:
			Sfail = GL_REPLACE;
			break;
		case StencilOp::Incr:
			Sfail = GL_INCR;
			break;
		case StencilOp::Incr_Wrap:
			Sfail = GL_INCR_WRAP;
			break;
		case StencilOp::Decr:
			Sfail = GL_DECR;
			break;
		case StencilOp::Decr_Wrap:
			Sfail = GL_DECR_WRAP;
			break;
		case StencilOp::Invert:
			Sfail = GL_INVERT;
			break;
		default:
		case StencilOp::Keep:
			Sfail = GL_KEEP;
			break;
		};
		uint32 DPfail = GL_KEEP;
		switch (dpfail)
		{
		case StencilOp::Zero:
			DPfail = GL_KEEP;
			break;
		case StencilOp::Replace:
			DPfail = GL_REPLACE;
			break;
		case StencilOp::Incr:
			DPfail = GL_INCR;
			break;
		case StencilOp::Incr_Wrap:
			DPfail = GL_INCR_WRAP;
			break;
		case StencilOp::Decr:
			DPfail = GL_DECR;
			break;
		case StencilOp::Decr_Wrap:
			DPfail = GL_DECR_WRAP;
			break;
		case StencilOp::Invert:
			DPfail = GL_INVERT;
			break;
		default:
		case StencilOp::Keep:
			DPfail = GL_KEEP;
			break;
		};
		uint32 DPPASS = GL_KEEP;
		switch (dppass)
		{
		case StencilOp::Zero:
			DPPASS = GL_KEEP;
			break;
		case StencilOp::Replace:
			DPPASS = GL_REPLACE;
			break;
		case StencilOp::Incr:
			DPPASS = GL_INCR;
			break;
		case StencilOp::Incr_Wrap:
			DPPASS = GL_INCR_WRAP;
			break;
		case StencilOp::Decr:
			DPPASS = GL_DECR;
			break;
		case StencilOp::Decr_Wrap:
			DPPASS = GL_DECR_WRAP;
			break;
		case StencilOp::Invert:
			DPPASS = GL_INVERT;
			break;
		default:
		case StencilOp::Keep:
			DPPASS = GL_KEEP;
			break;
		};
		// Set Stencil Op
		GLCHECKER(glStencilOp(Sfail, DPfail, DPPASS));
	}

	void IRenderer::ColorMask(const bool r, const bool g, const bool b, const bool a)
	{
		GLCHECKER(glColorMask((GLboolean)r, (GLboolean)g, (GLboolean)b, (GLboolean)a));
	}

	void IRenderer::ClearScreen()
	{
		GLCHECKER(glClear((GLuint)glBufferOptions));
	}

	void IRenderer::SetGlobalLight(const Vec4& Light)
	{
		GlobalLight = Light;
	}

	void IRenderer::EnableDepthBias(const Vec2& Bias)
	{
		if (!IsUsingDepthBias)
		{
			IsUsingDepthBias = true;
			GLCHECKER(glEnable(GL_POLYGON_OFFSET_FILL));    // enable polygon offset fill to combat "z-fighting"
		}
		GLCHECKER(glPolygonOffset(Bias.x, Bias.y));
	}

	void IRenderer::DisableDepthBias()
	{
		if (IsUsingDepthBias)
		{
			IsUsingDepthBias = false;
			GLCHECKER(glDisable(GL_POLYGON_OFFSET_FILL));
		}
	}

	void IRenderer::EnableBlending()
	{
		if (!blending)
		{
			// Enable Blending
			GLCHECKER(glEnable(GL_BLEND));
			blending = true;
		}
	}

	void IRenderer::DisableBlending()
	{
		if (blending)
		{
			// Disables Blending
			GLCHECKER(glDisable(GL_BLEND));
			blending = false;
			sfactor = dfactor = mode = -1;
		}
	}

	void IRenderer::BlendingFunction(const uint32 sfactor, const uint32 dfactor)
	{
		this->sfactor = sfactor;
		this->dfactor = dfactor;

		uint32 Sfactor = GL_ONE;
		switch (sfactor)
		{
		case BlendFunc::Zero:
			Sfactor = GL_ZERO;
			break;
		case BlendFunc::Src_Color:
			Sfactor = GL_SRC_COLOR;
			break;
		case BlendFunc::One_Minus_Src_Color:
			Sfactor = GL_ONE_MINUS_SRC_COLOR;
			break;
		case BlendFunc::Dst_Color:
			Sfactor = GL_DST_COLOR;
			break;
		case BlendFunc::One_Minus_Dst_Color:
			Sfactor = GL_ONE_MINUS_DST_COLOR;
			break;
		case BlendFunc::Src_Alpha:
			Sfactor = GL_SRC_ALPHA;
			break;
		case BlendFunc::One_Minus_Src_Alpha:
			Sfactor = GL_ONE_MINUS_SRC_ALPHA;
			break;
		case BlendFunc::Dst_Alpha:
			Sfactor = GL_DST_ALPHA;
			break;
		case BlendFunc::One_Minus_Dst_Alpha:
			Sfactor = GL_ONE_MINUS_DST_ALPHA;
			break;
		case BlendFunc::Constant_Color:
			Sfactor = GL_CONSTANT_COLOR;
			break;
		case BlendFunc::One_Minus_Constant_Color:
			Sfactor = GL_ONE_MINUS_CONSTANT_COLOR;
			break;
		case BlendFunc::Constant_Alpha:
			Sfactor = GL_CONSTANT_ALPHA;
			break;
		case BlendFunc::One_Minus_Constant_Alpha:
			Sfactor = GL_ONE_MINUS_CONSTANT_ALPHA;
			break;
		case BlendFunc::Src_Alpha_Saturate:
			Sfactor = GL_SRC_ALPHA_SATURATE;
			break;
#if !defined(GLES2)
		case BlendFunc::Src1_Color:
			Sfactor = GL_SRC1_COLOR;
			break;
		case BlendFunc::One_Minus_Src1_Color:
			Sfactor = GL_ONE_MINUS_SRC1_COLOR;
			break;
		case BlendFunc::Src1_Alpha:
			Sfactor = GL_SRC1_ALPHA;
			break;
		case BlendFunc::One_Minus_Src1_Alpha:
			Sfactor = GL_ONE_MINUS_SRC1_ALPHA;
			break;
#endif
		default:
		case BlendFunc::One:
			Sfactor = GL_ONE;
			break;
		}
		uint32 Dfactor = GL_ONE;
		switch (dfactor)
		{
		case BlendFunc::Zero:
			Dfactor = GL_ZERO;
			break;
		case BlendFunc::Src_Color:
			Dfactor = GL_SRC_COLOR;
			break;
		case BlendFunc::One_Minus_Src_Color:
			Dfactor = GL_ONE_MINUS_SRC_COLOR;
			break;
		case BlendFunc::Dst_Color:
			Dfactor = GL_DST_COLOR;
			break;
		case BlendFunc::One_Minus_Dst_Color:
			Dfactor = GL_ONE_MINUS_DST_COLOR;
			break;
		case BlendFunc::Src_Alpha:
			Dfactor = GL_SRC_ALPHA;
			break;
		case BlendFunc::One_Minus_Src_Alpha:
			Dfactor = GL_ONE_MINUS_SRC_ALPHA;
			break;
		case BlendFunc::Dst_Alpha:
			Dfactor = GL_DST_ALPHA;
			break;
		case BlendFunc::One_Minus_Dst_Alpha:
			Dfactor = GL_ONE_MINUS_DST_ALPHA;
			break;
		case BlendFunc::Constant_Color:
			Dfactor = GL_CONSTANT_COLOR;
			break;
		case BlendFunc::One_Minus_Constant_Color:
			Dfactor = GL_ONE_MINUS_CONSTANT_COLOR;
			break;
		case BlendFunc::Constant_Alpha:
			Dfactor = GL_CONSTANT_ALPHA;
			break;
		case BlendFunc::One_Minus_Constant_Alpha:
			Dfactor = GL_ONE_MINUS_CONSTANT_ALPHA;
			break;
		case BlendFunc::Src_Alpha_Saturate:
			Dfactor = GL_SRC_ALPHA_SATURATE;
			break;
#if !defined(GLES2)
		case BlendFunc::Src1_Color:
			Dfactor = GL_SRC1_COLOR;
			break;
		case BlendFunc::One_Minus_Src1_Color:
			Dfactor = GL_ONE_MINUS_SRC1_COLOR;
			break;
		case BlendFunc::Src1_Alpha:
			Dfactor = GL_SRC1_ALPHA;
			break;
		case BlendFunc::One_Minus_Src1_Alpha:
			Dfactor = GL_ONE_MINUS_SRC1_ALPHA;
			break;
#endif
		default:
		case BlendFunc::One:
			Dfactor = GL_ONE;
			break;
		}
		GLCHECKER(glBlendFunc(Sfactor, Dfactor));
	}

	void IRenderer::EnableScissorTest()
	{
		scissorTest = true;
	}

	void IRenderer::DisableScissorTest()
	{
		scissorTest = false;
	}

	void IRenderer::ScissorTestRect(const f32 x, const f32 y, const f32 width, const f32 height)
	{
		scissorTestX = x;
		scissorTestY = y;
		scissorTestWidth = width;
		scissorTestHeight = height;
	}

	void IRenderer::BlendingEquation(const uint32 mode)
	{
		this->mode = mode;

		uint32 Mode = GL_FUNC_ADD;
		switch (mode)
		{
		case BlendEq::Subtract:
			Mode = GL_FUNC_SUBTRACT;
			break;
		case BlendEq::Reverse_Subtract:
			Mode = GL_FUNC_REVERSE_SUBTRACT;
			break;
		default:
		case BlendEq::Add:
			Mode = GL_FUNC_ADD;
			break;
		}
		GLCHECKER(glBlendEquation(Mode));
	}

	void IRenderer::EnableWireFrame()
	{
#if !defined(GLES2)
		GLCHECKER(glPolygonMode(GL_FRONT_AND_BACK, GL_LINE));
#endif
	}

	void IRenderer::DisableWireFrame()
	{
#if !defined(GLES2)
		GLCHECKER(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
#endif
	}

	void IRenderer::EnableClipPlane(const uint32 &numberOfClipPlanes)
	{
		ClipPlane = true;
		ClipPlaneNumber = numberOfClipPlanes;
	}

	void IRenderer::DisableClipPlane()
	{
		ClipPlane = false;
	}

	void IRenderer::StartClippingPlanes()
	{
#if !defined(GLES2)
		if (ClipPlane)
		{
			for (uint32 k = 0; k < ClipPlaneNumber; k++)
				GLCHECKER(glEnable(GL_CLIP_DISTANCE0 + k));
		}
#endif
	}

	void IRenderer::EndClippingPlanes()
	{
#if !defined(GLES2)
		if (ClipPlane)
		{
			for (uint32 k = 0; k < ClipPlaneNumber; k++)
				GLCHECKER(glDisable(GL_CLIP_DISTANCE0 + k));
		}
#endif
	}

	void IRenderer::StartScissorTest()
	{
		if (scissorTest)
		{
			GLCHECKER(glScissor((GLint)scissorTestX, (GLint)scissorTestY, (GLsizei)scissorTestWidth, (GLsizei)scissorTestHeight));
			GLCHECKER(glEnable(GL_SCISSOR_TEST));
		}
	}

	void IRenderer::EndScissorTest()
	{
		if (scissorTest)
		{
			GLCHECKER(glDisable(GL_SCISSOR_TEST));
		}
	}

	void IRenderer::SetClipPlane0(const Vec4 &clipPlane)
	{
		ClipPlanes[0] = clipPlane;
	}

	void IRenderer::SetClipPlane1(const Vec4 &clipPlane)
	{
		ClipPlanes[1] = clipPlane;
	}

	void IRenderer::SetClipPlane2(const Vec4 &clipPlane)
	{
		ClipPlanes[2] = clipPlane;
	}

	void IRenderer::SetClipPlane3(const Vec4 &clipPlane)
	{
		ClipPlanes[3] = clipPlane;
	}

	void IRenderer::SetClipPlane4(const Vec4 &clipPlane)
	{
		ClipPlanes[4] = clipPlane;
	}

	void IRenderer::SetClipPlane5(const Vec4 &clipPlane)
	{
		ClipPlanes[5] = clipPlane;
	}

	void IRenderer::SetClipPlane6(const Vec4 &clipPlane)
	{
		ClipPlanes[6] = clipPlane;
	}

	void IRenderer::SetClipPlane7(const Vec4 &clipPlane)
	{
		ClipPlanes[7] = clipPlane;
	}

	void IRenderer::SetBackground(const Vec4& Color)
	{
		BackgroundColor = Color;
		BackgroundColorSet = true;
	}

	void IRenderer::UnsetBackground()
	{
		BackgroundColorSet = false;
	}

	void IRenderer::ActivateCulling(const uint32 cullingType)
	{
		culling = new FrustumCulling();
		IsCulling = true;
	}

	void IRenderer::DeactivateCulling()
	{
		IsCulling = false;
		delete culling;
	}

	bool IRenderer::CullingSphereTest(RenderingMesh* rmesh, GameObject* owner)
	{
		return culling->SphereInFrustum(owner->GetWorldPosition(), owner->GetBoundingSphereRadiusWorldSpace());
	}

	bool IRenderer::CullingBoxTest(RenderingMesh* rmesh, GameObject* owner)
	{
		AABox aabb = AABox(owner->GetBoundingMinValueWorldSpace(), owner->GetBoundingMaxValueWorldSpace());

		// Return test
		return culling->ABoxInFrustum(aabb);
	}

	bool IRenderer::CullingPointTest(RenderingMesh* rmesh, GameObject* owner)
	{
		return culling->PointInFrustum(owner->GetWorldPosition());
	}

	void IRenderer::UpdateCulling(const Matrix& ViewProjectionMatrix)
	{
		culling->Update(ViewProjectionMatrix);
	}

	void IRenderer::SendGlobalUniforms(RenderingMesh* rmesh, IMaterial* Material)
	{
		std::vector<int32> *_ShadersGlobalCache = &rmesh->ShadersGlobalCache[Material->GetShader()];

		// Send Global Uniforms
		uint32 counter = 0;
		for (std::list<Uniform>::iterator k = Material->GlobalUniforms.begin(); k != Material->GlobalUniforms.end(); k++)
		{
			if ((*_ShadersGlobalCache)[counter] == -2)
				(*_ShadersGlobalCache)[counter] = Shader::GetUniformLocation(Material->GetShader(), (*k).Name);

			if ((*_ShadersGlobalCache)[counter] >= 0)
			{
				switch ((*k).Usage)
				{
				case Uniforms::DataUsage::ViewMatrix:
					Shader::SendUniform((*k), &ViewMatrix, (*_ShadersGlobalCache)[counter]);
					break;
				case Uniforms::DataUsage::ProjectionMatrix:
					Shader::SendUniform((*k), &ProjectionMatrix, (*_ShadersGlobalCache)[counter]);
					break;
				case Uniforms::DataUsage::ViewProjectionMatrix:
					if (ViewProjectionMatrixIsDirty == true)
					{
						ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;
						ViewProjectionMatrixIsDirty = false;
					}
					Shader::SendUniform((*k), &ViewProjectionMatrix, (*_ShadersGlobalCache)[counter]);
					break;
				case Uniforms::DataUsage::ViewMatrixInverse:
					if (ViewMatrixInverseIsDirty == true)
					{
						ViewMatrixInverse = ViewMatrix.Inverse();
						ViewMatrixInverseIsDirty = false;
					}
					Shader::SendUniform((*k), &ViewMatrixInverse, (*_ShadersGlobalCache)[counter]);
					break;
				case Uniforms::DataUsage::ProjectionMatrixInverse:
					if (ProjectionMatrixInverseIsDirty == true)
					{
						ProjectionMatrixInverse = ProjectionMatrix.Inverse();
						ProjectionMatrixInverseIsDirty = false;
					}
					Shader::SendUniform((*k), &ProjectionMatrixInverse, (*_ShadersGlobalCache)[counter]);
					break;
				case Uniforms::DataUsage::ViewProjectionMatrixInverse:
					if (ViewProjectionMatrixInverseIsDirty == true)
					{
						ViewProjectionMatrixInverse = (ProjectionMatrix * ViewMatrix).Inverse();
						ViewProjectionMatrixInverseIsDirty = false;
					}
					Shader::SendUniform((*k), &ProjectionMatrixInverse, (*_ShadersGlobalCache)[counter]);
					break;
				case Uniforms::DataUsage::CameraPosition:
					Shader::SendUniform((*k), &CameraPosition, (*_ShadersGlobalCache)[counter]);
					break;
				case Uniforms::DataUsage::Timer:
				{
					f32 t = (f32)Timer;
					Shader::SendUniform((*k), &t, (*_ShadersGlobalCache)[counter]);
				}
				break;
				case Uniforms::DataUsage::GlobalAmbientLight:
					Shader::SendUniform((*k), &GlobalLight, (*_ShadersGlobalCache)[counter]);
					break;
				case Uniforms::DataUsage::Lights:
					if (Lights.size() > 0)
						Shader::SendUniform((*k), &Lights[0], (*_ShadersGlobalCache)[counter], NumberOfLights);
					break;
				case Uniforms::DataUsage::NumberOfLights:
					Shader::SendUniform((*k), &NumberOfLights, (*_ShadersGlobalCache)[counter]);
					break;
				case Uniforms::DataUsage::NearFarPlane:
					Shader::SendUniform((*k), &NearFarPlane, (*_ShadersGlobalCache)[counter]);
					break;
				case Uniforms::DataUsage::ScreenDimensions:
				{
					Vec2 dim = Vec2((f32)Width, (f32)Height);
					Shader::SendUniform((*k), &dim, (*_ShadersGlobalCache)[counter]);
				}
				break;
				case Uniforms::DataUsage::DirectionalShadowMap:
					if (DirectionalShadowMapsUnits.size() > 0)
						Shader::SendUniform((*k), &DirectionalShadowMapsUnits[0], (*_ShadersGlobalCache)[counter], DirectionalShadowMapsUnits.size());
					break;
				case Uniforms::DataUsage::DirectionalShadowMatrix:
					if (DirectionalShadowMatrix.size() > 0)
						Shader::SendUniform((*k), &DirectionalShadowMatrix[0], (*_ShadersGlobalCache)[counter], DirectionalShadowMatrix.size());
					break;
				case Uniforms::DataUsage::DirectionalShadowFar:
					Shader::SendUniform((*k), &DirectionalShadowFar, (*_ShadersGlobalCache)[counter]);
					break;
				case Uniforms::DataUsage::NumberOfDirectionalShadows:
					Shader::SendUniform((*k), &NumberOfDirectionalShadows, (*_ShadersGlobalCache)[counter]);
					break;
				case Uniforms::DataUsage::PointShadowMap:
					if (PointShadowMapsUnits.size() > 0)
						Shader::SendUniform((*k), &PointShadowMapsUnits[0], (*_ShadersGlobalCache)[counter], PointShadowMapsUnits.size());
					break;
				case Uniforms::DataUsage::PointShadowMatrix:
					Shader::SendUniform((*k), &PointShadowMatrix[0], (*_ShadersGlobalCache)[counter], PointShadowMatrix.size());
					break;
				case Uniforms::DataUsage::NumberOfPointShadows:
					Shader::SendUniform((*k), &NumberOfPointShadows, (*_ShadersGlobalCache)[counter]);
					break;
				case Uniforms::DataUsage::SpotShadowMap:
					Shader::SendUniform((*k), &SpotShadowMapsUnits[0], (*_ShadersGlobalCache)[counter], SpotShadowMapsUnits.size());
					break;
				case Uniforms::DataUsage::SpotShadowMatrix:
					Shader::SendUniform((*k), &SpotShadowMatrix[0], (*_ShadersGlobalCache)[counter], SpotShadowMatrix.size());
					break;
				case Uniforms::DataUsage::NumberOfSpotShadows:
					Shader::SendUniform((*k), &NumberOfSpotShadows, (*_ShadersGlobalCache)[counter]);
					break;
				case Uniforms::DataUsage::ClipPlanes:
					Shader::SendUniform((*k), &ClipPlanes, (*_ShadersGlobalCache)[counter], ClipPlaneNumber);
					break;
				default:
					Shader::SendUniform((*k), (*_ShadersGlobalCache)[counter]);
					break;
				}
			}
			counter++;
		}
	}

	void IRenderer::SendUserUniforms(RenderingMesh* rmesh, IMaterial* Material)
	{
		std::vector<int32>* _ShadersUserCache = &rmesh->ShadersUserCache[Material->GetShader()];

		// User Specific Uniforms
		uint32 counter = 0;
		for (std::list<Uniform>::iterator k = Material->UserUniforms.begin(); k != Material->UserUniforms.end(); k++)
		{
			if ((*_ShadersUserCache)[counter] == -2)
				(*_ShadersUserCache)[counter] = Shader::GetUniformLocation(Material->GetShader(), (*k).Name);

			if ((*_ShadersUserCache)[counter] >= 0)
				Shader::SendUniform((*k), (*_ShadersUserCache)[counter]);

			counter++;
		}
	}

	void IRenderer::SendModelUniforms(RenderingMesh* rmesh, IMaterial* Material)
	{
		uint32 counter = 0;

		std::vector<int32>* _ShadersModelCache = &rmesh->ShadersModelCache[Material->GetShader()];

		for (std::list<Uniform>::iterator k = Material->ModelUniforms.begin(); k != Material->ModelUniforms.end(); k++)
		{
			if ((*_ShadersModelCache)[counter] == -2)
				(*_ShadersModelCache)[counter] = Shader::GetUniformLocation(Material->GetShader(), (*k).Name);

			if ((*_ShadersModelCache)[counter] >= 0)
			{
				switch ((*k).Usage)
				{
				case Uniforms::DataUsage::ModelMatrix:
					Shader::SendUniform((*k), &ModelMatrix, (*_ShadersModelCache)[counter]);
					break;
				case Uniforms::DataUsage::NormalMatrix:
					if (NormalMatrixIsDirty == true)
					{
						NormalMatrix = (ViewMatrix*ModelMatrix);
						NormalMatrixIsDirty = false;
					}
					Shader::SendUniform((*k), &NormalMatrix, (*_ShadersModelCache)[counter]);
					break;
				case Uniforms::DataUsage::ModelViewMatrix:
					if (ModelViewMatrixIsDirty == true)
					{
						ModelViewMatrix = ViewMatrix*ModelMatrix;
						ModelViewMatrixIsDirty = false;
					}
					Shader::SendUniform((*k), &ModelViewMatrix, (*_ShadersModelCache)[counter]);
					break;
				case Uniforms::DataUsage::ModelViewProjectionMatrix:
					if (ModelViewProjectionMatrixIsDirty == true)
					{
						ModelViewProjectionMatrix = ProjectionMatrix*ViewMatrix*ModelMatrix;
						ModelViewProjectionMatrixIsDirty = false;
					}
					Shader::SendUniform((*k), &ModelViewProjectionMatrix, (*_ShadersModelCache)[counter]);
					break;
				case Uniforms::DataUsage::ModelMatrixInverse:
					if (ModelMatrixInverseIsDirty == true)
					{
						ModelMatrixInverse = ModelMatrix.Inverse();
						ModelMatrixInverseIsDirty = false;
					}
					Shader::SendUniform((*k), &ModelMatrixInverse, (*_ShadersModelCache)[counter]);
					break;
				case Uniforms::DataUsage::ModelViewMatrixInverse:
					if (ModelViewMatrixInverseIsDirty == true)
					{
						ModelViewMatrixInverse = (ViewMatrix*ModelMatrix).Inverse();
						ModelViewMatrixInverseIsDirty = false;
					}
					Shader::SendUniform((*k), &ModelViewMatrixInverse, (*_ShadersModelCache)[counter]);
					break;
				case Uniforms::DataUsage::ModelMatrixInverseTranspose:
					if (ModelMatrixInverseTransposeIsDirty == true)
					{
						ModelMatrixInverseTranspose = ModelMatrixInverse.Transpose();
						ModelMatrixInverseTransposeIsDirty = false;
					}
					Shader::SendUniform((*k), &ModelMatrixInverseTranspose, (*_ShadersModelCache)[counter]);
					break;
				case Uniforms::DataUsage::Skinning:
				{
					if (rmesh->SkinningBones.size() > 0)
						Shader::SendUniform((*k), &rmesh->SkinningBones[0], (*_ShadersModelCache)[counter], rmesh->SkinningBones.size());
				}
				break;
				case Uniforms::DataUsage::ModelViewProjectionMatrixInverse:
					if (ModelViewProjectionMatrixInverseIsDirty == true)
					{
						ModelViewProjectionMatrixInverse = (ProjectionMatrix * ViewMatrix * ModelMatrix).Inverse();
						ModelViewProjectionMatrixInverseIsDirty = false;
					}
					Shader::SendUniform((*k), &ModelViewProjectionMatrixInverse, (*_ShadersModelCache)[counter]);
				break;
				}
			}
			counter++;
		}
	}

	void IRenderer::BindMesh(RenderingMesh* rmesh, IMaterial* material)
	{
		std::vector< std::vector<int32> >* _ShadersAttributesCache = &rmesh->ShadersAttributesCache[material->GetShader()];
		if ((*_ShadersAttributesCache).size()==0)
		{
			// Reset Attribute IDs
			for (std::vector<AttributeArray*>::iterator i = rmesh->Geometry->Attributes.begin(); i != rmesh->Geometry->Attributes.end(); i++)
			{
				std::vector<int32> attribs;
				for (std::vector<VertexAttribute*>::iterator k = (*i)->Attributes.begin(); k != (*i)->Attributes.end(); k++)
				{
					attribs.push_back(Shader::GetAttributeLocation(material->GetShader(), (*k)->Name));
				}
				(*_ShadersAttributesCache).push_back(attribs);
			}

			std::vector<int32>* _ShadersGlobalCache = &rmesh->ShadersGlobalCache[material->GetShader()];
			for (std::list<Uniform>::iterator k = material->GlobalUniforms.begin(); k != material->GlobalUniforms.end(); k++)
			{
				(*_ShadersGlobalCache).push_back(Shader::GetUniformLocation(material->GetShader(), (*k).Name));
			}

			std::vector<int32>* _ShadersModelCache = &rmesh->ShadersModelCache[material->GetShader()];
			for (std::list<Uniform>::iterator k = material->ModelUniforms.begin(); k != material->ModelUniforms.end(); k++)
			{
				(*_ShadersModelCache).push_back(Shader::GetUniformLocation(material->GetShader(), (*k).Name));
			}

			std::vector<int32>* _ShadersUserCache = &rmesh->ShadersUserCache[material->GetShader()];
			for (std::list<Uniform>::iterator k = material->UserUniforms.begin(); k != material->UserUniforms.end(); k++)
			{
				(*_ShadersUserCache).push_back(Shader::GetUniformLocation(material->GetShader(), (*k).Name));
			}
		}
	}

	void IRenderer::UnbindMesh(RenderingMesh* rmesh, IMaterial* material)
	{
		// Disable Attributes
		if (rmesh->Geometry->Attributes.size() > 0)
		{
			std::vector< std::vector<int32> >* _ShadersAttributesCache = &rmesh->ShadersAttributesCache[material->GetShader()];
			uint32 counterBuffers = 0;
			for (std::vector<AttributeArray*>::iterator k = rmesh->Geometry->Attributes.begin(); k != rmesh->Geometry->Attributes.end(); k++)
			{
				uint32 counter = 0;
				for (std::vector<VertexAttribute*>::iterator l = (*k)->Attributes.begin(); l != (*k)->Attributes.end(); l++)
				{
					// If exists in shader
					if ((*_ShadersAttributesCache)[counterBuffers][counter] >= 0)
					{
						GLCHECKER(glDisableVertexAttribArray((*_ShadersAttributesCache)[counterBuffers][counter]));
					}
					counter++;
				}
				counterBuffers++;
			}
			if (rmesh->Geometry->GetGeometryType() == GeometryType::BUFFER)
				GLCHECKER(glBindBuffer(GL_ARRAY_BUFFER, 0));
		}
	}

	void IRenderer::BindShadowMaps(IMaterial* material)
	{
		// Bind Shadows Textures
		if (material->IsCastingShadows())
		{
			DirectionalShadowMapsUnits.clear();
			for (std::vector<Texture*>::iterator i = DirectionalShadowMapsTextures.begin(); i != DirectionalShadowMapsTextures.end(); i++)
			{
				(*i)->Bind();
				DirectionalShadowMapsUnits.push_back(Texture::GetLastBindedUnit());
			}

			PointShadowMapsUnits.clear();
			for (std::vector<Texture*>::iterator i = PointShadowMapsTextures.begin(); i != PointShadowMapsTextures.end(); i++)
			{
				(*i)->Bind();
				PointShadowMapsUnits.push_back(Texture::GetLastBindedUnit());
			}

			SpotShadowMapsUnits.clear();
			for (std::vector<Texture*>::iterator i = SpotShadowMapsTextures.begin(); i != SpotShadowMapsTextures.end(); i++)
			{
				(*i)->Bind();
				SpotShadowMapsUnits.push_back(Texture::GetLastBindedUnit());
			}
		}
	}

	void IRenderer::UnbindShadowMaps(IMaterial* material)
	{
		// Unbind Shadows Textures
		if (material->IsCastingShadows())
		{
			// Spot Lights
			for (std::vector<Texture*>::reverse_iterator i = SpotShadowMapsTextures.rbegin(); i != SpotShadowMapsTextures.rend(); i++)
			{
				(*i)->Unbind();
			}
			// Point Lights
			for (std::vector<Texture*>::reverse_iterator i = PointShadowMapsTextures.rbegin(); i != PointShadowMapsTextures.rend(); i++)
			{
				(*i)->Unbind();
			}
			// Directional Lights
			for (std::vector<Texture*>::reverse_iterator i = DirectionalShadowMapsTextures.rbegin(); i != DirectionalShadowMapsTextures.rend(); i++)
			{
				(*i)->Unbind();
			}
		}
	}

	void IRenderer::SendAttributes(RenderingMesh* rmesh, IMaterial* material)
	{
		// Check if custom Attributes exists
		if (rmesh->Geometry->Attributes.size() > 0)
		{
			if (rmesh->Geometry->GetGeometryType() == GeometryType::BUFFER)
			{

				// VBO
				uint32 counterBuffers = 0;
				for (std::vector<AttributeArray*>::iterator k = rmesh->Geometry->Attributes.begin(); k != rmesh->Geometry->Attributes.end(); k++)
				{

					AttributeBuffer* bf = (AttributeBuffer*)(*k);

					// Bind VAO
					GLCHECKER(glBindBuffer(GL_ARRAY_BUFFER, bf->Buffer->ID));

					// Get Struct Data
					if (bf->attributeSize == 0)
					{
						for (std::vector<VertexAttribute*>::iterator l = (*k)->Attributes.begin(); l != (*k)->Attributes.end(); l++)
						{
							bf->attributeSize += (*l)->byteSize;
						}
					}

					// Counter
					uint32 counter = 0;
					std::vector< std::vector<int32> >* _ShadersAttributesCache = &rmesh->ShadersAttributesCache[material->GetShader()];
					for (std::vector<VertexAttribute*>::iterator l = (*k)->Attributes.begin(); l != (*k)->Attributes.end(); l++)
					{
						// Check if is not set
						if ((*_ShadersAttributesCache)[counterBuffers][counter] == -2)
						{
							// set VAO ID
							(*_ShadersAttributesCache)[counterBuffers][counter] = Shader::GetAttributeLocation(material->GetShader(), (*l)->Name);

						}
						// If exists in shader
						if ((*_ShadersAttributesCache)[counterBuffers][counter] >= 0)
						{
							AttributeBuffer* bf = (AttributeBuffer*)(*k);
							GLCHECKER(glVertexAttribPointer(
								(*_ShadersAttributesCache)[counterBuffers][counter],
								Buffer::Attribute::GetTypeCount((*l)->Type),
								Buffer::Attribute::GetType((*l)->Type),
								GL_FALSE,
								bf->attributeSize,
								BUFFER_OFFSET((*l)->Offset)
								));

							// Enable Attribute
							GLCHECKER(glEnableVertexAttribArray((*_ShadersAttributesCache)[counterBuffers][counter]));
						}
						counter++;
					}
					counterBuffers++;
				}
			}
			else {

				// Arrays
				uint32 counterBuffers = 0;
				std::vector< std::vector<int32> >* _ShadersAttributesCache = &rmesh->ShadersAttributesCache[material->GetShader()];
				for (std::vector<AttributeArray*>::iterator k = rmesh->Geometry->Attributes.begin(); k != rmesh->Geometry->Attributes.end(); k++)
				{

					// Counter
					uint32 counter = 0;
					for (std::vector<VertexAttribute*>::iterator l = (*k)->Attributes.begin(); l != (*k)->Attributes.end(); l++)
					{
						// Check if is not set
						if ((*_ShadersAttributesCache)[counterBuffers][counter] == -2)
						{
							// set VAO ID
							(*_ShadersAttributesCache)[counterBuffers][counter] = Shader::GetAttributeLocation(material->GetShader(), (*l)->Name);

						}
						// If exists in shader
						if ((*_ShadersAttributesCache)[counterBuffers][counter] >= 0)
						{

							GLCHECKER(glEnableVertexAttribArray((*_ShadersAttributesCache)[counterBuffers][counter]));
							GLCHECKER(glVertexAttribPointer(
								(*_ShadersAttributesCache)[counterBuffers][counter],
								Buffer::Attribute::GetTypeCount((*l)->Type),
								Buffer::Attribute::GetType((*l)->Type),
								GL_FALSE,
								0,
								&(*l)->Data[0]
								));

							// Enable Attribute
							GLCHECKER(glEnableVertexAttribArray((*_ShadersAttributesCache)[counterBuffers][counter]));
						}
						counter++;
					}
					counterBuffers++;
				}
			}
		}
	}

};
