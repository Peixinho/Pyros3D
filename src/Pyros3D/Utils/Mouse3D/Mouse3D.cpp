//============================================================================
// Name        : Mouse3D.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Mouse 3D Class
//============================================================================

#include <Pyros3D/Utils/Mouse3D/Mouse3D.h>

namespace p3d {

    Mouse3D::Mouse3D() {
    }

    Mouse3D::~Mouse3D() {
    }

    const Vec3 &Mouse3D::GetDirection() const
    {
        return Direction;
    }
    const Vec3 &Mouse3D::GetOrigin() const
    {
        return Origin;
    }
    bool Mouse3D::rayDirection(const f32 &windowWidth, const f32 &windowHeight, const f32 &mouseX, const f32 &mouseY, const Matrix &ModelView, const Matrix &Projection) 
    {
        
        f32 glMouseY = windowHeight-mouseY;
        
        int32 viewPort[4];
        f32 x1,y1,z1,x2,y2,z2;
                       
        viewPort[0]=0; viewPort[1]=0; viewPort[2]=windowWidth; viewPort[3]=windowHeight;
        
        Vec3 v1,v2;
        
        if (UnProject(mouseX,glMouseY,0,&ModelView.m[0],&Projection.m[0],viewPort,&x1,&y1,&z1) == true 
            &&
            UnProject(mouseX,glMouseY,1,&ModelView.m[0],&Projection.m[0],viewPort,&x2,&y2,&z2) == true)
        {
            
            v1.x=x1;    v1.y=y1;    v1.z=z1;
            v2.x=x2;    v2.y=y2;    v2.z=z2;

            v2 = v2-v1;
            v2.normalizeSelf();
            
            Origin = v1;
            Direction = v2;               

            return true;
        }
        return false;
    }
    bool Mouse3D::rayOrigin(const f32 &windowWidth, const f32 &windowHeight, const f32 &mouseX, const f32 &mouseY, const Matrix &ModelView, const Matrix &Projection) 
    {

        int32 viewPort[4];
	viewPort[0]=0;viewPort[1]=0;viewPort[2]=windowWidth;viewPort[3]=windowHeight;
        
        f32 glMouseY;
        glMouseY = windowHeight-mouseY;
        
        f32 x1,y1,z1;
        
        if (UnProject(mouseX,glMouseY,0,&ModelView.m[0],&Projection.m[0],viewPort,&x1,&y1,&z1))
        {
            Vec3 v1;
            v1.x=x1;    v1.y=y1;    v1.z=z1;
            
            Origin = v1;
            return true;
        }
        
        return false;

    }

    bool Mouse3D::rayIntersectionTriangle(const Vec3 &v1, const Vec3 &v2, const Vec3 &v3, Vec3 *IntersectionPoint32, f32 *t) const
    {

        Vec3 V1 = v1;
        Vec3 V2 = v2;
        Vec3 V3 = v3;
        
        Vec3 e1 = V2 - V1, e2 = V3 - V1;
        Vec3 pvec = Direction.cross(e2);
        f32 det = e1.dotProduct(pvec);

        if (det > -EPSILON && det < EPSILON) 
        {
            return false;
        }

        f32 inv_det = 1.0f / det;
        Vec3 tvec = Origin - V1;
        f32 u = tvec.dotProduct(pvec) * inv_det;

        if (u < 0.0 || u > 1.0)
        {
            return false;
        };

        Vec3 qvec = tvec.cross(e1);
        f32 v = Direction.dotProduct(qvec) * inv_det;

        if (v < 0.0 || u + v > 1.0)
        {
            return false;
        };

        f32 _t = e2.dotProduct(qvec) * inv_det;

        if(t)
        {
            *t = _t;
        };

        if(IntersectionPoint32)
        {
            *IntersectionPoint32 = Origin + (Direction * _t);
        };        
        return true;
    }

    bool Mouse3D::UnProject(const f32& winX, const f32& winY, const f32& winZ, const f32 model[16], const f32 proj[16], const int32 view[4], f32 *objx, f32 *objy, f32 *objz)
    {

        f32 finalMatrix[16];
        f32 in[4];
        f32 out[4];

        __gluMultMatricesf(model, proj, finalMatrix);
        if (!__gluInvertMatrixf(finalMatrix, finalMatrix)) return false;

        in[0]=winX;
        in[1]=winY;
        in[2]=winZ;
        in[3]=1.0;

        /* Map x and y from window coordinates */
        in[0] = (in[0] - view[0]) / view[2];
        in[1] = (in[1] - view[1]) / view[3];

        /* Map to range -1 to 1 */
        in[0] = in[0] * 2 - 1;
        in[1] = in[1] * 2 - 1;
        in[2] = in[2] * 2 - 1;

        __gluMultMatrixVecf(finalMatrix, in, out);
        if (out[3] == 0.0) return false;
        out[0] /= out[3];
        out[1] /= out[3];
        out[2] /= out[3];
        *objx = out[0];
        *objy = out[1];
        *objz = out[2];
        return true;

    }
    bool Mouse3D::__gluInvertMatrixf(const f32 m[16], f32 invOut[16])
    {
        f32 inv[16], det;
        int32 i;

        inv[0] =   m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15]
                 + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
        inv[4] =  -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15]
                 - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
        inv[8] =   m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15]
                 + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
        inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14]
                 - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
        inv[1] =  -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15]
                 - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
        inv[5] =   m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15]
                 + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
        inv[9] =  -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15]
                 - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
        inv[13] =  m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14]
                 + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
        inv[2] =   m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15]
                 + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
        inv[6] =  -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15]
                 - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
        inv[10] =  m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15]
                 + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
        inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14]
                 - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
        inv[3] =  -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11]
                 - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
        inv[7] =   m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11]
                 + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
        inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11]
                 - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
        inv[15] =  m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10]
                 + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];

        det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
        if (det == 0)
            return false;

        det = 1.0 / det;

        for (i = 0; i < 16; i++)
            invOut[i] = inv[i] * det;

        return true;
    }
    void Mouse3D::__gluMultMatrixVecf(const f32 matrix[16], const f32 in[4], f32 out[4])
    {
        int32 i;

        for (i=0; i<4; i++) 
        {
             out[i] =    in[0] * matrix[0*4+i] +
                         in[1] * matrix[1*4+i] +
                         in[2] * matrix[2*4+i] +
                         in[3] * matrix[3*4+i];
        }
    }
    void Mouse3D::__gluMultMatricesf(const f32 a[16], const f32 b[16], f32 r[16])
    {
        int32 i, j;

        for (i = 0; i < 4; i++) {
             for (j = 0; j < 4; j++) {
                 r[i*4+j] =     a[i*4+0]*b[0*4+j] +
                                a[i*4+1]*b[1*4+j] +
                                a[i*4+2]*b[2*4+j] +
                                a[i*4+3]*b[3*4+j];
             }
        }
    }
}