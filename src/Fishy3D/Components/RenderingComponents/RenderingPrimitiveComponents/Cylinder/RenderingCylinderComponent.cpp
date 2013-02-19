//============================================================================
// Name        : Rendering Cylinder Component.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering Cylinder Component
//============================================================================

#include "RenderingCylinderComponent.h"

namespace Fishy3D {

    RenderingCylinderComponent::RenderingCylinderComponent() 
    {
        
    }
    RenderingCylinderComponent::RenderingCylinderComponent(const std::string& Name, const float& radius, const float& height, const int& segmentsW, const int& segmentsH, const bool& openEnded, SuperSmartPointer<IMaterial> material, bool SNormals) : RenderingPrimitiveComponent(Name, material, SNormals)
    {
        this->material = material;
        
        this->radius = radius;        
        this->height = height / 2;
        this->segmentsW = segmentsW;
        this->segmentsH = segmentsH;
        this->openEnded = openEnded;
        
        int i,j,jMin,jMax;
        
        vec3 normal;
        std::vector <vec3> aRowT, aRowB;
        
        std::vector <std::vector<vec3> > aVtc;
        
        if(!openEnded) {
            this->segmentsH += 2;
            jMin = 1;
            jMax = this->segmentsH -1;
            
            // Bottom
            vec3 oVtx = vec3(0, -this->height, 0);
            for (i=0;i<segmentsW;++i) {
                aRowB.push_back(oVtx);
            }
            aVtc.push_back(aRowB);
            
            //Top
            oVtx = vec3(0,this->height,0);          
            for (i=0;i<segmentsW;i++) {                
                aRowT.push_back(oVtx);
            }
            
        } else {
            jMin = 0;
            jMax = this->segmentsH;
        }
        
        for (j=jMin;j<=jMax;++j) {
            float z = -this->height+2*this->height*(float)(j-jMin)/(float)(jMax-jMin);
            std::vector <vec3> aRow;

            for (i=0;i<segmentsW;++i) {
                float verangle = (float)(2*(float)i/segmentsW*PI);
                float x = radius * sin(verangle);
                float y = radius * cos(verangle);
                vec3 oVtx = vec3(y,z,x);
                aRow.push_back(oVtx);
            }
            aVtc.push_back(aRow);
        }
        
        if (!openEnded)
            aVtc.push_back(aRowT);
        
        for (j=1;j<=this->segmentsH;++j) {
            for (i=0;i<segmentsW;++i) {
                vec3 a = aVtc[j][i];
                vec3 b = aVtc[j][(i-1+segmentsW)%segmentsW];
                vec3 c = aVtc[j-1][(i-1+segmentsW)%segmentsW];
                vec3 d = aVtc[j-1][i];
                          
                int i2;
                (i==0?i2=segmentsW:i2=i);
                
                float vab = (float)j/this->segmentsH;
                float vcd = (float)(j-1)/this->segmentsH;
                float uad = (float)i2/(float)segmentsW;
                float ubc = (float)(i2-1)/(float)segmentsW;
                
                vec2 aUV = vec2(uad,-vab);
                vec2 bUV = vec2(ubc,-vab);
                vec2 cUV = vec2(ubc,-vcd);
                vec2 dUV = vec2(uad,-vcd);                
                
                normal = ((a-b).cross(c-b)).normalize();
                tVertex.push_back(c);       tNormal.push_back(normal);      tTexcoord.push_back(cUV);
                tVertex.push_back(b);       tNormal.push_back(normal);      tTexcoord.push_back(bUV);
                tVertex.push_back(a);       tNormal.push_back(normal);      tTexcoord.push_back(aUV);

                IndexData.push_back(tVertex.size()-3);
                IndexData.push_back(tVertex.size()-2);
                IndexData.push_back(tVertex.size()-1);

                normal = ((a-c).cross(d-c)).normalize();
                tVertex.push_back(d);       tNormal.push_back(normal);      tTexcoord.push_back(dUV);
                tVertex.push_back(c);       tNormal.push_back(normal);      tTexcoord.push_back(cUV);
                tVertex.push_back(a);       tNormal.push_back(normal);      tTexcoord.push_back(aUV);    

                IndexData.push_back(tVertex.size()-3);
                IndexData.push_back(tVertex.size()-2);
                IndexData.push_back(tVertex.size()-1);                
                
            }            
        }
        
        if (!openEnded) {
            this->segmentsH -=2;
        }
        
        for (unsigned i=0;i<tVertex.size();i++)
        {
            tTexcoord[i].x = 1-tTexcoord[i].x;
        }  
        
        Build();        
        
    }
    
    void RenderingCylinderComponent::Start() {}
    void RenderingCylinderComponent::Update() {}
    void RenderingCylinderComponent::Shutdown() {}
    RenderingCylinderComponent::~RenderingCylinderComponent() {}

}