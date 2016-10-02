//============================================================================
// Name        : VR_ShootingRange.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : VR Game
//============================================================================

#include "VR_ShootingRange.h"

using namespace p3d;

VR_ShootingRange::VR_ShootingRange() : ClassName(1024, 768, "Pyros3D - VR Shooting Range", WindowType::Close | WindowType::Resize) {}
void VR_ShootingRange::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	ClassName::OnResize(width, height);

	vr->Resize(width, height);
}
void VR_ShootingRange::Init()
{
	// Initialization

	typeGame = TYPE_GAME::None;

	// Initialize Scene
	Scene = new SceneGraph();

	vr = new VR_Renderer(0.001f, 100.f, true, Width, Height);
	vr->Init();

	//vr->RenderControllers(true);
	//vr->RenderTrackers(true);

	lightsOn = new GenericShaderMaterial(ShaderUsage::Color);
	lightsOn->SetColor(Vec4(1, 1, 1, 1));

	// House
	shootingHouseHandle = new Model("assets/ShootingHouse/vr_shootout.p3dm", true, ShaderUsage::SpotShadow | ShaderUsage::Diffuse);
	shootingHouseRComp = new RenderingComponent(shootingHouseHandle);
	shootingHouseRComp->GetMeshes()[6]->Material = lightsOn;
	shootingHouseGO = new GameObject();
	shootingHouseGO->Add(shootingHouseRComp);
	Scene->Add(shootingHouseGO);

	// Options
	skillTexture = new Texture();
	precisionTexture = new Texture();
	exitTexture = new Texture();
	skillTexture->LoadTexture("assets/options/skill.png");
	precisionTexture->LoadTexture("assets/options/precision.png");
	exitTexture->LoadTexture("assets/options/exit.png");
	optionHandle = new Plane(0.05, 0.05);
	skillMaterial = new GenericShaderMaterial(ShaderUsage::Texture);
	skillMaterial->SetColorMap(skillTexture);
	skillMaterial->SetTransparencyFlag(true);
	precisionMaterial = new GenericShaderMaterial(ShaderUsage::Texture);
	precisionMaterial->SetColorMap(precisionTexture);
	precisionMaterial->SetTransparencyFlag(true);
	exitMaterial = new GenericShaderMaterial(ShaderUsage::Texture);
	exitMaterial->SetColorMap(exitTexture);
	exitMaterial->SetTransparencyFlag(true);
	skillRComp = new RenderingComponent(optionHandle, skillMaterial);
	precisionRComp = new RenderingComponent(optionHandle, precisionMaterial);
	exitRComp = new RenderingComponent(optionHandle, exitMaterial);
	skillGO = new GameObject();
	skillGO->Add(skillRComp);
	precisionGO = new GameObject();
	precisionGO->Add(precisionRComp);
	exitGO = new GameObject();
	exitGO->Add(exitRComp);
	skillGO->SetPosition(Vec3(0.7, 1.8, -0.465));
	skillGO->SetRotation(Vec3(0, -1.57, 0));
	precisionGO->SetPosition(Vec3(0.7, 1.8, -0.265));
	precisionGO->SetRotation(Vec3(0, -1.57, 0));
	exitGO->SetPosition(Vec3(0.7, 1.8, -0.065));
	exitGO->SetRotation(Vec3(0, -1.57, 0));
	Scene->Add(skillGO);
	Scene->Add(precisionGO);
	Scene->Add(exitGO);

	// Score
	scoreHandle = new Model("assets/score/score.p3dm", true, ShaderUsage::SpotShadow | ShaderUsage::Diffuse);
	scoreRComp = new RenderingComponent(scoreHandle);
	scoreGO = new GameObject();
	scoreGO->Add(scoreRComp);
	scoreGO->SetRotation(Vec3(1.57, 0, 1.57));
	scoreGO->SetPosition(Vec3(0.7, 1.5, -0.26));
	Scene->Add(scoreGO);

	// Score Values and Labels
	scoreTypeGameMat = new GenericShaderMaterial(ShaderUsage::Texture);
	scoreTypeGameMat->SetTransparencyFlag(true);
	scoreTypeGameMat->SetColorMap(precisionTexture);
	scoreTypeGameRComp = new RenderingComponent(optionHandle, scoreTypeGameMat);
	scoreTypeGameGO = new GameObject();
	scoreTypeGameGO->Add(scoreTypeGameRComp);
	scoreTypeGameGO->SetScale(Vec3(0.5, 0.5, 1));
	mts5 = new Texture();
	mts5->LoadTexture("assets/score/5mts.png");
	mts10 = new Texture();
	mts10->LoadTexture("assets/score/10mts.png");
	mts20 = new Texture();
	mts20->LoadTexture("assets/score/20mts.png");
	mts30 = new Texture();
	mts30->LoadTexture("assets/score/30mts.png");
	mts40 = new Texture();
	mts40->LoadTexture("assets/score/40mts.png");
	mtsHandle = new Plane(0.02, 0.02);
	mts5Mat = new GenericShaderMaterial(ShaderUsage::Texture);
	mts5Mat->SetTransparencyFlag(true);
	mts5Mat->SetColorMap(mts5);
	mts10Mat = new GenericShaderMaterial(ShaderUsage::Texture);
	mts10Mat->SetTransparencyFlag(true);
	mts10Mat->SetColorMap(mts10);
	mts20Mat = new GenericShaderMaterial(ShaderUsage::Texture);
	mts20Mat->SetTransparencyFlag(true);
	mts20Mat->SetColorMap(mts20);
	mts30Mat = new GenericShaderMaterial(ShaderUsage::Texture);
	mts30Mat->SetTransparencyFlag(true);
	mts30Mat->SetColorMap(mts30);
	mts40Mat = new GenericShaderMaterial(ShaderUsage::Texture);
	mts40Mat->SetTransparencyFlag(true);
	mts40Mat->SetColorMap(mts40);
	mts5RComp = new RenderingComponent(mtsHandle, mts5Mat);
	mts10RComp = new RenderingComponent(mtsHandle, mts10Mat);
	mts20RComp = new RenderingComponent(mtsHandle, mts20Mat);
	mts30RComp = new RenderingComponent(mtsHandle, mts30Mat);
	mts40RComp = new RenderingComponent(mtsHandle, mts40Mat);
	mts5GO = new GameObject();
	mts10GO = new GameObject();
	mts20GO = new GameObject();
	mts30GO = new GameObject();
	mts40GO = new GameObject();
	mts5GO->Add(mts5RComp);
	mts10GO->Add(mts10RComp);
	mts20GO->Add(mts20RComp);
	mts30GO->Add(mts30RComp);
	mts40GO->Add(mts40RComp);
	scoreTypeGameGO->SetPosition(Vec3(0.0, 0.001, -0.086));
	mts5GO->SetPosition(Vec3(-0.064, 0.001, -0.026));
	mts10GO->SetPosition(Vec3(-0.064, 0.001, 0.006));
	mts20GO->SetPosition(Vec3(-0.064, 0.001, 0.036));
	mts30GO->SetPosition(Vec3(-0.064, 0.001, 0.066));
	mts40GO->SetPosition(Vec3(-0.064, 0.001, 0.096));
	scoreTypeGameGO->SetRotation(Vec3(-1.57, 0, 0));
	mts5GO->SetRotation(Vec3(-1.57, 0, 0));
	mts10GO->SetRotation(Vec3(-1.57, 0, 0));
	mts20GO->SetRotation(Vec3(-1.57, 0, 0));
	mts30GO->SetRotation(Vec3(-1.57, 0, 0));
	mts40GO->SetRotation(Vec3(-1.57, 0, 0));
	scoreGO->Add(scoreTypeGameGO);
	scoreGO->Add(mts5GO);
	scoreGO->Add(mts10GO);
	scoreGO->Add(mts20GO);
	scoreGO->Add(mts30GO);
	scoreGO->Add(mts40GO);

	// Numbers Textures
	for (uint32 i = 0; i < 10; ++i)
	{
		Texture* tex = new Texture();
		std::ostringstream toStr; toStr << "assets/score/numbers/" << i << ".png";
		tex->LoadTexture(toStr.str());
		numbersTextures.push_back(tex);
	}

	// Points Textures
	Texture* tex0 = new Texture(); tex0->LoadTexture("assets/score/points/0.png");
	Texture* tex5 = new Texture(); tex5->LoadTexture("assets/score/points/5.png");
	Texture* tex6 = new Texture(); tex6->LoadTexture("assets/score/points/6.png");
	Texture* tex7 = new Texture(); tex7->LoadTexture("assets/score/points/7.png");
	Texture* tex8 = new Texture(); tex8->LoadTexture("assets/score/points/8.png");
	Texture* tex9 = new Texture(); tex9->LoadTexture("assets/score/points/9.png");
	Texture* tex10 = new Texture(); tex10->LoadTexture("assets/score/points/10.png");
	ptsTextures.push_back(tex0);
	ptsTextures.push_back(tex5);
	ptsTextures.push_back(tex6);
	ptsTextures.push_back(tex7);
	ptsTextures.push_back(tex8);
	ptsTextures.push_back(tex9);
	ptsTextures.push_back(tex10);

	// Lights
	pLight = new PointLight(Vec4(1, 1, 1, 1), 100);
	pLightGO = new GameObject();
	pLightGO->Add(pLight);
	pLightGO->SetPosition(Vec3(0, 13, -27));
	Scene->Add(pLightGO);

	sLight = new SpotLight(Vec4(1, 1, 1, 1), 10, Vec3(0, -1, 0), 45, 30);
	sLight->EnableCastShadows(2048, 2048, 0.001);
	sLight->SetShadowBias(5.f, 3.f);
	sLightGO = new GameObject();
	sLightGO->SetPosition(Vec3(0.0f, 3.f, -.3f));
	sLightGO->Add(sLight);
	Scene->Add(sLightGO);

	// Gun
	GunGO = new GameObject();
	GunHandle = new Model("assets/Handgun/Handgun_obj.p3dm", true, ShaderUsage::SpotShadow | ShaderUsage::Diffuse);
	GunRComp = new RenderingComponent(GunHandle);
	GunGO->Add(GunRComp);
	Scene->Add(GunGO);

	// Gun Fire
	GunFireGO = new GameObject();
	GunFireHandle = new Model("assets/firegun/fireGun.p3dm", true);
	GunFireComp = new RenderingComponent(GunFireHandle);
	GunFireGO->Add(GunFireComp);
	GunGO->Add(GunFireGO);
	GunFireGO->SetPosition(Vec3(0, -0.1, 0.0));
	GunFireGO->SetRotation(Vec3(DEGTORAD(-90), 0, 0));
	GunFireGO->SetScale(Vec3(0.05, 0.05, 0.05));
	Scene->Add(GunFireGO);
	GunFireComp->Disable();

	// Sound
	shotSound = new Sound();
	shotSound->LoadFromFile("assets/target/shot.ogg");

	// Target
	targetTexture = new Texture();
	targetTexture->LoadTexture("assets/target/target.png");
	targetMaterial = new GenericShaderMaterial(ShaderUsage::Texture);
	targetMaterial->SetColorMap(targetTexture);
	targetHandle = new Cube(0.5, 0.5, 0.02);
	targetRComp = new RenderingComponent(targetHandle, targetMaterial);
	targetGO = new GameObject();
	targetGO->Add(targetRComp);

	// Continue
	continueTexture = new Texture();
	continueTexture->LoadTexture("assets/target/continue.png");
	gameOverTexture = new Texture();
	gameOverTexture->LoadTexture("assets/target/gameover.png");
	continueHandle = new Plane(0.5, 0.5);
	continueMaterial = new GenericShaderMaterial(ShaderUsage::Texture);
	continueMaterial->SetTransparencyFlag(true);
	continueMaterial->SetColorMap(continueTexture);
	continueRComp = new RenderingComponent(continueHandle, continueMaterial);
	continueGO = new GameObject();
	continueGO->Add(continueRComp);
	continueGO->SetPosition(Vec3(0, .6, 0));
	targetGO->Add(continueGO);
	precisionWaitingShoot = false;

	// Decals
	decalHandle = new Plane(0.05, 0.05);
	decalMaterial = new GenericShaderMaterial(ShaderUsage::Texture);
	decalTexture = new Texture();
	decalTexture->LoadTexture("assets/target/bullet_hole.png");
	decalMaterial->SetColorMap(decalTexture);
	decalMaterial->SetTransparencyFlag(true);
	decalMaterial->EnableDethBias(-4, -4);
	decalMaterial->DisableDepthWrite();

	// Clock model for Skill Game
	clockHandle = new Model("assets/clock/clock.p3dm", true, ShaderUsage::SpotShadow | ShaderUsage::Diffuse);
	clockRComp = new RenderingComponent(clockHandle);
	clockGO = new GameObject();
	clockGO->Add(clockRComp);
	clockGO->SetScale(Vec3(0.1, 0.1, 0.1));
	clockGO->SetPosition(Vec3(0.5, 1.139, -0.573));
	clockGO->SetRotation(Vec3(3.142, -1.073, 3.142));
	Scene->Add(clockGO);

	clockBackground = new Texture();
	clockBackground->LoadTexture("assets/clock/background.png");
	clockBackgroundGO = new GameObject();
	clockBackgroundMaterial = new GenericShaderMaterial(ShaderUsage::Texture);
	clockBackgroundMaterial->SetColorMap(clockBackground);
	clockBackgroundMaterial->SetTransparencyFlag(true);
	clockBackgroundHandle = new Plane(1, 1);
	clockBackgroundRComp = new RenderingComponent(clockBackgroundHandle, clockBackgroundMaterial);
	clockBackgroundGO->Add(clockBackgroundRComp);
	clockGO->Add(clockBackgroundGO);
	clockBackgroundGO->SetRotation(Vec3(0, 1.57, 0));
	Scene->Add(clockBackgroundGO);

	ckockPointerHandle = new Model("assets/clock/clock_pointer.p3dm", true);
	minutePointerMaterial = new GenericShaderMaterial(ShaderUsage::Color);
	secondPointerMaterial = new GenericShaderMaterial(ShaderUsage::Color);
	minutePointerMaterial->SetColor(Vec4(0, 0, 1, 1));
	secondPointerMaterial->SetColor(Vec4(1, 0, 0, 1));
	minutePointerRComp = new RenderingComponent(ckockPointerHandle, minutePointerMaterial);
	secondsPointerRComp = new RenderingComponent(ckockPointerHandle, secondPointerMaterial);
	minutePointerGO = new GameObject();
	secondPointerGO = new GameObject();
	minutePointerGO->Add(minutePointerRComp);
	secondPointerGO->Add(secondsPointerRComp);
	minutePointerGO->SetScale(Vec3(0.5, 0.5, 1));
	Scene->Add(minutePointerGO);
	Scene->Add(secondPointerGO);
	clockGO->Add(minutePointerGO);
	clockGO->Add(secondPointerGO);

	isShooting = false;
	rumbleTime = 0;
	gunFireShowTime = 0;
}
void VR_ShootingRange::Update()
{
	// Update - Game Loop
	vr->HandleVRInputs();
	if (vr->GetController1() != NULL)
	{
		vr::VRControllerState_t state = vr->GetControllerEvents(vr->GetController1()->index);
		if (state.ulButtonPressed & vr::ButtonMaskFromId(vr::EVRButtonId::k_EButton_SteamVR_Trigger))
		{
			if (!isShooting)
			{
				Shoot();
			}
		}
		else isShooting = false;

		Matrix mat;
		mat.RotationX(DEGTORAD(15.f));
		mat.Translate(Vec3(0, -.05, .05));
		mat = vr->GetController1()->GetWorldTransformation()*mat;
		GunGO->SetTransformationMatrix(mat);
	}

	if (gunFireShowTime > GetTimeMilliSeconds() && !GunFireComp->IsActive())
		GunFireComp->Enable();
	else if (GunFireComp->IsActive())
		GunFireComp->Disable();

	if (rumbleTime > GetTimeMilliSeconds())
	{
		vr->RumbleController1(3000);
	}
	if (rumbleTime + 300 < GetTimeMilliSeconds()) isShooting = false;

	if (typeGame == TYPE_GAME::Precision && shotCount == 5)
	{
		if (targetGO->GetWorldPosition().z < -1)
			targetGO->SetPosition(Vec3(targetGO->GetWorldPosition().xy(), targetGO->GetWorldPosition().z + 0.1));
		else {
			if (precisionStep < 5)
				continueMaterial->SetColorMap(continueTexture);
			else continueMaterial->SetColorMap(gameOverTexture);
			precisionWaitingShoot = true;
			Scene->Add(continueGO);
			std::cout << finalScore << std::endl;
		}
	}
	else if (typeGame == TYPE_GAME::Skill)
	{
		if (!skillOver)
		{
			if (GetTimeMilliSeconds() > timeDeploy && GetTimeMilliSeconds() < endTime)
			{
				srand((unsigned int)time(NULL));
				timeDeploy += 1000;
				GameObject* go = new GameObject();
				RenderingComponent* rcomp = new RenderingComponent(targetHandle, targetMaterial);
				go->Add(rcomp);
				Scene->Add(go);
				go->SetPosition(Vec3((f32)(rand() % 10) - 5.f, 1.5f, (f32)(rand() % 35 * -1.f - 5.f)));

				targetsGO.push_back(go);
				targetsRComp.push_back(rcomp);
			}
			if (GetTimeMilliSeconds() >= endTime)
			{
				skillOver = true;
				std::cout << finalScore << std::endl;
			}

			f64 clockTime = endTime - GetTimeMilliSeconds();
			secondPointerGO->SetRotation(Vec3(DEGTORAD(360 * (clockTime*0.001) / 60.f)*-1.f, 0, 0));
			minutePointerGO->SetRotation(Vec3(DEGTORAD(360 * (clockTime*0.001) / 3600.f)*-1.f, 0, 0));

		}
	}

	// Update Scene
	Scene->Update(GetTime());

	vr->Renderer(Scene);
}
void VR_ShootingRange::PrecisionStep1()
{
	ClearDecals();
	precisionShotCount = 0;
	precisionStep = 1;
	targetGO->SetPosition(Vec3(0, 1.5, -5));
}
void VR_ShootingRange::PrecisionStep2()
{
	ClearDecals();
	precisionShotCount = 0;
	precisionStep = 2;
	targetGO->SetPosition(Vec3(0, 1.5, -10));
}
void VR_ShootingRange::PrecisionStep3()
{
	ClearDecals();
	precisionShotCount = 0;
	precisionStep = 3;
	targetGO->SetPosition(Vec3(0, 1.5, -20));
}
void VR_ShootingRange::PrecisionStep4()
{
	ClearDecals();
	precisionShotCount = 0;
	precisionStep = 4;
	targetGO->SetPosition(Vec3(0, 1.5, -30));
}
void VR_ShootingRange::PrecisionStep5()
{
	ClearDecals();
	precisionShotCount = 0;
	precisionStep = 5;
	targetGO->SetPosition(Vec3(0, 1.5, -40));
}
bool VR_ShootingRange::rayIntersectionTriangle(const Vec3 &Origin, const Vec3 &Direction, const Vec3 &v1, const Vec3 &v2, const Vec3 &v3, Vec3 *IntersectionPoint32, f32 *t) const
{

	Vec3 V1 = v1;
	Vec3 V2 = v2;
	Vec3 V3 = v3;

	Vec3 e1 = V2 - V1, e2 = V3 - V1;
	Vec3 pvec = Direction.cross(e2);
	f32 det = e1.dotProduct(pvec);

	if (det > -EPSILON && det < EPSILON)
	{
		return false;
	}

	f32 inv_det = 1.0f / det;
	Vec3 tvec = Origin - V1;
	f32 u = tvec.dotProduct(pvec) * inv_det;

	if (u < 0.0f || u > 1.0f)
	{
		return false;
	};

	Vec3 qvec = tvec.cross(e1);
	f32 v = Direction.dotProduct(qvec) * inv_det;

	if (v < 0.0f || u + v > 1.0f)
	{
		return false;
	};

	f32 _t = e2.dotProduct(qvec) * inv_det;

	if (!isnan((double)_t))
	{
		*t = _t;
	}
	else return false;

	if (IntersectionPoint32)
	{
		*IntersectionPoint32 = Origin + (Direction * _t);
	}
	else return false;
	return true;
}
bool VR_ShootingRange::GetIntersectedTriangle(const Vec3 &Origin, const Vec3 &Direction, RenderingComponent* rcomp, Vec3* intersection, Vec3* normal)
{
	Vec3 _intersection, finalIntersection;
	f32 t, dist;
	uint32 _meshID;
	bool init = false;
	Vec3 _normal;

	Matrix m = rcomp->GetOwner()->GetWorldTransformation();
	for (size_t k = 0; k < rcomp->GetMeshes().size(); k++)
	{
		for (size_t i = 0; i < rcomp->GetMeshes()[k]->Geometry->GetIndexData().size(); i += 3)
		{
			if (rayIntersectionTriangle(Origin, Direction,
				m*rcomp->GetMeshes()[k]->Geometry->GetVertexData()[rcomp->GetMeshes()[k]->Geometry->GetIndexData()[i]],
				m*rcomp->GetMeshes()[k]->Geometry->GetVertexData()[rcomp->GetMeshes()[k]->Geometry->GetIndexData()[i + 1]],
				m*rcomp->GetMeshes()[k]->Geometry->GetVertexData()[rcomp->GetMeshes()[k]->Geometry->GetIndexData()[i + 2]],
				&_intersection,
				&t
			))
			{
				if (!init) {
					finalIntersection = _intersection;
					_normal = rcomp->GetMeshes()[k]->Geometry->GetNormalData()[rcomp->GetMeshes()[k]->Geometry->GetIndexData()[i]];
					dist = t;
					init = true;
					_meshID = k;
					continue;
				}
				f32 dist2 = t;
				if (dist2 < dist)
				{
					dist = dist2;
					finalIntersection = _intersection;
					_normal = rcomp->GetMeshes()[k]->Geometry->GetNormalData()[rcomp->GetMeshes()[k]->Geometry->GetIndexData()[i]];
					_meshID = k;
				}
			}
		}
	}
	if (init) {
		*intersection = finalIntersection;
		*normal = _normal;
		return true;
	}
	return false;
}
void VR_ShootingRange::StartPrecisionGame()
{
	typeGame = TYPE_GAME::Precision;
	scoreTypeGameMat->SetColorMap(precisionTexture);
	Scene->Add(scoreTypeGameGO);
	Scene->Add(mts5GO);
	Scene->Add(mts10GO);
	Scene->Add(mts20GO);
	Scene->Add(mts30GO);
	Scene->Add(mts40GO);
	Scene->Add(targetGO);
	Scene->Remove(continueGO);
	precisionWaitingShoot = false;
	precisionShotCount = 0;
	shotCount = 0;
	finalScore = 0;

	secondPointerGO->SetRotation(Vec3(0, 0, 0));
	minutePointerGO->SetRotation(Vec3(0, 0, 0));

	ClearScore();
	ClearTargets();
	PrecisionStep1();
}
void VR_ShootingRange::StartSkillGame()
{
	typeGame = TYPE_GAME::Skill;
	scoreTypeGameMat->SetColorMap(skillTexture);
	Scene->Add(scoreTypeGameGO);
	Scene->Remove(mts5GO);
	Scene->Remove(mts10GO);
	Scene->Remove(mts20GO);
	Scene->Remove(mts30GO);
	Scene->Remove(mts40GO);
	Scene->Remove(targetGO);
	Scene->Remove(continueGO);
	shotCount = 0;
	finalScore = 0;

	initTime = GetTimeMilliSeconds();
	endTime = GetTimeMilliSeconds() + 30000; // 30 seconds
	timeDeploy = initTime + 1000;
	skillOver = false;

	ClearDecals();
	ClearScore();
	ClearTargets();
}
void VR_ShootingRange::ClearDecals() {
	for (std::vector<RenderingComponent*>::iterator i = DecalsRComp.begin(); i != DecalsRComp.end(); i++)
	{
		(*i)->GetOwner()->Remove((*i));
		delete (*i);
	}

	for (std::vector<GameObject*>::iterator i = DecalsGO.begin(); i != DecalsGO.end(); i++)
	{
		Scene->Remove((*i));
		targetGO->Remove((*i));
		delete (*i);
	}

	DecalsRComp.clear();
	DecalsGO.clear();
}
void VR_ShootingRange::Shoot()
{
	isShooting = true;
	shotSound->Play();
	rumbleTime = GetTimeMilliSeconds() + 100;
	gunFireShowTime = rumbleTime - 50;
	Matrix mat;
	mat.RotationX(DEGTORAD(15.f));
	mat.Translate(Vec3(0, -.05, .05));
	mat = vr->GetController1()->GetWorldTransformation()*mat;
	GunGO->SetTransformationMatrix(mat);
	Vec3 Origin = mat.GetTranslation();
	Vec3 Dest; Dest.y = -1;
	Dest = mat * Dest;
	Vec3 Direction = (Dest - Origin).normalize();
	Vec3 position = targetGO->GetWorldPosition();
	f32 radius = 0.5;

	Vec3 intersectionPoint32, normalPoint32;

	if ((typeGame == TYPE_GAME::Precision && shotCount < 5) || precisionWaitingShoot)
	{
		f32 distance = 1000;
		precisionShotCount++;
		shotCount++;
		if (GetIntersectedTriangle(Origin, Direction, targetRComp, &intersectionPoint32, &normalPoint32))
		{
			if (!precisionWaitingShoot)
			{
				// Get Distance
				Vec3 _intersection = Vec3(intersectionPoint32.xy(), targetGO->GetWorldPosition().z);
				distance = _intersection.distance(targetGO->GetWorldPosition());

				Matrix mat2;
				mat2.LookAt(intersectionPoint32, normalPoint32.negate(), Vec3(0, 1, 0));
				mat2.Inverse();
				mat2.Translate(intersectionPoint32);

				RenderingComponent* r = new RenderingComponent(decalHandle, decalMaterial);
				GameObject* go = new GameObject();
				go->Add(r);
				go->SetPosition(Vec3(intersectionPoint32.xy() - position.xy(), 0.025));
				targetGO->Add(go);

				DecalsGO.push_back(go);
				DecalsRComp.push_back(r);

				Scene->Add(go);
			}
			else
			{
				if (precisionStep < 5)
				{
					Scene->Remove(continueGO);
					precisionWaitingShoot = false;
				}
				shotCount = 0;
				switch (precisionStep)
				{
				case 1:
					PrecisionStep2();
					break;
				case 2:
					PrecisionStep3();
					break;
				case 3:
					PrecisionStep4();
					break;
				case 4:
					PrecisionStep5();
					break;
				}
				return;
			}
		}

		if (precisionShotCount <= 5)
		{
			uint32 score = 10;
			while (distance > 0.08333 && score >= 0)
			{
				score--;
				distance -= 0.08333;
			}

			uint32 scoreID = 0;
			switch (score)
			{
			case 5:
				scoreID = 1;
				finalScore += precisionStep * 5;
				break;
			case 6:
				scoreID = 2;
				finalScore += precisionStep * 6;
				break;
			case 7:
				scoreID = 3;
				finalScore += precisionStep * 7;
				break;
			case 8:
				scoreID = 4;
				finalScore += precisionStep * 8;
				break;
			case 9:
				scoreID = 5;
				finalScore += precisionStep * 9;
				break;
			case 10:
				scoreID = 6;
				finalScore += precisionStep * 10;
				break;
			case 0:
			default:
				scoreID = 0;
				break;
			}

			GameObject* scorePoint = new GameObject();
			GenericShaderMaterial* material = new GenericShaderMaterial(ShaderUsage::Texture);
			material->SetTransparencyFlag(true);
			material->SetColorMap(ptsTextures[scoreID]);
			RenderingComponent* rcomp = new RenderingComponent(optionHandle, material);
			scorePoint->SetRotation(Vec3(-1.57, 0, 0));
			scorePoint->SetPosition(Vec3(-0.034 + precisionShotCount*0.02, 0.001, -0.056 + precisionStep*0.03));
			scorePoint->SetScale(Vec3(0.15, 0.15, 15));
			scorePoint->Add(rcomp);
			scoreGO->Add(scorePoint);
			Scene->Add(scorePoint);

			ptsGO.push_back(scorePoint);
			ptsRComp.push_back(rcomp);
			ptsMat.push_back(material);
		}
	}

	if (typeGame == TYPE_GAME::Skill)
	{
		f32 distance = 1000;
		for (std::vector<RenderingComponent*>::iterator i = targetsRComp.begin(); i != targetsRComp.end(); i++)
		{
			if (GetIntersectedTriangle(Origin, Direction, (*i), &intersectionPoint32, &normalPoint32))
			{

				Vec3 _intersection = Vec3(intersectionPoint32.xy(), (*i)->GetOwner()->GetWorldPosition().z);
				distance = _intersection.distance((*i)->GetOwner()->GetWorldPosition());

				Scene->Remove((*i)->GetOwner());

				for (std::vector<GameObject*>::iterator k = targetsGO.begin(); k != targetsGO.end(); k++)
				{
					if ((*i)->GetOwner() == (*k))
					{
						(*k)->Remove((*i));
						delete (*k);
						targetsGO.erase(k);
						break;
					}
				}
				delete (*i);
				targetsRComp.erase(i);

				uint32 score = 10;
				while (distance > 0.08333 && score >= 0)
				{
					score--;
					distance -= 0.08333;
				}
				switch (score)
				{
				case 5:
					endTime += 250;
					break;
				case 6:
					endTime += 500;
					break;
				case 7:
					endTime += 1000;
					break;
				case 8:
					endTime += 1500;
					break;
				case 9:
					endTime += 2000;
					break;
				case 10:
					endTime += 3000;
					break;
				}
			}
			finalScore++;
		}
	}

	if (GetIntersectedTriangle(Origin, Direction, skillRComp, &intersectionPoint32, &normalPoint32))
	{
		StartSkillGame();
	}

	if (GetIntersectedTriangle(Origin, Direction, precisionRComp, &intersectionPoint32, &normalPoint32))
	{
		StartPrecisionGame();
	}

	if (GetIntersectedTriangle(Origin, Direction, exitRComp, &intersectionPoint32, &normalPoint32))
	{
		Close();
	}
}
void VR_ShootingRange::ClearScore()
{
	for (std::vector<RenderingComponent*>::iterator i = ptsRComp.begin(); i != ptsRComp.end(); i++)
	{
		(*i)->GetOwner()->Remove((*i));
		delete (*i);
	}
	for (std::vector<GameObject*>::iterator i = ptsGO.begin(); i != ptsGO.end(); i++)
	{
		Scene->Remove((*i));
		delete (*i);
	}
	for (std::vector<GenericShaderMaterial*>::iterator i = ptsMat.begin(); i != ptsMat.end(); i++)
	{
		delete (*i);
	}
	ptsGO.clear();
	ptsMat.clear();
	ptsRComp.clear();
}
void VR_ShootingRange::ClearTargets()
{
	for (std::vector<RenderingComponent*>::iterator k = targetsRComp.begin(); k != targetsRComp.end(); k++)
	{
		(*k)->GetOwner()->Remove(*k);
		delete (*k);
	}
	for (std::vector<GameObject*>::iterator i = targetsGO.begin(); i != targetsGO.end(); i++)
	{
		Scene->Remove(*i);
		delete (*i);
	}
	targetsGO.clear();
	targetsRComp.clear();
}
void VR_ShootingRange::Shutdown()
{
	// All your Shutdown Code Here

	ClearScore();
	ClearDecals();
	ClearTargets();

	// Remove GameObjects From Scene
	Scene->Remove(sLightGO);
	Scene->Remove(pLightGO);
	Scene->Remove(shootingHouseGO);
	Scene->Remove(targetGO);
	Scene->Remove(GunFireGO);
	Scene->Remove(skillGO);
	Scene->Remove(precisionGO);
	Scene->Remove(exitGO);
	Scene->Remove(mts5GO);
	Scene->Remove(mts10GO);
	Scene->Remove(mts20GO);
	Scene->Remove(mts30GO);
	Scene->Remove(mts40GO);
	Scene->Remove(scoreTypeGameGO);
	Scene->Remove(continueGO);
	Scene->Remove(clockGO);
	Scene->Remove(minutePointerGO);
	Scene->Remove(secondPointerGO);
	Scene->Remove(clockBackgroundGO);

	sLightGO->Remove(sLight);
	pLightGO->Remove(pLight);
	shootingHouseGO->Remove(shootingHouseRComp);
	targetGO->Remove(targetRComp);
	GunFireGO->Remove(GunFireComp);
	skillGO->Remove(skillRComp);
	precisionGO->Remove(precisionRComp);
	exitGO->Remove(exitRComp);
	scoreTypeGameGO->Remove(scoreTypeGameRComp);
	continueGO->Remove(continueRComp);
	minutePointerGO->Remove(minutePointerRComp);
	secondPointerGO->Remove(secondsPointerRComp);
	clockGO->Remove(clockRComp);
	clockBackgroundGO->Remove(clockBackgroundRComp);

	// Delete Numbers Textures
	for (uint32 i = 0; i < 10; ++i)
	{
		delete numbersTextures[i];
	}
	numbersTextures.clear();

	// Delete
	delete clockGO;
	delete minutePointerGO;
	delete secondPointerGO;
	delete clockHandle;
	delete clockRComp;
	delete minutePointerMaterial;
	delete minutePointerRComp;
	delete secondPointerMaterial;
	delete secondsPointerRComp;
	delete clockBackgroundGO;
	delete clockBackgroundHandle;
	delete clockBackground;
	delete clockBackgroundMaterial;
	delete clockBackgroundRComp;
	delete continueGO;
	delete continueHandle;
	delete continueMaterial;
	delete continueRComp;
	delete continueTexture;
	delete gameOverTexture;
	delete decalHandle;
	delete decalMaterial;
	delete decalTexture;
	delete scoreTypeGameGO;
	delete scoreTypeGameRComp;
	delete scoreTypeGameMat;
	delete mts5RComp;
	delete mts10RComp;
	delete mts20RComp;
	delete mts30RComp;
	delete mts40RComp;
	delete mts5GO;
	delete mts10GO;
	delete mts20GO;
	delete mts30GO;
	delete mts40GO;
	delete mts5Mat;
	delete mts10Mat;
	delete mts20Mat;
	delete mts30Mat;
	delete mts40Mat;
	delete mts5;
	delete mts10;
	delete mts20;
	delete mts30;
	delete mts40;
	delete mtsHandle;
	delete skillGO;
	delete skillMaterial;
	delete skillRComp;
	delete skillTexture;
	delete precisionGO;
	delete precisionMaterial;
	delete precisionRComp;
	delete precisionTexture;
	delete exitGO;
	delete exitMaterial;
	delete exitRComp;
	delete exitTexture;
	delete GunFireGO;
	delete GunFireComp;
	delete GunFireHandle;
	delete targetGO;
	delete targetMaterial;
	delete targetRComp;
	delete targetTexture;
	delete targetHandle;
	delete shotSound;
	delete shootingHouseHandle;
	delete shootingHouseRComp;
	delete shootingHouseGO;
	delete sLight;
	delete sLightGO;
	delete pLight;
	delete pLightGO;
	delete lightsOn;
	delete Scene;
}
VR_ShootingRange::~VR_ShootingRange() {}