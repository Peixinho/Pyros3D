//============================================================================
// Name        : Matrix.cpp
// Author      : Duarm Peixinho
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

#include <Pyros3D/Core/Math/Matrix.h>
#include <Pyros3D/Core/Math/Quaternion.h>

namespace p3d {
    
    namespace Math {

        Matrix::Matrix()
        {
            identity();
        }

        Matrix::Matrix(const f32 n11, const f32 n21, const f32 n31, const f32 n41, const f32 n12, const f32 n22, const f32 n32, const f32 n42, const f32 n13, const f32 n23, const f32 n33, const f32 n43, const f32 n14, const f32 n24, const f32 n34, const f32 n44)
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

        void Matrix::LookAt(const Vec3 &eye, const Vec3 &center, const Vec3 &up)
        {
            
            Vec3 zAxis = (eye - center).normalize();
            Vec3 xAxis = up.cross(zAxis).normalize();
            Vec3 yAxis = zAxis.cross(xAxis);
            
            if (xAxis.magnitude()==0)
            {
                zAxis.x = 0.0001f;
                xAxis = up.cross(zAxis).normalize();
            }
            
            m[0] = xAxis.x;                  m[1] = yAxis.x;                      m[2] = zAxis.x;                  m[3] = 0;
            m[4] = xAxis.y;                  m[5] = yAxis.y;                      m[6] = zAxis.y;                  m[7] = 0;
            m[8] = xAxis.z;                  m[9] = yAxis.z;                      m[10] = zAxis.z;                  m[11] = 0;
            m[12] = xAxis.dotProduct(eye.negate());   m[13] = yAxis.dotProduct(eye.negate());       m[14] = zAxis.dotProduct(eye.negate());   m[15] = 1;
        }
        void Matrix::LookAt(const Vec3 &eye, const Vec3 &center)
        {
            Vec3 dir = center - eye;
            Vec3 up;
            if (dir.y>0.99 || dir.y<-0.99)
            {
                up = Vec3(0,0,1);
            } else 
            {
                up = Vec3(0,1,0);
            }
            LookAt(eye,center,up);
        }
        
