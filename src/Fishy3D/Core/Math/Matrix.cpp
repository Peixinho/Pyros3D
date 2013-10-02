//============================================================================
// Name        : Matrix.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Matrix
//
// M11 = m[0] M12 = m[4] M13 = m[8] M14 = m[12]
// M21 = m[1] M22 = m[5] M23 = m[9] M24 = m[13]
// M31 = m[2] M32 = m[6] M33 = m[10] M34 = m[14]
// M41 = m[3] M42 = m[7] M43 = m[11] M44 = m[15]
//
//============================================================================

#include "Matrix.h"
#include "Quaternion.h"

namespace Fishy3D {

Matrix::Matrix()
{
    identity();    
}

Matrix::Matrix(const float n11, const float n21, const float n31, const float n41, const float n12, const float n22, const float n32, const float n42, const float n13, const float n23, const float n33, const float n43, const float n14, const float n24, const float n34, const float n44)
{
    // Matrix array
    m[0] = n11; m[4] = n12; m[8] = n13; m[12] = n14;
    m[1] = n21; m[5] = n22; m[9] = n23; m[13] = n24;
    m[2] = n31; m[6] = n32; m[10] = n33; m[14] = n34;
    m[3] = n41; m[7] = n42; m[11] = n43; m[15] = n44;
}

void Matrix::identity()
{   
    // Matrix array
    m[0] = 1; m[4] = 0; m[8] = 0; m[12] = 0;
    m[1] = 0; m[5] = 1; m[9] = 0; m[13] = 0;
    m[2] = 0; m[6] = 0; m[10] = 1; m[14] = 0;
    m[3] = 0; m[7] = 0; m[11] = 0; m[15] = 1;    
}

void Matrix::LookAtRH(const vec3 &eye, const vec3 &center, const vec3 &up)
{
    vec3 zAxis = (eye - center).normalize();
    vec3 xAxis = up.cross(zAxis).normalize();
    vec3 yAxis = zAxis.cross(xAxis).normalize();

    m[0] = xAxis.x;                  m[1] = yAxis.x;                      m[2] = zAxis.x;                  m[3] = 0;
    m[4] = xAxis.y;                  m[5] = yAxis.y;                      m[6] = zAxis.y;                  m[7] = 0;
    m[8] = xAxis.z;                  m[9] = yAxis.z;                      m[10] = zAxis.z;                  m[11] = 0;
    m[12] = -xAxis.dotProduct(eye);   m[13] = -yAxis.dotProduct(eye);       m[14] = -zAxis.dotProduct(eye);   m[15] = 1;
}

void Matrix::LookAtLH(const vec3 &eye, const vec3 &center, const vec3 &up)
{
    vec3 zAxis = (center - eye).normalize();
    vec3 xAxis = up.cross(zAxis).normalize();
    vec3 yAxis = zAxis.cross(xAxis).normalize();

    m[0] = xAxis.x;                  m[1] = yAxis.x;                      m[2] = zAxis.x;                  m[3] = 0;
    m[4] = xAxis.y;                  m[5] = yAxis.y;                      m[6] = zAxis.y;                  m[7] = 0;
    m[8] = xAxis.z;                  m[9] = yAxis.z;                      m[10] = zAxis.z;                  m[11] = 0;
    m[12] = -xAxis.dotProduct(eye);   m[13] = -yAxis.dotProduct(eye);       m[14] = -zAxis.dotProduct(eye);   m[15] = 1;
}

void Matrix::Translate(const float &x, const float &y, const float &z)
{
    m[12] = x; m[13] = y; m[14] = z;
}
void Matrix::Translate(const vec3 &xyz)
{
    m[12] = xyz.x; m[13] = xyz.y; m[14] = xyz.z;
}
void Matrix::TranslateX(const float &x)
{
    m[12] = x;
}
void Matrix::TranslateY(const float &y)
{
    m[13] = y;
}
void Matrix::TranslateZ(const float &z)
{
    m[14] = z;
}
vec3 Matrix::GetTranslation() const
{
    return vec3(m[12],m[13],m[14]);
}

void Matrix::RotationX(const float &angle)
{    
    float c = cos( angle ), s = sin( angle );
 
    Matrix mrot;
    mrot.identity();
    
    mrot.m[5] = mrot.m[10] = c;
    mrot.m[6] = s;
    mrot.m[9] = -mrot.m[6];
    *this*=mrot;
}
void Matrix::RotationY(const float &angle)
{
    
    float c = cos( angle ), s = sin( angle );
    
    Matrix mrot;
    mrot.identity();
    mrot.m[0] = mrot.m[10] = c;
    mrot.m[8] = s;
    mrot.m[2] = -mrot.m[8];
    *this*=mrot;
}
void Matrix::RotationZ(const float &angle)
{
    float c = cos( angle ), s = sin( angle );
    
    Matrix mrot;
    mrot.identity();
    mrot.m[0] = mrot.m[5] = c;
    mrot.m[1] = s;
    mrot.m[4] = -mrot.m[1];
    *this*=mrot;
}
void Matrix::Scale(const float &sx, const float &sy, const float &sz)
{    
     m[0] = sx;
     m[5] = sy;
     m[10] = sz;
     m[15] = 1.0f;
}
void Matrix::Scale(const vec3 &xyz)
{
     m[0] = xyz.x;
     m[5] = xyz.y;
     m[10] = xyz.z;
     m[15] = 1.0f;
}
void Matrix::ScaleX(const float& x)
{
    m[0] = x;
}
void Matrix::ScaleY(const float& y)
{
    m[5] = y;
}
void Matrix::ScaleZ(const float& z)
{
    m[10] = z;
}
Matrix Matrix::GetRotation(const vec3 &s) const
{
    
    float invScaleX=1/s.x, invScaleY=1/s.y, invScaleZ=1/s.z;
    
    Matrix rot;
    rot.m[0] = m[0] * invScaleX;
    rot.m[1] = m[1] * invScaleX;
    rot.m[2] = m[2] * invScaleX;
    
    rot.m[4] = m[4] * invScaleY;
    rot.m[5] = m[5] * invScaleY;
    rot.m[6] = m[6] * invScaleY;
    
    rot.m[8] = m[8] * invScaleZ;
    rot.m[9] = m[9] * invScaleZ;
    rot.m[10] = m[10] * invScaleZ;
    
    return rot;
    
}

float Matrix::Determinant() const
{
    return (
            m[12] * m[9] * m[6] * m[3]-
            m[8] * m[13] * m[6] * m[3]-
            m[12] * m[5] * m[10] * m[3]+
            m[4] * m[13] * m[10] * m[3]+

            m[8] * m[5] * m[14] * m[3]-
            m[4] * m[9] * m[14] * m[3]-
            m[12] * m[9] * m[2] * m[7]+
            m[8] * m[13] * m[2] * m[7]+

            m[12] * m[1] * m[10] * m[7]-
            m[0] * m[13] * m[10] * m[7]-
            m[8] * m[1] * m[14] * m[7]+
            m[0] * m[9] * m[14] * m[7]+

            m[12] * m[5] * m[2] * m[11]-
            m[4] * m[13] * m[2] * m[11]-
            m[12] * m[1] * m[6] * m[11]+
            m[0] * m[13] * m[6] * m[11]+

            m[4] * m[1] * m[14] * m[11]-
            m[0] * m[5] * m[14] * m[11]-
            m[8] * m[5] * m[2] * m[15]+
            m[4] * m[9] * m[2] * m[15]+

            m[8] * m[1] * m[6] * m[15]-
            m[0] * m[9] * m[6] * m[15]-
            m[4] * m[1] * m[10] * m[15]+
            m[0] * m[5] * m[10] * m[15]
            );
}

Matrix Matrix::Transpose() const
{
    Matrix a,b = *this;

    a.m[0] = b.m[0];    a.m[1] = b.m[4];    a.m[2] = b.m[8];  a.m[3] = b.m[12];
    a.m[4] = b.m[1];    a.m[5] = b.m[5];    a.m[6] = b.m[9];  a.m[7] = b.m[13];
    a.m[8] = b.m[2];    a.m[9] = b.m[6];    a.m[10] = b.m[10];  a.m[11] = b.m[14];
    a.m[12] = b.m[3];    a.m[13] = b.m[7];    a.m[14] = b.m[11];  a.m[15] = b.m[15];

    return a;
}

Matrix Matrix::Inverse() const
{

        Matrix m2,m1 = *this;
        
	m2.m[0] = m[9]*m[14]*m[7] - m[13]*m[10]*m[7] + m[13]*m[6]*m[11] - m[5]*m[14]*m[11] - m[9]*m[6]*m[15] + m[5]*m[10]*m[15];
	m2.m[4] = m[12]*m[10]*m[7] - m[8]*m[14]*m[7] - m[12]*m[6]*m[11] + m[4]*m[14]*m[11] + m[8]*m[6]*m[15] - m[4]*m[10]*m[15];
	m2.m[8] = m[8]*m[13]*m[7] - m[12]*m[9]*m[7] + m[12]*m[5]*m[11] - m[4]*m[13]*m[11] - m[8]*m[5]*m[15] + m[4]*m[9]*m[15];
	m2.m[12] = m[12]*m[9]*m[6] - m[8]*m[13]*m[6] - m[12]*m[5]*m[10] + m[4]*m[13]*m[10] + m[8]*m[5]*m[14] - m[4]*m[9]*m[14];
	m2.m[1] = m[13]*m[10]*m[3] - m[9]*m[14]*m[3] - m[13]*m[2]*m[11] + m[1]*m[14]*m[11] + m[9]*m[2]*m[15] - m[1]*m[10]*m[15];
	m2.m[5] = m[8]*m[14]*m[3] - m[12]*m[10]*m[3] + m[12]*m[2]*m[11] - m[0]*m[14]*m[11] - m[8]*m[2]*m[15] + m[0]*m[10]*m[15];
	m2.m[9] = m[12]*m[9]*m[3] - m[8]*m[13]*m[3] - m[12]*m[1]*m[11] + m[0]*m[13]*m[11] + m[8]*m[1]*m[15] - m[0]*m[9]*m[15];
	m2.m[13] = m[8]*m[13]*m[2] - m[12]*m[9]*m[2] + m[12]*m[1]*m[10] - m[0]*m[13]*m[10] - m[8]*m[1]*m[14] + m[0]*m[9]*m[14];
	m2.m[2] = m[5]*m[14]*m[3] - m[13]*m[6]*m[3] + m[13]*m[2]*m[7] - m[1]*m[14]*m[7] - m[5]*m[2]*m[15] + m[1]*m[6]*m[15];
	m2.m[6] = m[12]*m[6]*m[3] - m[4]*m[14]*m[3] - m[12]*m[2]*m[7] + m[0]*m[14]*m[7] + m[4]*m[2]*m[15] - m[0]*m[6]*m[15];
	m2.m[10] = m[8]*m[13]*m[3] - m[12]*m[5]*m[3] + m[12]*m[1]*m[7] - m[0]*m[13]*m[7] - m[4]*m[1]*m[15] + m[0]*m[5]*m[15];
	m2.m[14] = m[12]*m[5]*m[2] - m[4]*m[13]*m[2] - m[12]*m[1]*m[6] + m[0]*m[13]*m[6] + m[4]*m[1]*m[14] - m[0]*m[5]*m[14];
	m2.m[3] = m[9]*m[6]*m[3] - m[5]*m[10]*m[3] - m[9]*m[2]*m[7] + m[1]*m[10]*m[7] + m[5]*m[2]*m[11] - m[1]*m[6]*m[11];
	m2.m[7] = m[4]*m[10]*m[3] - m[8]*m[6]*m[3] + m[8]*m[2]*m[7] - m[0]*m[10]*m[7] - m[4]*m[2]*m[11] + m[0]*m[6]*m[11];
	m2.m[11] = m[8]*m[5]*m[3] - m[4]*m[9]*m[3] - m[8]*m[1]*m[7] + m[0]*m[9]*m[7] + m[4]*m[1]*m[11] - m[0]*m[5]*m[11];
	m2.m[15] = m[4]*m[9]*m[2] - m[8]*m[5]*m[2] + m[8]*m[1]*m[6] - m[0]*m[9]*m[6] - m[4]*m[1]*m[10] + m[0]*m[5]*m[10];
	m2*( 1 / m1.Determinant() );

	return m2;
}

Quaternion Matrix::ConvertToQuaternion() const
{
    Quaternion q;
    
    float tr = m[0] + m[5] + m[10];
    
    if (tr > 0) { 
          float S = sqrt(tr+1.0) * 2; // S=4*qw 
          q.w = 0.25 * S;
          q.x = (m[9] - m[6]) / S;
          q.y = (m[2] - m[8]) / S; 
          q.z = (m[4] - m[1]) / S; 
        } else if ((m[0] > m[5])&(m[0] > m[10])) { 
          float S = sqrt(1.0 + m[0] - m[5] - m[10]) * 2; // S=4*qx 
          q.w = (m[9] - m[6]) / S;
          q.x = 0.25 * S;
          q.y = (m[1] + m[4]) / S; 
          q.z = (m[2] + m[8]) / S; 
        } else if (m[5] > m[10]) { 
          float S = sqrt(1.0 + m[5] - m[0] - m[10]) * 2; // S=4*qy
          q.w = (m[2] - m[8]) / S;
          q.x = (m[1] + m[4]) / S; 
          q.y = 0.25 * S;
          q.z = (m[6] + m[9]) / S; 
        } else { 
          float S = sqrt(1.0 + m[10] - m[0] - m[5]) * 2; // S=4*qz
          q.w = (m[4] - m[1]) / S;
          q.x = (m[2] + m[8]) / S;
          q.y = (m[6] + m[9]) / S;
          q.z = 0.25 * S;
        }   
    return q;
}

Matrix Matrix::PerspectiveMatrixLH(const float &fov, const float &aspect, const float &near, const float &far)
{
    
    const float h = (float)(1.0f/tan(fov*PI/360));
    float neg_depth = far-near;
    
    Matrix mp;

    mp.m[0] = h/aspect;     mp.m[1] = 0;     mp.m[2] = 0;                      mp.m[3] = 0;
    mp.m[4] = 0;            mp.m[5] = h;     mp.m[6] = 0;                      mp.m[7] = 0;
    mp.m[8] = 0;            mp.m[9] = 0;     mp.m[10] = (far+near)/neg_depth;   mp.m[11] = 2*(near+far)/neg_depth;
    mp.m[12] = 0;            mp.m[13] = 0;     mp.m[14] = -1;                     mp.m[15] = 0;
    
    return mp;
}
Matrix Matrix::PerspectiveMatrixRH(const float &fov, const float &aspect, const float &near, const float &far)
{
    
    const float h = (float)(1.0f/tan(fov*PI/360));
    float neg_depth = near-far;
    
    Matrix mp;    
    mp.m[0] = h/aspect;  mp.m[4] = 0;  mp.m[8] = 0;   mp.m[12] = 0;
    mp.m[1] = 0;  mp.m[5] = h;  mp.m[9] = 0;   mp.m[13] = 0;
    mp.m[2] = 0;  mp.m[6] = 0;  mp.m[10] = (far+near)/neg_depth;   mp.m[14] = 2*(near+far)/neg_depth;
    mp.m[3] = 0;  mp.m[7] = 0;  mp.m[11] = -1;   mp.m[15] = 0;
    
    return mp;
}
Matrix Matrix::MakeFrustum(const float &left, const float &right, const float &bottom, const float &top, const float &near, const float &far)
{
    float x = 2 * near / ( right - left );
    float y = 2 * near / ( top - bottom );

    float a = ( right + left ) / ( right - left );
    float b = ( top + bottom ) / ( top - bottom );
    float c = - ( far + near ) / ( far - near );
    float d = - 2 * far * near / ( far - near );

    Matrix mp;
    
    mp.m[0] = x;  mp.m[4] = 0;  mp.m[8] = 0;   mp.m[12] = 0;
    mp.m[1] = 0;  mp.m[5] = y;  mp.m[9] = 0;   mp.m[13] = 0;
    mp.m[2] = a;  mp.m[6] = b;  mp.m[10] = c;   mp.m[14] = d;
    mp.m[3] = 0;  mp.m[7] = 0;  mp.m[11] = -1;   mp.m[15] = 0;

    return mp;
}
Matrix Matrix::OrthoMatrixLH(const float &left, const float &right, const float &bottom, const float &top, const float &near, const float &far)
{
       Matrix mo;

       mo.m[0] = 2/(right-left);             mo.m[4] = 0;                       mo.m[8] = 0;                 mo.m[12] = -(right+left)/(right-left);
       mo.m[1] = 0;                          mo.m[5] = 2/(top-bottom);       mo.m[9] = 0;                 mo.m[13] = -(top+bottom)/(top-bottom);
       mo.m[2] = 0;                          mo.m[6] = 0;                        mo.m[10] = -2/(near-far);    mo.m[14] = -(far+near)/(near-far);
       mo.m[3] = 0;                          mo.m[7] = 0;                        mo.m[11] = 0;                 mo.m[15] = 1;
       
       return mo;
}
Matrix Matrix::OrthoMatrixRH(const float &left, const float &right, const float &bottom, const float &top, const float &near, const float &far)
{
       Matrix mo;

       mo.m[0] = 2/(right-left);             mo.m[4] = 0;                       mo.m[8] = 0;                 mo.m[12] = -(right+left)/(right-left);
       mo.m[1] = 0;                          mo.m[5] = 2/(top-bottom);       mo.m[9] = 0;                 mo.m[13] = -(top+bottom)/(top-bottom);
       mo.m[2] = 0;                          mo.m[6] = 0;                        mo.m[10] = -2/(far-near);    mo.m[14] = -(far+near)/(far-near);
       mo.m[3] = 0;                          mo.m[7] = 0;                        mo.m[11] = 0;                 mo.m[15] = 1;
       
       return mo;
}
Matrix Matrix::operator*(const Matrix &mo) const
{    
    Matrix self,mxm;
    
    self=(*this);

    float a11 = self.m[0], a12 = self.m[4], a13 = self.m[8], a14 = self.m[12],
    a21 = self.m[1], a22 = self.m[5], a23 = self.m[9], a24 = self.m[13],
    a31 = self.m[2], a32 = self.m[6], a33 = self.m[10], a34 = self.m[14],
    a41 = self.m[3], a42 = self.m[7], a43 = self.m[11], a44 = self.m[15],

    b11 = mo.m[0], b12 = mo.m[4], b13 = mo.m[8], b14 = mo.m[12],
    b21 = mo.m[1], b22 = mo.m[5], b23 = mo.m[9], b24 = mo.m[13],
    b31 = mo.m[2], b32 = mo.m[6], b33 = mo.m[10], b34 = mo.m[14],
    b41 = mo.m[3], b42 = mo.m[7], b43 = mo.m[11], b44 = mo.m[15];

    mxm.m[0] = a11 * b11 + a12 * b21 + a13 * b31 + a14 * b41;
    mxm.m[4] = a11 * b12 + a12 * b22 + a13 * b32 + a14 * b42;
    mxm.m[8] = a11 * b13 + a12 * b23 + a13 * b33 + a14 * b43;
    mxm.m[12] = a11 * b14 + a12 * b24 + a13 * b34 + a14 * b44;

    mxm.m[1] = a21 * b11 + a22 * b21 + a23 * b31 + a24 * b41;
    mxm.m[5] = a21 * b12 + a22 * b22 + a23 * b32 + a24 * b42;
    mxm.m[9] = a21 * b13 + a22 * b23 + a23 * b33 + a24 * b43;
    mxm.m[13] = a21 * b14 + a22 * b24 + a23 * b34 + a24 * b44;

    mxm.m[2] = a31 * b11 + a32 * b21 + a33 * b31 + a34 * b41;
    mxm.m[6] = a31 * b12 + a32 * b22 + a33 * b32 + a34 * b42;
    mxm.m[10] = a31 * b13 + a32 * b23 + a33 * b33 + a34 * b43;
    mxm.m[14] = a31 * b14 + a32 * b24 + a33 * b34 + a34 * b44;

    mxm.m[3] = a41 * b11 + a42 * b21 + a43 * b31 + a44 * b41;
    mxm.m[7] = a41 * b12 + a42 * b22 + a43 * b32 + a44 * b42;
    mxm.m[11] = a41 * b13 + a42 * b23 + a43 * b33 + a44 * b43;
    mxm.m[15] = a41 * b14 + a42 * b24 + a43 * b34 + a44 * b44;
    return mxm;

}
Matrix Matrix::operator*(const float &f) const
{
    Matrix mat;
    for (int i=0;i<16;i++)        
            mat.m[i]=m[i]*f;

    return mat;
}
vec3 Matrix::operator *(const vec3& v) const
{
        return vec3(
                m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12],
                m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
                m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]
                );
}
vec4 Matrix::operator *(const vec4& v) const
{
        return vec4(
                m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12] * v.w,
                m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13] * v.w,
                m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w,
                m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w
                );
}
void Matrix::operator*=(const Matrix &m)
{
    *this = *this * m;
}

