//============================================================================
// Name        : RigAsset.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Per-skeleton authoring data - bone masks, joint limits, IK chains
//============================================================================

#include <Pyros3D/AnimationManager/RigAsset.h>
#include <Pyros3D/Utils/Json/json.hpp>
#include <Pyros3D/Core/Math/Math.h>
#include <fstream>

namespace p3d {

	namespace {

		using json = nlohmann::json;

		const int32 kRigVersion = 1;

		Vec3 ReadVec3(const json &j, const Vec3 &fallback)
		{
			if (!j.is_array() || j.size() != 3) return fallback;
			return Vec3(j[0].get<f32>(), j[1].get<f32>(), j[2].get<f32>());
		}

		json WriteVec3(const Vec3 &v)
		{
			return json::array({ v.x, v.y, v.z });
		}

		// DEGTORAD/RADTODEG in Core/Math/Math.h are function-like macros
		// yielding double, so they are applied per component rather than to
		// the vector.
		Vec3 DegToRad(const Vec3 &v)
		{
			return Vec3((f32)DEGTORAD(v.x), (f32)DEGTORAD(v.y), (f32)DEGTORAD(v.z));
		}

		Vec3 RadToDeg(const Vec3 &v)
		{
			return Vec3((f32)RADTODEG(v.x), (f32)RADTODEG(v.y), (f32)RADTODEG(v.z));
		}

	}

	std::string RigAsset::SidecarPathFor(const std::string &modelPath)
	{
		// Replace the extension rather than appending, so human.p3dm gives
		// human.rig.json and not human.p3dm.rig.json. Only the final
		// extension is dropped, so a dotted directory name is safe.
		const size_t slash = modelPath.find_last_of("/\\");
		const size_t dot = modelPath.find_last_of('.');
		if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
			return modelPath + ".rig.json";
		return modelPath.substr(0, dot) + ".rig.json";
	}

	bool RigAsset::Load(const std::string &filename)
	{
		BoneMasks.clear();
		JointLimits.clear();
		IKChains.clear();

		std::ifstream in(filename.c_str());
		// No file is the normal case for a model nobody has authored a rig
		// for yet, so it is emphatically not an error - reporting one would
		// make every mesh bind log a warning.
		if (!in.good()) return true;

		json j;
		try { in >> j; }
		catch (const std::exception &e)
		{
			echo(std::string("ERROR: RigAsset - could not parse ") + filename + ": " + e.what());
			return false;
		}
		if (!j.is_object())
		{
			echo(std::string("ERROR: RigAsset - ") + filename + " is not a JSON object");
			return false;
		}

		if (j.contains("boneMasks") && j["boneMasks"].is_array())
		{
			for (const json &m : j["boneMasks"])
			{
				BoneMask mask;
				mask.Name = m.value("name", std::string());
				if (mask.Name.empty()) continue;
				if (m.contains("bones") && m["bones"].is_array())
					for (const json &b : m["bones"])
						if (b.is_string()) mask.Bones.push_back(b.get<std::string>());
				BoneMasks.push_back(mask);
			}
		}

		if (j.contains("jointLimits") && j["jointLimits"].is_array())
		{
			for (const json &l : j["jointLimits"])
			{
				const std::string bone = l.value("bone", std::string());
				if (bone.empty()) continue;

				// DEGREES on disk, radians in memory. The engine's maths is
				// all radians (Quaternion::SetRotationFromEuler and
				// Matrix::GetEulerFromRotationMatrix both), but this file is
				// the one place a person types an angle by hand, and "knee
				// bends 0 to 150" is how a person thinks about it. The field
				// names say Deg so the two can never be confused.
				JointLimit limit;
				limit.Min = DegToRad(ReadVec3(l["minDeg"], Vec3(-180.f, -180.f, -180.f)));
				limit.Max = DegToRad(ReadVec3(l["maxDeg"], Vec3(180.f, 180.f, 180.f)));
				limit.Enabled = l.value("enabled", true);
				JointLimits[bone] = limit;
			}
		}

		if (j.contains("ikChains") && j["ikChains"].is_array())
		{
			for (const json &c : j["ikChains"])
			{
				IKChainDef chain;
				chain.Name = c.value("name", std::string());
				chain.RootBone = c.value("root", std::string());
				chain.EffectorBone = c.value("effector", std::string());
				if (chain.Name.empty() || chain.RootBone.empty() || chain.EffectorBone.empty()) continue;
				chain.UsePole = c.value("usePole", false);
				if (c.contains("pole")) chain.Pole = ReadVec3(c["pole"], Vec3(0.f, 0.f, 0.f));
				IKChains.push_back(chain);
			}
		}

		return true;
	}

