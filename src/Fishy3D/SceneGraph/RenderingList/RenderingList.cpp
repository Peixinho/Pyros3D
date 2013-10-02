//============================================================================
// Name        : RenderingList.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering List
//============================================================================

#include "RenderingList.h"
#include "../../Components/LightComponents/DirectionalLight/DirectionalLightComponent.h"
#include "../../Components/LightComponents/PointLight/PointLightComponent.h"
#include "../../Components/LightComponents/SpotLight/SpotLightComponent.h"

namespace Fishy3D {

    RenderingList::RenderingList() {}

    RenderingList::~RenderingList() {}

    // compare Z values to sort
    bool RenderingList::CompareZ(IRenderingComponent* a, IRenderingComponent* b)
    {
        vec3 CameraPos = CameraPosition;

        if (CameraPos.distance(a->GetOwner()->GetWorldPosition()) > CameraPos.distance(b->GetOwner()->GetWorldPosition())) return true;
        else if (CameraPos.distance(a->GetOwner()->GetWorldPosition()) < CameraPos.distance(b->GetOwner()->GetWorldPosition())) return false;
        else return false;

    }
    void RenderingList::Sort(const vec3 &CameraPos)
    {

        // saving position
        CameraPosition = CameraPos;
        
        // Sort Opaque From Translucid
        SimpleSort();        
        
        // sort transparency by z far
        if (_RenderingTranslucidList.size()>0)
        {
            _RenderingTranslucidList.sort(RenderingList::CompareZ);
        }
        
    }

    void RenderingList::SimpleSort()
    {

        if (_dirtyRenderingList == true)
        {
            // temporary lists
            std::list<IRenderingComponent*> NormalObjects;
            std::list<IRenderingComponent*> TransparentObjects;

            for (std::list<IRenderingComponent*>::iterator i=_RenderingList.begin();i!=_RenderingList.end();i++)
            {
                // Just check opacity if Material exists
                if ((*i)->GetMaterial()!=NULL) 
                {
                    if ((*i)->GetMaterial()->IsTransparent()==true)
                        TransparentObjects.push_back(*i);
                    else
                        NormalObjects.push_back(*i);
                }
            }

            // clear RenderingOpaqueList and RenderingTranslucidList
            _RenderingOpaqueList.clear();
            _RenderingTranslucidList.clear();

            // copy to RenderingList
            for (std::list<IRenderingComponent*>::iterator i=NormalObjects.begin();i!=NormalObjects.end();i++)
                _RenderingOpaqueList.push_back(*i);
            for (std::list<IRenderingComponent*>::iterator i=TransparentObjects.begin();i!=TransparentObjects.end();i++)
                _RenderingTranslucidList.push_back(*i);

            // clear arrays
            NormalObjects.clear();
            TransparentObjects.clear();

            // Set dirty false
            _dirtyRenderingList = false;
        }
    }
    
