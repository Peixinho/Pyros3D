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

namespace p3d {

Geometry::Geometry() {
}

void Geometry::CalculateTriangleTangentAndBinormal(const Vec3 &vertex1, const Vec2 &texcoord1,
                                                   const Vec3 &vertex2, const Vec2 &texcoord2,
                                                   const Vec3 &vertex3, const Vec2 &texcoord3,
                                                   Vec3* Binormal, Vec3* Tangent)
{
    Vec3 v2v1 = vertex2-vertex1;
    Vec3 v3v1 = vertex3-vertex1;
    
    // calculate the direction of the triangle based on texture coordinates
    
    // calculate c2c1_T and c2c1_B
    f32 c2c1_T = texcoord2.x - texcoord1.x;
    f32 c2c1_B = texcoord2.y - texcoord1.y;
    
    // calculate c3c1_T and c3c1_B
    f32 c3c1_T = texcoord3.x - texcoord1.x;
    f32 c3c1_B = texcoord3.y - texcoord1.y;
    
    // look at the references for more explanation for this one
    f32 fDenominator = c2c1_T * c3c1_B - c3c1_T * c2c1_B;
    
    if ((int)(fDenominator+0.5f)==0.0f)
    {
        *Tangent = Vec3(1.f,0.f,0.f);
        *Binormal = Vec3(0.f,1.f,0.f);        
    } else {
        
        f32 fScale1 = 1.f/fDenominator;
        
        *Tangent = Vec3((c3c1_B * v2v1.x - c2c1_B * v3v1.x) * fScale1,
                       (c3c1_B * v2v1.y - c2c1_B * v3v1.y) * fScale1,
                       (c3c1_B * v2v1.z - c2c1_B * v3v1.z) * fScale1);
        
        *Binormal = Vec3((-c3c1_T * v2v1.x + c2c1_T * v3v1.x) * fScale1,
                        (-c3c1_T * v2v1.y + c2c1_T * v3v1.y) * fScale1,
                        (-c3c1_T * v2v1.z + c2c1_T * v3v1.z) * fScale1);
                                                                    
    }
    
}

void Geometry::CreateBoundingBox(const Vec3& minBound, const Vec3& maxBound, std::vector<uint32> *index, std::vector<Vec3> *geometry)
{       
    std::vector<Vec3> tVertex;

    Vec3 a = Vec3(maxBound.x,maxBound.y,minBound.z); Vec3 b = Vec3(minBound.x,maxBound.y,minBound.z); Vec3 c = Vec3(minBound.x,maxBound.y,maxBound.z); Vec3 d = Vec3(maxBound.x,maxBound.y,maxBound.z);      
    tVertex.push_back(a);
    tVertex.push_back(b);
    tVertex.push_back(c);

    tVertex.push_back(c);
    tVertex.push_back(d);
    tVertex.push_back(a);

    a = Vec3(maxBound.x,minBound.y,maxBound.z); b = Vec3(minBound.x,minBound.y,maxBound.z); c = Vec3(minBound.x,minBound.y,minBound.z); d = Vec3(maxBound.x,minBound.y,minBound.z);    
    tVertex.push_back(a);
    tVertex.push_back(b);
    tVertex.push_back(c);

    tVertex.push_back(c);
    tVertex.push_back(d);
    tVertex.push_back(a);

    a = Vec3(maxBound.x,maxBound.y,maxBound.z); b = Vec3(minBound.x,maxBound.y,maxBound.z); c = Vec3(minBound.x,minBound.y,maxBound.z); d = Vec3(maxBound.x,minBound.y,maxBound.z);    
    tVertex.push_back(a);
    tVertex.push_back(b);
    tVertex.push_back(c);

    tVertex.push_back(c);
    tVertex.push_back(d);
    tVertex.push_back(a);

    a = Vec3(maxBound.x,minBound.y,minBound.z); b = Vec3(minBound.x,minBound.y,minBound.z); c = Vec3(minBound.x,maxBound.y,minBound.z); d = Vec3(maxBound.x,maxBound.y,minBound.z);    
    tVertex.push_back(a);
    tVertex.push_back(b);
    tVertex.push_back(c);

    tVertex.push_back(c);
    tVertex.push_back(d);
    tVertex.push_back(a);

    a = Vec3(minBound.x,maxBound.y,maxBound.z); b = Vec3(minBound.x, maxBound.y, minBound.z); c = Vec3(minBound.x,minBound.y,minBound.z); d = Vec3(minBound.x,minBound.y,maxBound.z);    
    tVertex.push_back(a);
    tVertex.push_back(b);
    tVertex.push_back(c);

    tVertex.push_back(c);
    tVertex.push_back(d);
    tVertex.push_back(a);

    a = Vec3(maxBound.x,maxBound.y,minBound.z); b = Vec3(maxBound.x,maxBound.y,maxBound.z); c = Vec3(maxBound.x,minBound.y,maxBound.z); d = Vec3(maxBound.x,minBound.y,minBound.z);    
    tVertex.push_back(a);
    tVertex.push_back(b);
    tVertex.push_back(c);

    tVertex.push_back(c);
    tVertex.push_back(d);
    tVertex.push_back(a);        
    
    std::vector<uint32> Index;
    for (uint32 i=0;i<tVertex.size();i++)
    {
        Index.push_back(i);
        std::cout << tVertex[i].toString() << std::endl;
    } 
    
    index->resize(Index.size());
    geometry->resize(tVertex.size());
    
    memcpy(&(*geometry)[0],&tVertex[0],sizeof(Vec3)*tVertex.size());
    memcpy(&(*index)[0],&Index[0],sizeof(uint32)*Index.size());
}
void Geometry::CreateBoundingSphere(const Vec3& minBound, const Vec3& maxBound, std::vector<uint32> *index, std::vector<Vec3> *geometry)
{
    std::vector<Vec3> tVertex;
    std::vector<uint32> Index;
    
    Vec3 center = Vec3((fabs(maxBound.x)+fabs(minBound.x))/2,(fabs(maxBound.y)+fabs(minBound.y))/2,(fabs(maxBound.z)+fabs(minBound.z))/2);

    f32 radius = center.z;
    
    int32 i,j;
    f32 iHor=(f32)8;
    f32 iVer=(f32)6;

    std::vector <std::vector<Vec3> > aVtc;    

    for (j=0;j<(iVer+1);++j) {
            f32 fRad1 = j / iVer;        
            f32 fZ = (f32)(-radius * cos(fRad1 * PI));
            f32 fRds = (f32)(radius * sin(fRad1 * PI));

            std::vector<Vec3> aRow;
            Vec3 oVtx;

            for (i=0;i< iHor;++i) {            
                    f32 fRamaxBound = (2 * i / iHor);
                    f32 fX = (f32)(fRds * sin(fRamaxBound * PI));            
                    f32 fY = (f32)(fRds * cos(fRamaxBound * PI));
                    if (!((j == 0 || j == iVer) && i > 0)) {
                            oVtx=Vec3(fY,fZ,fX);
                    }
                    aRow.push_back(oVtx);
            }
            aVtc.push_back(aRow);
    }

    int32 iVerNum = aVtc.size();

    for (j=0;j<iVerNum;++j) {
        int32 iHorNum = aVtc[j].size();
        if (j > 0) {
            for (i=0;i<iHorNum;++i) {
                bool bEnd = i == (iHorNum - 1);                
                Vec3 aP1 = aVtc[j][bEnd?0:(i + 1)];
                Vec3 aP2 = aVtc[j][(bEnd?(iHorNum-1):i)];
                Vec3 aP3 = aVtc[j - 1][(bEnd?(iHorNum - 1):i)];
                Vec3 aP4 = aVtc[j - 1][bEnd?0:(i + 1)];                              

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
    
    memcpy(&(*geometry)[0],&tVertex[0],sizeof(Vec3)*tVertex.size());
    memcpy(&(*index)[0],&Index[0],sizeof(uint32)*Index.size());   
}
void Geometry::CreateBoundingPlane(const Vec3& minBound, const Vec3& maxBound, std::vector<uint32> *index, std::vector<Vec3> *geometry)
{

    std::vector<Vec3> tVertex;

    Vec3 a = Vec3(maxBound.x,maxBound.y,(fabs(maxBound.z)+fabs(minBound.z))/2); Vec3 b = Vec3(minBound.x,maxBound.y,minBound.z); Vec3 c = Vec3(minBound.x,maxBound.y,maxBound.z); Vec3 d = Vec3(maxBound.x,maxBound.y,maxBound.z);      
    tVertex.push_back(a);
    tVertex.push_back(b);
    tVertex.push_back(c);

    tVertex.push_back(c);
    tVertex.push_back(d);
    tVertex.push_back(a);

    a = Vec3(maxBound.x,minBound.y,(fabs(maxBound.z)+fabs(minBound.z))/2); b = Vec3(minBound.x,minBound.y,maxBound.z); c = Vec3(minBound.x,minBound.y,minBound.z); d = Vec3(maxBound.x,minBound.y,minBound.z);    
    tVertex.push_back(a);
    tVertex.push_back(b);
    tVertex.push_back(c);

    tVertex.push_back(c);
    tVertex.push_back(d);
    tVertex.push_back(a);
    
    std::vector<uint32> Index;
    for (uint32 i=0;i<tVertex.size();i++)
    {
        Index.push_back(i);
    } 
        
    index->resize(Index.size());
    geometry->resize(tVertex.size());
    
    memcpy(&(*geometry)[0],&tVertex[0],sizeof(Vec3)*tVertex.size());
    memcpy(&(*index)[0],&Index[0],sizeof(uint32)*Index.size());    
}

Geometry::~Geometry() {
}

}
