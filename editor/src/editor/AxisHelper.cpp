//============================================================================
// Name        : AxisHelper.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Axis Helper
//============================================================================

#include "AxisHelper.h"

using namespace p3d;

AxisHelper::AxisHelper()
{
	// Initialization

	// Initialize Scene
	Scene = new SceneGraph();

	// Initialize Renderer
	Renderer = new ForwardRenderer(100, 100);
	// No SetGlobalLight() call here (was hardcoded to 0.5,0.5,0.5,0.5) -
	// IRenderer's ambient UBO (AmbientLightUniformsUBO/CachedGlobalLight)
	// is a process-wide static, shared by every IRenderer instance, and
	// this renderer's Render() runs every frame right after the main
	// SceneEditor viewport's - so a fixed value here silently won that
	// per-frame race and overwrote whatever ambient the real scene had
	// just configured, every single frame, regardless of what the value
	// was (0.5 originally; still true even at IRenderer's own 0.2 default
	// once the explicit call was removed, since this Renderer keeps
	// rendering every frame either way) - found chasing a "scene ambient
	// light setting has no visible effect" report. See SetAmbientLight():
	// the actual fix is SceneEditor calling it every frame to keep this
	// renderer's value equal to the real scene's, so there's no
	// conflicting value left to race over regardless of draw order.
	axisHelper = std::make_shared<GameObject>();
	axisHelperHandle = std::make_shared<Model>("assets/axis.p3dm", false);
	axisRcomp = std::make_shared<RenderingComponent>(axisHelperHandle, ShaderUsage::Diffuse);
	axisRcomp->DisableCullTest();
	axisHelper->Add(axisRcomp);
	Scene->Add(axisHelper);
	xHelper = std::make_shared<GameObject>();
	yHelper = std::make_shared<GameObject>();
	zHelper = std::make_shared<GameObject>();
	axisLabelHelper = std::make_shared<Plane>(0.7, 0.7);
	font = new Font("assets/arialbd.ttf", 16);
	font->CreateText("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
	tX = std::make_shared<Text>(font, "X", 1.5, 1.5, Vec4(255,255,255,255));
	tY = std::make_shared<Text>(font, "Y", 1.5, 1.5, Vec4(255, 255, 255, 255));
	tZ = std::make_shared<Text>(font, "Z", 1.5, 1.5, Vec4(255, 255, 255, 255));
	xMat = std::make_shared<GenericShaderMaterial>(ShaderUsage::TextRendering);
	xMat->SetTextFont(font);
	xMat->SetTransparencyFlag(true);
	yMat = std::make_shared<GenericShaderMaterial>(ShaderUsage::TextRendering);
	yMat->SetTextFont(font);
	yMat->SetTransparencyFlag(true);
	zMat = std::make_shared<GenericShaderMaterial>(ShaderUsage::TextRendering);
	zMat->SetTextFont(font);
	zMat->SetTransparencyFlag(true);
	xRcomp = std::make_shared<RenderingComponent>(tX, xMat);
	xRcomp->GetMeshes()[0]->Pivot.Translate(Vec3(-0.5, -0.5, 0));
	yRcomp = std::make_shared<RenderingComponent>(tY, yMat);
	yRcomp->GetMeshes()[0]->Pivot.Translate(Vec3(-0.5, -0.5, 0));
	zRcomp = std::make_shared<RenderingComponent>(tZ, zMat);
	zRcomp->GetMeshes()[0]->Pivot.Translate(Vec3(-0.5, -0.5, 0));
	xRcomp->DisableCullTest();
	yRcomp->DisableCullTest();
	zRcomp->DisableCullTest();

	xHelper->Add(xRcomp);
	yHelper->Add(yRcomp);
	zHelper->Add(zRcomp);

	Scene->Add(xHelper);
	Scene->Add(yHelper);
	Scene->Add(zHelper);

	selectedMesh = NULL;
	std::shared_ptr<GenericShaderMaterial> selMat = std::make_shared<GenericShaderMaterial>(ShaderUsage::Color);
	selMat->SetColor(Vec4(1, 1, 0, 1));
	selectedMaterial = selMat;

	// The widget's own camera - see AxisHelper.h. Everything below used to
	// erase each material's global uViewMatrix and re-add it as a user
	// uniform so a fixed view matrix could be pushed per frame; that path is
	// dead now that uViewMatrix lives in a uniform block, so the fixed view
	// is expressed as an identity camera plus a translated model matrix.
	axisCamera = std::make_shared<GameObject>();
	Scene->Add(axisCamera);

	// Add a Directional Light
	Light = std::make_shared<GameObject>();
	dLight = std::make_shared<DirectionalLight>(Vec4(1, 1, 1, 1), Vec3(0, -1, -1));
	Light->Add(dLight);
	Scene->Add(Light);

	selectedAxis = -1;

	// Label
	projectionLabel.Ortho(-6, 6, -6, 6, 0.01, 100);
	labelCamera = std::make_shared<GameObject>();
	labelCamera->SetPosition(Vec3(0, 0, 1));
	labelObj = std::make_shared<GameObject>();
	labelObj->SetPosition(Vec3(-4, -5, 0));
	labelmat = std::make_shared<GenericShaderMaterial>(ShaderUsage::TextRendering);
	labelmat->SetTextFont(font);
	labelmat->SetTransparencyFlag(true);
	labelText = std::make_shared<Text>(font, "Perspective", 1.6, 1.4);
	labelRcomp = std::make_shared<RenderingComponent>(labelText, labelmat);
	labelObj->Add(labelRcomp);
	SceneLabel = new SceneGraph();
	SceneLabel->Add(labelCamera);
	SceneLabel->Add(labelObj);
}

bool AxisHelper::Update(const double time, GameObject* Camera, const Vec2 &mousePos)
{
	bool isSelecting = false;

	this->Camera = Camera;

	Matrix m;
	m.Translate(Vec3(0, 0, -8));

	Matrix rot, scale;
	rot.SetRotationFromEuler(Camera->GetWorldTransformation().Inverse().GetEulerFromRotationMatrix());

	// model = T(0,0,-8) * rot, with an identity view - the same
	// projection * view * model product the old uViewMatrix override was
	// reaching for, expressed where the engine can actually see it.
	axisHelper->SetTransformationMatrix(m * rot);

	xHelper->SetPosition(rot*Vec3(4.0, 0.0, 0.0) + Vec3(0, 0, -8));
	yHelper->SetPosition(rot*Vec3(0.0, 4.0, 0.0) + Vec3(0, 0, -8));
	zHelper->SetPosition(rot*Vec3(0.0, 0.0, 4.0) + Vec3(0, 0, -8));

	mPos = mousePos;
	if ((mPos.x > 0 && mPos.y > 0) &&
		(mPos.x < lastDim.z && mPos.y < lastDim.w))
	{
		Mouse3D mouse = Mouse3D();
		Vec3 intersection, FinalIntersection;
		f32 dist = 1000000000;
		f32 t;
		// Center
		selectedAxis = -1;
		mouse.GenerateRay(lastDim.z, lastDim.w, mPos.x, mPos.y, m * rot, Matrix(), projection.GetProjectionMatrix());
		if (mouse.rayIntersectionSphere(Vec3(0,0,0), 1, &intersection, &t))
		{
			if (selectedAxis == -1 || t<dist)
			{
				selectedAxis = AXIS_HELPER_AXIS::CENTER;
				dist = t;
			}
		}

		// XX
		if (mouse.rayIntersectionSphere(Vec3(-2, 0 , 0), 1, &intersection, &t)
			||
			mouse.rayIntersectionSphere(Vec3(-2.5, 0, 0), 1, &intersection, &t))
		{
			if (selectedAxis == -1 || t<dist)
			{
				selectedAxis = AXIS_HELPER_AXIS::NEGATIVE_X;
				dist = t;
			}
		}
		if (mouse.rayIntersectionSphere(Vec3(2, 0, 0), 1, &intersection, &t)
			&&
			mouse.rayIntersectionSphere(Vec3(2.5, 0, 0), 1, &intersection, &t))
		{
			if (selectedAxis == -1 || t<dist)
			{
				selectedAxis = AXIS_HELPER_AXIS::POSITIVE_X;
				dist = t;
			}
		}

		// YY
		if (mouse.rayIntersectionSphere(Vec3(0, -2, 0), 1, &intersection, &t)
			||
			mouse.rayIntersectionSphere(Vec3(0, -2.5, 0), 1, &intersection, &t))
		{
			if (selectedAxis == -1 || t<dist)
			{
				selectedAxis = AXIS_HELPER_AXIS::NEGATIVE_Y;
				dist = t;
			}
		}
		if (mouse.rayIntersectionSphere(Vec3(0, 2, 0), 1, &intersection, &t)
			||
			mouse.rayIntersectionSphere(Vec3(0, 2.5, 0), 1, &intersection, &t))
		{
			if (selectedAxis == -1 || t<dist)
			{
				selectedAxis = AXIS_HELPER_AXIS::POSITIVE_Y;
				dist = t;
			}
		}

		// ZZ
		if (mouse.rayIntersectionSphere(Vec3(0, 0, -2), 1, &intersection, &t) 
			||
			mouse.rayIntersectionSphere(Vec3(0, 0, -2.5), 1, &intersection, &t))
		{
			if (selectedAxis == -1 || t<dist)
			{
				selectedAxis = AXIS_HELPER_AXIS::NEGATIVE_Z;
				dist = t;
			}
		}
		if (mouse.rayIntersectionSphere(Vec3(0, 0, 2), 1, &intersection, &t)
			||
			mouse.rayIntersectionSphere(Vec3(0, 0, 2.5), 1, &intersection, &t))
		{
			if (selectedAxis == -1 || t<dist)
			{
				selectedAxis = AXIS_HELPER_AXIS::POSITIVE_Z;
				dist = t;
			}
		}

		if (selectedMesh!=NULL)
			selectedMesh->Material = tempMaterial;

		selectedMesh = NULL;
		switch (selectedAxis)
		{
		case AXIS_HELPER_AXIS::CENTER:
			selectedMesh = axisRcomp->GetMeshes()[0];
			break;
		case AXIS_HELPER_AXIS::NEGATIVE_X:
			selectedMesh = axisRcomp->GetMeshes()[5];
			break;
		case AXIS_HELPER_AXIS::POSITIVE_X:
			selectedMesh = axisRcomp->GetMeshes()[4];
			break;
		case AXIS_HELPER_AXIS::NEGATIVE_Y:
			selectedMesh = axisRcomp->GetMeshes()[3];
			break;
		case AXIS_HELPER_AXIS::POSITIVE_Y:
			selectedMesh = axisRcomp->GetMeshes()[6];
			break;
		case AXIS_HELPER_AXIS::NEGATIVE_Z:
			selectedMesh = axisRcomp->GetMeshes()[2];
			break;
		case AXIS_HELPER_AXIS::POSITIVE_Z:
			selectedMesh = axisRcomp->GetMeshes()[1];
			break;
		default:
			break;
		}
		if (selectedMesh != NULL)
		{
			isSelecting = true;
			tempMaterial = selectedMesh->Material;
			selectedMesh->Material = selectedMaterial;
		}
	}
	else {
		if (selectedMesh != NULL)
		{
			selectedAxis = -1;
			selectedMesh->Material = tempMaterial;
			selectedMesh = NULL;
		}
	}

	// Update Scene
	Scene->Update(time);
	SceneLabel->Update(time);

	return isSelecting;
}

int32 AxisHelper::MouseClick()
{
	return selectedAxis;
}

void AxisHelper::Render(const uint32 x, const uint32 y, const uint32 distx, const uint32 disty, bool isPerspective)
{
	// Render Scene
	if (isPerspective != this->isPerspective)
	{
		if (isPerspective)
		{
			labelText->UpdateText("Perspective");
			labelObj->SetPosition(Vec3(-4, -5, 0));
		}
		else {
			labelText->UpdateText("Ortho");
			labelObj->SetPosition(Vec3(-2, -5, 0));
		}
		this->isPerspective = isPerspective;
	}

	// Always ortho, and always the same extents: this is a navigation
	// widget of a fixed on-screen size, so it must not shrink or grow with
	// the scene camera's zoom or projection. isPerspective only selects the
	// label text below.
	projection.Ortho(-6, 6, -6, 6, 0.01, 100);

	lastDim = Vec4(x, y, distx, disty);
	Renderer->ClearBufferBit(Buffer_Bit::Depth);
	Renderer->ResetViewPort();
	Renderer->SetViewPort(x, y, distx, disty);
	Renderer->PreRender(axisCamera.get(), Scene);
	Renderer->RenderScene(projection, axisCamera.get(), Scene);
	Renderer->PreRender(labelCamera.get(), SceneLabel);
	Renderer->RenderScene(projectionLabel, labelCamera.get(), SceneLabel);
}

AxisHelper::~AxisHelper()
{
	// Detach before dropping our references, so the scene isn't left
	// holding the last one after this helper is gone.
	Scene->Remove(axisHelper);
	Scene->Remove(xHelper);
	Scene->Remove(yHelper);
	Scene->Remove(zHelper);
	Scene->Remove(Light);
	Scene->Remove(axisCamera);
	SceneLabel->Remove(labelCamera);
	SceneLabel->Remove(labelObj);

	axisHelper.reset();
	axisHelperHandle.reset();
	axisRcomp.reset();
	xHelper.reset();
	yHelper.reset();
	zHelper.reset();
	axisLabelHelper.reset();
	xMat.reset();
	yMat.reset();
	zMat.reset();
	xRcomp.reset();
	yRcomp.reset();
	zRcomp.reset();
	tX.reset();
	tY.reset();
	tZ.reset();
	labelRcomp.reset();
	labelmat.reset();
	labelText.reset();
	labelObj.reset();
	labelCamera.reset();
	axisCamera.reset();
	Light.reset();
	dLight.reset();
	selectedMaterial.reset();
	tempMaterial.reset();

	delete SceneLabel;
	delete Scene;
	delete Renderer;
	delete font;
}
