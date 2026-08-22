//============================================================================
// Name        : ParticleHelper.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Viewport icon for ParticleSystem components
//============================================================================

#ifndef PARTICLEHELPER_H
#define	PARTICLEHELPER_H

#include "IHelper.h"

using namespace p3d;

class ParticleHelper : public IHelper {
public:

	ParticleHelper(GameObject* Owner);
	virtual ~ParticleHelper();

	virtual void Update(GameObject *Camera, Matrix projection, bool isPerspective, f32 right, f32 top);

	static std::shared_ptr<GenericShaderMaterial> material;
	static std::shared_ptr<Texture> texture;
	static std::shared_ptr<Renderable> handle;
	static uint32 ParticleResourcesCounter;
};

#endif	/* PARTICLEHELPER_H */
