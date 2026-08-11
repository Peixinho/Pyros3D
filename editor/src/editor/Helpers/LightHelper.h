//============================================================================
// Name        : LightHelper.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Light helper
//============================================================================

#ifndef LIGHTHELPER_H
#define	LIGHTHELPER_H

#include "IHelper.h"

using namespace p3d;

class LightHelper : public IHelper {
public:

	LightHelper(GameObject* Owner);
	virtual ~LightHelper();

	virtual void Update(GameObject *Camera, Matrix projection, bool isPerspective, f32 right, f32 top);

	static std::shared_ptr<GenericShaderMaterial> material;
	static std::shared_ptr<Texture> texture;
	static std::shared_ptr<Renderable> handle;
	static uint32 LightResourcesCounter;
	
};

#endif	/* LIGHTHELPER_H */
