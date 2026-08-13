#pragma once

#include <string>
#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Lights/PointLight/PointLight.h>
#include <Pyros3D/Rendering/Components/Lights/SpotLight/SpotLight.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Audio/AudioSource.h>
#include "SceneObjects.h"

namespace EditorIcons {

inline std::string ForComponent(p3d::IComponent* c)
{
	if (dynamic_cast<p3d::DirectionalLight*>(c)) return u8"\uf185 ";
	if (dynamic_cast<p3d::PointLight*>(c)) return u8"\uf0eb ";
	if (dynamic_cast<p3d::SpotLight*>(c)) return u8"\uf0eb ";
	if (dynamic_cast<p3d::RenderingComponent*>(c)) return u8"\uf1b2 ";
	if (dynamic_cast<p3d::AudioSource*>(c)) return u8"\uf028 ";
	return u8"\uf0ad ";
}

inline std::string ForSceneObject(SceneObject* obj, bool isCamera)
{
	if (!obj) return "";
	switch (obj->GetType())
	{
	case SceneObjectTypes::GAMEOBJECT:
		return isCamera ? u8"\uf030 " : u8"\uf247 ";
	case SceneObjectTypes::RENDERING_COMPONENT:
		return u8"\uf1b2 ";
	case SceneObjectTypes::DIRECTIONALLIGHT_COMPONENT:
		return u8"\uf185 ";
	case SceneObjectTypes::POINTLIGHT_COMPONENT:
	case SceneObjectTypes::SPOTLIGHT_COMPONENT:
		return u8"\uf0eb ";
	case SceneObjectTypes::PHYSICS_COMPONENT:
		return u8"\uf0ad ";
	case SceneObjectTypes::AUDIO_SOURCE_COMPONENT:
		return u8"\uf028 ";
	case SceneObjectTypes::LUA_COMPONENT:
		return u8"\uf121 ";
	default:
		return u8"\uf111 ";
	}
}

inline const char* VisibilityOn() { return u8"\uf06e"; }
inline const char* VisibilityOff() { return u8"\uf070"; }

}
