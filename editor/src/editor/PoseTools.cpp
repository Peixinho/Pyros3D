//=============================================================================
// Name        : PoseTools.cpp
// Description : See PoseTools.h.
//=============================================================================

#include "PoseTools.h"
#include <algorithm>
#include <cmath>

using namespace p3d;

namespace PoseTools {

namespace {

// Side markers, most specific first. Each pair is tried in both directions.
struct SidePair { const char* a; const char* b; bool prefix; bool suffix; bool anywhere; };
const SidePair kPairs[] = {
	{ "Left", "Right", false, false, true },
	{ "left", "right", false, false, true },
	{ "LEFT", "RIGHT", false, false, true },
	{ ".L", ".R", false, true, false },
	{ ".l", ".r", false, true, false },
	{ "_L", "_R", false, true, false },
	{ "_l", "_r", false, true, false },
	{ "-L", "-R", false, true, false },
	{ " L", " R", false, true, false },
	{ "L_", "R_", true, false, false },
	{ "l_", "r_", true, false, false },
	{ "L.", "R.", true, false, false },
	{ "_L_", "_R_", false, false, true },
	{ "_l_", "_r_", false, false, true },
	{ ".L.", ".R.", false, false, true },
};

// A namespace ("mixamorig:") is not part of the side test: a prefix marker
// sits after it.
size_t BodyStart(const std::string& name)
{
	const size_t colon = name.rfind(':');
	return colon == std::string::npos ? 0 : colon + 1;
}

bool Swap(const std::string& name, const std::string& from, const std::string& to,
	const SidePair& p, std::string& out)
{
	const size_t body = BodyStart(name);
	if (p.prefix)
	{
		if (name.compare(body, from.size(), from) == 0)
		{
			out = name.substr(0, body) + to + name.substr(body + from.size());
			return true;
		}
		return false;
	}
	if (p.suffix)
	{
		if (name.size() >= from.size() && name.size() - from.size() >= body
			&& name.compare(name.size() - from.size(), from.size(), from) == 0)
		{
			out = name.substr(0, name.size() - from.size()) + to;
			return true;
		}
		return false;
	}
	const size_t at = name.find(from, body);
	if (at == std::string::npos) return false;
	out = name.substr(0, at) + to + name.substr(at + from.size());
	return true;
}

// Model-space globals from per-bone locals, parents first. Bones are in id
// order with every parent at a lower index (the instance's own invariant),
// so one forward pass composes the whole tree.
std::vector<Matrix> Globals(SkeletonAnimationInstance* inst, const std::vector<Matrix>& locals)
{
	const std::vector<Bone>& bones = inst->GetSkeletonBones();
	std::vector<Matrix> g(bones.size());
	for (size_t i = 0; i < bones.size(); i++)
	{
		const int p = bones[i].parent;
		g[i] = (p >= 0 && p < (int)i) ? g[p] * locals[i] : locals[i];
	}
	return g;
}

} // namespace

std::string CounterpartName(const std::string& name)
{
	for (size_t i = 0; i < sizeof(kPairs) / sizeof(kPairs[0]); i++)
	{
		std::string out;
		if (Swap(name, kPairs[i].a, kPairs[i].b, kPairs[i], out)) return out;
		if (Swap(name, kPairs[i].b, kPairs[i].a, kPairs[i], out)) return out;
	}
	return std::string();
}

int CounterpartBone(SkeletonAnimationInstance* inst, int boneId)
{
	if (!inst || boneId < 0 || boneId >= (int)inst->GetNumberBones()) return -1;
	const std::vector<Bone>& bones = inst->GetSkeletonBones();
	const std::string other = CounterpartName(bones[boneId].name);
	if (other.empty()) return boneId;
	for (size_t i = 0; i < bones.size(); i++)
		if (bones[i].name == other) return bones[i].self;
	// A side in the name but no partner (a one-armed rig, a prop bone):
	// mirror it onto itself rather than dropping it.
	return boneId;
}

std::vector<Matrix> MirrorPose(SkeletonAnimationInstance* inst, const std::vector<Matrix>& locals, int axis)
{
	if (!inst) return locals;
	const std::vector<Bone>& bones = inst->GetSkeletonBones();
	const size_t n = bones.size();
	if (locals.size() != n) return locals;

	std::vector<Matrix> bindLocals(n);
	for (size_t i = 0; i < n; i++) bindLocals[i] = inst->GetBindPoseLocal((int32)i);
	const std::vector<Matrix> bind = Globals(inst, bindLocals);
	const std::vector<Matrix> now = Globals(inst, locals);

	Matrix M;
	if (axis == 1) M.m[5] = -1.f;
	else if (axis == 2) M.m[10] = -1.f;
	else M.m[0] = -1.f;

	// Each bone's mirrored model-space transform: its counterpart's change
	// from bind, reflected, on this bone's own bind.
	std::vector<Matrix> mirrored(n);
	for (size_t j = 0; j < n; j++)
	{
		const int i = CounterpartBone(inst, (int)j);
		const Matrix delta = now[i] * bind[i].Inverse();
		mirrored[j] = (M * delta * M) * bind[j];
	}

	// Back to locals through the MIRRORED parents.
	std::vector<Matrix> out(n);
	for (size_t j = 0; j < n; j++)
	{
		const int p = bones[j].parent;
		out[j] = (p >= 0 && p < (int)j) ? mirrored[p].Inverse() * mirrored[j] : mirrored[j];
	}
	return out;
}

std::string NormalizeBoneName(const std::string& name)
{
	std::string n = name.substr(BodyStart(name));
	std::string low;
	for (size_t i = 0; i < n.size(); i++)
	{
		const char c = n[i];
		if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) low += c;
		else if (c >= 'A' && c <= 'Z') low += (char)(c - 'A' + 'a');
	}
	static const char* prefixes[] = { "mixamorig", "bip01", "bip001", "armature", "def", "skeleton" };
	for (size_t p = 0; p < sizeof(prefixes) / sizeof(prefixes[0]); p++)
	{
		const std::string pre = prefixes[p];
		if (low.size() > pre.size() && low.compare(0, pre.size(), pre) == 0) { low = low.substr(pre.size()); break; }
	}
	return low;
}

