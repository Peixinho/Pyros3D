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

	ForwardRenderer::ForwardRenderer(const uint32 Width, const uint32 Height) : IRenderer(Width, Height)
	{
		echo("SUCCESS: Forward Renderer Created");

		ActivateCulling(CullingMode::FrustumCulling);

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
};