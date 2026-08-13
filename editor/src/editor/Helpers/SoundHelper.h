//============================================================================
// Name        : SoundHelper.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Viewport icon for AudioSource components
//============================================================================

#ifndef SOUNDHELPER_H
#define	SOUNDHELPER_H

#include "IHelper.h"

using namespace p3d;

class SoundHelper : public IHelper {
public:

	SoundHelper(GameObject* Owner);
	virtual ~SoundHelper();

	virtual void Update(GameObject *Camera, Matrix projection, bool isPerspective, f32 right, f32 top);

	static std::shared_ptr<GenericShaderMaterial> material;
	static std::shared_ptr<Texture> texture;
	static std::shared_ptr<Renderable> handle;
	static uint32 SoundResourcesCounter;
};

#endif	/* SOUNDHELPER_H */
