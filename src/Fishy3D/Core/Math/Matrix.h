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
#include "vec3.h"
#include "vec4.h"
#include "Quaternion.h"

namespace Fishy3D {

    class Quaternion;
    
    class Matrix {
        public:

            // vars
            float m[16];

            // methods
            Matrix();
            Matrix(const float n11,const float n21,const float n31,const float n41,const float n12,const float n22,const float n32,const float n42,const float n13,const float n23,const float n33,const float n43,const float n14,const float n24,const float n34,const float n44);
            void identity();
            void LookAtRH(const vec3 &eye, const vec3 &center, const vec3 &up);
            void LookAtLH(const vec3 &eye, const vec3 &center, const vec3 &up);
            void Translate(const float &x, const float &y, const float &z);
            void Translate(const vec3 &xyz);
            void TranslateX(const float &x);
            void TranslateY(const float &y);
            void TranslateZ(const float &z);
            vec3 GetTranslation() const;
            void RotationX(const float &angle);
            void RotationY(const float &angle);
            void RotationZ(const float &angle);
            Matrix GetRotation(const vec3 &scale) const;
            void Scale(const float &sx, const float &sy, const float &sz);
            void Scale(const vec3 &xyz);
            void ScaleX(const float &x);
            void ScaleY(const float &y);
            void ScaleZ(const float &z);

            // float
            float Determinant() const;

            // matrix
            Matrix Transpose() const;
            Matrix Inverse() const;
            
            // LH
            static Matrix PerspectiveMatrixLH(const float &fov, const float &aspect, const float &near, const float &far);
            static Matrix OrthoMatrixLH(const float &left, const float &right, const float &bottom, const float &top, const float &near, const float &far);
            // RH
            static Matrix PerspectiveMatrixRH(const float &fov, const float &aspect, const float &near, const float &far);
            static Matrix OrthoMatrixRH(const float &left, const float &right, const float &bottom, const float &top, const float &near, const float &far);

            Quaternion ConvertToQuaternion() const;
            
            // operators
            Matrix operator*(const Matrix &m) const;
            Matrix operator*(const float &f) const;
            vec3 operator*(const vec3 &v) const;
            vec4 operator*(const vec4 &v) const;
            void operator*=(const Matrix &m);
            bool operator==(const Matrix &m) const;
            bool operator!=(const Matrix &m) const;

            // toString
            std::string toString() const;
            
            static const Matrix BIAS;
            
    private:
        
            static Matrix MakeFrustum(const float &left, const float &right, const float &bottom, const float &top, const float &near, const float &far);
            
    };

}

#endif	/* MATRIX_H */
