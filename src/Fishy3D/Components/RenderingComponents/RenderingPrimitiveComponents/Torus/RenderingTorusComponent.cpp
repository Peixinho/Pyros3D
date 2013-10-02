//============================================================================
// Name        : Rendering Torus Component.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering Torus Component
//============================================================================
#include "RenderingTorusComponent.h"

namespace Fishy3D {

    RenderingTorusComponent::RenderingTorusComponent() {}
    
    RenderingTorusComponent::RenderingTorusComponent(const std::string &Name, const float &radius, const float &tube, SuperSmartPointer<IMaterial> material, const int &segmentsW, const int segmentsH, bool SNormals) : RenderingPrimitiveComponent(Name, material, SNormals)
    {

        this->material = material;

        this->radius = radius;
        this->tube = tube;
        this->segmentsW = segmentsW;
        this->segmentsH = segmentsH;

        // TODO NORMALS
        vec3 normal;

        int i, j;
        std::vector <std::vector<vec3> > aVtc;  

        for (i=0;i<segmentsW;++i) {

            std::vector<vec3> aRow;
            vec3 oVtx;

            for (j=0;j<segmentsH;++j) {
                float u = (float)((float)i/segmentsW*2*PI);
                float v = (float)((float)j/segmentsH*2*PI);
                float x = (radius + tube * cos(v)) * cos(u);
                float y = tube * sin(v);
                float z = (radius + tube * cos(v)) * sin(u);

                oVtx = vec3(x,y,z);
                aRow.push_back(oVtx);

            }
            aVtc.push_back(aRow);
        }
        for (i=0;i<segmentsW;++i) {
            for (j=0;j<segmentsH;++j) {
                int ip = (i+1)%segmentsW;
                int jp = (j+1)%segmentsH;
                vec3 a = aVtc[i ][j];
                vec3 b = aVtc[ip][j];
                vec3 c = aVtc[i ][jp];
                vec3 d = aVtc[ip][jp];

                vec2 aUV = vec2((float)i/segmentsW,         -(float)j/segmentsH);
                vec2 bUV = vec2((float)(i+1)/segmentsW,     -(float)j/segmentsH);
                vec2 cUV = vec2((float)i/segmentsW,         -(float)(j+1)/segmentsH);
                vec2 dUV = vec2((float)(i+1)/segmentsW,     -(float)(j+1)/segmentsH);

                normal = ((a-b).cross(c-b)).normalize();
                tVertex.push_back(c);       tNormal.push_back(normal);      tTexcoord.push_back(cUV);
                tVertex.push_back(b);       tNormal.push_back(normal);      tTexcoord.push_back(bUV);
                tVertex.push_back(a);       tNormal.push_back(normal);      tTexcoord.push_back(aUV);

                IndexData.push_back(tVertex.size()-3);
                IndexData.push_back(tVertex.size()-2);
                IndexData.push_back(tVertex.size()-1);

                normal = ((d-c).cross(b-c)).normalize();
                tVertex.push_back(b);       tNormal.push_back(normal);      tTexcoord.push_back(bUV);
                tVertex.push_back(c);       tNormal.push_back(normal);      tTexcoord.push_back(cUV);
                tVertex.push_back(d);       tNormal.push_back(normal);      tTexcoord.push_back(dUV);    

                IndexData.push_back(tVertex.size()-3);
                IndexData.push_back(tVertex.size()-2);
                IndexData.push_back(tVertex.size()-1);
            }
        }        

        for (unsigned i=0;i<tVertex.size();i++)
        {
            tTexcoord[i].x = 1-tTexcoord[i].x;
        }
        
        Build();    

    }
    void RenderingTorusComponent::Start() {}
    void RenderingTorusComponent::Update() {}
    void RenderingTorusComponent::Shutdown() {}  
    RenderingTorusComponent::~RenderingTorusComponent() {}

}