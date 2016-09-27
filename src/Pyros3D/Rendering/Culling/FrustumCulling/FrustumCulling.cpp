//============================================================================
// Name        : FrustumCulling.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Frustum Culling Class
//============================================================================

#include <Pyros3D/Rendering/Culling/FrustumCulling/FrustumCulling.h>
#include <iostream>

namespace  p3d {

	FrustumCulling::FrustumCulling() {}

	FrustumCulling::~FrustumCulling() {}

	void FrustumCulling::Update(const Matrix &ViewProjectionMatrix)
	{
		Matrix m = ViewProjectionMatrix;

		f32 me0 = m.m[0], me1 = m.m[1], me2 = m.m[2], me3 = m.m[3];
		f32 me4 = m.m[4], me5 = m.m[5], me6 = m.m[6], me7 = m.m[7];
		f32 me8 = m.m[8], me9 = m.m[9], me10 = m.m[10], me11 = m.m[11];
		f32 me12 = m.m[12], me13 = m.m[13], me14 = m.m[14], me15 = m.m[15];

		pl[0].SetNormalAndConstant(me3 - me0, me7 - me4, me11 - me8, me15 - me12);
		pl[0].normalize();
		pl[1].SetNormalAndConstant(me3 + me0, me7 + me4, me11 + me8, me15 + me12);
		pl[1].normalize();
		pl[2].SetNormalAndConstant(me3 + me1, me7 + me5, me11 + me9, me15 + me13);
		pl[2].normalize();
		pl[3].SetNormalAndConstant(me3 - me1, me7 - me5, me11 - me9, me15 - me13);
		pl[3].normalize();
		pl[4].SetNormalAndConstant(me3 - me2, me7 - me6, me11 - me10, me15 - me14);
		pl[4].normalize();
		pl[5].SetNormalAndConstant(me3 + me2, me7 + me6, me11 + me10, me15 + me14);
		pl[5].normalize();


		// Extracting Corners from Ortho
//        Vec4 viewFrustumCorners[8];
//
//        viewFrustumCorners[0] = Vec4(-1.0f, 1.0f, 0.0f, 1.0f);
//        viewFrustumCorners[1] = Vec4(1.0f, 1.0f, 0.0f, 1.0f);
//        viewFrustumCorners[2] = Vec4(1.0f, -1.0f, 0.0f, 1.0f);
//        viewFrustumCorners[3] = Vec4(-1.0f, -1.0f, 0.0f, 1.0f);
//        viewFrustumCorners[4] = Vec4(-1.0f, 1.0f, 1.0f, 1.0f);
//        viewFrustumCorners[5] = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
//        viewFrustumCorners[6] = Vec4(1.0f, -1.0f, 1.0f, 1.0f);
//        viewFrustumCorners[7] = Vec4(-1.0f, -1.0f, 1.0f, 1.0f);
//
//        for (uint32 i = 0; i < 8; i++)
//        {
//            viewFrustumCorners[i] = ViewProjectionMatrix.Inverse() * viewFrustumCorners[i];
//
//            viewFrustumCorners[i].x /= viewFrustumCorners[i].w;
//            viewFrustumCorners[i].y /= viewFrustumCorners[i].w;
//            viewFrustumCorners[i].z /= viewFrustumCorners[i].w;
//        }
	}

	bool FrustumCulling::PointInFrustum(const Vec3& p)
	{
		bool result = true;

		for (int32 i = 0; i < 6; i++)
		{
			if (pl[i].Distance(p) < 0) return false;
		}
		return result;
	}
	bool FrustumCulling::SphereInFrustum(const Vec3& p, const f32 radius)
	{
		for (int32 i = 0; i < 6; ++i)
		{
			f32 distance = pl[i].Distance(p);
			if (distance < -radius)
				return false;
		};
		return true;
	}
	bool FrustumCulling::ABoxInFrustum(AABox box)
	{
		Vec3 p1, p2;
		for (uint32 i = 0; i < 6; i++)
		{
			p1.x = pl[i].normal.x > 0 ? box.xmin : box.xmax;
			p2.x = pl[i].normal.x > 0 ? box.xmax : box.xmin;
			p1.y = pl[i].normal.y > 0 ? box.ymin : box.ymax;
			p2.y = pl[i].normal.y > 0 ? box.ymax : box.ymin;
			p1.z = pl[i].normal.z > 0 ? box.zmin : box.zmax;
			p2.z = pl[i].normal.z > 0 ? box.zmax : box.zmin;

			f32 d1 = pl[i].Distance(p1);
			f32 d2 = pl[i].Distance(p2);

			// if both outside plane, no intersection
			if (d1 < 0 && d2 < 0) {
				return false;
			}
			return true;
		}
		return false;
	}
	bool FrustumCulling::OBoxInFrustum(OBBox box)
	{
		return 0;
	}
}