    void RenderingList::AddRenderingComponent(IRenderingComponent* comp)
    {        
        _RenderingList.push_back(comp);
        _dirtyRenderingList = true;
    }
    void RenderingList::RemoveRenderingComponent(IRenderingComponent* comp)
    {        
        for (std::list<IRenderingComponent*>::iterator i = _RenderingList.begin();i!=_RenderingList.end();i++)
        {
            if ((*i)==comp) 
            {
                _RenderingList.erase(i);
                _dirtyRenderingList = true;
                break;
            }
        }
    }    
    void RenderingList::AddLightComponent(const std::string &lightID, ILightComponent* light)
    {
        StringID ID(MakeStringID(lightID));
        _LightsID[ID]=light;
    }
    void RenderingList::AddLightComponent(const StringID &lightID, ILightComponent* light)
    {        
        _LightsID[lightID]=light;
    }    
    void RenderingList::RemoveLightComponent(const std::string &lightID)
    {
        StringID ID(MakeStringID(lightID));
        _LightsID.erase(ID);
    }
    void RenderingList::RemoveLightComponent(const StringID &lightID)
    {        
        _LightsID.erase(lightID);
    }       
    bool RenderingList::FindLight(const std::string &lightID) 
    {
        StringID ID(MakeStringID(lightID));
        return (_LightsID.find(ID)==_LightsID.end());
    }
    ILightComponent* RenderingList::GetLight(const std::string &lightName)
    {
        StringID ID(MakeStringID(lightName));
        return _LightsID.find(ID)->second;
    }
    ILightComponent* RenderingList::GetLight(const unsigned &lightID)
    {        
        return _LightsID.find(lightID)->second;
    }    
    void RenderingList::ClearRenderingList()
    {        
        _RenderingList.clear();
    }
    void RenderingList::ClearLightsList()
    {
        _LightsID.clear();
    }    
    const std::list<IRenderingComponent*> &RenderingList::GetRenderingOpaqueList() const
    {
        return _RenderingOpaqueList;
    }
    const std::list<IRenderingComponent*> &RenderingList::GetRenderingTranslucidList() const
    {
        return _RenderingTranslucidList;
    }
    const std::vector<Texture> &RenderingList::GetShadowMaps() const
    {
        return _ShadowMaps;
    }
    const std::vector<Matrix> &RenderingList::GetShadowMatrixBias() const
    {
        return _ShadowMatrix;
    }
    unsigned RenderingList::GetNumberOfShadows()
    {
        return _ShadowMaps.size();
    }
	const vec4 &RenderingList::GetShadowFar() const
	{
		return _ShadowFar;
	}
    void RenderingList::ComputeLights()
    {
        // Clear Lights List
        _Lights.clear();
        
        if (_LightsID.size()>0) 
        {
            for (std::map<StringID, ILightComponent*>::iterator i = _LightsID.begin();i!=_LightsID.end();i++)
            {
                if (DirectionalLightComponent *d = dynamic_cast<DirectionalLightComponent*>((*i).second)) {
                    
                    // Directional Lights
                    vec4 color = d->GetColor();
                    vec3 position;                    
                    vec3 direction = (d->GetOwner()->GetWorldPosition().normalize());
                    float attenuation = 1.f;
                    vec2 cones;
                    int type = 1;
                    
                    Matrix directionalLight = Matrix();
                    directionalLight.m[0] = color.x;         directionalLight.m[1] = color.y;             directionalLight.m[2] = color.z;             directionalLight.m[3] = color.w;
                    directionalLight.m[4] = position.x;      directionalLight.m[5] = position.y;          directionalLight.m[6] = position.z; 
                    directionalLight.m[7] = direction.x;     directionalLight.m[8] = direction.y;         directionalLight.m[9] = direction.z;
                    directionalLight.m[10] = attenuation;   //directionalLight.m[11] = attenuation.y;       directionalLight.m[12] = attenuation.z;
                    directionalLight.m[13] = cones.x;         directionalLight.m[14] = cones.y;
                    directionalLight.m[15] = type;
                    
                    _Lights.push_back(directionalLight);
                    
                } else if (PointLightComponent *p = dynamic_cast<PointLightComponent*>((*i).second)) {
                    
                    // Point Lights
                    vec4 color = p->GetColor();
                    vec3 position = (p->GetOwner()->GetWorldPosition());
                    vec3 direction;
                    float attenuation = p->GetRadius();
                    vec2 cones;
                    int type = 2;
                    
                    Matrix pointLight = Matrix();
                    pointLight.m[0] = color.x;       pointLight.m[1] = color.y;           pointLight.m[2] = color.z;           pointLight.m[3] = color.w;
                    pointLight.m[4] = position.x;    pointLight.m[5] = position.y;        pointLight.m[6] = position.z; 
                    pointLight.m[7] = direction.x;   pointLight.m[8] = direction.y;       pointLight.m[9] = direction.z;
                    pointLight.m[10] = attenuation; //pointLight.m[11] = attenuation.y;     pointLight.m[12] = attenuation.z;
                    pointLight.m[13] = cones.x;       pointLight.m[14] = cones.y;
                    pointLight.m[15] = type;
                    
                    _Lights.push_back(pointLight);
                    
                } else if (SpotLightComponent *s = dynamic_cast<SpotLightComponent*>((*i).second)) {
                    
                    // Spot Lights
                    vec4 color = s->GetColor();
                    vec3 position = (s->GetOwner()->GetWorldPosition());                   
                    vec3 direction = s->GetDirection();
                    float attenuation = s->GetRadius();
                    vec2 cones = vec2(s->GetCosInnerCone(),s->GetCosOutterCone());
                    int type = 3;
                    
                    Matrix spotLight = Matrix();
                    spotLight.m[0] = color.x;        spotLight.m[1] = color.y;            spotLight.m[2] = color.z;            spotLight.m[3] = color.w;
                    spotLight.m[4] = position.x;     spotLight.m[5] = position.y;         spotLight.m[6] = position.z; 
                    spotLight.m[7] = direction.x;    spotLight.m[8] = direction.y;        spotLight.m[9] = direction.z;
                    spotLight.m[10] = attenuation;  //spotLight.m[11] = attenuation.y;      spotLight.m[12] = attenuation.z;
                    spotLight.m[13] = cones.x;        spotLight.m[14] = cones.y;
                    spotLight.m[15] = type;
                    
                    _Lights.push_back(spotLight);
                    
                }
            }
        }
    }
	void RenderingList::ComputeShadows()
	{
        if (_LightsID.size()>0) 
        {		
			_ShadowMaps.clear();
			_ShadowMatrix.clear();
            for (std::map<StringID, ILightComponent*>::iterator i = _LightsID.begin();i!=_LightsID.end();i++)
            {
				if (DirectionalLightComponent *d = dynamic_cast<DirectionalLightComponent*>((*i).second)) {
					// CASCADE
					if (d->IsUsingCascadeShadows())
					{
						// Get Lights Shadow Map Texture
						for (unsigned i=0;i<d->GetCascadeShadowMapping()->GetNumberOfSplits();i++)
						{
							_ShadowMaps.push_back(d->GetCascadeShadowMapping()->f[i].fbo->GetTexture(0));
							_ShadowMatrix.push_back((Matrix::BIAS * (d->GetCascadeShadowMapping()->f[i].GetCropMatrix() * d->GetCascadeShadowMapping()->GetViewMatrix())));
						}
						if (d->GetCascadeShadowMapping()->GetNumberOfSplits()>0) _ShadowFar.x = d->GetCascadeShadowMapping()->f[0].Far;
						if (d->GetCascadeShadowMapping()->GetNumberOfSplits()>1) _ShadowFar.y = d->GetCascadeShadowMapping()->f[1].Far;
						if (d->GetCascadeShadowMapping()->GetNumberOfSplits()>2) _ShadowFar.z = d->GetCascadeShadowMapping()->f[2].Far;
						if (d->GetCascadeShadowMapping()->GetNumberOfSplits()>3) _ShadowFar.w = d->GetCascadeShadowMapping()->f[3].Far;
					}
				}
			}
		}
	}
    const std::map<StringID, ILightComponent*> &RenderingList::GetLights() const
    {
        return _LightsID;
    }
    std::vector<Matrix> RenderingList::GetLightsPackaging()
    {
        return _Lights;
    }
    unsigned RenderingList::GetNumberOfLights()
    {
        return _Lights.size();
    }
    // static Camera Position
    vec3 RenderingList::CameraPosition;
    
}