        void Matrix::Translate(const f32 x, const f32 y, const f32 z)
        {
            m[12] = x; m[13] = y; m[14] = z;
        }
        void Matrix::Translate(const Vec3 &xyz)
        {
            m[12] = xyz.x; m[13] = xyz.y; m[14] = xyz.z;
        }
        void Matrix::TranslateX(const f32 x)
        {
            m[12] = x;
        }
        void Matrix::TranslateY(const f32 y)
        {
            m[13] = y;
        }
        void Matrix::TranslateZ(const f32 z)
        {
            m[14] = z;
        }
        Vec3 Matrix::GetTranslation() const
        {
            return Vec3(m[12],m[13],m[14]);
        }
        Vec3 Matrix::GetScale() const
        {
            return Vec3(m[0],m[5],m[10]);
        }
        void Matrix::RotationX(const f32 angle)
        {    
            f32 c = cos( angle ), s = sin( angle );

            Matrix mrot;
            mrot.identity();

            mrot.m[5] = mrot.m[10] = c;
            mrot.m[6] = s;
            mrot.m[9] = -mrot.m[6];
            *this*=mrot;
        }
        void Matrix::RotationY(const f32 angle)
        {

            f32 c = cos( angle ), s = sin( angle );

            Matrix mrot;
            mrot.identity();
            mrot.m[0] = mrot.m[10] = c;
            mrot.m[8] = s;
            mrot.m[2] = -mrot.m[8];
            *this*=mrot;
        }
        void Matrix::RotationZ(const f32 angle)
        {
            f32 c = cos( angle ), s = sin( angle );

            Matrix mrot;
            mrot.identity();
            mrot.m[0] = mrot.m[5] = c;
            mrot.m[1] = s;
            mrot.m[4] = -mrot.m[1];
            *this*=mrot;
        }
		void Matrix::ForceScale(const f32 sx, const f32 sy, const f32 sz)
		{
			m[0] = sx;
			m[5] = sy;
			m[10] = sz;
			m[15] = 1.0f;
		}
		void Matrix::ForceScale(const Vec3 &xyz)
		{
			m[0] = xyz.x;
			m[5] = xyz.y;
			m[10] = xyz.z;
			m[15] = 1.0f;
		}
        void Matrix::Scale(const f32 sx, const f32 sy, const f32 sz)
        {    
             m[0] *= sx;
             m[5] *= sy;
             m[10] *= sz;
             m[15] *= 1.0f;
        }
        void Matrix::Scale(const Vec3 &xyz)
        {
             m[0] *= xyz.x;
             m[5] *= xyz.y;
             m[10] *= xyz.z;
             m[15] *= 1.0f;
        }
        void Matrix::ScaleX(const f32 x)
        {
            m[0] *= x;
        }
        void Matrix::ScaleY(const f32 y)
        {
            m[5] *= y;
        }
        void Matrix::ScaleZ(const f32 z)
        {
            m[10] *= z;
        }
        Matrix Matrix::GetRotation(const Vec3 &s) const
        {

            f32 invScaleX=1/s.x, invScaleY=1/s.y, invScaleZ=1/s.z;

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

        f32 Matrix::Determinant() const
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

                Matrix m2;

                f32 n11 = m[ 0 ], n12 = m[ 4 ], n13 = m[ 8 ], n14 = m[ 12 ];
                f32 n21 = m[ 1 ], n22 = m[ 5 ], n23 = m[ 9 ], n24 = m[ 13 ];
                f32 n31 = m[ 2 ], n32 = m[ 6 ], n33 = m[ 10 ], n34 = m[ 14 ];
                f32 n41 = m[ 3 ], n42 = m[ 7 ], n43 = m[ 11 ], n44 = m[ 15 ];


                m2.m[ 0 ] = n23 * n34 * n42 - n24 * n33 * n42 + n24 * n32 * n43 - n22 * n34 * n43 - n23 * n32 * n44 + n22 * n33 * n44;
                m2.m[ 4 ] = n14 * n33 * n42 - n13 * n34 * n42 - n14 * n32 * n43 + n12 * n34 * n43 + n13 * n32 * n44 - n12 * n33 * n44;                
                m2.m[ 8 ] = n13 * n24 * n42 - n14 * n23 * n42 + n14 * n22 * n43 - n12 * n24 * n43 - n13 * n22 * n44 + n12 * n23 * n44;
                m2.m[ 12 ] = n14 * n23 * n32 - n13 * n24 * n32 - n14 * n22 * n33 + n12 * n24 * n33 + n13 * n22 * n34 - n12 * n23 * n34;
                m2.m[ 1 ] = n24 * n33 * n41 - n23 * n34 * n41 - n24 * n31 * n43 + n21 * n34 * n43 + n23 * n31 * n44 - n21 * n33 * n44;
                m2.m[ 5 ] = n13 * n34 * n41 - n14 * n33 * n41 + n14 * n31 * n43 - n11 * n34 * n43 - n13 * n31 * n44 + n11 * n33 * n44;
                m2.m[ 9 ] = n14 * n23 * n41 - n13 * n24 * n41 - n14 * n21 * n43 + n11 * n24 * n43 + n13 * n21 * n44 - n11 * n23 * n44;
                m2.m[ 13 ] = n13 * n24 * n31 - n14 * n23 * n31 + n14 * n21 * n33 - n11 * n24 * n33 - n13 * n21 * n34 + n11 * n23 * n34;
                m2.m[ 2 ] = n22 * n34 * n41 - n24 * n32 * n41 + n24 * n31 * n42 - n21 * n34 * n42 - n22 * n31 * n44 + n21 * n32 * n44;
                m2.m[ 6 ] = n14 * n32 * n41 - n12 * n34 * n41 - n14 * n31 * n42 + n11 * n34 * n42 + n12 * n31 * n44 - n11 * n32 * n44;
                m2.m[ 10 ] = n12 * n24 * n41 - n14 * n22 * n41 + n14 * n21 * n42 - n11 * n24 * n42 - n12 * n21 * n44 + n11 * n22 * n44;
                m2.m[ 14 ] = n14 * n22 * n31 - n12 * n24 * n31 - n14 * n21 * n32 + n11 * n24 * n32 + n12 * n21 * n34 - n11 * n22 * n34;
                m2.m[ 3 ] = n23 * n32 * n41 - n22 * n33 * n41 - n23 * n31 * n42 + n21 * n33 * n42 + n22 * n31 * n43 - n21 * n32 * n43;
                m2.m[ 7 ] = n12 * n33 * n41 - n13 * n32 * n41 + n13 * n31 * n42 - n11 * n33 * n42 - n12 * n31 * n43 + n11 * n32 * n43;
                m2.m[ 11 ] = n13 * n22 * n41 - n12 * n23 * n41 - n13 * n21 * n42 + n11 * n23 * n42 + n12 * n21 * n43 - n11 * n22 * n43;
                m2.m[ 15 ] = n12 * n23 * n31 - n13 * n22 * n31 + n13 * n21 * n32 - n11 * n23 * n32 - n12 * n21 * n33 + n11 * n22 * n33;

                f32 det = n11 * m2.m[ 0 ] + n21 * m2.m[ 4 ] + n31 * m2.m[ 8 ] + n41 * m2.m[ 12 ];

                if ( det == 0 ) {
                    echo("ERROR: Can't invert matrix, determinant is 0");
                    m2.identity();
                }

                m2 = m2 * ( 1 / det );

                return m2;
        }

