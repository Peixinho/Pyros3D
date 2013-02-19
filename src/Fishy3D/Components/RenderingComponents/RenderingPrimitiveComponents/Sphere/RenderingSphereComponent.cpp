//============================================================================
// Name        : Rendering Sphere Component.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering Sphere Component
//============================================================================

#include "RenderingSphereComponent.h"

namespace Fishy3D {

    RenderingSphereComponent::RenderingSphereComponent() 
    {
        
    }
    
    RenderingSphereComponent::RenderingSphereComponent(const std::string& Name, const float& radius, SuperSmartPointer<IMaterial> material, const int& segmentsW, const int segmentsH, bool HalfSphere,bool SNormals) : RenderingPrimitiveComponent(Name, material ,SNormals)
    {   
        this->material = material;
        
        this->radius = radius;
        this->segmentsW = segmentsW;
        this->segmentsH = segmentsH;
        
        int i,j;
        float iHor=(float)segmentsW;
        float iVer=(float)segmentsH;
        
        std::vector <std::vector<vec3> > aVtc;    
        
        for (j=0;j<(iVer+1);++j) {
                float fRad1 = j / iVer;        
                float fZ = (float)(-radius * cos(fRad1 * PI));
                float fRds = (float)(radius * sin(fRad1 * PI));
	
                std::vector<vec3> aRow;
                vec3 oVtx;
                
                for (i=0;i< iHor;++i) {            
                        float fRad2 = (2 * i / iHor);
                        float fX = (float)(fRds * sin(fRad2 * PI));            
                        float fY = (float)(fRds * cos(fRad2 * PI));
                        if (!((j == 0 || j == iVer) && i > 0)) {
                                oVtx=vec3(fY,fZ,fX);
                        }
                        aRow.push_back(oVtx);
                }
                aVtc.push_back(aRow);
        }

        int iVerNum = aVtc.size();

        for (j=0;j<iVerNum;++j) {
            int iHorNum = aVtc[j].size();
            if (j > 0) {
                for (i=0;i<iHorNum;++i) {
                    bool bEnd = i == (iHorNum - 1);                
                    vec3 aP1 = aVtc[j][bEnd?0:(i + 1)];
                    vec3 aP2 = aVtc[j][(bEnd?(iHorNum-1):i)];
                    vec3 aP3 = aVtc[j - 1][(bEnd?(iHorNum - 1):i)];
                    vec3 aP4 = aVtc[j - 1][bEnd?0:(i + 1)];

                    float iVerNumf=(float)iVerNum;float iHorNumf = (float)iHorNum;                

                    float fJ0 = j/(iVerNumf-1.f);
                    float fJ1 = (j-1.f) / (iVerNumf-1.f);
                    float fI0 = (i+1.f) / iHorNumf;
                    float fI1 = i / iHorNumf;

                    vec2 aP1uv = vec2(fI0,fJ0);
                    vec2 aP2uv = vec2(fI1,fJ0);
                    vec2 aP3uv = vec2(fI1,fJ1);
                    vec2 aP4uv = vec2(fI0,fJ1);

                    if (j < (aVtc.size() - 1)) {                    
                        vec3 normal = ((aP1-aP2).cross(aP3-aP2)).normalize();     
                        
                        tVertex.push_back(aP3); tNormal.push_back(normal);      tTexcoord.push_back(aP3uv);
                        tVertex.push_back(aP2); tNormal.push_back(normal);      tTexcoord.push_back(aP2uv);
                        tVertex.push_back(aP1); tNormal.push_back(normal);      tTexcoord.push_back(aP1uv);
                        
                        IndexData.push_back(tVertex.size()-3);
                        IndexData.push_back(tVertex.size()-2);
                        IndexData.push_back(tVertex.size()-1);
                        
                    }
                    if (j > 1) {
                        vec3 normal = ((aP1-aP3).cross(aP4-aP3)).normalize();  
                        
                        tVertex.push_back(aP4); tNormal.push_back(normal);      tTexcoord.push_back(aP4uv);
                        tVertex.push_back(aP3); tNormal.push_back(normal);      tTexcoord.push_back(aP3uv);
                        tVertex.push_back(aP1); tNormal.push_back(normal);      tTexcoord.push_back(aP1uv);                        
                        
                        IndexData.push_back(tVertex.size()-3);
                        IndexData.push_back(tVertex.size()-2);
                        IndexData.push_back(tVertex.size()-1);
                        
                    }
                }
            }
        }                
        
        for (int i = 0;i < tTexcoord.size(); i++) {
            tTexcoord[i].x = 1-tTexcoord[i].x;
            tTexcoord[i].y = 1-tTexcoord[i].y;
        }
        
        if (HalfSphere==true)
        {
            IndexData.resize(IndexData.size()/2);
            tVertex.resize(tVertex.size()/2);
            tNormal.resize(tNormal.size()/2);
            tTexcoord.resize(tTexcoord.size()/2);
        }
        Build();        
        
    }

    RenderingSphereComponent::~RenderingSphereComponent() 
    {
        
    }
    
    void RenderingSphereComponent::Start() {}
    void RenderingSphereComponent::Update() {}
    void RenderingSphereComponent::Shutdown() {}   

}