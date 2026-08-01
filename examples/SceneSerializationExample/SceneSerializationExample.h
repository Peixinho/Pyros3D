//============================================================================
// Name        : SceneSerializationExample.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Scene save/load verification - real round trip, not a demo
//============================================================================

#ifndef SCENE_SERIALIZATION_EXAMPLE_H
#define	SCENE_SERIALIZATION_EXAMPLE_H

#define _STR(path) #path
#define STR(path) _STR(path)

#if defined(_SDL)
#include "../WindowManagers/SDL/SDLContext.h"
#define ClassName SDLContext
#elif defined(_SDL2VULKAN)
#include "../WindowManagers/SDL2Vulkan/SDL2VulkanContext.h"
#define ClassName SDL2VulkanContext
#else
#include "../WindowManagers/SDL2/SDL2Context.h"
#define ClassName SDL2Context
#endif

#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Physics/Physics.h>
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>

using namespace p3d;

class SceneSerializationExample : public ClassName {

public:

	SceneSerializationExample();
	virtual ~SceneSerializationExample();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);

private:

	void BuildScene();
	void RunRoundTripAndVerify();

	SceneGraph* scene;
	ForwardRenderer* renderer;
	Projection projection;
	Physics* physics;
	GameObject* camera;

	uint32 frameCount;
	bool verified;
};

#endif	/* SCENE_SERIALIZATION_EXAMPLE_H */
