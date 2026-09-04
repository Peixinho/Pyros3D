//============================================================================
// Name        : Character2DAsset.cpp
// Description : Reading and writing .p3d2d - see the header for what one is.
//
//               JSON rather than the binary layout .p3da uses. A character is
//               small (tens of bones, tens of sprites) and is the one asset in
//               this engine a person is likely to want to diff, review or fix
//               by hand after a bad edit; a .p3da is neither of those things
//               and is written by an importer besides.
//============================================================================

#include <Pyros3D/Assets/Character2D/Character2DAsset.h>
#include <Pyros3D/Utils/Json/json.hpp>
#include <Pyros3D/Core/Math/Math.h>
#include <fstream>
#include <sstream>
#include <set>

namespace p3d {

	namespace {

		using json = nlohmann::json;

		const int32 kCharacter2DVersion = 1;

		Vec2 ReadVec2(const json &j, const Vec2 &fallback)
		{
			if (!j.is_array() || j.size() != 2) return fallback;
			return Vec2(j[0].get<f32>(), j[1].get<f32>());
		}

		json WriteVec2(const Vec2 &v) { return json::array({ v.x, v.y }); }

		Vec3 ReadVec3(const json &j, const Vec3 &fallback)
		{
			if (!j.is_array() || j.size() != 3) return fallback;
			return Vec3(j[0].get<f32>(), j[1].get<f32>(), j[2].get<f32>());
		}

		json WriteVec3(const Vec3 &v) { return json::array({ v.x, v.y, v.z }); }

		// Key interpolation. Omitted for a plain linear key with default
		// tangents - the overwhelming majority - so a hand-read file is not
		// buried in "interp": 0.
		void WriteKeyInterp(json &j, const uchar mode, const f32 inTan, const f32 outTan)
		{
			if (mode == INTERP_LINEAR && inTan == 1.f && outTan == 1.f) return;
			j["interp"] = (int)mode;
			if (mode == INTERP_BEZIER)
			{
				j["in"] = (double)inTan;
				j["out"] = (double)outTan;
			}
		}

		void ReadKeyInterp(const json &j, uchar &mode, f32 &inTan, f32 &outTan)
		{
			const int m = j.value("interp", (int)INTERP_LINEAR);
			mode = (uchar)((m >= 0 && m < (int)kInterpolationModeCount) ? m : INTERP_LINEAR);
			inTan = (f32)j.value("in", 1.0);
			outTan = (f32)j.value("out", 1.0);
		}

		// Animation::Guid is 16 RAW bytes (AnimationLoader::GenerateGuid), not
		// text. JSON strings are UTF-8, and nlohmann refuses to dump anything
		// else - so a clip's guid has to be hex here or writing the file
		// throws. Hex rather than base64 because it is trivially reversible by
		// eye in a file people are meant to read.
		std::string GuidToHex(const std::string &raw)
		{
			static const char *digits = "0123456789abcdef";
			std::string out;
			out.reserve(raw.size() * 2);
			for (size_t i = 0; i < raw.size(); i++)
			{
				const unsigned char c = (unsigned char)raw[i];
				out.push_back(digits[c >> 4]);
				out.push_back(digits[c & 0xF]);
			}
			return out;
		}

		std::string GuidFromHex(const std::string &hex)
		{
			// An odd-length or non-hex string is not something this wrote;
			// treat it as "no guid" rather than decoding half of it, since a
			// half-decoded guid would still compare unequal to everything and
			// would look like a real one.
			if (hex.size() % 2) return std::string();
			std::string out;
			out.reserve(hex.size() / 2);
			for (size_t i = 0; i < hex.size(); i += 2)
			{
				int hi = -1, lo = -1;
				for (int k = 0; k < 16; k++)
				{
					const char d = "0123456789abcdef"[k];
					if (hex[i] == d) hi = k;
					if (hex[i + 1] == d) lo = k;
				}
				if (hi < 0 || lo < 0) return std::string();
				out.push_back((char)((hi << 4) | lo));
			}
			return out;
		}

		bool NameTaken(const std::vector<std::string> &names, const std::string &n)
		{
			for (size_t i = 0; i < names.size(); i++) if (names[i] == n) return true;
			return false;
		}

		std::string MakeUnique(const std::vector<std::string> &taken, const std::string &wanted)
		{
			const std::string base = wanted.empty() ? std::string("Unnamed") : wanted;
			if (!NameTaken(taken, base)) return base;
			for (int n = 2; n < 10000; n++)
			{
				std::ostringstream os;
				os << base << " " << n;
				if (!NameTaken(taken, os.str())) return os.str();
			}
			return base;
		}

	}

