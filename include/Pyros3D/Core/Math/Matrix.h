//============================================================================
// Name        : Matrix.h
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

#ifndef MATRIX_H
#define	MATRIX_H

#include "Math.h"
#include "../Logs/Log.h"
#include <string>

namespace p3d {
    
    namespace Math {

        class PYROS3D_API Quaternion;
        class PYROS3D_API Vec3;
        class PYROS3D_API Vec4;
        
        class PYROS3D_API Matrix {
            public:

                // vars
                f32 m[16];

                // methods
                Matrix();
                Matrix(const f32 n11,const f32 n21,const f32 n31,const f32 n41,const f32 n12,const f32 n22,const f32 n32,const f32 n42,const f32 n13,const f32 n23,const f32 n33,const f32 n43,const f32 n14,const f32 n24,const f32 n34,const f32 n44);
                void identity();
                void LookAt(const Vec3 &eye, const Vec3 &center, const Vec3 &up);
                void LookAt(const Vec3 &eye, const Vec3 &center);
                void Translate(const f32 &x, const f32 &y, const f32 &z);
                void Translate(const Vec3 &xyz);
                void TranslateX(const f32 &x);
                void TranslateY(const f32 &y);
                void TranslateZ(const f32 &z);
                Vec3 GetTranslation() const;
                void RotationX(const f32 &angle);
                void RotationY(const f32 &angle);
                void RotationZ(const f32 &angle);

                // Euler
                void SetRotationFromEuler(const Vec3 &rotation, const uint32 &order = 0);
                Vec3 GetEulerFromRotationMatrix(const uint32 &order = 0);

                Matrix GetRotation(const Vec3 &scale) const;
                void Scale(const f32 &sx, const f32 &sy, const f32 &sz);
                void Scale(const Vec3 &xyz);
                void ScaleX(const f32 &x);
                void ScaleY(const f32 &y);
                void ScaleZ(const f32 &z);
                Vec3 GetScale() const;

                // f32
                f32 Determinant() const;

                // matrix
                Matrix Transpose() const;
                Matrix Inverse() const;

                // RH
                static Matrix PerspectiveMatrix(const f32 &fov, const f32 &aspect, const f32 &near, const f32 &far);
                static Matrix OrthoMatrix(const f32 &left, const f32 &right, const f32 &bottom, const f32 &top, const f32 &near, const f32 &far);

                Quaternion ConvertToQuaternion() const;

                // operators
                Matrix operator*(const Matrix &m) const;
                Matrix operator*(const f32 &f) const;
                Vec3 operator*(const Vec3 &v) const;
                Vec4 operator*(const Vec4 &v) const;
                void operator*=(const Matrix &m);
                bool operator==(const Matrix &m) const;
                bool operator!=(const Matrix &m) const;

                // toString
                std::string toString() const;

                static const Matrix BIAS;

            private:

                static Matrix MakeFrustum(const f32 &left, const f32 &right, const f32 &bottom, const f32 &top, const f32 &near, const f32 &far);

        };

    };
};

#endif	/* MATRIX_H */
