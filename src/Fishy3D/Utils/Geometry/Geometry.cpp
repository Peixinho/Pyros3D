//============================================================================
// Name        : Geomtry.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Geometry Utils
//============================================================================

#include <string.h>
#include <vector>
#include <iostream>
#include "Geometry.h"

namespace Fishy3D {

Geometry::Geometry() {
}

void Geometry::CalculateTriangleTangentAndBinormal(const vec3 &vertex1, const vec2 &texcoord1,
                                                   const vec3 &vertex2, const vec2 &texcoord2,
                                                   const vec3 &vertex3, const vec2 &texcoord3,
                                                   vec3* Binormal, vec3* Tangent)
{
    vec3 v2v1 = vertex2-vertex1;
    vec3 v3v1 = vertex3-vertex1;
    
    // calculate the direction of the triangle based on texture coordinates
    
    // calculate c2c1_T and c2c1_B
    float c2c1_T = texcoord2.x - texcoord1.x;
    float c2c1_B = texcoord2.y - texcoord1.y;
    
    // calculate c3c1_T and c3c1_B
    float c3c1_T = texcoord3.x - texcoord1.x;
    float c3c1_B = texcoord3.y - texcoord1.y;
    
    // look at the references for more explanation for this one
    float fDenominator = c2c1_T * c3c1_B - c3c1_T * c2c1_B;
    
    if ((int)(fDenominator+0.5f)==0.0f)
    {
        *Tangent = vec3(1.f,0.f,0.f);
        *Binormal = vec3(0.f,1.f,0.f);        
    } else {
        
        float fScale1 = 1.f/fDenominator;
        
        *Tangent = vec3((c3c1_B * v2v1.x - c2c1_B * v3v1.x) * fScale1,
                       (c3c1_B * v2v1.y - c2c1_B * v3v1.y) * fScale1,
                       (c3c1_B * v2v1.z - c2c1_B * v3v1.z) * fScale1);
        
        *Binormal = vec3((-c3c1_T * v2v1.x + c2c1_T * v3v1.x) * fScale1,
                        (-c3c1_T * v2v1.y + c2c1_T * v3v1.y) * fScale1,
                        (-c3c1_T * v2v1.z + c2c1_T * v3v1.z) * fScale1);
                                                                    
    }
    
}

void Geometry::CreateBoundingBox(const vec3& minBound, const vec3& maxBound, std::vector<unsigned int> *index, std::vector<vec3> *geometry)
{       
    std::vector<vec3> tVertex;

    vec3 a = vec3(maxBound.x,maxBound.y,minBound.z); vec3 b = vec3(minBound.x,maxBound.y,minBound.z); vec3 c = vec3(minBound.x,maxBound.y,maxBound.z); vec3 d = vec3(maxBound.x,maxBound.y,maxBound.z);      
    tVertex.push_back(a);
    tVertex.push_back(b);
    tVertex.push_back(c);

    tVertex.push_back(c);
    tVertex.push_back(d);
    tVertex.push_back(a);

    a = vec3(maxBound.x,minBound.y,maxBound.z); b = vec3(minBound.x,minBound.y,maxBound.z); c = vec3(minBound.x,minBound.y,minBound.z); d = vec3(maxBound.x,minBound.y,minBound.z);    
    tVertex.push_back(a);
    tVertex.push_back(b);
    tVertex.push_back(c);

    tVertex.push_back(c);
    tVertex.push_back(d);
    tVertex.push_back(a);

    a = vec3(maxBound.x,maxBound.y,maxBound.z); b = vec3(minBound.x,maxBound.y,maxBound.z); c = vec3(minBound.x,minBound.y,maxBound.z); d = vec3(maxBound.x,minBound.y,maxBound.z);    
    tVertex.push_back(a);
    tVertex.push_back(b);
    tVertex.push_back(c);

    tVertex.push_back(c);
    tVertex.push_back(d);
    tVertex.push_back(a);

    a = vec3(maxBound.x,minBound.y,minBound.z); b = vec3(minBound.x,minBound.y,minBound.z); c = vec3(minBound.x,maxBound.y,minBound.z); d = vec3(maxBound.x,maxBound.y,minBound.z);    
    tVertex.push_back(a);
    tVertex.push_back(b);
    tVertex.push_back(c);

    tVertex.push_back(c);
    tVertex.push_back(d);
    tVertex.push_back(a);

    a = vec3(minBound.x,maxBound.y,maxBound.z); b = vec3(minBound.x, maxBound.y, minBound.z); c = vec3(minBound.x,minBound.y,minBound.z); d = vec3(minBound.x,minBound.y,maxBound.z);    
    tVertex.push_back(a);
    tVertex.push_back(b);
    tVertex.push_back(c);

    tVertex.push_back(c);
    tVertex.push_back(d);
    tVertex.push_back(a);

    a = vec3(maxBound.x,maxBound.y,minBound.z); b = vec3(maxBound.x,maxBound.y,maxBound.z); c = vec3(maxBound.x,minBound.y,maxBound.z); d = vec3(maxBound.x,minBound.y,minBound.z);    
    tVertex.push_back(a);
    tVertex.push_back(b);
    tVertex.push_back(c);

    tVertex.push_back(c);
    tVertex.push_back(d);
    tVertex.push_back(a);        
    
    std::vector<unsigned int> Index;
    for (unsigned int i=0;i<tVertex.size();i++)
    {
        Index.push_back(i);
        std::cout << tVertex[i].toString() << std::endl;
    } 
    
    index->resize(Index.size());
    geometry->resize(tVertex.size());
    
    memcpy(&(*geometry)[0],&tVertex[0],sizeof(vec3)*tVertex.size());
    memcpy(&(*index)[0],&Index[0],sizeof(unsigned int)*Index.size());
}
void Geometry::CreateBoundingSphere(const vec3& minBound, const vec3& maxBound, std::vector<unsigned int> *index, std::vector<vec3> *geometry)
{
    std::vector<vec3> tVertex;
    std::vector<unsigned int> Index;
    
    vec3 center = vec3((fabs(maxBound.x)+fabs(minBound.x))/2,(fabs(maxBound.y)+fabs(minBound.y))/2,(fabs(maxBound.z)+fabs(minBound.z))/2);

    float radius = center.z;
    
    int i,j;
    float iHor=(float)8;
    float iVer=(float)6;

    std::vector <std::vector<vec3> > aVtc;    

    for (j=0;j<(iVer+1);++j) {
            float fRad1 = j / iVer;        
            float fZ = (float)(-radius * cos(fRad1 * PI));
            float fRds = (float)(radius * sin(fRad1 * PI));

            std::vector<vec3> aRow;
            vec3 oVtx;

            for (i=0;i< iHor;++i) {            
                    float fRamaxBound = (2 * i / iHor);
                    float fX = (float)(fRds * sin(fRamaxBound * PI));            
                    float fY = (float)(fRds * cos(fRamaxBound * PI));
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

                if (j < (aVtc.size() - 1)) {                                          

                    tVertex.push_back(center+aP3);
                    tVertex.push_back(center+aP2);
                    tVertex.push_back(center+aP1);

                    Index.push_back(tVertex.size()-3);
                    Index.push_back(tVertex.size()-2);
                    Index.push_back(tVertex.size()-1);

                }
                if (j > 1) {                    

                    tVertex.push_back(center+aP4);
                    tVertex.push_back(center+aP3);
                    tVertex.push_back(center+aP1);                     

                    Index.push_back(tVertex.size()-3);
                    Index.push_back(tVertex.size()-2);
                    Index.push_back(tVertex.size()-1);

                }
            }
        }
    }
    index->resize(Index.size());
    geometry->resize(tVertex.size());
    
