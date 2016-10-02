//============================================================================
// Name        : Vr_shootIngrange.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rotating Cube Example
//============================================================================

#ifndef VR_SHOOTINGRANGE_H
#define	VR_SHOOTINGRANGE_H

#if defined(_SDL)
#include "WindowManagers/SDL/SDLContext.h"
#define ClassName SDLContext
#elif defined(_SDL2)
#include "WindowManagers/SDL2/SDL2Context.h"
#define ClassName SDL2Context
#else
#include "WindowManagers/SFML/SFMLContext.h"
#define ClassName SFMLContext
#endif

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Utils/Colors/Colors.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/VR/VR_Renderer.h>
#include <Pyros3D/Assets/Sounds/Sound.h>
#include <Pyros3D/Assets/Renderable/Decals/Decals.h>

using namespace p3d;

namespace TYPE_GAME {
	enum {
		None = 0,
		Precision,
		Skill
	};
}

class VR_ShootingRange : public ClassName
{

public:

	VR_ShootingRange();
	virtual ~VR_ShootingRange();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);

private:

	bool GetIntersectedTriangle(const Vec3 &Origin, const Vec3 &Direction, RenderingComponent* rcomp, Vec3* intersection, Vec3* normal);
	bool rayIntersectionTriangle(const Vec3 &Origin, const Vec3 &Direction, const Vec3 &v1, const Vec3 &v2, const Vec3 &v3, Vec3 *IntersectionPoint32, f32 *t) const;

	void StartPrecisionGame();
	void StartSkillGame();
	void ClearDecals();
	void ClearScore();
	void ClearTargets();

	void Shoot();
	void PrecisionStep1();
	void PrecisionStep2();
	void PrecisionStep3();
	void PrecisionStep4();
	void PrecisionStep5();

	// Scene
	SceneGraph* Scene;

	// Virtual Reality
	VR_Renderer* vr;

	Model* shootingHouseHandle;
	GameObject* shootingHouseGO;
	RenderingComponent* shootingHouseRComp;

	// Lights
	PointLight* pLight;
	SpotLight* sLight;
	GameObject* pLightGO;
	GameObject* sLightGO;

	// Gun
	GameObject* GunGO;
	RenderingComponent* GunRComp;
	Model* GunHandle;

	// Fire From Gun
	GameObject* GunFireGO;
	RenderingComponent* GunFireComp;
	Model* GunFireHandle;

	// Materials
	GenericShaderMaterial* lightsOn;
	GenericShaderMaterial* decalMaterial;

	// Target
	Texture* targetTexture;
	GenericShaderMaterial* targetMaterial;
	Cube* targetHandle;
	RenderingComponent* targetRComp;
	GameObject* targetGO;
	Texture *continueTexture, *gameOverTexture;
	Plane* continueHandle;
	GenericShaderMaterial* continueMaterial;
	RenderingComponent* continueRComp;
	GameObject* continueGO;

	// Score
	Model* scoreHandle;
	RenderingComponent* scoreRComp;
	GameObject* scoreGO;

	// Options
	Texture *skillTexture, *precisionTexture, *exitTexture;
	Plane *optionHandle;
	GenericShaderMaterial *skillMaterial, *precisionMaterial, *exitMaterial;
	RenderingComponent *skillRComp, *precisionRComp, *exitRComp;
	GameObject *skillGO, *precisionGO, *exitGO;

	// Score values and labels
	GameObject* scoreTypeGameGO;
	RenderingComponent* scoreTypeGameRComp;
	GenericShaderMaterial *scoreTypeGameMat;
	Texture *pts5, *pts6, *pts7, *pts8, *pts9, *pts10, *mts5, *mts10, *mts20, *mts30, *mts40;
	GenericShaderMaterial *pts5Mat, *pts6Mat, *pts7Mat, *pts8Mat, *pts9Mat, *pts10Mat, *mts5Mat, *mts10Mat, *mts20Mat, *mts30Mat, *mts40Mat;
	Plane* ptsHandle, *mtsHandle;
	GameObject *mts5GO, *mts10GO, *mts20GO, *mts30GO, *mts40GO;
	RenderingComponent *mts5RComp, *mts10RComp, *mts20RComp, *mts30RComp, *mts40RComp;
	std::vector<GameObject*> ptsGO;
	std::vector<RenderingComponent*> ptsRComp;
	std::vector<Texture*> ptsTextures;
	std::vector<GenericShaderMaterial*> ptsMat;

	// Decals
	std::vector<GameObject*> DecalsGO;
	std::vector<RenderingComponent*> DecalsRComp;
	Plane* decalHandle;
	Texture* decalTexture;

	// Sound
	Sound* shotSound;

	bool isShooting;
	bool precisionWaitingShoot;
	uint64 rumbleTime;
	uint64 gunFireShowTime;
	uint32 typeGame;
	uint32 shotCount;
	uint32 precisionShotCount;
	uint32 precisionStep;

	// Skill Game
	std::vector<GameObject*> targetsGO;
	std::vector<RenderingComponent*> targetsRComp;
	uint64 initTime;
	uint64 endTime;
	uint64 timeDeploy;
	bool skillOver;
	GameObject* clockGO;
	RenderingComponent* clockRComp;
	Model* clockHandle;
	Model *ckockPointerHandle;
	RenderingComponent *minutePointerRComp, *secondsPointerRComp;
	GenericShaderMaterial *minutePointerMaterial, *secondPointerMaterial;
	GameObject* minutePointerGO, *secondPointerGO;
	Texture* clockBackground;
	GameObject* clockBackgroundGO;
	RenderingComponent* clockBackgroundRComp;
	Plane* clockBackgroundHandle;
	GenericShaderMaterial* clockBackgroundMaterial;

	// FinalScore
	uint32 finalScore;

	// Numbers for score
	std::vector<Texture*> numbersTextures;

};

#endif	/* VR_SHOOTINGRANGE_H */