	int Character2DAsset::FindBone(const std::string &name) const
	{
		for (size_t i = 0; i < bones.size(); i++)
			if (bones[i].name == name) return (int)i;
		return -1;
	}

	int Character2DAsset::FindClip(const std::string &name) const
	{
		for (size_t i = 0; i < clips.size(); i++)
			if (clips[i].AnimationName == name) return (int)i;
		return -1;
	}

	std::string Character2DAsset::UniqueBoneName(const std::string &wanted) const
	{
		std::vector<std::string> taken;
		for (size_t i = 0; i < bones.size(); i++) taken.push_back(bones[i].name);
		return MakeUnique(taken, wanted);
	}

	std::string Character2DAsset::UniquePartName(const std::string &wanted) const
	{
		std::vector<std::string> taken;
		for (size_t i = 0; i < parts.size(); i++) taken.push_back(parts[i].name);
		return MakeUnique(taken, wanted);
	}

	std::string Character2DAsset::UniqueClipName(const std::string &wanted) const
	{
		std::vector<std::string> taken;
		for (size_t i = 0; i < clips.size(); i++) taken.push_back(clips[i].AnimationName);
		return MakeUnique(taken, wanted);
	}

	// The whole reader, given already-parsed JSON. Both the file path and the
	// string path land here, so a character loaded from disk and one restored
	// from an undo snapshot cannot be read differently.
	static bool ParseCharacter2D(const json &j, Character2DAsset &out, std::string *errorOut)
	{
		Character2DAsset a;

		// ---- bones -------------------------------------------------------
		// Parents are stored BY NAME. An index would be a second thing to keep
		// consistent with the array order, and every edit that reorders bones
		// (adding one in the middle of a chain, deleting one) would have to
		// rewrite every later index or silently reparent the skeleton.
		if (j.contains("bones") && j["bones"].is_array())
		{
			const json &bj = j["bones"];
			a.bones.resize(bj.size());
			std::vector<std::string> parentNames(bj.size());
			for (size_t i = 0; i < bj.size(); i++)
			{
				Bone &b = a.bones[i];
				b.name = bj[i].value("name", std::string());
				b.self = (int32)i;
				b.parent = -1;
				parentNames[i] = bj[i].value("parent", std::string());
				// A 2D bone lives in the XY plane and turns about Z only, so
				// that is what the file stores: a pair and a scalar, not two
				// triples with a zero in each. The Bone it fills is the
				// engine's 3D one, which is why the widening happens here.
				const Vec2 p2 = ReadVec2(bj[i].value("pos", json()), Vec2(0.f, 0.f));
				b.pos = Vec3(p2.x, p2.y, 0.f);
				// DEGREES in the file, radians in memory - the engine's Euler
				// angles are radians, but a file a person may edit by hand
				// should not be.
				const f32 deg = (f32)bj[i].value("rot", 0.0);
				b.rot = Quaternion();
				b.rot.SetRotationFromEuler(Vec3(0.f, 0.f, (f32)DEGTORAD(deg)), RotationOrder::XYZ);
				b.scale = Vec3(1.f, 1.f, 1.f);
				b.skinned = true;
				b.bindPoseMat = Matrix();
				b.bindPoseMat.Translate(b.pos);
				b.bindPoseMat *= b.rot.ConvertToMatrix();
				b.bindPoseMat.Scale(b.scale);
			}

			// Resolve parents, then verify the result is a tree with parents
			// ahead of children. Both are invariants the pose composer walks
			// on; a file that breaks them must be rejected here rather than
			// crash somewhere far away.
			for (size_t i = 0; i < a.bones.size(); i++)
			{
				if (parentNames[i].empty()) continue;
				const int p = a.FindBone(parentNames[i]);
				if (p < 0)
				{
					if (errorOut) *errorOut = "bone \"" + a.bones[i].name + "\" names a parent that does not exist: \"" + parentNames[i] + "\"";
					return false;
				}
				if (p == (int)i)
				{
					if (errorOut) *errorOut = "bone \"" + a.bones[i].name + "\" is its own parent";
					return false;
				}
				a.bones[i].parent = (int32)p;
			}
			for (size_t i = 0; i < a.bones.size(); i++)
			{
				if (a.bones[i].parent >= (int32)i)
				{
					if (errorOut) *errorOut = "bone \"" + a.bones[i].name + "\" is stored before its parent";
					return false;
				}
			}
		}

		// ---- sprites -----------------------------------------------------
		if (j.contains("sprites") && j["sprites"].is_array())
		{
			for (const json &pj : j["sprites"])
			{
				SpritePart2D p;
				p.name = pj.value("name", std::string());
				p.bone = pj.value("bone", std::string());
				p.texture = pj.value("texture", std::string());
				p.offset = ReadVec2(pj.value("offset", json()), Vec2(0.f, 0.f));
				p.scale = ReadVec2(pj.value("scale", json()), Vec2(1.f, 1.f));
				p.pivot = ReadVec2(pj.value("pivot", json()), Vec2(0.5f, 0.5f));
				p.z = (f32)pj.value("z", 0.0);
				p.lit = pj.value("lit", false);
				a.parts.push_back(p);
			}
		}

		// ---- clips -------------------------------------------------------
		if (j.contains("clips") && j["clips"].is_array())
		{
			for (const json &cj : j["clips"])
			{
				Animation clip;
				clip.AnimationName = cj.value("name", std::string());
				clip.Duration = (f32)cj.value("duration", 1.0);
				// Times are written in seconds, so the sampler must not scale
				// them - see AnimationLoader::Save's round-trip note.
				clip.TicksPerSecond = 1.f;
				clip.Flags = (uint32)cj.value("flags", 0);
				clip.AuthoredFps = (f32)cj.value("fps", 0.0);
				clip.Guid = GuidFromHex(cj.value("guid", std::string()));
				if (cj.contains("channels") && cj["channels"].is_array())
				{
					for (const json &chj : cj["channels"])
					{
						Channel ch;
						ch.NodeName = chj.value("bone", std::string());
						if (chj.contains("rotations") && chj["rotations"].is_array())
						{
							for (const json &kj : chj["rotations"])
							{
								RotationData k;
								k.Time = (f32)kj.value("t", 0.0);
								const json &q = kj.value("q", json());
								// Stored [x,y,z,w]; the constructor takes
								// (w,x,y,z). Getting this backwards produces a
								// valid-looking quaternion that rotates wrong,
								// which is why it is spelled out.
								if (q.is_array() && q.size() == 4)
									k.Rot = Quaternion(q[3].get<f32>(), q[0].get<f32>(),
									                   q[1].get<f32>(), q[2].get<f32>());
								ReadKeyInterp(kj, k.Mode, k.InTangent, k.OutTangent);
								ch.rotations.push_back(k);
							}
						}
						if (chj.contains("positions") && chj["positions"].is_array())
						{
							for (const json &kj : chj["positions"])
							{
								PositionData k;
								k.Time = (f32)kj.value("t", 0.0);
								k.Pos = ReadVec3(kj.value("p", json()), Vec3(0.f, 0.f, 0.f));
								ReadKeyInterp(kj, k.Mode, k.InTangent, k.OutTangent);
								ch.positions.push_back(k);
							}
						}
						if (chj.contains("scales") && chj["scales"].is_array())
						{
							for (const json &kj : chj["scales"])
							{
								ScalingData k;
								k.Time = (f32)kj.value("t", 0.0);
								k.Scale = ReadVec3(kj.value("s", json()), Vec3(1.f, 1.f, 1.f));
								ReadKeyInterp(kj, k.Mode, k.InTangent, k.OutTangent);
								ch.scales.push_back(k);
							}
						}
						clip.Channels.push_back(ch);
					}
				}
				a.clips.push_back(clip);
			}
		}

		a.defaultClip = j.value("defaultClip", std::string());
		a.defaultClipLoops = j.value("defaultClipLoops", true);
		a.authoredHalfHeight = (f32)j.value("authoredHalfHeight", 0.0);

		out = a;
		return true;
	}

