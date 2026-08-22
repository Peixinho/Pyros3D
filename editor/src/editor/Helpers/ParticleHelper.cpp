//============================================================================
// Name        : ParticleHelper.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Viewport icon for ParticleSystem components
//============================================================================

#include "ParticleHelper.h"

using namespace p3d;

uint32 ParticleHelper::ParticleResourcesCounter = 0;
std::shared_ptr<GenericShaderMaterial> ParticleHelper::material;
std::shared_ptr<Texture> ParticleHelper::texture;
std::shared_ptr<Renderable> ParticleHelper::handle;

ParticleHelper::ParticleHelper(GameObject* owner) : IHelper(HELPER_TYPE::PARTICLES)
{
	this->owner = owner;

	if (ParticleResourcesCounter == 0)
	{
		handle = std::make_shared<Plane>(1, 1);
		texture = std::make_shared<Texture>();
		texture->LoadTexture("assets/particles.png", TextureType::Texture);
		material = std::make_shared<GenericShaderMaterial>(ShaderUsage::Texture);
		material->DisableDepthTest();
		material->DisableDepthWrite();
		material->SetColorMap(texture);
		material->SetTransparencyFlag(true);
		material->SetCullFace(CullFace::DoubleSided);
	}

	ParticleResourcesCounter++;

	rcomp = std::make_shared<RenderingComponent>(handle, material);
	rcomp->DisableCastShadows();

	Add(rcomp);
}

void ParticleHelper::Update(GameObject* Camera, Matrix projection, bool isPerspective, f32 right, f32 top)
{
	SetPosition(owner->GetWorldPosition());
	LookAt(Camera);
	f32 distance;
	if (isPerspective) distance = Camera->GetWorldPosition().distance(owner->GetWorldPosition());
	else distance = Max(right, top) * 1.3f;
	SetScale(Vec3(1.0f, 1.0f, 1.0f) * distance * 0.03f);
}

ParticleHelper::~ParticleHelper()
{
	if (rcomp && rcomp->GetOwner() == this)
		Remove(rcomp);
	rcomp.reset();

	if (--ParticleResourcesCounter == 0)
	{
		material.reset();
		texture.reset();
		handle.reset();
	}
}
