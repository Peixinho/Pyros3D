//============================================================================
// Name        : AxisHelper.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Axis Helper
//============================================================================

#ifndef AXISHELPER_H
#define	AXISHELPER_H

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/Assets/Renderable/Text/Text.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Utils/Mouse3D/Mouse3D.h>
#include <memory>
using namespace p3d;

namespace AXIS_HELPER_AXIS {
	enum {
		CENTER = 0,
		POSITIVE_X,
		NEGATIVE_X,
		POSITIVE_Y,
		NEGATIVE_Y,
		POSITIVE_Z,
		NEGATIVE_Z
	};
};

class AxisHelper {
public:

	AxisHelper();
	virtual ~AxisHelper();

	bool Update(const double time, GameObject* Camera, const Vec2 &mousePos);
	void Render(const uint32 x, const uint32 y, const uint32 distx, const uint32 disty, bool isPerspective = true);
	int32 MouseClick();
	

private:

	// Scene
	SceneGraph* Scene;
	// Renderer
	ForwardRenderer* Renderer;
	// Projection
	Projection projection;
	// Camera - Its a regular GameObject
	GameObject* Camera;

	std::shared_ptr<GameObject> axisHelper;
	std::shared_ptr<Renderable> axisHelperHandle;
	std::shared_ptr<RenderingComponent> axisRcomp;
	std::shared_ptr<GameObject> xHelper, yHelper, zHelper;
	std::shared_ptr<Renderable> axisLabelHelper;
	std::shared_ptr<GenericShaderMaterial> xMat, yMat, zMat;
	std::shared_ptr<RenderingComponent> xRcomp, yRcomp, zRcomp;
	Font* font;
	std::shared_ptr<Text> tX, tY, tZ;
	std::shared_ptr<GameObject> Light;
	std::shared_ptr<DirectionalLight> dLight;
	Vec2 mPos;
	Vec4 lastDim;
	Mouse3D mouse;
	std::shared_ptr<IMaterial> selectedMaterial, tempMaterial;
	RenderingMesh *selectedMesh;
	int32 selectedAxis;

	// The widget's own camera, left at the origin with an identity
	// transform: the view matrix is then identity and the intended
	// "8 units in front of a fixed viewpoint" lives in the model matrices
	// instead. This replaces a per-material uViewMatrix override that the
	// engine can no longer honour - uViewMatrix moved into the
	// GlobalMatrices uniform block, so glGetUniformLocation() returns -1 and
	// every SetValue() on those handles went nowhere.
	std::shared_ptr<GameObject> axisCamera;

	// Label
	SceneGraph* SceneLabel;
	Projection projectionLabel;
	std::shared_ptr<Text> labelText;
	std::shared_ptr<RenderingComponent> labelRcomp;
	std::shared_ptr<GenericShaderMaterial> labelmat;
	std::shared_ptr<GameObject> labelCamera, labelObj;
	bool isPerspective;
};

#endif	/* AXISHELPER_H */