    memcpy(&(*geometry)[0],&tVertex[0],sizeof(vec3)*tVertex.size());
    memcpy(&(*index)[0],&Index[0],sizeof(unsigned int)*Index.size());   
}
void Geometry::CreateBoundingPlane(const vec3& minBound, const vec3& maxBound, std::vector<unsigned int> *index, std::vector<vec3> *geometry)
{

    std::vector<vec3> tVertex;

    vec3 a = vec3(maxBound.x,maxBound.y,(fabs(maxBound.z)+fabs(minBound.z))/2); vec3 b = vec3(minBound.x,maxBound.y,minBound.z); vec3 c = vec3(minBound.x,maxBound.y,maxBound.z); vec3 d = vec3(maxBound.x,maxBound.y,maxBound.z);      
    tVertex.push_back(a);
    tVertex.push_back(b);
    tVertex.push_back(c);

    tVertex.push_back(c);
    tVertex.push_back(d);
    tVertex.push_back(a);

    a = vec3(maxBound.x,minBound.y,(fabs(maxBound.z)+fabs(minBound.z))/2); b = vec3(minBound.x,minBound.y,maxBound.z); c = vec3(minBound.x,minBound.y,minBound.z); d = vec3(maxBound.x,minBound.y,minBound.z);    
    tVertex.push_back(a);
    tVertex.push_back(b);
    tVertex.push_back(c);

    tVertex.push_back(c);
    tVertex.push_back(d);
    tVertex.push_back(a);
    
    std::vector<unsigned int> Index;
    for (unsigned i=0;i<tVertex.size();i++)
    {
        Index.push_back(i);
    } 
        
    index->resize(Index.size());
    geometry->resize(tVertex.size());
    
    memcpy(&(*geometry)[0],&tVertex[0],sizeof(vec3)*tVertex.size());
    memcpy(&(*index)[0],&Index[0],sizeof(unsigned int)*Index.size());    
}

Geometry::~Geometry() {
}

}
