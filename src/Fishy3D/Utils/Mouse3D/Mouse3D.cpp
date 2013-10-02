//============================================================================
// Name        : Mouse3D.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Mouse 3D Class
//============================================================================

#include "Mouse3D.h"

namespace Fishy3D {

    Mouse3D::Mouse3D() {
    }

    Mouse3D::~Mouse3D() {
    }

    const vec3 &Mouse3D::GetDirection() const
    {
        return Direction;
    }
    const vec3 &Mouse3D::GetOrigin() const
    {
        return Origin;
    }
    bool Mouse3D::rayDirection(const float &windowWidth, const float &windowHeight, const float &mouseX, const float &mouseY, const Matrix &ModelView, const Matrix &Projection) 
    {
        
        float glMouseY = windowHeight-mouseY;
        
        int viewPort[4];
        float x1,y1,z1,x2,y2,z2;
                       
        viewPort[0]=0; viewPort[1]=0; viewPort[2]=windowWidth; viewPort[3]=windowHeight;
        
        vec3 v1,v2;
        
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
    bool Mouse3D::rayOrigin(const float &windowWidth, const float &windowHeight, const float &mouseX, const float &mouseY, const Matrix &ModelView, const Matrix &Projection) 
    {

        int viewPort[4];
	viewPort[0]=0;viewPort[1]=0;viewPort[2]=windowWidth;viewPort[3]=windowHeight;
        
        float glMouseY;
        glMouseY = windowHeight-mouseY;
        
        float x1,y1,z1;
        
        if (UnProject(mouseX,glMouseY,0,&ModelView.m[0],&Projection.m[0],viewPort,&x1,&y1,&z1))
        {
            vec3 v1;
            v1.x=x1;    v1.y=y1;    v1.z=z1;
            
            Origin = v1;
            return true;
        }
        
        return false;

    }

    bool Mouse3D::rayIntersectionTriangle(const vec3 &v1, const vec3 &v2, const vec3 &v3, vec3 *IntersectionPoint, float *t) const
    {

        vec3 V1 = v1;
        vec3 V2 = v2;
        vec3 V3 = v3;
        
        vec3 e1 = V2 - V1, e2 = V3 - V1;
        vec3 pvec = Direction.cross(e2);
        float det = e1.dotProduct(pvec);

        if (det > -EPSILON && det < EPSILON) 
        {
            return false;
        }

        float inv_det = 1.0f / det;
        vec3 tvec = Origin - V1;
        float u = tvec.dotProduct(pvec) * inv_det;

        if (u < 0.0 || u > 1.0)
        {
            return false;
        };

        vec3 qvec = tvec.cross(e1);
        float v = Direction.dotProduct(qvec) * inv_det;

        if (v < 0.0 || u + v > 1.0)
        {
            return false;
        };

        float _t = e2.dotProduct(qvec) * inv_det;

        if(t)
        {
            *t = _t;
        };

        if(IntersectionPoint)
        {
            *IntersectionPoint = Origin + (Direction * _t);
        };        
        return true;
    }

    bool Mouse3D::UnProject(const float& winX, const float& winY, const float& winZ, const float model[16], const float proj[16], const int view[4], float *objx, float *objy, float *objz)
    {

        float finalMatrix[16];
        float in[4];
        float out[4];

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
    bool Mouse3D::__gluInvertMatrixf(const float m[16], float invOut[16])
    {
        float inv[16], det;
        int i;

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
    void Mouse3D::__gluMultMatrixVecf(const float matrix[16], const float in[4], float out[4])
    {
        int i;

        for (i=0; i<4; i++) 
        {
             out[i] =    in[0] * matrix[0*4+i] +
                         in[1] * matrix[1*4+i] +
                         in[2] * matrix[2*4+i] +
                         in[3] * matrix[3*4+i];
        }
    }
    void Mouse3D::__gluMultMatricesf(const float a[16], const float b[16], float r[16])
    {
        int i, j;

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