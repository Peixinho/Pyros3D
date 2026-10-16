//=============================================================================
// Name        : RetargetSession.cpp
// Description : See RetargetSession.h.
//=============================================================================

#include "RetargetSession.h"
#include "PoseTools.h"
#include "AnimationEditorDocument.h"
#include <Pyros3D/Assets/Renderable/Models/Model.h>
#include <Pyros3D/Utils/ModelLoaders/MultiModelLoader/AnimationLoader.h>

using namespace p3d;

bool RetargetSession::Load(const std::string& clipAbs, const std::string& rigAbs, SkeletonAnimationInstance* target)
{
	error.clear();
	source = NULL;
	rc.reset();
	anim.reset();
	model.reset();
	clips.clear();
	chosen.clear();
	map.clear();
	clipFile = clipAbs;
	rigFile = rigAbs;
	if (!target) { error = "bind a rig to retarget onto first"; return false; }

	AnimationLoader loader;
	if (!loader.Load(clipAbs) || loader.animations.empty()) { error = "could not read clips from " + clipAbs; return false; }
	clips = loader.animations;
	chosen.assign(clips.size(), true);
	if (!clips.empty() && clips[0].AuthoredFps > 0.f) fps = clips[0].AuthoredFps;

	try { model = std::make_shared<Model>(rigAbs, true); }
	catch (...) { model.reset(); }
	if (!model) { error = "could not load " + rigAbs; return false; }
	rc.reset(new RenderingComponent(model, ShaderUsage::Diffuse));
	if (!rc->HasBones()) { error = rigAbs + " has no skeleton"; rc.reset(); model.reset(); return false; }
	anim = std::make_shared<SkeletonAnimation>();
	source = anim->CreateInstance(rc.get());
	source->ResetToBindPose();
	map = PoseTools::AutoBoneMap(source, target);
	if (map.empty()) error = "no bone names match - map them by hand below";
	return true;
}

std::vector<std::string> RetargetSession::ImportInto(AnimationEditorDocument& doc)
{
	std::vector<std::string> added;
	SkeletonAnimationInstance* target = doc.RigInstance();
	if (!source || !target) return added;
	for (size_t i = 0; i < clips.size(); i++)
	{
		if (i < chosen.size() && !chosen[i]) continue;
		Animation out = PoseTools::RetargetClip(source, target, clips[i], map, fps);
		out.AnimationName = doc.MakeUniqueClipName(clips[i].AnimationName);
		out.Guid = AnimationLoader::GenerateGuid();
		doc.clips.push_back(out);
		if (reduce) doc.ReduceKeys((int)doc.clips.size() - 1, 0.0005f, 0.1f);
		added.push_back(out.AnimationName);
	}
	if (!added.empty()) doc.activeClip = (int)doc.clips.size() - 1;
	return added;
}
