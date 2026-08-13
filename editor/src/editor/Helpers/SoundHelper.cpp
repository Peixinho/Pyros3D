//============================================================================
// Name        : SoundHelper.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Viewport icon for AudioSource components
//============================================================================

#include "SoundHelper.h"

using namespace p3d;

uint32 SoundHelper::SoundResourcesCounter = 0;
std::shared_ptr<GenericShaderMaterial> SoundHelper::material;
std::shared_ptr<Texture> SoundHelper::texture;
std::shared_ptr<Renderable> SoundHelper::handle;

SoundHelper::SoundHelper(GameObject* owner) : IHelper(HELPER_TYPE::SOUND)
{
	this->owner = owner;

	if (SoundResourcesCounter == 0)
	{
		handle = std::make_shared<Plane>(1, 1);
		texture = std::make_shared<Texture>();
		texture->LoadTexture("assets/sound.png", TextureType::Texture);
		material = std::make_shared<GenericShaderMaterial>(ShaderUsage::Texture);
		material->DisableDepthTest();
		material->DisableDepthWrite();
		material->SetColorMap(texture);
		material->SetTransparencyFlag(true);
		material->SetCullFace(CullFace::DoubleSided);
	}

	SoundResourcesCounter++;

	rcomp = std::make_shared<RenderingComponent>(handle, material);
	rcomp->DisableCastShadows();

	Add(rcomp);
}

void SoundHelper::Update(GameObject* Camera, Matrix projection, bool isPerspective, f32 right, f32 top)
{
	SetPosition(owner->GetWorldPosition());
	LookAt(Camera);
	f32 distance;
	if (isPerspective) distance = Camera->GetWorldPosition().distance(owner->GetWorldPosition());
	else distance = Max(right, top) * 1.3f;
	SetScale(Vec3(1.0f, 1.0f, 1.0f) * distance * 0.03f);
}

SoundHelper::~SoundHelper()
{
	if (rcomp && rcomp->GetOwner() == this)
		Remove(rcomp);
	rcomp.reset();

	if (--SoundResourcesCounter == 0)
	{
		material.reset();
		texture.reset();
		handle.reset();
	}
}
