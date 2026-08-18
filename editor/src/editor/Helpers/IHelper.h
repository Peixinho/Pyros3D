//============================================================================
// Name        : LightHelper.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Light helper
//============================================================================

#ifndef IHELPER_H
#define	IHELPER_H

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <memory>

using namespace p3d;

namespace HELPER_TYPE
{
	enum {
		GAMEOBJECT,
		LIGHT,
		SOUND,
		PARTICLES
	};
}

class IHelper : public GameObject {
public:
	IHelper(uint32 Type) { type = Type; }
	virtual void Update(GameObject *Camera, Matrix projection, bool isPerspective, f32 right, f32 top) = 0;
	std::shared_ptr<RenderingComponent> rcomp;
	GameObject* owner;
	uint32 type;
};

#endif	/* IHELPER_H */
