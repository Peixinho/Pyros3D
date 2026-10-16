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
#include <algorithm>
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
		// A file written by hand may spell them out. Anything else is said, not
		// guessed silently: read as ">" a "less" made two opposite transitions
		// both fire, and the state flipped every frame.
		int OpFromName(const std::string &s)
		{
			for (int i = 0; i < 6; i++) if (s == kOpNames[i]) return i;
			static const char* words[] = { "greater", "less", "equal", "notEqual", "isTrue", "isFalse" };
			for (int i = 0; i < 6; i++) if (s == words[i]) return i;
			echo("WARNING: Animator - unknown condition '" + s + "' (use >, <, ==, !=, true or false); read as '>'");
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

	int AnimatorLayer::FindState(const std::string &name) const
	{
		for (size_t i = 0; i < states.size(); i++) if (states[i].name == name) return (int)i;
		return -1;
	}

	std::string AnimatorLayer::UniqueStateName(const std::string &wanted) const
	{
		return Unique(wanted, [this](const std::string &n) { return FindState(n) >= 0; });
	}

	int AnimatorController::FindParam(const std::string &name) const
	{
		for (size_t i = 0; i < params.size(); i++) if (params[i].name == name) return (int)i;
		return -1;
	}

	int AnimatorController::FindLayer(const std::string &name) const
	{
		for (size_t i = 0; i < layers.size(); i++) if (layers[i].name == name) return (int)i;
		return -1;
	}

	std::string AnimatorController::UniqueParamName(const std::string &wanted) const
	{
		return Unique(wanted.empty() ? std::string("param") : wanted, [this](const std::string &n) { return FindParam(n) >= 0; });
	}

	std::string AnimatorController::UniqueLayerName(const std::string &wanted) const
	{
		return Unique(wanted.empty() ? std::string("Layer") : wanted, [this](const std::string &n) { return FindLayer(n) >= 0 || n.empty(); });
	}

	namespace {
		json MachineToJson(const AnimatorLayer &m)
		{
			json j;
			j["default"] = m.defaultState;
			json states = json::array();
			for (size_t i = 0; i < m.states.size(); i++)
			{
				const AnimatorState &s = m.states[i];
				json sj = { { "name", s.name }, { "clip", s.clip }, { "speed", s.speed }, { "loop", s.loop },
					{ "pos", json::array({ s.position.x, s.position.y }) } };
				if (s.IsBlend())
				{
					json clips = json::array();
					for (size_t k = 0; k < s.blend.size(); k++) clips.push_back({ { "clip", s.blend[k].clip }, { "at", s.blend[k].at } });
					sj["blend"] = { { "param", s.blendParam }, { "clips", clips } };
					if (s.pace) { sj["blend"]["pace"] = true; sj["blend"]["paceMin"] = s.paceMin; sj["blend"]["paceMax"] = s.paceMax; }
				}
				states.push_back(sj);
			}
			j["states"] = states;
			json trs = json::array();
			for (size_t i = 0; i < m.transitions.size(); i++)
			{
				const AnimatorTransition &t = m.transitions[i];
				json conds = json::array();
				for (size_t k = 0; k < t.conditions.size(); k++)
					conds.push_back({ { "param", t.conditions[k].param }, { "op", kOpNames[std::max(0, std::min(5, t.conditions[k].op))] },
						{ "value", t.conditions[k].value } });
				trs.push_back({ { "from", t.from }, { "to", t.to }, { "duration", t.duration }, { "exitTime", t.exitTime },
					{ "conditions", conds } });
			}
			j["transitions"] = trs;
			return j;
		}

		void MachineFromJson(const json &j, AnimatorLayer &m)
		{
			m.defaultState = j.value("default", std::string());
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
					if (s.contains("blend") && s["blend"].is_object())
					{
						as.blendParam = s["blend"].value("param", std::string());
						as.pace = s["blend"].value("pace", false);
						as.paceMin = s["blend"].value("paceMin", as.paceMin);
						as.paceMax = s["blend"].value("paceMax", as.paceMax);
						if (s["blend"].contains("clips") && s["blend"]["clips"].is_array())
							for (const json &bc : s["blend"]["clips"])
							{
								AnimatorBlendClip c;
								c.clip = bc.value("clip", std::string());
								c.at = bc.value("at", 0.f);
								as.blend.push_back(c);
							}
					}
					if (!as.name.empty()) m.states.push_back(as);
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
					m.transitions.push_back(at);
				}
		}
	}

	std::string AnimatorControllerToString(const AnimatorController &c)
	{
		// The base layer's fields stay at the top level, as before layers.
		json j = MachineToJson(c);
		j["version"] = 2;
		json params = json::array();
		for (size_t i = 0; i < c.params.size(); i++)
			params.push_back({ { "name", c.params[i].name }, { "type", kTypeNames[std::max(0, std::min(2, c.params[i].type))] },
				{ "default", c.params[i].defaultValue } });
		j["params"] = params;
		if (!c.layers.empty())
		{
			json layers = json::array();
			for (size_t i = 0; i < c.layers.size(); i++)
			{
				json lj = MachineToJson(c.layers[i]);
				lj["name"] = c.layers[i].name;
				lj["root"] = c.layers[i].rootBone;
				layers.push_back(lj);
			}
			j["layers"] = layers;
		}
		return j.dump(1, '\t');
	}

	bool AnimatorControllerFromString(const std::string &text, AnimatorController &out, std::string *errorOut)
	{
		json j;
		try { j = json::parse(text); }
		catch (const std::exception &e) { if (errorOut) *errorOut = e.what(); return false; }
		if (!j.is_object()) { if (errorOut) *errorOut = "not a JSON object"; return false; }
		AnimatorController c;
		MachineFromJson(j, c);
		if (j.contains("params") && j["params"].is_array())
			for (const json &p : j["params"])
			{
				AnimatorParam ap;
				ap.name = p.value("name", std::string());
				ap.type = TypeFromName(p.value("type", std::string("float")));
				ap.defaultValue = p.value("default", 0.f);
				if (!ap.name.empty()) c.params.push_back(ap);
			}
		if (j.contains("layers") && j["layers"].is_array())
			for (const json &lj : j["layers"])
			{
				AnimatorLayer l;
				l.name = lj.value("name", std::string());
				l.rootBone = lj.value("root", std::string());
				MachineFromJson(lj, l);
				if (!l.name.empty()) c.layers.push_back(l);
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

	Animator::Animator() : instance(NULL) {}

	void Animator::Bind(const AnimatorController &c, SkeletonAnimationInstance* inst)
	{
		controller = c;
		instance = inst;
		values.clear();
		for (size_t i = 0; i < controller.params.size(); i++)
			values[controller.params[i].name] = controller.params[i].type == ANIMATOR_TRIGGER ? 0.f : controller.params[i].defaultValue;
		machines.clear();
		Machine base;
		base.def = &controller;
		machines.push_back(base);
		for (size_t i = 0; i < controller.layers.size(); i++)
		{
			Machine m;
			m.def = &controller.layers[i];
			m.layerName = controller.layers[i].name;
			machines.push_back(m);
			// The skeleton's own layer: the root bone and everything under it.
			if (instance && !m.layerName.empty() && !controller.layers[i].rootBone.empty())
			{
				instance->CreateLayer(m.layerName);
				instance->AddBoneAndChilds(m.layerName, controller.layers[i].rootBone, true);
			}
		}
	}

	void Animator::SetInstance(SkeletonAnimationInstance* inst)
	{
		const std::map<std::string, f32> kept = values;
		std::vector<std::string> states(machines.size());
		for (size_t i = 0; i < machines.size(); i++) if (machines[i].started) states[i] = machines[i].current;
		const AnimatorController c = controller;
		Bind(c, inst);
		values = kept;
		for (size_t i = 0; i < states.size() && i < machines.size(); i++)
		{
			const int si = states[i].empty() ? -1 : machines[i].def->FindState(states[i]);
			if (si < 0) continue;
			machines[i].started = true;
			Enter(machines[i], si, 0.f, -1);
		}
	}

	void Animator::Unbind()
	{
		instance = NULL;
		machines.clear();
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

	const Animator::Machine* Animator::FindMachine(const std::string &layer) const
	{
		for (size_t i = 0; i < machines.size(); i++) if (machines[i].layerName == layer) return &machines[i];
		return NULL;
	}

	const std::string &Animator::GetState(const std::string &layer) const
	{
		static const std::string none;
		const Machine* m = FindMachine(layer);
		return m ? m->current : none;
	}

	int Animator::GetLastTransition(const std::string &layer) const
	{
		const Machine* m = FindMachine(layer);
		return m ? m->lastTransition : -1;
	}

	std::vector<f32> Animator::GetBlendWeights(const std::string &layer) const
	{
		const Machine* m = FindMachine(layer);
		return m ? m->weights : std::vector<f32>();
	}

	f32 Animator::Progress(const Machine &m) const
	{
		if (m.currentIndex < 0 || m.clipDuration <= 0.f) return 0.f;
		const AnimatorState &s = m.def->states[m.currentIndex];
		const f32 played = m.stateTime * std::fabs(s.speed) / m.clipDuration;
		if (!s.loop) return played > 1.f ? 1.f : played;
		return played - std::floor(played);
	}

	f32 Animator::GetStateProgress(const std::string &layer) const
	{
		const Machine* m = FindMachine(layer);
		return m ? Progress(*m) : 0.f;
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

	namespace {
		bool OnLayer(SkeletonAnimationInstance* inst, const uint32 order, const std::string &layer)
		{
			return inst->GetAnimationLayerName(order) == layer;
		}
	}

	// A blend state's weights from its parameter, pushed to its clips: the
	// engine blends the playing list in order, each clip over what is below
	// it by its scale (0 all of it, 1 none), so for clips k..n in play order
	// with weights w the scale of k is (sum of the weights below k) / (sum
	// from k down) - the bottom one 0. And every clip runs at the speed that
	// stretches it to the blended length, so they stay in step.
	void Animator::Reweigh(Machine &m, const f32 /*fadeIn*/)
	{
		if (!instance || m.currentIndex < 0) return;
		const AnimatorState &s = m.def->states[m.currentIndex];
		if (!s.IsBlend()) return;
		SkeletonAnimation* owner = instance->GetOwner();
		if (!owner) return;
		const size_t n = s.blend.size();
		// Weights along the parameter, over the clips sorted by position.
		std::vector<size_t> sorted(n);
		for (size_t i = 0; i < n; i++) sorted[i] = i;
		std::sort(sorted.begin(), sorted.end(), [&](size_t a, size_t b) { return s.blend[a].at < s.blend[b].at; });
		std::vector<f32> w(n, 0.f);
		const f32 v = GetFloat(s.blendParam);
		if (v <= s.blend[sorted[0]].at) w[sorted[0]] = 1.f;
		else if (v >= s.blend[sorted[n - 1]].at) w[sorted[n - 1]] = 1.f;
		else
			for (size_t k = 0; k + 1 < n; k++)
			{
				const f32 a = s.blend[sorted[k]].at, b = s.blend[sorted[k + 1]].at;
				if (v >= a && v <= b)
				{
					const f32 u = b > a ? (v - a) / (b - a) : 0.f;
					w[sorted[k]] = 1.f - u;
					w[sorted[k + 1]] = u;
					break;
				}
			}
		m.weights = w;

		// The clips' places in the playing list, and their lengths.
		std::vector<int32> order(n, -1);
		std::vector<f32> len(n, 0.f);
		f32 blended = 0.f;
		for (size_t i = 0; i < n; i++)
		{
			const int32 id = owner->GetAnimationIDByName(s.blend[i].clip);
			if (id < 0) continue;
			order[i] = instance->GetAnimationPositionInVector((uint32)id);
			if (order[i] < 0) continue;
			len[i] = instance->GetAnimationDuration((uint32)order[i]);
			blended += w[i] * len[i];
		}
		// The rate of the whole mix: fixed, or paced by the parameter.
		f32 rate = s.speed;
		if (s.pace)
		{
			f32 at = 0.f;
			for (size_t i = 0; i < n; i++) at += w[i] * s.blend[i].at;
			f32 k = at > 1e-4f ? v / at : 1.f;
			const f32 lo = std::min(s.paceMin, s.paceMax), hi = std::max(s.paceMin, s.paceMax);
			k = std::max(lo, std::min(hi, k));
			rate *= k;
		}
		m.clipDuration = std::fabs(rate) > 1e-6f ? blended * std::fabs(s.speed / rate) : blended;
		// Top of the group (lowest list position) first.
		std::vector<size_t> byOrder;
		for (size_t i = 0; i < n; i++) if (order[i] >= 0) byOrder.push_back(i);
		std::sort(byOrder.begin(), byOrder.end(), [&](size_t a, size_t b) { return order[a] < order[b]; });
		f32 rest = 0.f;
		for (size_t k = 0; k < byOrder.size(); k++) rest += w[byOrder[k]];
		for (size_t k = 0; k < byOrder.size(); k++)
		{
			const size_t i = byOrder[k];
			const f32 below = rest - w[i];
			const f32 scale = (k + 1 == byOrder.size()) ? 0.f : (rest > 1e-6f ? below / rest : 1.f);
			instance->SetAnimationScale((uint32)order[i], scale);
			if (blended > 1e-6f && len[i] > 0.f) instance->SetAnimationSpeed((uint32)order[i], rate * len[i] / blended);
			rest = below;
		}
	}

	void Animator::Enter(Machine &m, const int stateIndex, const f32 fade, const int viaTransition)
	{
		if (stateIndex < 0 || stateIndex >= (int)m.def->states.size()) return;
		const AnimatorState &s = m.def->states[stateIndex];
		const bool sameClip = m.currentIndex >= 0 && !m.def->states[m.currentIndex].IsBlend()
			&& !s.IsBlend() && m.def->states[m.currentIndex].clip == s.clip;
		m.current = s.name;
		m.currentIndex = stateIndex;
		m.stateTime = 0.f;
		m.lastTransition = viaTransition;
		m.clipDuration = 0.f;
		m.weights.clear();
		if (!instance || !instance->GetOwner()) return;
		SkeletonAnimation* owner = instance->GetOwner();

		if (s.IsBlend())
		{
			std::vector<int32> ids;
			for (size_t i = 0; i < s.blend.size(); i++)
			{
				const int32 id = owner->GetAnimationIDByName(s.blend[i].clip);
				if (id < 0) echo("WARNING: Animator - blend state '" + s.name + "' uses a clip '" + s.blend[i].clip + "' this character does not have");
				ids.push_back(id);
			}
			// The phase the group starts at: a member already playing keeps it
			// (a walk carries on into the locomotion blend without a hitch).
			f32 phase = -1.f;
			for (size_t i = 0; i < ids.size() && phase < 0.f; i++)
			{
				if (ids[i] < 0) continue;
				const int32 o = instance->GetAnimationPositionInVector((uint32)ids[i]);
				if (o >= 0 && OnLayer(instance, (uint32)o, m.layerName))
				{
					const f32 d = instance->GetAnimationDuration((uint32)o);
					const f32 p = d > 0.f ? instance->GetAnimationCurrentTime((uint32)o) / d : 0.f;
					phase = p - std::floor(p);
				}
			}
			if (phase < 0.f) phase = 0.f;
			// Everything else on this layer fades out over the group.
			for (int32 o = (int32)instance->GetNumberPlayingAnimations() - 1; o >= 0; o--)
			{
				if (!OnLayer(instance, (uint32)o, m.layerName)) continue;
				bool member = false;
				for (size_t i = 0; i < ids.size(); i++) if (ids[i] >= 0 && instance->GetAnimationID((uint32)o) == (uint32)ids[i]) member = true;
				if (member) instance->StopAnimation((uint32)o);   // re-queued below, at the bottom
				else instance->FadeOut((uint32)o, fade);
			}
			for (size_t i = 0; i < ids.size(); i++)
				if (ids[i] >= 0) instance->Play((uint32)ids[i], phase, s.loop ? -1.f : 1.f, s.speed, 1.f, m.layerName);
			Reweigh(m, fade);
			return;
		}

		const int32 id = s.clip.empty() ? -1 : owner->GetAnimationIDByName(s.clip);
		if (id < 0)
		{
			if (!s.clip.empty())
				echo("WARNING: Animator - state '" + s.name + "' plays a clip '" + s.clip + "' this character does not have");
			// An empty state on a layer: the layer lets go and the base shows.
			if (!m.layerName.empty())
				for (int32 o = (int32)instance->GetNumberPlayingAnimations() - 1; o >= 0; o--)
					if (OnLayer(instance, (uint32)o, m.layerName)) instance->FadeOut((uint32)o, fade);
			return;
		}
		// The same clip carries on where it is (a state change that only
		// changes speed must not restart a walk); a new one starts at 0.
		const int32 order = instance->CrossFade((uint32)id, fade, s.loop ? -1.f : 1.f, s.speed, sameClip ? -1.f : 0.f, m.layerName);
		if (order >= 0) m.clipDuration = instance->GetAnimationDuration((uint32)order);
	}

	bool Animator::Play(const std::string &state, const f32 fade, const std::string &layer)
	{
		for (size_t k = 0; k < machines.size(); k++)
		{
			if (machines[k].layerName != layer) continue;
			const int i = machines[k].def->FindState(state);
			if (i < 0) return false;
			machines[k].started = true;
			Enter(machines[k], i, fade, -1);
			return true;
		}
		return false;
	}

	void Animator::UpdateMachine(Machine &m, const f32 dt)
	{
		if (!m.started)
		{
			m.started = true;
			int first = m.def->FindState(m.def->defaultState);
			if (first < 0 && !m.def->states.empty()) first = 0;
			Enter(m, first, 0.f, -1);
			return;
		}
		m.stateTime += dt;
		if (m.currentIndex >= 0 && m.def->states[m.currentIndex].IsBlend()) Reweigh(m, 0.f);
		const f32 progress = Progress(m);
		const bool done = m.currentIndex >= 0 && !m.def->states[m.currentIndex].loop
			&& m.clipDuration > 0.f && m.stateTime * std::fabs(m.def->states[m.currentIndex].speed) >= m.clipDuration;

		// Any-state transitions first: "got hit" wins over whatever is playing.
		for (int pass = 0; pass < 2; pass++)
			for (size_t i = 0; i < m.def->transitions.size(); i++)
			{
				const AnimatorTransition &t = m.def->transitions[i];
				const bool any = t.from == "*";
				if ((pass == 0) != any) continue;
				if (!any && t.from != m.current) continue;
				if (any && t.to == m.current) continue;
				if (t.exitTime >= 0.f && !(progress >= t.exitTime || done)) continue;
				if (t.exitTime < 0.f && t.conditions.empty()) continue;   // would fire every frame
				if (!Evaluate(t)) continue;
				const int to = m.def->FindState(t.to);
				if (to < 0) continue;
				// Triggers this transition read are used up by it.
				for (size_t k = 0; k < t.conditions.size(); k++)
				{
					const int p = controller.FindParam(t.conditions[k].param);
					if (p >= 0 && controller.params[p].type == ANIMATOR_TRIGGER) values[t.conditions[k].param] = 0.f;
				}
				Enter(m, to, t.duration, (int)i);
				return;
			}
	}

	void Animator::Update(const f32 dt)
	{
		if (!instance) return;
		// The base first, so a layer entering the same frame sits over it.
		for (size_t i = 0; i < machines.size(); i++) UpdateMachine(machines[i], dt);
	}

}
