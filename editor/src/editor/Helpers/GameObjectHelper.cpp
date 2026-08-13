//============================================================================
// Name        : GameObjectHelper.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : GameObject Helper
//============================================================================

#include "GameObjectHelper.h"

using namespace p3d;

uint32 GameObjectHelper::GameObjectResourcesCounter = 0;
std::shared_ptr<GenericShaderMaterial> GameObjectHelper::material;
std::shared_ptr<Texture> GameObjectHelper::texture;
std::shared_ptr<Renderable> GameObjectHelper::handle;

GameObjectHelper::GameObjectHelper(GameObject* owner) : IHelper(HELPER_TYPE::GAMEOBJECT)
{
	this->owner = owner;

	if (GameObjectResourcesCounter == 0)
	{
		handle = std::make_shared<Plane>(1, 1);
		texture = std::make_shared<Texture>();
		// .png, not the .dds this used to load. The engine did decode DDS
		// once (Texture::LoadDDS, added in f45680b) but 5321eb4 removed it,
		// leaving .dds to fall through to stb_image, which has no DDS
		// support - see the note where those declarations used to be in
		// Texture.h. Second argument is a TextureType, not a ShaderUsage:
		// ShaderUsage::Diffuse passed 16384, far outside that enum.
		texture->LoadTexture("assets/gameobject.png", TextureType::Texture);
		material = std::make_shared<GenericShaderMaterial>(ShaderUsage::Texture);
		material->DisableDepthTest();
		material->DisableDepthWrite();
		material->SetColorMap(texture);
		material->SetTransparencyFlag(true);
		material->SetCullFace(CullFace::DoubleSided);
	}

	GameObjectResourcesCounter++;

	rcomp = std::make_shared<RenderingComponent>(handle, material);
	rcomp->DisableCastShadows();

	Add(rcomp);
}

void GameObjectHelper::Update(GameObject* Camera, Matrix projection, bool isPerspective, f32 right, f32 top)
{
	SetPosition(owner->GetWorldPosition());
	LookAt(Camera);
	f32 distance;
	if (isPerspective) distance = Camera->GetWorldPosition().distance(owner->GetWorldPosition());
	else distance = Max(right, top) *1.3;
	SetScale(Vec3(1.0,1.0,1) * distance * 0.03);
}

GameObjectHelper::~GameObjectHelper()
{
	if (rcomp && rcomp->GetOwner() == this)
		Remove(rcomp);
	rcomp.reset();

	if (--GameObjectResourcesCounter == 0)
	{
		material.reset();
		texture.reset();
		handle.reset();
	}
}