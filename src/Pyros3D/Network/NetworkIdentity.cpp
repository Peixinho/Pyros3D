//============================================================================
// Name        : NetworkIdentity.cpp
// Author      : Duarte Peixinho
// Description : See NetworkIdentity.h.
//============================================================================

#include <Pyros3D/Network/NetworkIdentity.h>
#include <Pyros3D/GameObjects/GameObject.h>

#include <algorithm>

namespace p3d {

	namespace {
		std::vector<NetworkIdentity*> &Registry()
		{
			static std::vector<NetworkIdentity*> all;
			return all;
		}
	}

	bool NetValue::operator==(const NetValue &o) const
	{
		if (type != o.type) return false;
		switch (type)
		{
		case Number: return number == o.number;
		case Bool: return boolean == o.boolean;
		case String: return text == o.text;
		case Vector: return vector.x == o.vector.x && vector.y == o.vector.y && vector.z == o.vector.z;
		default: return true;
		}
	}

	NetworkIdentity::~NetworkIdentity()
	{
		std::vector<NetworkIdentity*> &all = Registry();
		all.erase(std::remove(all.begin(), all.end(), this), all.end());
	}

	void NetworkIdentity::Register(SceneGraph* Scene)
	{
		if (Registered) return;
		Registered = true;
		Registry().push_back(this);
	}

	void NetworkIdentity::Unregister(SceneGraph* Scene)
	{
		if (!Registered) return;
		Registered = false;
		std::vector<NetworkIdentity*> &all = Registry();
		all.erase(std::remove(all.begin(), all.end(), this), all.end());
	}

	const std::vector<NetworkIdentity*> &NetworkIdentity::All() { return Registry(); }

	void NetworkIdentity::SetVar(const std::string &name, const NetValue &value)
	{
		std::map<std::string, NetValue>::iterator it = vars.find(name);
		if (it != vars.end() && it->second == value) return;
		vars[name] = value;
		varVersion[name] = ++versionCounter;
	}

	bool NetworkIdentity::GetVar(const std::string &name, NetValue &out) const
	{
		std::map<std::string, NetValue>::const_iterator it = vars.find(name);
		if (it == vars.end()) return false;
		out = it->second;
		return true;
	}

	std::string NetworkIdentity::ScenePath(GameObject* go)
	{
		std::string path;
		for (GameObject* g = go; g; g = g->GetParent())
			path = g->GetName() + (path.empty() ? "" : "/" + path);
		return path;
	}

}
