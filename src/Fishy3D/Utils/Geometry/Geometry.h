//============================================================================
// Name        : Geomtry.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Geometry Utils
//============================================================================


#ifndef GEOMETRY_H
#define	GEOMETRY_H

#include "../../Core/Math/Math.h"

namespace Fishy3D {

class Geometry {
public:
    Geometry();
    
    // Create Tangent and Bitangent
    void CalculateTriangleTangentAndBinormal(const vec3& vertex1, const vec2& texcoord1, const vec3& vertex2, const vec2& texcoord2, const vec3 &vertex3, const vec2& texcoord3, vec3* Binormal, vec3* Tangent);
    
    // Bounding Box
    static void CreateBoundingBox(const vec3 &minBound, const vec3 &maxBound, std::vector<unsigned int> *index, std::vector<vec3> *geometry);
    static void CreateBoundingSphere(const vec3 &minBound, const vec3 &maxBound, std::vector<unsigned int> *index, std::vector<vec3> *geometry);
    static void CreateBoundingPlane(const vec3 &minBound, const vec3 &maxBound,  std::vector<unsigned int> *index, std::vector<vec3> *geometry);
    
    virtual ~Geometry();
private:

};

}

#endif	/* GEOMETRY_H */