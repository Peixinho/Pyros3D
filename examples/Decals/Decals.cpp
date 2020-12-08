//============================================================================
// Name        : Decals.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "Decals.h"

using namespace p3d;

Decals::Decals() : BaseExample(1024, 768, "Pyros3D - Decals", WindowType::Close | WindowType::Resize) {}

void Decals::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	BaseExample::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 1000.f);
}

void Decals::Init()
{
	// Initialization
	BaseExample::Init();

	ShowMouse();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 1000.f);

	// Create Game Object
	CubeObject = new GameObject();
	SphereObject = new GameObject();
	ModelObject = new GameObject();

	cubeMesh = new Cube(30, 30, 30);
	sphereMesh = new Sphere(30, 16, 16);
	modelMesh = new Model(STR(EXAMPLES_PATH)"/assets/teapotLOD1.p3dm");

	rCube = new RenderingComponent(cubeMesh);
	rSphere = new RenderingComponent(sphereMesh);
	rModel = new RenderingComponent(modelMesh);

	CubeObject->Add(rCube);
	SphereObject->Add(rSphere);
	ModelObject->Add(rModel);

	CubeObject->SetPosition(Vec3(-100, 0, 0));
	SphereObject->SetPosition(Vec3(-20, 0, 0));
	ModelObject->SetPosition(Vec3(100, 0, 0));

	decalMaterial = new GenericShaderMaterial(ShaderUsage::Texture);
	Texture* texture = new Texture();
	texture->LoadTexture(STR(EXAMPLES_PATH)"/assets/pyros.png", TextureType::Texture);
	decalMaterial->SetColorMap(texture);
	decalMaterial->SetTransparencyFlag(true);
	decalMaterial->EnableDethBias(-4, -4);
	decalMaterial->DisableDepthWrite();

	// Add GameObject to Scene
	Scene->Add(CubeObject);
	Scene->Add(SphereObject);
	Scene->Add(ModelObject);

	// Input
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Mouse::Left, this, &Decals::OnMouseRelease);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::W, this, &Decals::MoveFrontPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::S, this, &Decals::MoveBackPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::A, this, &Decals::StrafeLeftPress);
	InputManager::AddEvent(Event::Type::OnPress, Event::Input::Keyboard::D, this, &Decals::StrafeRightPress);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::W, this, &Decals::MoveFrontRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::S, this, &Decals::MoveBackRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::A, this, &Decals::StrafeLeftRelease);
	InputManager::AddEvent(Event::Type::OnRelease, Event::Input::Keyboard::D, this, &Decals::StrafeRightRelease);
	InputManager::AddEvent(Event::Type::OnMove, Event::Input::Mouse::Move, this, &Decals::LookTo);

	SetMousePosition((uint32)(Width *.5f), (uint32)(Height *.5f));
	mouseCenter = Vec2((f32)Width *.5f, (f32)Height *.5f);
	mouseLastPosition = mouseCenter;
	counterX = counterY = 0.f;

	_strafeLeft = _strafeRight = _moveBack = _moveFront = 0.f;
}

void Decals::Update()
{
	// Update - Game Loop

	// Update Scene
	Scene->Update(GetTime());

	// Game Logic Here
	CubeObject->SetRotation(Vec3(0.f, (f32)GetTime(), 0.f));
	SphereObject->SetRotation(Vec3(0.f, (f32)GetTime(), 0.f));
	ModelObject->SetRotation(Vec3(0.f, (f32)GetTime()*.5f, 0.f));

	// Render Scene
	Renderer->PreRender(FPSCamera, Scene);
	Renderer->RenderScene(projection, FPSCamera, Scene);

	Vec3 finalPosition;
	Vec3 direction = FPSCamera->GetDirection();
	f64 dt = GetTimeInterval();
	f32 speed = (f32)dt * 200.f;
	if (_moveFront)
	{
		finalPosition -= direction*speed;
	}
	if (_moveBack)
	{
		finalPosition += direction*speed;
	}
	if (_strafeLeft)
	{
		finalPosition += direction.cross(Vec3(0, 1, 0)).normalize()*speed;
	}
	if (_strafeRight)
	{
		finalPosition -= direction.cross(Vec3(0, 1, 0)).normalize()*speed;
	}

	FPSCamera->SetPosition(FPSCamera->GetPosition() + finalPosition);

}

void Decals::Shutdown()
{
	// All your Shutdown Code Here

	// Remove GameObjects From Scene
	Scene->Remove(CubeObject);
	Scene->Remove(SphereObject);
	Scene->Remove(ModelObject);
	Scene->Remove(FPSCamera);

	CubeObject->Remove(rCube);
	SphereObject->Remove(rSphere);
	ModelObject->Remove(rModel);

	// Delete
	delete cubeMesh;
	delete sphereMesh;
	delete modelMesh;
	delete rCube;
	delete rSphere;
	delete rModel;
	delete CubeObject;
	delete SphereObject;
	delete ModelObject;
	delete decalMaterial;
	for (std::vector<RenderingComponent*>::iterator i = rdecals.begin(); i != rdecals.end(); i++) delete *i;
	for (std::vector<DecalGeometry*>::iterator i = decals.begin(); i != decals.end(); i++) delete *i;
	delete Renderer;
}

Decals::~Decals() {}

void Decals::OnMouseRelease(Event::Input::Info e) {
	CreateDecal();
}

