//============================================================================
// Name        : PyrosEmbindHelpers.h
// Description : Shared Embind wrappers (Emscripten only).
//============================================================================

#ifndef PYROSEMBIND_HELPERS_H
#define PYROSEMBIND_HELPERS_H

#if defined(__EMSCRIPTEN__) || defined(EMSCRIPTEN)

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Particles/ParticleSystem.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Lights/PointLight/PointLight.h>
#include <Pyros3D/Rendering/Components/Lights/SpotLight/SpotLight.h>
#include <Pyros3D/Physics/Components/IPhysicsComponent.h>
#include <Pyros3D/Audio/AudioSource.h>
#include <memory>
#include <string>

namespace p3d {
namespace embind_helpers {

	inline float DegToRad(float deg) { return DEGTORAD(deg); }

	// ----- GameObject add helpers (shared_ptr<Derived> → IComponent) -----
	inline void GameObject_AddComponent(GameObject &go, std::shared_ptr<IComponent> c)
	{
		go.Add(c);
	}
	inline void GameObject_RemoveComponent(GameObject &go, std::shared_ptr<IComponent> c)
	{
		go.RemoveComponent(c);
	}
	inline void GameObject_AddRenderingComponent(GameObject &go, std::shared_ptr<RenderingComponent> c)
	{
		go.Add(std::static_pointer_cast<IComponent>(c));
	}
	inline void GameObject_AddDirectionalLight(GameObject &go, std::shared_ptr<DirectionalLight> c)
	{
		go.Add(std::static_pointer_cast<IComponent>(c));
	}
	inline void GameObject_AddPointLight(GameObject &go, std::shared_ptr<PointLight> c)
	{
		go.Add(std::static_pointer_cast<IComponent>(c));
	}
	inline void GameObject_AddSpotLight(GameObject &go, std::shared_ptr<SpotLight> c)
	{
		go.Add(std::static_pointer_cast<IComponent>(c));
	}
	inline void GameObject_AddParticleSystem(GameObject &go, std::shared_ptr<ParticleSystem> c)
	{
		go.Add(std::static_pointer_cast<IComponent>(c));
	}
	inline void GameObject_AddPhysicsComponent(GameObject &go, std::shared_ptr<IPhysicsComponent> c)
	{
		go.Add(std::static_pointer_cast<IComponent>(c));
	}
	inline void GameObject_AddAudioSource(GameObject &go, std::shared_ptr<AudioSource> c)
	{
		go.Add(std::static_pointer_cast<IComponent>(c));
	}
	inline void GameObject_AddChild(GameObject &parent, std::shared_ptr<GameObject> child)
	{
		parent.Add(child);
	}
	inline void GameObject_RemoveChild(GameObject &parent, std::shared_ptr<GameObject> child)
	{
		parent.RemoveGameObject(child);
	}
	inline std::string GameObject_GetName(const GameObject &go)
	{
		return go.GetName();
	}
	inline bool GameObject_HaveTagStr(GameObject &g, const std::string &tag)
	{
		return g.HaveTag(tag);
	}
	inline bool GameObject_HaveTagUint(GameObject &g, const uint32 tag)
	{
		return g.HaveTag(tag);
	}

	inline void Scene_Add(SceneGraph &scene, std::shared_ptr<GameObject> go)
	{
		scene.Add(go);
	}
	inline void Scene_Remove(SceneGraph &scene, std::shared_ptr<GameObject> go)
	{
		scene.Remove(go);
	}
	inline void Scene_AddGameObject(SceneGraph &scene, std::shared_ptr<GameObject> go)
	{
		scene.AddGameObject(go);
	}
	inline void Scene_RemoveGameObject(SceneGraph &scene, std::shared_ptr<GameObject> go)
	{
		scene.RemoveGameObject(go);
	}

	// ----- FrameBuffer (Texture via shared_ptr; Bind default) -----
	inline void FrameBuffer_InitTex(FrameBuffer &f, const uint32 attachmentFormat, const uint32 texType, const std::shared_ptr<Texture> &attachment)
	{
		f.Init(attachmentFormat, texType, attachment.get());
	}
	inline void FrameBuffer_InitRenderBuffer4(FrameBuffer &f, const uint32 attachmentFormat, const uint32 attachmentDataType, const uint32 Width, const uint32 Height)
	{
		f.Init(attachmentFormat, attachmentDataType, Width, Height, 0);
	}
	inline void FrameBuffer_InitRenderBuffer5(FrameBuffer &f, const uint32 attachmentFormat, const uint32 attachmentDataType, const uint32 Width, const uint32 Height, const uint32 msaa)
	{
		f.Init(attachmentFormat, attachmentDataType, Width, Height, msaa);
	}
	inline void FrameBuffer_AddAttachTex(FrameBuffer &f, const uint32 attachmentFormat, const uint32 texType, const std::shared_ptr<Texture> &attachment)
	{
		f.AddAttach(attachmentFormat, texType, attachment.get());
	}
	inline void FrameBuffer_AddAttachRenderBuffer4(FrameBuffer &f, const uint32 attachmentFormat, const uint32 attachmentDataType, const uint32 Width, const uint32 Height)
	{
		f.AddAttach(attachmentFormat, attachmentDataType, Width, Height, 0);
	}
	inline void FrameBuffer_AddAttachRenderBuffer5(FrameBuffer &f, const uint32 attachmentFormat, const uint32 attachmentDataType, const uint32 Width, const uint32 Height, const uint32 msaa)
	{
		f.AddAttach(attachmentFormat, attachmentDataType, Width, Height, msaa);
	}
	inline void FrameBuffer_Bind(FrameBuffer &f, uint32 access)
	{
		f.Bind(access);
	}
	inline void FrameBuffer_BindDefault(FrameBuffer &f)
	{
		f.Bind();
	}

	inline void Texture_Resize2(Texture &t, uint32 width, uint32 height)
	{
		t.Resize(width, height, 0);
	}
	inline void Texture_Resize3(Texture &t, uint32 width, uint32 height, uint32 level)
	{
		t.Resize(width, height, level);
	}
	inline bool Texture_LoadTexture(Texture &tex, const std::string &path)
	{
		return tex.LoadTexture(path, TextureType::Texture, true, 0);
	}
	inline bool Texture_LoadTextureFull(Texture &tex, const std::string &path, uint32 type, bool mipmap, uint32 level)
	{
		return tex.LoadTexture(path, type, mipmap, level);
	}

} // namespace embind_helpers
} // namespace p3d

#endif /* EMSCRIPTEN */
#endif /* PYROSEMBIND_HELPERS_H */