        Quaternion Matrix::ConvertToQuaternion() const
        {
            Quaternion q;

                f32	m11 = m[0], m12 = m[4], m13 = m[8],
                                m21 = m[1], m22 = m[5], m23 = m[9],
                                m31 = m[2], m32 = m[6], m33 = m[10],

                                echo = m11 + m22 + m33, s;

                if ( echo > 0 ) 
                {

                        s = 0.5f / sqrtf( echo + 1.0f );

                        q.w = 0.25f / s;
                        q.x = ( m32 - m23 ) * s;
                        q.y = ( m13 - m31 ) * s;
                        q.z = ( m21 - m12 ) * s;

                } else if ( m11 > m22 && m11 > m33 ) {

                        s = 2.0f * sqrtf( 1.0f + m11 - m22 - m33 );

                        q.w = (m32 - m23 ) / s;
                        q.x = 0.25f * s;
                        q.y = (m12 + m21 ) / s;
                        q.z = (m13 + m31 ) / s;

                } else if ( m22 > m33 ) {

                        s = 2.0f * sqrtf( 1.0f + m22 - m11 - m33 );

                        q.w = (m13 - m31 ) / s;
                        q.x = (m12 + m21 ) / s;
                        q.y = 0.25f * s;
                        q.z = (m23 + m32 ) / s;

                } else {

                        s = 2.0f * sqrtf( 1.0f + m33 - m11 - m22 );

                        q.w = ( m21 - m12 ) / s;
                        q.x = ( m13 + m31 ) / s;
                        q.y = ( m23 + m32 ) / s;
                        q.z = 0.25f * s;

                }

            return q;
        }

