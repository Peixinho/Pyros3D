//============================================================================
// Name        : AnimatorController.cpp
// Description : See AnimatorController.h.
//============================================================================

#include <Pyros3D/AnimationManager/AnimatorController.h>
#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <Pyros3D/Utils/Json/json.hpp>
#include <Pyros3D/Core/Logs/Log.h>
#include <cmath>
#include <functional>
#include <fstream>
#include <sstream>

namespace p3d {

	using json = nlohmann::json;

	namespace {
		const char* kTypeNames[] = { "float", "bool", "trigger" };
		const char* kOpNames[] = { ">", "<", "==", "!=", "true", "false" };

		int TypeFromName(const std::string &s)
		{
			for (int i = 0; i < 3; i++) if (s == kTypeNames[i]) return i;
			return ANIMATOR_FLOAT;
		}
		int OpFromName(const std::string &s)
		{
			for (int i = 0; i < 6; i++) if (s == kOpNames[i]) return i;
			return ANIMATOR_GREATER;
		}
		std::string Unique(const std::string &wanted, const std::function<bool(const std::string&)> &taken)
		{
			const std::string base = wanted.empty() ? std::string("State") : wanted;
			if (!taken(base)) return base;
			for (int i = 2; ; i++)
			{
				const std::string n = base + " " + std::to_string(i);
				if (!taken(n)) return n;
			}
		}
	}

	int AnimatorController::FindState(const std::string &name) const
	{
		for (size_t i = 0; i < states.size(); i++) if (states[i].name == name) return (int)i;
		return -1;
	}

	int AnimatorController::FindParam(const std::string &name) const
	{
		for (size_t i = 0; i < params.size(); i++) if (params[i].name == name) return (int)i;
		return -1;
	}

	std::string AnimatorController::UniqueStateName(const std::string &wanted) const
	{
		return Unique(wanted, [this](const std::string &n) { return FindState(n) >= 0; });
	}

	std::string AnimatorController::UniqueParamName(const std::string &wanted) const
	{
		return Unique(wanted.empty() ? std::string("param") : wanted, [this](const std::string &n) { return FindParam(n) >= 0; });
	}

	std::string AnimatorControllerToString(const AnimatorController &c)
	{
		json j;
		j["version"] = 1;
		j["default"] = c.defaultState;
		json params = json::array();
		for (size_t i = 0; i < c.params.size(); i++)
			params.push_back({ { "name", c.params[i].name }, { "type", kTypeNames[std::max(0, std::min(2, c.params[i].type))] },
				{ "default", c.params[i].defaultValue } });
		j["params"] = params;
		json states = json::array();
		for (size_t i = 0; i < c.states.size(); i++)
		{
			const AnimatorState &s = c.states[i];
			states.push_back({ { "name", s.name }, { "clip", s.clip }, { "speed", s.speed }, { "loop", s.loop },
				{ "pos", json::array({ s.position.x, s.position.y }) } });
		}
		j["states"] = states;
		json trs = json::array();
		for (size_t i = 0; i < c.transitions.size(); i++)
		{
			const AnimatorTransition &t = c.transitions[i];
			json conds = json::array();
			for (size_t k = 0; k < t.conditions.size(); k++)
				conds.push_back({ { "param", t.conditions[k].param }, { "op", kOpNames[std::max(0, std::min(5, t.conditions[k].op))] },
					{ "value", t.conditions[k].value } });
			trs.push_back({ { "from", t.from }, { "to", t.to }, { "duration", t.duration }, { "exitTime", t.exitTime },
				{ "conditions", conds } });
		}
		j["transitions"] = trs;
		return j.dump(1, '\t');
	}

