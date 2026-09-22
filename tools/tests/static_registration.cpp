// Every static object must be live after ONE scene update, not after
// three.
//
// SceneGraph keeps static objects in a separate list and moves them to
// the "already initialised" list the first time it updates them. That
// loop both erased and incremented its iterator, so it stepped over
// every second object: six static objects came up three registered
// after the first frame, five after the second, six after the third.
//
// A running scene converges within a few frames, which is why this
// survived - by the time anyone looked, it was right. What it broke is
// everything that reads the scene ONCE and early: a GI bake at load
// time saw half the walls and lit the room through the gaps.
//
// Lights are the probe here for the same reason child_registration
// uses them: ILightComponent::GetLightsOnScene() is a registry a
// renderer really reads, and building one needs no render device, so
// this runs headless.
//
//   c++ -std=c++17 -I include $(pkg-config --cflags freetype2) \
//       tools/tests/static_registration.cpp -o /tmp/static_registration \
//       -L build_gl -lPyrosEngine -Wl,-rpath,$PWD/build_gl
//   /tmp/static_registration
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Rendering/Components/Lights/PointLight/PointLight.h>
#include <Pyros3D/Rendering/Components/Lights/ILightComponent.h>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace p3d;

static int failures = 0;
static void check(bool cond, const std::string &what, const std::string &extra = std::string())
{
	printf("%s  %s%s%s\n", cond ? "PASS" : "FAIL", what.c_str(),
		extra.empty() ? "" : " - ", extra.c_str());
	if (!cond) failures++;
}

static uint32 LightsOn(SceneGraph &scene)
{
	return (uint32)ILightComponent::GetLightsOnScene(&scene).size();
}

int main()
{
	// ---- the case that was broken --------------------------------------
	{
		SceneGraph scene;
		std::vector<std::shared_ptr<GameObject> > objs;
		const uint32 N = 6;
		for (uint32 i = 0; i < N; i++)
		{
			// Static is the point: a dynamic object was always
			// registered on the first update.
			std::shared_ptr<GameObject> go = std::make_shared<GameObject>(true);
			go->Add(std::make_shared<PointLight>(Vec4(1,1,1,1), 10.f));
			scene.Add(go);
			objs.push_back(go);
		}
		check(LightsOn(scene) == 0, "nothing is registered before the first update",
			std::to_string(LightsOn(scene)));

		scene.Update(0.016);
		check(LightsOn(scene) == N,
			"every static object is registered after ONE update",
			std::to_string(LightsOn(scene)) + " of " + std::to_string(N));

		// And it stays that way - a second pass must not double-register.
		scene.Update(0.032);
		check(LightsOn(scene) == N, "and is not registered twice by the next one",
			std::to_string(LightsOn(scene)) + " of " + std::to_string(N));
	}

	// ---- an odd count, and a mix ----------------------------------------
	//
	// The old loop skipped every other element, so an odd count and an
	// interleaved static/dynamic scene are where an off-by-one in the
	// fix would show up.
	{
		SceneGraph scene;
		std::vector<std::shared_ptr<GameObject> > objs;
		const uint32 N = 7;
		for (uint32 i = 0; i < N; i++)
		{
			std::shared_ptr<GameObject> go = std::make_shared<GameObject>((i % 2) == 0);
			go->Add(std::make_shared<PointLight>(Vec4(1,1,1,1), 10.f));
			scene.Add(go);
			objs.push_back(go);
		}
		scene.Update(0.016);
		check(LightsOn(scene) == N, "a mixed static/dynamic scene registers in one update",
			std::to_string(LightsOn(scene)) + " of " + std::to_string(N));
	}

	// ---- objects added later ---------------------------------------------
	//
	// A static object added after the scene has been running has to go
	// live on the next update too, not sit in the pending list.
	{
		SceneGraph scene;
		std::shared_ptr<GameObject> first = std::make_shared<GameObject>(true);
		first->Add(std::make_shared<PointLight>(Vec4(1,1,1,1), 10.f));
		scene.Add(first);
		scene.Update(0.016);

		std::vector<std::shared_ptr<GameObject> > more;
		for (uint32 i = 0; i < 4; i++)
		{
			std::shared_ptr<GameObject> go = std::make_shared<GameObject>(true);
			go->Add(std::make_shared<PointLight>(Vec4(1,1,1,1), 10.f));
			scene.Add(go);
			more.push_back(go);
		}
		scene.Update(0.032);
		check(LightsOn(scene) == 5, "static objects added later register on the next update",
			std::to_string(LightsOn(scene)) + " of 5");
	}

	printf("\n%s  static_registration: %d failure(s)\n", failures?"FAIL":"PASS", failures);
	return failures ? 1 : 0;
}