        Matrix Matrix::PerspectiveMatrix(const f32 fov, const f32 aspect, const f32 near, const f32 far)
        {    
            const f32 h = (f32)(1.0f/tan(fov*PI/360.f));
            f32 neg_depth = near-far;

            Matrix mp;    
            mp.m[0] = h/aspect;  mp.m[4] = 0.f;  mp.m[8] = 0.f;   mp.m[12] = 0.f;
            mp.m[1] = 0.f;  mp.m[5] = h;  mp.m[9] = 0.f;   mp.m[13] = 0.f;
            mp.m[2] = 0.f;  mp.m[6] = 0.f;  mp.m[10] = (far+near)/neg_depth;   mp.m[14] = 2.0f*(near*far)/neg_depth;
            mp.m[3] = 0.f;  mp.m[7] = 0.f;  mp.m[11] = -1.f;   mp.m[15] = 0.f;
            
            return mp;
        }
        Matrix Matrix::MakeFrustum(const f32 left, const f32 right, const f32 bottom, const f32 top, const f32 near, const f32 far)
        {
            f32 x = 2.f * near / ( right - left );
            f32 y = 2.f * near / ( top - bottom );

            f32 a = ( right + left ) / ( right - left );
            f32 b = ( top + bottom ) / ( top - bottom );
            f32 c = - ( far + near ) / ( far - near );
            f32 d = - 2.f * far * near / ( far - near );

            Matrix mp;

            mp.m[0] = x;  mp.m[4] = 0.f;  mp.m[8] = 0.f;   mp.m[12] = 0.f;
            mp.m[1] = 0.f;  mp.m[5] = y;  mp.m[9] = 0.f;   mp.m[13] = 0.f;
            mp.m[2] = a;  mp.m[6] = b;  mp.m[10] = c;   mp.m[14] = d;
            mp.m[3] = 0.f;  mp.m[7] = 0.f;  mp.m[11] = -1.f;   mp.m[15] = 0.f;
            
            return mp;
        }
        Matrix Matrix::OrthoMatrix(const f32 left, const f32 right, const f32 bottom, const f32 top, const f32 near, const f32 far)
        {
            Matrix mo;

            mo.m[0] = 2.f/(right-left);             mo.m[4] = 0.f;                       mo.m[8] = 0.f;                 mo.m[12] = -(right+left)/(right-left);
            mo.m[1] = 0.f;                          mo.m[5] = 2.f/(top-bottom);       mo.m[9] = 0.f;                 mo.m[13] = -(top+bottom)/(top-bottom);
            mo.m[2] = 0.f;                          mo.m[6] = 0.f;                        mo.m[10] = -2.f/(far-near);    mo.m[14] = -(far+near)/(far-near);
            mo.m[3] = 0.f;                          mo.m[7] = 0.f;                        mo.m[11] = 0.f;                 mo.m[15] = 1.f;
               
            return mo;
        }
        Matrix Matrix::operator*(const Matrix &mo) const
        {    
            Matrix self,mxm;

            self=(*this);

            f32 a11 = self.m[0], a12 = self.m[4], a13 = self.m[8], a14 = self.m[12],
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
        Matrix Matrix::operator*(const f32 f) const
        {
            Matrix mat;
            for (int i=0;i<16;i++)        
                    mat.m[i]=m[i]*f;

            return mat;
        }
        Vec3 Matrix::operator *(const Vec3& v) const
        {
                return Vec3(
                        m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12],
                        m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
                        m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]
                        );
        }
        Vec4 Matrix::operator *(const Vec4& v) const
        {
                return Vec4(
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

               toStr << m[0] << "| " << m[1] << " | " << m[2] << " | " << m[3] << " |\n" <<
                        m[4] << "| " << m[5] << " | " << m[6] << " | " << m[7] << " |\n" <<
                        m[8] << "| " << m[9] << " | " << m[10] << " | " << m[11] << " |\n" <<
                        m[12] << "| " << m[13] << " | " << m[14] << " | " << m[15] << " |";

               return "Matrix\n" + toStr.str();
        }
        void Matrix::SetRotationFromEuler(const Vec3 &rotation, const uint32 order) 
        {
                f32 x = rotation.x, y = rotation.y, z = rotation.z;
                f32 a = cosf( x ), b = sinf( x );
                f32 c = cosf( y ), d = sinf( y );
                f32 e = cosf( z ), f = sinf( z );

                switch (order)
                {
                        case RotationOrder::XYZ:
                        {
                                f32 ae = a * e, af = a * f, be = b * e, bf = b * f;

                                m[0] = c * e;
                                m[4] = - c * f;
                                m[8] = d;

                                m[1] = af + be * d;
                                m[5] = ae - bf * d;
                                m[9] = - b * c;

                                m[2] = bf - ae * d;
                                m[6] = be + af * d;
                                m[10] = a * c;
                        }
                        break;
                        case RotationOrder::YXZ:
                        {
                                f32 ce = c * e, cf = c * f, de = d * e, df = d * f;

                                m[0] = ce + df * b;
                                m[4] = de * b - cf;
                                m[8] = a * d;

                                m[1] = a * f;
                                m[5] = a * e;
                                m[9] = - b;

                                m[2] = cf * b - de;
                                m[6] = df + ce * b;
                                m[10] = a * c;
                        }
                        break;
                        case RotationOrder::ZXY:
                        {
                                f32 ce = c * e, cf = c * f, de = d * e, df = d * f;

                                m[0] = ce - df * b;
                                m[4] = - a * f;
                                m[8] = de + cf * b;

                                m[1] = cf + de * b;
                                m[5] = a * e;
                                m[9] = df - ce * b;

                                m[2] = - a * d;
                                m[6] = b;
                                m[10] = a * c;
                        }
                        break;
                        case RotationOrder::ZYX:
                        {
                                f32 ae = a * e, af = a * f, be = b * e, bf = b * f;

                                m[0] = c * e;
                                m[4] = be * d - af;
                                m[8] = ae * d + bf;

                                m[1] = c * f;
                                m[5] = bf * d + ae;
                                m[9] = af * d - be;

                                m[2] = - d;
                                m[6] = b * c;
                                m[10] = a * c;
                        }
                        break;
                        case RotationOrder::YZX:
                        {
                                f32 ac = a * c, ad = a * d, bc = b * c, bd = b * d;

                                m[0] = c * e;
                                m[4] = bd - ac * f;
                                m[8] = bc * f + ad;

                                m[1] = f;
                                m[5] = a * e;
                                m[9] = - b * e;

                                m[2] = - d * e;
                                m[6] = ad * f + bc;
                                m[10] = ac - bd * f;
                        }
                        break;
                        case RotationOrder::XZY:
                        {
                                f32 ac = a * c, ad = a * d, bc = b * c, bd = b * d;

                                m[0] = c * e;
                                m[4] = - f;
                                m[8] = d * e;

                                m[1] = ac * f + bd;
                                m[5] = a * e;
                                m[9] = ad * f - bc;

                                m[2] = bc * f - ad;
                                m[6] = b * e;
                                m[10] = bd * f + ac;
                        }
                        break;
                };

        }
        Vec3 Matrix::GetEulerFromRotationMatrix(const uint32 order)
        {

                Vec3 euler;

                // assumes the upper 3x3 of m is a pure rotation matrix (i.e, unscaled)

                // Clamp, to handle numerical problems

                f32 m11 = m[0], m12 = m[4], m13 = m[8];
                f32 m21 = m[1], m22 = m[5], m23 = m[9];
                f32 m31 = m[2], m32 = m[6], m33 = m[10];

                switch(order)
                {
                        case RotationOrder::XYZ:
                        {
                            euler.y = asinf( Clamp( m13 ) );

                            if ( fabs( m13 ) < 0.99999 ) 
                            {

                                euler.x = atan2f( - m23, m33 );
                                euler.z = atan2f( - m12, m11 );

                            } else {

                                euler.x = atan2f( m32, m22 );
                                euler.z = 0.f;

                            }
                        }
                        break;
                        case RotationOrder::YXZ:
                        {
                            euler.x = asinf( - Clamp( m23 ) );

                            if ( fabs( m23 ) < 0.99999 ) {

                                euler.y = atan2f( m13, m33 );
                                euler.z = atan2f( m21, m22 );

                            } else {

                                euler.y = atan2f( - m31, m11 );
                                euler.z = 0.f;

                            }
                        }
                        break;
                        case RotationOrder::ZXY:
                        {
                            euler.x = asinf( Clamp( m32 ) );

                            if ( fabs( m32 ) < 0.99999 ) 
                            {

                                euler.y = atan2f( - m31, m33 );
                                euler.z = atan2f( - m12, m22 );

                            } else {

                                euler.y = 0.f;
                                euler.z = atan2f( m21, m11 );

                            }
                        }
                        break;
                        case RotationOrder::ZYX:
                        {
                            euler.y = asinf( - Clamp( m31 ) );

                            if ( fabs( m31 ) < 0.99999 ) 
                            {

                                euler.x = atan2f( m32, m33 );
                                euler.z = atan2f( m21, m11 );

                            } else {

                                euler.x = 0.f;
                                euler.z = atan2f( - m12, m22 );

                            }
                        }
                        break;
                        case RotationOrder::YZX:
                        {
                            euler.z = asinf( Clamp( m21 ) );

                            if ( fabs( m21 ) < 0.99999 ) 
                            {

                                euler.x = atan2f( - m23, m22 );
                                euler.y = atan2f( - m31, m11 );

                            } else {

                                euler.x = 0.f;
                                euler.y = atan2f( m13, m33 );

                            }
                        }
                        break;
                        case RotationOrder::XZY:
                        {
                            euler.z = asinf( - Clamp( m12 ) );

                            if ( fabs( m12 ) < 0.99999 ) 
                            {

                                euler.x = atan2f( m32, m22 );
                                euler.y = atan2f( m13, m11 );

                            } else {

                                euler.x = atan2f( - m23, m33 );
                                euler.y = 0.f;

                            }
                        }
                        break;
                }

                return euler;

        }

        const Matrix Matrix::BIAS = Matrix(0.5f,0.0f,0.0f,0.0f,0.0f,0.5f,0.0f,0.0f,0.0f,0.0f,0.5f,0.0f,0.5f,0.5f,0.5f,1.0f);
    };
};