	bool AnimatorControllerFromString(const std::string &text, AnimatorController &out, std::string *errorOut)
	{
		json j;
		try { j = json::parse(text); }
		catch (const std::exception &e) { if (errorOut) *errorOut = e.what(); return false; }
		if (!j.is_object()) { if (errorOut) *errorOut = "not a JSON object"; return false; }
		AnimatorController c;
		c.defaultState = j.value("default", std::string());
		if (j.contains("params") && j["params"].is_array())
			for (const json &p : j["params"])
			{
				AnimatorParam ap;
				ap.name = p.value("name", std::string());
				ap.type = TypeFromName(p.value("type", std::string("float")));
				ap.defaultValue = p.value("default", 0.f);
				if (!ap.name.empty()) c.params.push_back(ap);
			}
		if (j.contains("states") && j["states"].is_array())
			for (const json &s : j["states"])
			{
				AnimatorState as;
				as.name = s.value("name", std::string());
				as.clip = s.value("clip", std::string());
				as.speed = s.value("speed", 1.f);
				as.loop = s.value("loop", true);
				if (s.contains("pos") && s["pos"].is_array() && s["pos"].size() >= 2)
					as.position = Vec2(s["pos"][0].get<f32>(), s["pos"][1].get<f32>());
				if (!as.name.empty()) c.states.push_back(as);
			}
		if (j.contains("transitions") && j["transitions"].is_array())
			for (const json &t : j["transitions"])
			{
				AnimatorTransition at;
				at.from = t.value("from", std::string());
				at.to = t.value("to", std::string());
				at.duration = t.value("duration", 0.2f);
				at.exitTime = t.value("exitTime", -1.f);
				if (t.contains("conditions") && t["conditions"].is_array())
					for (const json &k : t["conditions"])
					{
						AnimatorCondition ac;
						ac.param = k.value("param", std::string());
						ac.op = OpFromName(k.value("op", std::string(">")));
						ac.value = k.value("value", 0.f);
						at.conditions.push_back(ac);
					}
				c.transitions.push_back(at);
			}
		out = c;
		return true;
	}

	bool LoadAnimatorController(const std::string &filename, AnimatorController &out, std::string *errorOut)
	{
		std::ifstream f(filename.c_str());
		if (!f) { if (errorOut) *errorOut = "could not open " + filename; return false; }
		std::stringstream ss;
		ss << f.rdbuf();
		return AnimatorControllerFromString(ss.str(), out, errorOut);
	}

	bool SaveAnimatorController(const std::string &filename, const AnimatorController &c, std::string *errorOut)
	{
		std::ofstream f(filename.c_str());
		if (!f) { if (errorOut) *errorOut = "could not write " + filename; return false; }
		f << AnimatorControllerToString(c);
		return (bool)f;
	}

	// ---- Animator ---------------------------------------------------------------

	Animator::Animator() : instance(NULL), currentIndex(-1), stateTime(0.f), clipDuration(0.f),
		started(false), lastTransition(-1) {}

	void Animator::Bind(const AnimatorController &c, SkeletonAnimationInstance* inst)
	{
		controller = c;
		instance = inst;
		values.clear();
		for (size_t i = 0; i < controller.params.size(); i++)
			values[controller.params[i].name] = controller.params[i].type == ANIMATOR_TRIGGER ? 0.f : controller.params[i].defaultValue;
		current.clear();
		currentIndex = -1;
		stateTime = 0.f;
		started = false;
		lastTransition = -1;
	}

	void Animator::Unbind()
	{
		instance = NULL;
		started = false;
	}

	bool Animator::SetFloat(const std::string &name, const f32 v)
	{
		std::map<std::string, f32>::iterator it = values.find(name);
		if (it == values.end()) return false;
		it->second = v;
		return true;
	}

	bool Animator::SetBool(const std::string &name, const bool v) { return SetFloat(name, v ? 1.f : 0.f); }
	bool Animator::SetTrigger(const std::string &name) { return SetFloat(name, 1.f); }
	bool Animator::ResetTrigger(const std::string &name) { return SetFloat(name, 0.f); }

	f32 Animator::GetFloat(const std::string &name) const
	{
		std::map<std::string, f32>::const_iterator it = values.find(name);
		return it == values.end() ? 0.f : it->second;
	}

	bool Animator::GetBool(const std::string &name) const { return GetFloat(name) > 0.5f; }

