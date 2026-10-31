//============================================================================
// Name        : ReflectionProbes.cpp
// Description : What a place looks like from inside it, for what shines there.
//============================================================================

#include <Pyros3D/Rendering/GI/ReflectionProbes.h>
#include <algorithm>

namespace p3d {

	namespace {
		std::vector<ReflectionProbe*> g_probes;
	}

	ReflectionProbe::ReflectionProbe() : scene(NULL), extents(5.f, 3.f, 5.f), blend(1.f) {}
	ReflectionProbe::~ReflectionProbe() { ReflectionProbes::Remove(this); }

	void ReflectionProbe::Register(SceneGraph* Scene) { scene = Scene; ReflectionProbes::Add(this); }
	void ReflectionProbe::Unregister(SceneGraph* Scene) { ReflectionProbes::Remove(this); scene = NULL; }

	void ReflectionProbe::SetExtents(const Vec3 &e)
	{
		extents = Vec3(std::max(e.x, 0.05f), std::max(e.y, 0.05f), std::max(e.z, 0.05f));
	}

	void ReflectionProbes::Add(ReflectionProbe* p)
	{
		if (std::find(g_probes.begin(), g_probes.end(), p) == g_probes.end()) g_probes.push_back(p);
		p->Capture();
	}

	void ReflectionProbes::Remove(ReflectionProbe* p)
	{
		std::vector<ReflectionProbe*>::iterator it = std::find(g_probes.begin(), g_probes.end(), p);
		if (it != g_probes.end()) g_probes.erase(it);
	}

	const std::vector<ReflectionProbe*>& ReflectionProbes::All() { return g_probes; }

	uint32 ReflectionProbes::Count(SceneGraph* scene)
	{
		uint32 n = 0;
		for (size_t i = 0; i < g_probes.size(); i++)
			if (scene == NULL || g_probes[i]->scene == scene) n++;
		return n;
	}

	void ReflectionProbes::CaptureAll(SceneGraph* scene)
	{
		for (size_t i = 0; i < g_probes.size(); i++)
			if (scene == NULL || g_probes[i]->scene == scene) g_probes[i]->Capture();
	}
}