std::map<std::string, std::string> AutoBoneMap(SkeletonAnimationInstance* source, SkeletonAnimationInstance* target)
{
	std::map<std::string, std::string> out;
	if (!source || !target) return out;
	std::map<std::string, std::string> byNorm;
	const std::vector<Bone>& sb = source->GetSkeletonBones();
	for (size_t i = 0; i < sb.size(); i++) byNorm[NormalizeBoneName(sb[i].name)] = sb[i].name;
	const std::vector<Bone>& tb = target->GetSkeletonBones();
	for (size_t i = 0; i < tb.size(); i++)
	{
		std::map<std::string, std::string>::const_iterator it = byNorm.find(NormalizeBoneName(tb[i].name));
		if (it != byNorm.end()) out[tb[i].name] = it->second;
	}
	return out;
}

namespace {
Matrix RotOnly(Matrix m)
{
	for (int c = 0; c < 3; c++)
	{
		const f32 len = std::sqrt(m.m[c * 4] * m.m[c * 4] + m.m[c * 4 + 1] * m.m[c * 4 + 1] + m.m[c * 4 + 2] * m.m[c * 4 + 2]);
		if (len > 1e-8f) for (int k = 0; k < 3; k++) m.m[c * 4 + k] /= len;
	}
	m.Translate(Vec3(0.f, 0.f, 0.f));
	return m;
}
}

