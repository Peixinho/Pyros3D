//============================================================================
// Name        : Rendering Plane Component.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering Plane Component
//============================================================================

#include <vector>

#include "RenderingPlaneComponent.h"

namespace Fishy3D {

RenderingPlaneComponent::RenderingPlaneComponent() {}

RenderingPlaneComponent::RenderingPlaneComponent(const std::string &Name, const float& width, const float& height, SuperSmartPointer<IMaterial> material, bool SNormals) : RenderingPrimitiveComponent(Name, material, SNormals)
{
    this->material = material;
    
    this->width = width;
    this->height = height;
    
    float w2 = width/2; float h2 = height/2;       
        
    vec3 a = vec3(-w2,-h2,0); vec3 b = vec3(w2,-h2,0); vec3 c = vec3(w2,h2,0); vec3 d = vec3(-w2,h2,0);
    vec3 normal = ((c-b).cross(a-b)).normalize();    
    tVertex.push_back(a);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(0,0));
    tVertex.push_back(b);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(1,0));
    tVertex.push_back(c);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(1,1));                
    tVertex.push_back(d);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(0,1));    
    
    IndexData.push_back(0);
    IndexData.push_back(1);
    IndexData.push_back(2);
    IndexData.push_back(2);
    IndexData.push_back(3);
    IndexData.push_back(0);
    
    for (int i = 0;i < tTexcoord.size(); i++) tTexcoord[i].y = 1-tTexcoord[i].y;
    
    Build();
    
}

void RenderingPlaneComponent::Start() {}
void RenderingPlaneComponent::Update() {}
void RenderingPlaneComponent::Shutdown() {}

RenderingPlaneComponent::~RenderingPlaneComponent() {}

}

