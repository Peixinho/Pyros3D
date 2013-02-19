//============================================================================
// Name        : Rendering Capsule Component.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering Capsule Component
//============================================================================

#include "RenderingCapsuleComponent.h"

namespace Fishy3D {

RenderingCapsuleComponent::RenderingCapsuleComponent() {}
RenderingCapsuleComponent::RenderingCapsuleComponent(const std::string &Name, const float &mRadius, const float &mHeight, const unsigned &mNumRings, const unsigned &mNumSegments, const unsigned &mNumSegHeight, SuperSmartPointer<IMaterial> material, bool SNormals) : RenderingPrimitiveComponent(Name, material, SNormals)
{
    this->material = material;
    
    this->radius = radius;
    this->rings = mNumRings;
    this->segmentsW = mNumSegments;
    this->segmentsH = mNumSegHeight;
    
    float fDeltaRingAngle = (float)(PI/2/mNumRings);
    float fDeltaSegAngle = (float)(PI*2/mNumSegments);
    
    
    // TOP
    float sphereRatio = mRadius / (2*mRadius+mHeight);
    float cylinderRatio = mHeight / (2*mRadius+mHeight);
    int offset=0;
    
    for (unsigned int ring = 0;ring<=mNumRings; ring++) 
    {
        float r0 = mRadius * sin(ring*fDeltaRingAngle);
        float y0 = mRadius * cos(ring*fDeltaRingAngle);
        
        for (unsigned int seg = 0;seg<=mNumSegments; seg++)
        {
            float x0 = r0 * cos(seg * fDeltaSegAngle);
            float z0 = r0 * sin(seg * fDeltaSegAngle);
            
//            vertexData.push_back(BasicGeometry::VertexData(vec3(x0,0.5f*mHeight+y0,z0),vec3(x0,y0,z0),vec2((float)seg/(float)mNumSegments, (float)ring/(float)mNumRings*sphereRatio)));
            tVertex.push_back(vec3(x0,0.5f*mHeight+y0,z0));
            tNormal.push_back(vec3(x0,y0,z0));
            tTexcoord.push_back(vec2((float)seg/(float)mNumSegments, (float)ring/(float)mNumRings*sphereRatio));
            
            IndexData.push_back(offset+mNumSegments+1);
            IndexData.push_back(offset+mNumSegments);
            IndexData.push_back(offset);
            IndexData.push_back(offset+mNumSegments+1);
            IndexData.push_back(offset);
            IndexData.push_back(offset+1); 
                
            offset++;
        }
    }
    
    // Cylinder
    
    float deltaAngle = (float)(PI*2 / mNumSegments);
    float deltamHeight = mHeight/(float)mNumSegHeight;
    
    for (unsigned short i = 1;i<mNumSegHeight; i++)
        for (unsigned  short j=0;j<=mNumSegments; j++) 
        {
            float x0 = mRadius * cos(j*deltaAngle);
            float z0 = mRadius * sin(j*deltaAngle);
//            vertexData.push_back(BasicGeometry::VertexData(vec3(x0,0.5f*mHeight-i*deltamHeight,z0),vec3(x0,0,z0),vec2((float)j/(float)mNumSegments, (float)i/(float)mNumSegHeight*cylinderRatio+sphereRatio)));
            tVertex.push_back(vec3(x0,0.5f*mHeight-i*deltamHeight,z0));         
            tNormal.push_back(vec3(x0,0,z0));
            tTexcoord.push_back(vec2((float)j/(float)mNumSegments, (float)i/(float)mNumSegHeight*cylinderRatio+sphereRatio));
            
            IndexData.push_back(offset+mNumSegments+1);
            IndexData.push_back(offset+mNumSegments);
            IndexData.push_back(offset);
            IndexData.push_back(offset+mNumSegments+1);
            IndexData.push_back(offset);
            IndexData.push_back(offset+1);
            
            offset++;
        }
    
    // Bottom
    for (unsigned int ring = 0;ring <= mNumRings; ring++) 
    {
        float r0 = (float)(mRadius * sin(PI/2 + ring*fDeltaRingAngle));
        float y0 = (float)(mRadius * cos(PI/2 + ring*fDeltaRingAngle));
        
        for (unsigned int seg = 0;seg<=mNumSegments; seg++)
        {
            float x0 = r0 * cos(seg * fDeltaSegAngle);
            float z0 = r0 * sin(seg * fDeltaSegAngle);            
            //vertexData.push_back(BasicGeometry::VertexData(vec3(x0,-0.5f*mHeight+y0,z0),vec3(x0,y0,z0),vec2((float)seg/(float)mNumSegments, (float)ring/(float)mNumRings*sphereRatio+cylinderRatio+sphereRatio)));
            tVertex.push_back(vec3(x0,-0.5f*mHeight+y0,z0));
            tNormal.push_back(vec3(x0,y0,z0));
            tTexcoord.push_back(vec2((float)seg/(float)mNumSegments, (float)ring/(float)mNumRings*sphereRatio+cylinderRatio+sphereRatio));
           
            if (ring != mNumRings)
            {            
                IndexData.push_back(offset+mNumSegments+1);
                IndexData.push_back(offset+mNumSegments);
                IndexData.push_back(offset);
                IndexData.push_back(offset+mNumSegments+1);
                IndexData.push_back(offset);
                IndexData.push_back(offset+1);
            }
            
            offset++;
        }
    }
    
    for (unsigned i=0;i<tVertex.size();i++)
    {
        tTexcoord[i].x = 1-tTexcoord[i].x;
    }      
    
    Build();
    
}
void RenderingCapsuleComponent::Start() {}
void RenderingCapsuleComponent::Update() {}
void RenderingCapsuleComponent::Shutdown() {}
RenderingCapsuleComponent::~RenderingCapsuleComponent() {}

}