bool Decals::GetIntersectedTriangle(RenderingComponent* rcomp, Mouse3D mouse, Vec3* intersection, Vec3* normal, uint32* meshID)
{
	Vec3 _intersection, finalIntersection;
	f32 t, dist;
	uint32 _meshID;
	bool init = false;
	Vec3 _normal;

	for (size_t k = 0; k < rcomp->GetMeshes().size(); k++)
		for (size_t i = 0; i < rcomp->GetMeshes()[k]->Geometry->GetIndexData().size(); i += 3)
		{
			if (mouse.rayIntersectionTriangle(
				rcomp->GetMeshes()[k]->Geometry->GetVertexData()[rcomp->GetMeshes()[k]->Geometry->GetIndexData()[i]],
				rcomp->GetMeshes()[k]->Geometry->GetVertexData()[rcomp->GetMeshes()[k]->Geometry->GetIndexData()[i + 1]],
				rcomp->GetMeshes()[k]->Geometry->GetVertexData()[rcomp->GetMeshes()[k]->Geometry->GetIndexData()[i + 2]],
				&_intersection,
				&t
				))
			{

				Vec3 forward = FPSCamera->GetDirection().negate();
				if (forward.dotProduct(rcomp->GetOwner()->GetWorldTransformation() * _intersection - FPSCamera->GetWorldPosition()) < 0) continue;

				if (!init) {
					finalIntersection = _intersection;
					_normal = rcomp->GetMeshes()[k]->Geometry->GetNormalData()[rcomp->GetMeshes()[k]->Geometry->GetIndexData()[i]];
					dist = t;
					init = true;
					_meshID = k;
					continue;
				}
				f32 dist2 = t;
				if (dist2 < dist)
				{
					dist = dist2;
					finalIntersection = _intersection;
					_normal = rcomp->GetMeshes()[k]->Geometry->GetNormalData()[rcomp->GetMeshes()[k]->Geometry->GetIndexData()[i]];
					_meshID = k;
				}
			}
		}
	if (init) {
		*intersection = finalIntersection;
		*normal = _normal;
		*meshID = _meshID;
		return true;
	}
	return false;
}

void Decals::CreateDecal()
{
	Mouse3D mouse = Mouse3D();

	Vec3 intersection, FinalIntersection, normal, FinalNormal;
	f32 dist = 1000000000;
	f32 t;
	uint32 meshID;
	RenderingComponent* rcomp = NULL;
	GameObject* gobj = NULL;

	mouse.GenerateRay((f32)Width, (f32)Height, InputManager::GetMousePosition().x, InputManager::GetMousePosition().y, SphereObject->GetWorldTransformation(), FPSCamera->GetWorldTransformation().Inverse(), projection.GetProjectionMatrix());
	if (mouse.rayIntersectionSphere(Vec3(0, 0, 0), 30, &intersection, &t))
	{
		if (GetIntersectedTriangle(rSphere, mouse, &intersection, &normal, &meshID))
		{
			rcomp = rSphere;
			gobj = SphereObject;
			FinalIntersection = intersection;
			dist = intersection.distanceSQR(FPSCamera->GetWorldPosition());
			FinalNormal = normal;
		}
	}
	mouse.GenerateRay((f32)Width, (f32)Height, InputManager::GetMousePosition().x, InputManager::GetMousePosition().y, CubeObject->GetWorldTransformation(), FPSCamera->GetWorldTransformation().Inverse(), projection.GetProjectionMatrix());
	if (mouse.rayIntersectionBox(rCube->GetBoundingMinValue(), rCube->GetBoundingMaxValue(), &t))
	{
		if (GetIntersectedTriangle(rCube, mouse, &intersection, &normal, &meshID))
		{
			f32 dist2 = intersection.distanceSQR(FPSCamera->GetWorldPosition());
			if (dist2 < dist)
			{
				rcomp = rCube;
				gobj = CubeObject;
				dist = dist2;
				FinalIntersection = intersection;
				FinalNormal = normal;
			}
		}
	}
	mouse.GenerateRay((f32)Width, (f32)Height, InputManager::GetMousePosition().x, InputManager::GetMousePosition().y, ModelObject->GetWorldTransformation(), FPSCamera->GetWorldTransformation().Inverse(), projection.GetProjectionMatrix());
	if (mouse.rayIntersectionBox(rModel->GetBoundingMinValue(), rModel->GetBoundingMaxValue(), &t))
	{
		if (GetIntersectedTriangle(rModel, mouse, &intersection, &normal, &meshID))
		{
			f32 dist2 = intersection.distanceSQR(FPSCamera->GetWorldPosition());
			if (dist2 < dist)
			{
				rcomp = rModel;
				gobj = ModelObject;
				dist = dist2;
				FinalIntersection = intersection;
				FinalNormal = normal;
			}
		}
	}

	if (rcomp != NULL)
	{
		Matrix m;
		m.LookAt(FinalIntersection, FinalNormal.negate(), Vec3(0, 1, 0));
		m = m.Inverse();
		m.Translate(FinalIntersection);

		DecalGeometry* decal = new DecalGeometry(rcomp->GetMeshes()[0], gobj->GetWorldTransformation(), m, Vec3(10, 10, 10));
		RenderingComponent* r = new RenderingComponent(decal->GetDecal(), decalMaterial);
		decals.push_back(decal);
		rdecals.push_back(r);
		gobj->Add(r);
	}
}
