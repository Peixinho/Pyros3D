//============================================================================
// Name        : Rendering Cube Component.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering Cube Component
//============================================================================


#include "RenderingCubeComponent.h"

namespace Fishy3D {

    RenderingCubeComponent::RenderingCubeComponent() 
    {
    }    

    RenderingCubeComponent::RenderingCubeComponent(const std::string &Name, const float &width, const float &height, const float &depth, SuperSmartPointer<IMaterial> material, const bool SNormals) : RenderingPrimitiveComponent(Name, material, SNormals)
    {
        
        this->material = material;
        
        float w2 = width  / 2;
        float h2 = height / 2;
        float d2 = depth  / 2;

        // [a, b, c, d] => [a, b, c, c, d, a]
        
        // Front Face
        vec3 a = vec3(-w2,-h2,d2); vec3 b = vec3(w2,-h2,d2); vec3 c = vec3(w2,h2,d2); vec3 d = vec3(-w2,h2,d2);
        vec3 normal = ((c-b).cross(a-b)).normalize();
        tVertex.push_back(a);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(0,0));
        tVertex.push_back(b);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(1,0));
        tVertex.push_back(c);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(1,1));                
        
        tVertex.push_back(c);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(1,1));
        tVertex.push_back(d);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(0,1));
        tVertex.push_back(a);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(0,0)); 

        // Right Face
        a = vec3(w2,-h2,-d2); b = vec3(w2,h2,-d2); c = vec3(w2,h2,d2); d = vec3(w2,-h2,d2);
        normal = ((c-b).cross(a-b)).normalize();        
        tVertex.push_back(a);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(1,0));
        tVertex.push_back(b);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(1,1));
        tVertex.push_back(c);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(0,1));                
        
        tVertex.push_back(c);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(0,1));
        tVertex.push_back(d);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(0,0));
        tVertex.push_back(a);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(1,0));               
        
        
        // Top Face
        a = vec3(-w2,h2,-d2); b = vec3(-w2,h2,d2); c = vec3(w2,h2,d2); d = vec3(w2,h2,-d2);
        normal = ((c-b).cross(a-b)).normalize();
        tVertex.push_back(a);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(0,1));
        tVertex.push_back(b);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(0,0));
        tVertex.push_back(c);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(1,0));                
        
        tVertex.push_back(c);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(1,0));
        tVertex.push_back(d);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(1,1));
        tVertex.push_back(a);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(0,1));        
        
        // Left Face
        a = vec3(-w2,-h2,-d2); b = vec3(-w2,-h2,d2); c = vec3(-w2,h2,d2); d = vec3(-w2,h2,-d2);
        normal = ((c-b).cross(a-b)).normalize();
        tVertex.push_back(a);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(0,0));
        tVertex.push_back(b);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(1,0));
        tVertex.push_back(c);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(1,1));                
        
        tVertex.push_back(c);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(1,1));
        tVertex.push_back(d);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(0,1));
        tVertex.push_back(a);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(0,0));           

        // Bottom Face
        a = vec3(-w2,-h2,-d2); b = vec3(w2, -h2, -d2); c = vec3(w2,-h2,d2); d = vec3(-w2,-h2,d2);
        normal = ((c-b).cross(a-b)).normalize();
        tVertex.push_back(a);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(1,1));
        tVertex.push_back(b);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(0,1));
        tVertex.push_back(c);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(0,0));                
        
        tVertex.push_back(c);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(0,0));
        tVertex.push_back(d);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(1,0));
        tVertex.push_back(a);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(1,1));                  

        // Back Face
        a = vec3(-w2,-h2,-d2); b = vec3(-w2,h2,-d2); c = vec3(w2,h2,-d2); d = vec3(w2,-h2,-d2);
        normal = ((c-b).cross(a-b)).normalize();
        tVertex.push_back(a);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(1,0));
        tVertex.push_back(b);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(1,1));
        tVertex.push_back(c);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(0,1));                
        
        tVertex.push_back(c);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(0,1));
        tVertex.push_back(d);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(0,0));
        tVertex.push_back(a);   tNormal.push_back(normal);      tTexcoord.push_back(vec2(1,0));
        
        for (unsigned i=0;i<tVertex.size();i++)
        {
            IndexData.push_back(i);
            tTexcoord[i].y = 1-tTexcoord[i].y;
        }                
        
        Build();        
        
    }

    void RenderingCubeComponent::Start() {}
    void RenderingCubeComponent::Update() {}
    void RenderingCubeComponent::Shutdown() {}  

    RenderingCubeComponent::~RenderingCubeComponent() 
    {
        
    }

}