Animation RetargetClip(SkeletonAnimationInstance* source, SkeletonAnimationInstance* target,
	const Animation& clip, const std::map<std::string, std::string>& targetToSource, float fps)
{
	Animation out;
	out.AnimationName = clip.AnimationName;
	out.Duration = clip.Duration;
	out.TicksPerSecond = 1.f;
	out.Flags = clip.Flags;
	out.AuthoredFps = fps;
	out.Events = clip.Events;
	out.Tracks = clip.Tracks;
	if (!source || !target || fps <= 0.f) return out;

	const std::vector<Bone>& sb = source->GetSkeletonBones();
	const std::vector<Bone>& tb = target->GetSkeletonBones();
	const size_t ns = sb.size(), nt = tb.size();
	std::vector<Matrix> sBindL(ns), tBindL(nt);
	for (size_t i = 0; i < ns; i++) sBindL[i] = source->GetBindPoseLocal((int32)i);
	for (size_t i = 0; i < nt; i++) tBindL[i] = target->GetBindPoseLocal((int32)i);
	const std::vector<Matrix> sBind = Globals(source, sBindL);
	const std::vector<Matrix> tBind = Globals(target, tBindL);

	// Target bone -> source bone id.
	std::vector<int> srcOf(nt, -1);
	for (size_t j = 0; j < nt; j++)
	{
		std::map<std::string, std::string>::const_iterator it = targetToSource.find(tb[j].name);
		if (it == targetToSource.end()) continue;
		for (size_t i = 0; i < ns; i++) if (sb[i].name == it->second) { srcOf[j] = (int)i; break; }
	}
	// The mapped root: the first mapped target bone with no mapped ancestor.
	int root = -1;
	for (size_t j = 0; j < nt && root < 0; j++)
	{
		if (srcOf[j] < 0) continue;
		bool ancestorMapped = false;
		for (int p = tb[j].parent; p >= 0; p = tb[p].parent) if (srcOf[p] >= 0) { ancestorMapped = true; break; }
		if (!ancestorMapped) root = (int)j;
	}
	// Travel scales with the root's height in bind (a hip height).
	f32 travelScale = 1.f;
	if (root >= 0)
	{
		const f32 hs = sBind[srcOf[root]].GetTranslation().y, ht = tBind[root].GetTranslation().y;
		if (std::fabs(hs) > 1e-5f && std::fabs(ht) > 1e-5f) travelScale = ht / hs;
	}

	for (size_t j = 0; j < nt; j++)
		if (srcOf[j] >= 0)
		{
			Channel ch;
			ch.NodeName = tb[j].name;
			out.Channels.push_back(ch);
		}

	const int samples = std::max(1, (int)std::ceil(clip.Duration * fps - 1e-4f));
	for (int k = 0; k <= samples; k++)
	{
		const f32 t = std::min(clip.Duration, k / fps);
		source->ApplyAnimationAtTime(clip, t);
		std::vector<Matrix> sNow(ns);
		for (size_t i = 0; i < ns; i++) sNow[i] = source->GetBoneGlobalTransform((int32)i);

		std::vector<Matrix> tNow(nt);
		std::vector<Matrix> tLocal(nt);
		for (size_t j = 0; j < nt; j++)
		{
			const int p = tb[j].parent;
			const Matrix parentG = (p >= 0 && p < (int)j) ? tNow[p] : Matrix();
			Matrix local = tBindL[j];
			if (srcOf[j] >= 0)
			{
				const int i = srcOf[j];
				// Rotation: the source bone's change from bind, in model space,
				// applied on the target bone's own bind.
				const Matrix delta = RotOnly(sNow[i]) * RotOnly(sBind[i]).Inverse();
				Matrix wantRot = delta * RotOnly(tBind[j]);
				Matrix l = RotOnly(parentG).Inverse() * wantRot;
				Vec3 lt = tBindL[j].GetTranslation();
				if ((int)j == root)
				{
					// Root position: the target's bind plus the source's travel, scaled.
					const Vec3 travel = (sNow[i].GetTranslation() - sBind[i].GetTranslation()) * travelScale;
					const Vec3 want = tBind[j].GetTranslation() + travel;
					lt = parentG.Inverse() * want;
				}
				l.Translate(lt);
				local = l;
			}
			tLocal[j] = local;
			tNow[j] = parentG * local;
		}

		size_t c = 0;
		for (size_t j = 0; j < nt; j++)
		{
			if (srcOf[j] < 0) continue;
			Channel& ch = out.Channels[c++];
			ch.rotations.push_back(RotationData(t, RotOnly(tLocal[j]).ConvertToQuaternion()));
			if ((int)j == root) ch.positions.push_back(PositionData(t, tLocal[j].GetTranslation()));
		}
	}
	target->ResetToBindPose();
	return out;
}

} // namespace PoseTools

