//============================================================================
// Name        : Rendering Primitive Component.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering Primitive Component
//============================================================================

#include "RenderingPrimitiveComponent.h"

namespace Fishy3D {

    RenderingPrimitiveComponent::RenderingPrimitiveComponent() {}    

    RenderingPrimitiveComponent::RenderingPrimitiveComponent(const std::string& Name, SuperSmartPointer<IMaterial> Material, bool SNormals) : IRenderingComponent(Name, Material, SNormals)
    {
        smoothNormals = SNormals;                
    }

    void RenderingPrimitiveComponent::Build()
    {        
        
        if (smoothNormals==true) SmoothNormals();
     
        // Calculate Bounds
        CalculateBounding();
        
        SuperSmartPointer <AttributeBuffer> Vertex (new AttributeBuffer(Buffer::Type::Attribute,Buffer::Draw::Static));
        Vertex->AddAttribute("aPosition", Buffer::Attribute::Type::Vec3,&tVertex[0],tVertex.size());
        Vertex->AddAttribute("aNormal", Buffer::Attribute::Type::Vec3,&tNormal[0],tNormal.size());
        Vertex->AddAttribute("aTexcoord", Buffer::Attribute::Type::Vec2,&tTexcoord[0],tTexcoord.size());
        AddBuffer(Vertex);                
        
        SendBuffers();
    }
    
    void RenderingPrimitiveComponent::SmoothNormals()
    {
        if (tNormal.size()>0)
        {
            std::vector<vec3> CopyNormals;
            for (unsigned i=0;i<tNormal.size();i++) {
                    CopyNormals.push_back(tNormal[i]);
            }
            for (unsigned i=0;i<tNormal.size();i++) {
                    for (unsigned j=0;j<tNormal.size();j++) {
                            if (i!=j && tVertex[i]==tVertex[j])
                                    tNormal[j]+=CopyNormals[i];            
                    }
            }
            for (unsigned i=0;i<tNormal.size();i++) {
                    tNormal[i].normalizeSelf();
            }
        }
    }

    void RenderingPrimitiveComponent::CalculateBounding()
    {
        // Bounding Box
        for (unsigned i=0;i<tVertex.size();i++)
        {
            if (i==0) {
                minBounds = tVertex[i];
                maxBounds = tVertex[i];
            } else {
                if (tVertex[i].x<minBounds.x) minBounds.x = tVertex[i].x;
                if (tVertex[i].y<minBounds.y) minBounds.y = tVertex[i].y;
                if (tVertex[i].z<minBounds.z) minBounds.z = tVertex[i].z;
                if (tVertex[i].x>maxBounds.x) maxBounds.x = tVertex[i].x;
                if (tVertex[i].y>maxBounds.y) maxBounds.y = tVertex[i].y;
                if (tVertex[i].z>maxBounds.z) maxBounds.z = tVertex[i].z;
            }            
        }
        // Bounding Sphere
        boundSphereCenter = maxBounds-minBounds;
        float a = maxBounds.distance(boundSphereCenter);
        float b = minBounds.distance(boundSphereCenter);        
        boundSphereRadius = (a>b?a:b);
        
        // Debug
        #if _DEBUG
        // Bounding Box        
        tPositionBoundingBox.push_back((vec3(minBounds.x,minBounds.y,maxBounds.z)));
        tPositionBoundingBox.push_back((vec3(maxBounds.x,minBounds.y,maxBounds.z)));
        tPositionBoundingBox.push_back((vec3(maxBounds.x,minBounds.y,minBounds.z)));
        tPositionBoundingBox.push_back((vec3(minBounds.x,minBounds.y,minBounds.z)));
        tPositionBoundingBox.push_back((vec3(minBounds.x,maxBounds.y,maxBounds.z)));
        tPositionBoundingBox.push_back((vec3(maxBounds.x,maxBounds.y,maxBounds.z)));
        tPositionBoundingBox.push_back((vec3(maxBounds.x,maxBounds.y,minBounds.z)));
        tPositionBoundingBox.push_back((vec3(minBounds.x,maxBounds.y,minBounds.z)));
        tIndexBounding.push_back(0);
        tIndexBounding.push_back(1);
        tIndexBounding.push_back(1);
        tIndexBounding.push_back(2);
        tIndexBounding.push_back(2);
        tIndexBounding.push_back(3);
        tIndexBounding.push_back(3);
        tIndexBounding.push_back(0);
        tIndexBounding.push_back(0);
        tIndexBounding.push_back(4);
        tIndexBounding.push_back(4);
        tIndexBounding.push_back(5);
        tIndexBounding.push_back(5);
        tIndexBounding.push_back(6);
        tIndexBounding.push_back(6);
        tIndexBounding.push_back(7);
        tIndexBounding.push_back(7);
        tIndexBounding.push_back(4);
        tIndexBounding.push_back(7);
        tIndexBounding.push_back(3);
        tIndexBounding.push_back(6);
        tIndexBounding.push_back(2);
        tIndexBounding.push_back(5);
        tIndexBounding.push_back(1);
        
        // Normals
        unsigned count  = 0;
        float LineHeight = 1.f;
        for (int i=0;i+2<IndexData.size();i+=3)
        {
            float centroidX, centroidY, centroidZ;                                                        
            centroidX = (tVertex[IndexData[i]].x + tVertex[IndexData[i+1]].x + tVertex[IndexData[i+2]].x)/3;
            centroidY = (tVertex[IndexData[i]].y + tVertex[IndexData[i+1]].y + tVertex[IndexData[i+2]].y)/3;
            centroidZ = (tVertex[IndexData[i]].z + tVertex[IndexData[i+1]].z + tVertex[IndexData[i+2]].z)/3;

            tPositionNormals.push_back(vec3(centroidX,centroidY,centroidZ));
            tIndexNormals.push_back(count);
            count++;
            vec3 Normals = (tNormal[IndexData[i]] + tNormal[IndexData[i+1]] + tNormal[IndexData[i+2]])/3;
            tPositionNormals.push_back(vec3(centroidX,centroidY,centroidZ)+Normals.normalize()*LineHeight);
            tIndexNormals.push_back(count);
            count++;
        }
        #endif
        
    }
        
    RenderingPrimitiveComponent::~RenderingPrimitiveComponent() {}

}
