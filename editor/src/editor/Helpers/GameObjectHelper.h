//============================================================================
// Name        : GameObjectHelper.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : GameObject helper
//============================================================================

#ifndef GAMEOBJECTHELPER_H
#define	GAMEOBJECTHELPER_H

#include "IHelper.h"
using namespace p3d;

class GameObjectHelper : public IHelper {
public:

	GameObjectHelper(GameObject* Owner);
	virtual ~GameObjectHelper();

	virtual void Update(GameObject *Camera, Matrix projection, bool isPerspective, f32 right, f32 top);

	static std::shared_ptr<GenericShaderMaterial> material;
	static std::shared_ptr<Texture> texture;
	static std::shared_ptr<Renderable> handle;
	static uint32 GameObjectResourcesCounter;
	
};

#endif	/* GAMEOBJECTHELPER_H */
