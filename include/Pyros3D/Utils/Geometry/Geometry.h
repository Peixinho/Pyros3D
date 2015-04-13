//============================================================================
// Name        : Geomtry.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Geometry Utils
//============================================================================


#ifndef GEOMETRY_H
#define	GEOMETRY_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>

namespace p3d {

class PYROS3D_API Geometry {
	public:
	    Geometry();
	    
	    // Create Tangent and Bitangent
	    static void CalculateTriangleTangentAndBinormal(const Vec3& vertex1, const Vec2& texcoord1, const Vec3& vertex2, const Vec2& texcoord2, const Vec3 &vertex3, const Vec2& texcoord3, Vec3* Binormal, Vec3* Tangent);
	    
	    // Bounding Box
	    static void CreateBoundingBox(const Vec3 &minBound, const Vec3 &maxBound, std::vector<uint32> *index, std::vector<Vec3> *geometry);
	    static void CreateBoundingSphere(const Vec3 &minBound, const Vec3 &maxBound, std::vector<uint32> *index, std::vector<Vec3> *geometry);
	    static void CreateBoundingPlane(const Vec3 &minBound, const Vec3 &maxBound,  std::vector<uint32> *index, std::vector<Vec3> *geometry);
	    
	    virtual ~Geometry();
	private:

};

}

#endif	/* GEOMETRY_H */