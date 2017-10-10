//============================================================================
// Name        : RacingGame.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : RacingGame Example
//============================================================================

#ifndef RACINGGAME_H
#define	RACINGGAME_H

//#if defined(_SDL)
//#include "../WindowManagers/SDL/SDLContext.h"
//#define ClassName SDLContext
//#elif defined(_SDL2)
//#include "../WindowManagers/SDL2/SDL2Context.h"
//#define ClassName SDL2Context
//#else
//#include "../WindowManagers/SFML/SFMLContext.h"
//#define ClassName SFMLContext
//#endif

#include "imgui/imgui_impl_sfml.h"
#define ClassName imguiContext

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/Assets/Renderable/Models/Model.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Utils/Colors/Colors.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#include <Pyros3D/Physics/Physics.h>
#include <Pyros3D/Utils/Mouse3D/PainterPick.h>
#include <Pyros3D/Assets/Renderable/Text/Text.h>
#include <Pyros3D/Rendering/Renderer/SpecialRenderers/CubemapRenderer/CubemapRenderer.h>
#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>
#include <Pyros3D/Physics/Components/IPhysicsComponent.h>
#include <Pyros3D/Physics/Components/Vehicle/PhysicsVehicle.h>
#include <Pyros3D/Physics/Components/TriangleMesh/PhysicsTriangleMesh.h>
#include <Pyros3D/Utils/ModelLoaders/MultiModelLoader/ModelLoader.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/Assets/Sounds/Sound.h>
using namespace p3d;

inline float clamp(float x, float a, float b)
{
	return x < a ? a : (x > b ? b : x);
}

namespace TERRAIN {
	enum {
		ASPHALT = 0,
		GRASS,
		SAND
	};
}

struct Portal {
	RenderingComponent* rPortal;
	GameObject* gPortal;
	IPhysicsComponent* pPortal;
	int32 portalPassage;
	Vec3 direction;
};

class RacingGame : public ClassName {
public:

	RacingGame();
	virtual ~RacingGame();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);
private:

	// Scene
	SceneGraph* Scene;
	// Renderer
	ForwardRenderer* Renderer;
	// Projection
	Projection projection;
	// Physics
	Physics* physics;
	// Cubemap renderer
	CubemapRenderer* dRenderer;
	
	// Camera - Its a regular GameObject
	GameObject* Camera, *FollowCamera, *HoodCamera;
	Vec3 CameraPosition;
	bool _followCamera;

	// Geometry Handles
	Renderable *trackHandle, *skyboxHandle, *carHandle;
	
	// Track GameObject
	GameObject* Track;
	// Track Component
	RenderingComponent* rTrack;
	// Physics Component
	PhysicsTriangleMesh* pTrack, *pSand, *pGrass, *pRestTrack;
	// Light GameObject
	GameObject* Light;
	// Light Component
	DirectionalLight* dLight;

	// Car GameObject
	GameObject* Car;
	// Car Components
	RenderingComponent* rCar;
	PhysicsVehicle* carPhysics;

	// Skybox GameObject
	GameObject* Skybox;
	// Skybox Component
	RenderingComponent* rSkybox;
	// Skybox Material
	GenericShaderMaterial* SkyboxMaterial;

	// Wheels
	GameObject *gWheelFL, *gWheelFR, *gWheelBL, *gWheelBR;
	RenderingComponent *rWheelFL, *rWheelFR, *rWheelBL, *rWheelBR;
	Renderable *wheelFLHandle, *wheelFRHandle, *wheelBLHandle, *wheelBRHandle;

	void CloseApp(Event::Input::Info e);

	void LeftUp(Event::Input::Info e);
	void LeftDown(Event::Input::Info e);
	void RightUp(Event::Input::Info e);
	void RightDown(Event::Input::Info e);
	void UpUp(Event::Input::Info e);
	void UpDown(Event::Input::Info e);
	void DownUp(Event::Input::Info e);
	void DownDown(Event::Input::Info e);
	void SpaceUp(Event::Input::Info e);
	void SpaceDown(Event::Input::Info e);
	void AnalogicMove(Event::Input::Info e);

	void ChangeCamera(Event::Input::Info e);
	void Reset(Event::Input::Info e);

	bool _upPressed, _downPressed, _leftPressed, _rightPressed, _brakePressed;
	float gVehicleSteering, steeringIncrement;

	std::vector<Portal> portals;
	Renderable* planeHandle;

	float timeInterval;

	Sound* sound;
	Sound* crash;

	void addPortal(const Vec3 &pos, const Vec3 &rot);

	IMaterial *brakingMat, *defaultBrakingMat;

	void LightBrakesON();
	void LightBrakesOFF();

	// Shadow Debugging
	f32 a, b, c;
	ImVec4 clear_color;

	// Racing Time
	bool raceStart;
	int32 portalNumber;
	f32 raceInitTime, raceTime;
	f32 lapInitTime;
	std::vector<f32> lapTime;
	f32 sectorAInitTime, sectorATime;
	f32 sectorBInitTime, sectorBTime;
	f32 sectorCInitTime, sectorCTime;
	int lap, lapLimit;

	bool startedDrivingLikeAGirl;
	f32 timeStartedDrivingLikeAGirl;
	Texture* startedDrivingLikeAGirlTexture;
};

#endif	/* RACINGGAME_H */