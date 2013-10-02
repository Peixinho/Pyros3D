//============================================================================
// Name        : Rendering Torus Knot Component.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering Torus Knot Component
//============================================================================

#include "RenderingTorusKnotComponent.h"

namespace Fishy3D {

        RenderingTorusKnotComponent::RenderingTorusKnotComponent() 
        {
        }
        RenderingTorusKnotComponent::RenderingTorusKnotComponent(const std::string &Name, const float& radius, const float& tube, SuperSmartPointer<IMaterial> material, const int& segmentsW, const int &segmentsH, const float &p, const float &q, const int &heightscale, bool SNormals) : RenderingPrimitiveComponent(Name, material, SNormals)
        {
            this->material = material;
            
            this->radius = radius;
            this->tube = tube;
            this->segmentsW = segmentsW;
            this->segmentsH = segmentsH;
            this->p = p;
            this->q = q;
            this->heightScale = heightscale;          
            
            // TODO NORMALS
            vec3 normal;    
            
            // counters
            int i,j;      
            
            // vars aux
            vec3 tang,n,bitan;
            
            std::vector< std::vector <vec3> > aVtc;
            
            for (i=0;i<segmentsW;++i) {
                
                std::vector<vec3> aRow;
                
                for (j=0;j<segmentsH;++j) {
                    float u = (float)((float)i/segmentsW*2*this->p*PI);
                    float v  = (float)((float)j/segmentsH*2*PI);
                    vec3 p = GetPos(u,v);
                    vec3 p2 = GetPos((float)(u+.01),(float)(v));
                    float cx, cy;
                    tang.x = p2.x - p.x; tang.y = p2.y-p.y; tang.z = p2.z-p.z;
                    n.x = p2.x + p.x; n.y = p2.y + p.y; n.z = p2.z + p.z;
                    bitan = tang.cross(n);
                    n = bitan.cross(tang);
                    bitan.normalizeSelf();
                    n.normalizeSelf();
                    
                    cx = tube*cos(v); cy = tube*sin(v);
                    p.x += cx * n.x + cy * bitan.x;
                    p.y += cx * n.y + cy * bitan.y;
                    p.z += cx * n.z + cy * bitan.z;
                    
                    vec3 oVtx = vec3(p.x, p.z, p.y);
                    aRow.push_back(oVtx);                                        
                }
                aVtc.push_back(aRow);
            }
            
            for (i=0;i<segmentsW;++i) {
                for(j=0;j<segmentsH;++j) {
                    int ip = (i+1)%segmentsW;
                    int jp = (j+1)%segmentsH;
                    vec3 a = aVtc[i ][j];
                    vec3 b = aVtc[ip][j];
                    vec3 c = aVtc[i ][jp];
                    vec3 d = aVtc[ip][jp];
                    
                    vec2 aUV = vec2((float)i      /segmentsW, (float)j      /segmentsH);
                    vec2 bUV = vec2((float)(i+1)  /segmentsW, (float)j      /segmentsH);
                    vec2 cUV = vec2((float)i      /segmentsW, (float)(j+1)    /segmentsH);
                    vec2 dUV = vec2((float)(i+1)  /segmentsW, (float)(j+1)    /segmentsH);                    
            
                    normal = ((c-b).cross(a-b)).normalize();
                    tVertex.push_back(a);       tNormal.push_back(normal);      tTexcoord.push_back(aUV);
                    tVertex.push_back(b);       tNormal.push_back(normal);      tTexcoord.push_back(bUV);
                    tVertex.push_back(c);       tNormal.push_back(normal);      tTexcoord.push_back(cUV);

                    IndexData.push_back(tVertex.size()-3);
                    IndexData.push_back(tVertex.size()-2);
                    IndexData.push_back(tVertex.size()-1);

                    normal = ((b-c).cross(d-c)).normalize();
                    tVertex.push_back(d);       tNormal.push_back(normal);      tTexcoord.push_back(dUV);
                    tVertex.push_back(c);       tNormal.push_back(normal);      tTexcoord.push_back(cUV);
                    tVertex.push_back(b);       tNormal.push_back(normal);      tTexcoord.push_back(bUV);                        
                                        
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
        
        RenderingTorusKnotComponent::~RenderingTorusKnotComponent() 
        {
        }
        
        void RenderingTorusKnotComponent::Start() {}
        void RenderingTorusKnotComponent::Update() {}
        void RenderingTorusKnotComponent::Shutdown() {}

        vec3 RenderingTorusKnotComponent::GetPos(const float& u, const float& v) const
        {
            float cu = cos(u);
            float su = sin(u);
            float quOverP = this->q/this->p*u;
            float cs = cos(quOverP);
            vec3 pos;
            pos.x = (float)(this->radius*(2+cs)*.5*cu);
            pos.y = (float)(this->radius*(2+cs)*su*.5);
            pos.z = (float)(heightScale*radius*sin(quOverP)*.5);

            return pos;
        }
        
}