	static json BuildCharacter2DJson(const Character2DAsset &asset)
	{
		json j;
		j["version"] = kCharacter2DVersion;

		json bones = json::array();
		for (size_t i = 0; i < asset.bones.size(); i++)
		{
			const Bone &b = asset.bones[i];
			json bj;
			bj["name"] = b.name;
			if (b.parent >= 0 && (size_t)b.parent < asset.bones.size())
				bj["parent"] = asset.bones[b.parent].name;
			bj["pos"] = WriteVec2(Vec2(b.pos.x, b.pos.y));
			// Degrees out, radians in memory - the same asymmetry the read
			// side documents. Omitted when zero, which is almost every bone.
			Quaternion rot = b.rot;   // GetEulerFromQuaternion is not const
			const Vec3 e = rot.GetEulerFromQuaternion(RotationOrder::XYZ);
			const double deg = RADTODEG(e.z);
			if (deg != 0.0) bj["rot"] = deg;
			bones.push_back(bj);
		}
		j["bones"] = bones;

		json sprites = json::array();
		for (size_t i = 0; i < asset.parts.size(); i++)
		{
			const SpritePart2D &p = asset.parts[i];
			json pj;
			pj["name"] = p.name;
			pj["bone"] = p.bone;
			pj["texture"] = p.texture;
			pj["offset"] = WriteVec2(p.offset);
			pj["scale"] = WriteVec2(p.scale);
			pj["pivot"] = WriteVec2(p.pivot);
			pj["z"] = (double)p.z;
			if (p.lit) pj["lit"] = true;
			sprites.push_back(pj);
		}
		j["sprites"] = sprites;

		json clips = json::array();
		for (size_t ci = 0; ci < asset.clips.size(); ci++)
		{
			const Animation &c = asset.clips[ci];
			json cj;
			cj["name"] = c.AnimationName;
			cj["duration"] = (double)c.Duration;
			if (c.Flags) cj["flags"] = c.Flags;
			if (c.AuthoredFps > 0.f) cj["fps"] = (double)c.AuthoredFps;
			if (!c.Guid.empty()) cj["guid"] = GuidToHex(c.Guid);
			json chans = json::array();
			for (size_t chi = 0; chi < c.Channels.size(); chi++)
			{
				const Channel &ch = c.Channels[chi];
				// A channel with no keys at all animates nothing and only
				// grows the file; the editor creates one the moment a bone is
				// selected, so these are common.
				if (ch.rotations.empty() && ch.positions.empty() && ch.scales.empty()) continue;
				json chj;
				chj["bone"] = ch.NodeName;
				if (!ch.rotations.empty())
				{
					json arr = json::array();
					for (size_t k = 0; k < ch.rotations.size(); k++)
					{
						json kj;
						kj["t"] = (double)ch.rotations[k].Time;
						kj["q"] = json::array({ ch.rotations[k].Rot.x, ch.rotations[k].Rot.y,
						                        ch.rotations[k].Rot.z, ch.rotations[k].Rot.w });
						WriteKeyInterp(kj, ch.rotations[k].Mode, ch.rotations[k].InTangent, ch.rotations[k].OutTangent);
						arr.push_back(kj);
					}
					chj["rotations"] = arr;
				}
				if (!ch.positions.empty())
				{
					json arr = json::array();
					for (size_t k = 0; k < ch.positions.size(); k++)
					{
						json kj;
						kj["t"] = (double)ch.positions[k].Time;
						kj["p"] = WriteVec3(ch.positions[k].Pos);
						WriteKeyInterp(kj, ch.positions[k].Mode, ch.positions[k].InTangent, ch.positions[k].OutTangent);
						arr.push_back(kj);
					}
					chj["positions"] = arr;
				}
				if (!ch.scales.empty())
				{
					json arr = json::array();
					for (size_t k = 0; k < ch.scales.size(); k++)
					{
						json kj;
						kj["t"] = (double)ch.scales[k].Time;
						kj["s"] = WriteVec3(ch.scales[k].Scale);
						WriteKeyInterp(kj, ch.scales[k].Mode, ch.scales[k].InTangent, ch.scales[k].OutTangent);
						arr.push_back(kj);
					}
					chj["scales"] = arr;
				}
				chans.push_back(chj);
			}
			cj["channels"] = chans;
			clips.push_back(cj);
		}
		j["clips"] = clips;

		if (!asset.defaultClip.empty())
		{
			j["defaultClip"] = asset.defaultClip;
			j["defaultClipLoops"] = asset.defaultClipLoops;
		}
		if (asset.authoredHalfHeight > 0.f) j["authoredHalfHeight"] = (double)asset.authoredHalfHeight;

		return j;
	}

