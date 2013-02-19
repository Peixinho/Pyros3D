//============================================================================
// Name        : DirectionalLightComponent.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : DirectionalLightComponent
//============================================================================

#include "DirectionalLightComponent.h"

namespace Fishy3D {

    DirectionalLightComponent::DirectionalLightComponent() {}
        
    DirectionalLightComponent::DirectionalLightComponent(const std::string& name, const vec4& color) : ILightComponent(name, color), useCascadeShadows(false) {}

    DirectionalLightComponent::~DirectionalLightComponent() {}
        
    void DirectionalLightComponent::Destroy() 
    {
		if (useCascadeShadows)
        for (unsigned i=0;i<splits;i++)
        {
			Cascade->f[i].fbo.Dispose();
        }
    }

    void DirectionalLightComponent::EnableCascadeShadows(const unsigned &splits, const Projection &projection, const float &nearDistance, const float &farDistance, const unsigned &mapResolutionWidth, const unsigned &mapResolutionHeight)
    {
        this->splits = splits;
        this->nearDistance = nearDistance;
        this->farDistance = farDistance;
		this->useCascadeShadows = true;
		this->shadowMapWidth = mapResolutionWidth;
		this->shadowMapHeight = mapResolutionHeight;

        Cascade = SuperSmartPointer<CascadeShadowMapping> (new CascadeShadowMapping());
        Cascade->Init(projection.Fov,projection.Aspect);
        Cascade->UpdateSplitDist(splits,nearDistance,farDistance);
        for (unsigned i=0;i<splits;i++)
        {
            Cascade->f[i].fbo=SuperSmartPointer<FrameBuffer> (new FrameBuffer());
			Cascade->f[i].fbo->Init(mapResolutionWidth,mapResolutionHeight,FrameBufferTypes::Depth,FrameBufferAttachmentFormat::Color_Attachment_Floating_Point_16F,false);
            Cascade->f[i].fbo->GetTexture(0).SetRepeat(TextureRepeat::Clamp,TextureRepeat::Clamp);
        }
    }
    void DirectionalLightComponent::UpdateCascadeShadowsSplits(const unsigned &splits)
    {
        unsigned oldNumberSplits = Cascade->GetNumberOfSplits();
        // Update Cascade Splits
        Cascade->UpdateSplitDist(splits,nearDistance,farDistance);

        // Destroy FrameBuffers not on Use anymore
        if (oldNumberSplits>splits)
        {
            for (unsigned i=splits;i<oldNumberSplits;i++)
            {
                Cascade->f[i].fbo.Dispose();
            }
        } else if (oldNumberSplits<splits)
        {
            for (unsigned i=oldNumberSplits-1;i<splits;i++)
            {
				Cascade->f[i].fbo=SuperSmartPointer<FrameBuffer> (new FrameBuffer());
				Cascade->f[i].fbo->Init(shadowMapWidth,shadowMapHeight,FrameBufferTypes::Depth,FrameBufferAttachmentFormat::Color_Attachment_Floating_Point_16F,false);
				Cascade->f[i].fbo->GetTexture(0).SetRepeat(TextureRepeat::Clamp,TextureRepeat::Clamp);
            }
        }
        this->splits = splits;
    }
	void DirectionalLightComponent::DisableCascadeShadows()
	{
		useCascadeShadows = false;
		for (unsigned i=0;i<splits;i++)
        {
			Cascade->f[i].fbo.Dispose();
        }
		splits = 0;
	}
	void DirectionalLightComponent::UpdateCascadeShadowsResolution(const unsigned &mapResolutionWidth, const unsigned &mapResolutionHeight)
	{
		this->shadowMapWidth = mapResolutionWidth;
		this->shadowMapHeight = mapResolutionHeight;

		for (unsigned i=0;i<splits;i++)
        {
			Cascade->f[i].fbo->Resize(mapResolutionWidth, mapResolutionHeight);
        }
	}
    void DirectionalLightComponent::UpdateCascadeShadowsProjection(const Projection &projection)
    {
        Cascade->Init(projection.Fov, projection.Aspect);
    }
    void DirectionalLightComponent::UpdateCascadeShadowsDistance(const float &nearDistance, const float &farDistance)
    {
        this->nearDistance = nearDistance;
        this->farDistance = farDistance;
    }
    void DirectionalLightComponent::UpdateCascadeShadowMapping()
    {
        Cascade->UpdateSplitDist(splits,nearDistance,farDistance);
    }
    CascadeShadowMapping* DirectionalLightComponent::GetCascadeShadowMapping()
    {
        return Cascade.Get();
    }
	bool DirectionalLightComponent::IsUsingCascadeShadows()
	{
		return useCascadeShadows;
	}
}