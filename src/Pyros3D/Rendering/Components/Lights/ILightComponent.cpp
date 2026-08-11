//============================================================================
// Name        : ILightComponent
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Component For Lights
//============================================================================

#include <Pyros3D/Rendering/Components/Lights/ILightComponent.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>

namespace p3d {

	// Initialize Rendering Components vector
	std::vector<IComponent*> ILightComponent::Components;

	ILightComponent::ILightComponent(const uint32 type) : IComponent()
	{
		LightType = type;
		isCastingShadows = false;
		ShadowBiasFactor = ShadowBiasUnits = 0.f;
		// Off by default - see SetVolumetricScattering().
		volumetricDensity = 0.f;
		volumetricAnisotropy = 0.6f;
		volumetricSteps = 32.f;
		Intensity = 1.f;
		pcfTexel = 0.0001f;
	}

	ILightComponent::~ILightComponent() = default;

	void ILightComponent::Register(SceneGraph* Scene)
	{
		if (!Registered)
		{
			// Add Self to Components vector
			Components.push_back(this);

			// Set Flag
			Registered = true;

			// Add To Scene
			Scene->GetLights().push_back(this);
		}
	}
	void ILightComponent::Unregister(SceneGraph* Scene)
	{
		for (std::vector<IComponent*>::iterator i = Components.begin(); i != Components.end(); i++)
		{
			if ((*i) == this)
			{
				Components.erase(i);
				for (std::vector<IComponent*>::iterator k = Scene->GetLights().begin(); k != Scene->GetLights().end(); k++)
				{
					if ((*k) == this)
					{
						Scene->GetLights().erase(k);
						break;
					}
				}
				break;
			}
		}

		// Unset Flag
		Registered = false;
	}

	std::vector<IComponent*> &ILightComponent::GetComponents()
	{
		return Components;
	}

	std::vector<IComponent*> &ILightComponent::GetLightsOnScene(SceneGraph* Scene)
	{
		return Scene->GetLights();
	}

	const Vec4 &ILightComponent::GetLightColor() const
	{
		return Color;
	}

	void ILightComponent::ReleaseShadowResources()
	{
		// See the header. Nothing is bound at this point, but the GPU may
		// still be reading these from an earlier submission.
		if (IsActiveRenderDeviceSet())
			GetActiveRenderDevice().WaitIdle();
		shadowsFBO.reset();
		ShadowMap.reset();
	}

	void ILightComponent::DisableCastShadows()
	{
		isCastingShadows = false;
		ReleaseShadowResources();
	}

	FrameBuffer* ILightComponent::GetShadowFBO()
	{
		if (isCastingShadows)
		{
			return shadowsFBO.get();
		}
		else echo("ERROR: Frame Buffer Is Not Created");
		return NULL;
	}
};