	std::string Character2DToString(const Character2DAsset &asset)
	{
		return BuildCharacter2DJson(asset).dump(1, '\t') + "\n";
	}

	bool Character2DFromString(const std::string &text, Character2DAsset &out, std::string *errorOut)
	{
		json j;
		try { j = json::parse(text); }
		catch (const std::exception &e)
		{
			if (errorOut) *errorOut = std::string("malformed character data: ") + e.what();
			return false;
		}
		return ParseCharacter2D(j, out, errorOut);
	}

	bool LoadCharacter2D(const std::string &filename, Character2DAsset &out, std::string *errorOut)
	{
		std::ifstream in(filename.c_str());
		if (!in.is_open())
		{
			if (errorOut) *errorOut = "could not open " + filename;
			return false;
		}

		json j;
		try { in >> j; }
		catch (const std::exception &e)
		{
			if (errorOut) *errorOut = std::string("malformed .p3d2d: ") + e.what();
			return false;
		}
		return ParseCharacter2D(j, out, errorOut);
	}

	bool SaveCharacter2D(const std::string &filename, const Character2DAsset &asset, std::string *errorOut)
	{
		std::ofstream out(filename.c_str(), std::ios::trunc);
		if (!out.is_open())
		{
			if (errorOut) *errorOut = "could not write " + filename;
			return false;
		}
		out << Character2DToString(asset);
		if (!out.good())
		{
			if (errorOut) *errorOut = "failed while writing " + filename;
			return false;
		}
		return true;
	}

}