	bool RigAsset::Save(const std::string &filename) const
	{
		json j;
		j["version"] = kRigVersion;
		// Not read back - purely so the file explains itself when someone
		// opens it to hand-edit a limit.
		j["_comment"] = "Rig data for the .p3dm beside this file. Keyed by bone name, "
			"so models sharing a skeleton can share this file. Joint limit angles are DEGREES.";

		json masks = json::array();
		for (size_t i = 0; i < BoneMasks.size(); i++)
		{
			json m;
			m["name"] = BoneMasks[i].Name;
			m["bones"] = BoneMasks[i].Bones;
			masks.push_back(m);
		}
		j["boneMasks"] = masks;

		json limits = json::array();
		for (std::map<std::string, JointLimit>::const_iterator it = JointLimits.begin(); it != JointLimits.end(); ++it)
		{
			json l;
			l["bone"] = it->first;
			l["minDeg"] = WriteVec3(RadToDeg(it->second.Min));
			l["maxDeg"] = WriteVec3(RadToDeg(it->second.Max));
			l["enabled"] = it->second.Enabled;
			limits.push_back(l);
		}
		j["jointLimits"] = limits;

		json chains = json::array();
		for (size_t i = 0; i < IKChains.size(); i++)
		{
			json c;
			c["name"] = IKChains[i].Name;
			c["root"] = IKChains[i].RootBone;
			c["effector"] = IKChains[i].EffectorBone;
			c["usePole"] = IKChains[i].UsePole;
			if (IKChains[i].UsePole) c["pole"] = WriteVec3(IKChains[i].Pole);
			chains.push_back(c);
		}
		j["ikChains"] = chains;

		std::ofstream out(filename.c_str());
		if (!out.good())
		{
			echo(std::string("ERROR: RigAsset - could not write ") + filename);
			return false;
		}
		out << j.dump(1, '\t') << std::endl;
		return true;
	}

	std::map<int32, JointLimit> RigAsset::ResolveLimits(SkeletonAnimationInstance* inst) const
	{
		std::map<int32, JointLimit> out;
		if (!inst) return out;

		const std::vector<Bone> &bones = inst->GetSkeletonBones();
		for (std::map<std::string, JointLimit>::const_iterator it = JointLimits.begin(); it != JointLimits.end(); ++it)
		{
			for (size_t i = 0; i < bones.size(); i++)
			{
				if (bones[i].name.compare(it->first) == 0)
				{
					out[bones[i].self] = it->second;
					break;
				}
			}
		}
		return out;
	}

	const IKChainDef* RigAsset::FindChain(const std::string &name) const
	{
		for (size_t i = 0; i < IKChains.size(); i++)
			if (IKChains[i].Name.compare(name) == 0) return &IKChains[i];
		return NULL;
	}

	const BoneMask* RigAsset::FindMask(const std::string &name) const
	{
		for (size_t i = 0; i < BoneMasks.size(); i++)
			if (BoneMasks[i].Name.compare(name) == 0) return &BoneMasks[i];
		return NULL;
	}

	bool RigAsset::ResolveChain(SkeletonAnimationInstance* inst, const std::string &name, IKChain &out) const
	{
		const IKChainDef* def = FindChain(name);
		if (!def || !inst) return false;

		const std::vector<Bone> &bones = inst->GetSkeletonBones();
		out.Name = def->Name;
		out.RootBone = -1;
		out.EffectorBone = -1;
		out.Pole = def->UsePole ? def->Pole : Vec3(0.f, 0.f, 0.f);

		for (size_t i = 0; i < bones.size(); i++)
		{
			if (bones[i].name.compare(def->RootBone) == 0)     out.RootBone = bones[i].self;
			if (bones[i].name.compare(def->EffectorBone) == 0) out.EffectorBone = bones[i].self;
		}
		if (out.RootBone < 0 || out.EffectorBone < 0) return false;

		out.Bones = IKSolver::BuildChain(inst, out.RootBone, out.EffectorBone);
		return !out.Bones.empty();
	}

}
