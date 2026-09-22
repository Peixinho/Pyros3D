//============================================================================
// Name        : GIDemo.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A Cornell box lit entirely by bounced light, built in code.
//
//               Exists to be the thing you can point a browser at. Every
//               other GI demo loads a scene file and a pile of textures;
//               this one builds its geometry from primitives and its
//               materials from colours, so the whole payload is the wasm
//               plus the engine's shaders. That also makes it the only
//               place the WebGL2 path gets exercised end to end - no
//               compute there, so the probes are traced on the CPU under
//               a millisecond budget and the room converges over a few
//               seconds rather than instantly.
//============================================================================

#ifndef GIDEMO_H
#define	GIDEMO_H

#include "../BaseExample/BaseExample.h"

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Sphere.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/PointLight/PointLight.h>
#include <Pyros3D/Rendering/GI/SceneGI.h>
#include <memory>
#include <vector>

using namespace p3d;

class GIDemo : public BaseExample {

public:

	GIDemo();
	virtual ~GIDemo();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);
	virtual void DrawUI();

private:

	// One wall, floor or block. Cube takes HALF-extents.
	std::shared_ptr<GameObject> AddBox(const Vec3 &position, const Vec3 &halfExtents,
		const std::shared_ptr<GenericShaderMaterial> &material);
	std::shared_ptr<GameObject> AddSphere(const Vec3 &position, const f32 radius,
		const std::shared_ptr<GenericShaderMaterial> &material);

	Projection projection;

	std::shared_ptr<GenericShaderMaterial> whiteMat, redMat, greenMat, mirrorMat, brushedMat;
	std::vector<std::shared_ptr<GameObject> > objects;
	std::shared_ptr<GameObject> blockObj, lightObj;
	std::shared_ptr<PointLight> lamp;
	Vec3 blockHome;

	bool giReady = false;
	f64 startTime = 0.0;
};

#endif	/* GIDEMO_H */
