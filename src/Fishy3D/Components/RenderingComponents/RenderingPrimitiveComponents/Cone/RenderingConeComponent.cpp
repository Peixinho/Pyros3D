//============================================================================
// Name        : Rendering Cone Component.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering Cone Component
//============================================================================

#include "RenderingConeComponent.h"

namespace Fishy3D {

    RenderingConeComponent::RenderingConeComponent() {}
    RenderingConeComponent::RenderingConeComponent(const std::string &Name, const float &radius, const float &height, const int &segmentsW, const int segmentsH, const bool &openEnded, SuperSmartPointer<IMaterial> material, bool SNormals) : RenderingPrimitiveComponent(Name, material, SNormals)
    {    
        this->material = material;

        this->radius = radius;
        this->height = height;
        this->segmentsW = segmentsW;
        this->segmentsH = segmentsH;
        this->openEnded = openEnded;

        vec3 normal;

        int i,j,jMin;

        float _height = height / 2;

        std::vector <std::vector <vec3> >aVtc;

        if (!openEnded) {
            jMin = 1;
            this->segmentsH += 1;
            vec3 bottom = vec3(0,-_height*2, 0);
            std::vector <vec3> aRowB;
            for (i=0;i<segmentsW;++i) {
                aRowB.push_back(bottom);
            }
            aVtc.push_back(aRowB);
        } else {
            jMin = 0;
        }

        for (j=jMin;j<this->segmentsH;++j) {
            float z = -height + 2 * height * (float)(j-jMin) / (float)(this->segmentsH-jMin);

            std::vector <vec3> aRow;
            for (i=0;i<segmentsW;++i) {
                float verangle = (float)(2 * (float)i / (float)segmentsW*PI);
                float ringradius = radius * (float)(this->segmentsH-j)/(float)(this->segmentsH-jMin);
                float x = ringradius * sin(verangle);
                float y = ringradius * cos(verangle);

                vec3 oVtx = vec3(y,z,x);

                aRow.push_back(oVtx);
            }
            aVtc.push_back(aRow);
        }

        vec3 top = vec3(0,height,0);
        std::vector <vec3> aRowT;

        for (i=0;i<this->segmentsW;++i)
            aRowT.push_back(top);

        aVtc.push_back(aRowT);

        for (j=1;j<=this->segmentsH;++j) {
            for (i=0;i<segmentsW;++i) {
                vec3 a = aVtc[j][i];
                vec3 b = aVtc[j][(i-1+segmentsW)%segmentsW];
                vec3 c = aVtc[j-1][(i-1+segmentsW)%segmentsW];
                vec3 d = aVtc[j-1][i];

                int i2 = i;
                if(i==0) i2 = segmentsW;

                float vab = (float)j/(float)segmentsH;
                float vcd = (float)(j-1)/(float)segmentsH;
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

                normal = ((c-d).cross(a-d)).normalize();
                tVertex.push_back(a);       tNormal.push_back(normal);      tTexcoord.push_back(aUV);
                tVertex.push_back(d);       tNormal.push_back(normal);      tTexcoord.push_back(dUV);
                tVertex.push_back(c);       tNormal.push_back(normal);      tTexcoord.push_back(cUV);    

                IndexData.push_back(tVertex.size()-3);
                IndexData.push_back(tVertex.size()-2);
                IndexData.push_back(tVertex.size()-1);

            }
        }

        if (!openEnded) this->segmentsH-=1;

        for (unsigned i=0;i<tVertex.size();i++)
        {
            tTexcoord[i].x = 1-tTexcoord[i].x;
        }
        
        Build();
        
    }
    void RenderingConeComponent::Start() {}
    void RenderingConeComponent::Update() {}
    void RenderingConeComponent::Shutdown() {} 
    RenderingConeComponent::~RenderingConeComponent() {}

}