	f32 Animator::GetStateProgress() const
	{
		if (currentIndex < 0 || clipDuration <= 0.f) return 0.f;
		const AnimatorState &s = controller.states[currentIndex];
		const f32 played = stateTime * std::fabs(s.speed) / clipDuration;
		if (!s.loop) return played > 1.f ? 1.f : played;
		return played - std::floor(played);
	}

	bool Animator::Evaluate(const AnimatorTransition &t) const
	{
		for (size_t i = 0; i < t.conditions.size(); i++)
		{
			const AnimatorCondition &c = t.conditions[i];
			const f32 v = GetFloat(c.param);
			bool ok = false;
			switch (c.op)
			{
			case ANIMATOR_GREATER:   ok = v > c.value; break;
			case ANIMATOR_LESS:      ok = v < c.value; break;
			case ANIMATOR_EQUAL:     ok = std::fabs(v - c.value) < 1e-5f; break;
			case ANIMATOR_NOT_EQUAL: ok = std::fabs(v - c.value) >= 1e-5f; break;
			case ANIMATOR_TRUE:      ok = v > 0.5f; break;
			case ANIMATOR_FALSE:     ok = v <= 0.5f; break;
			}
			if (!ok) return false;
		}
		return true;
	}

	void Animator::Enter(const int stateIndex, const f32 fade, const int viaTransition)
	{
		if (stateIndex < 0 || stateIndex >= (int)controller.states.size()) return;
		const AnimatorState &s = controller.states[stateIndex];
		const bool sameClip = currentIndex >= 0 && controller.states[currentIndex].clip == s.clip;
		current = s.name;
		currentIndex = stateIndex;
		stateTime = 0.f;
		lastTransition = viaTransition;
		clipDuration = 0.f;
		if (!instance || !instance->GetOwner()) return;
		const int32 id = instance->GetOwner()->GetAnimationIDByName(s.clip);
		if (id < 0)
		{
			echo("WARNING: Animator - state '" + s.name + "' plays a clip '" + s.clip + "' this character does not have");
			return;
		}
		// The same clip carries on where it is (a state change that only
		// changes speed must not restart a walk); a new one starts at 0.
		const int32 order = instance->CrossFade((uint32)id, fade, s.loop ? -1.f : 1.f, s.speed, sameClip ? -1.f : 0.f);
		if (order >= 0) clipDuration = instance->GetAnimationDuration((uint32)order);
	}

	bool Animator::Play(const std::string &state, const f32 fade)
	{
		const int i = controller.FindState(state);
		if (i < 0) return false;
		started = true;
		Enter(i, fade, -1);
		return true;
	}

	void Animator::Update(const f32 dt)
	{
		if (!instance) return;
		if (!started)
		{
			started = true;
			int first = controller.FindState(controller.defaultState);
			if (first < 0 && !controller.states.empty()) first = 0;
			Enter(first, 0.f, -1);
			return;
		}
		stateTime += dt;
		const f32 progress = GetStateProgress();
		const bool done = currentIndex >= 0 && !controller.states[currentIndex].loop
			&& clipDuration > 0.f && stateTime * std::fabs(controller.states[currentIndex].speed) >= clipDuration;

		// Any-state transitions first: "got hit" wins over whatever is playing.
		for (int pass = 0; pass < 2; pass++)
			for (size_t i = 0; i < controller.transitions.size(); i++)
			{
				const AnimatorTransition &t = controller.transitions[i];
				const bool any = t.from == "*";
				if ((pass == 0) != any) continue;
				if (!any && t.from != current) continue;
				if (any && t.to == current) continue;
				if (t.exitTime >= 0.f && !(progress >= t.exitTime || done)) continue;
				if (t.exitTime < 0.f && t.conditions.empty()) continue;   // would fire every frame
				if (!Evaluate(t)) continue;
				const int to = controller.FindState(t.to);
				if (to < 0) continue;
				// Triggers this transition read are used up by it.
				for (size_t k = 0; k < t.conditions.size(); k++)
				{
					const int p = controller.FindParam(t.conditions[k].param);
					if (p >= 0 && controller.params[p].type == ANIMATOR_TRIGGER) values[t.conditions[k].param] = 0.f;
				}
				Enter(to, t.duration, (int)i);
				return;
			}
	}

}