bool Matrix::operator==(const Matrix& m) const
{
    return (
                this->m[0] == m.m[0] && this->m[4] == m.m[4] && this->m[8] == m.m[8] && this->m[12] == m.m[12] &&
                this->m[1] == m.m[1] && this->m[5] == m.m[5] && this->m[9] == m.m[9] && this->m[13] == m.m[13] &&
                this->m[2] == m.m[2] && this->m[6] == m.m[6] && this->m[10] == m.m[10] && this->m[14] == m.m[14] &&
                this->m[3] == m.m[3] && this->m[7] == m.m[7] && this->m[11] == m.m[11] && this->m[15] == m.m[15]);
}

bool Matrix::operator!=(const Matrix& m) const
{
    return (
                this->m[0] != m.m[0] || this->m[4] != m.m[4] || this->m[8] != m.m[8] || this->m[12] != m.m[12] ||
                this->m[1] != m.m[1] || this->m[5] != m.m[5] || this->m[9] != m.m[9] || this->m[13] != m.m[13] ||
                this->m[2] != m.m[2] || this->m[6] != m.m[6] || this->m[10] != m.m[10] || this->m[14] != m.m[14] ||
                this->m[3] != m.m[3] || this->m[7] != m.m[7] || this->m[11] != m.m[11] || this->m[15] != m.m[15]);
}

std::string Matrix::toString() const {
       std::ostringstream toStr;

       toStr << m[0] << "| " << m[4] << " | " << m[8] << " | " << m[12] << " |\n" <<
                m[1] << "| " << m[5] << " | " << m[9] << " | " << m[13] << " |\n" <<
                m[2] << "| " << m[6] << " | " << m[10] << " | " << m[14] << " |\n" <<
                m[3] << "| " << m[7] << " | " << m[11] << " | " << m[15] << " |";

       return "Matrix\n" + toStr.str();
}

const Matrix Matrix::BIAS = Matrix(0.5,0.0,0.0,0.0,0.0,0.5,0.0,0.0,0.0,0.0,0.5,0.0,0.5,0.5,0.5,1.0);

}
