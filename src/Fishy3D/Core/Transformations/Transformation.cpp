//============================================================================
// Name        : Transform.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Transform
//============================================================================

#include "Transformation.h"

namespace Fishy3D {

    Transformation::Transformation() {}

    Transformation::~Transformation() {}

    Matrix Transformation::getTransformationMatrix()
    {
        // return transformation Matrix
        return TransformationMatrix;
    }
    Matrix Transformation::getRotationMatrix()
    {
		rotationMatrix.identity();
		rotationMatrix.RotationZ(rotationValues.z);
		rotationMatrix.RotationY(rotationValues.y);
		rotationMatrix.RotationX(rotationValues.x);
        return rotationMatrix;
    }
    Matrix Transformation::getScaleMatrix() const
    {        
        return scaleMatrix;
    }
    Matrix Transformation::getTranslationMatrix() const
    {
        return positionMatrix;
    }
    
    void Transformation::SetRotation(const vec3& rotation)
    {
        isDirty = true;

		rotationValues = rotation;
    }
    void Transformation::RotationX(const float &angleX)
    {
        isDirty = true;

		rotationValues.x = angleX;
    }
    void Transformation::RotationY(const float &angleY)
    {
        isDirty = true;

		rotationValues.y = angleY;
    }
    void Transformation::RotationZ(const float &angleZ)
    {
        isDirty = true;

		rotationValues.z = angleZ;
    }
	const vec3 Transformation::GetRotation() const
	{
		return rotationValues;
	}
    void Transformation::ScaleX(const float &scaleX)
    {
        isDirty = true;
        scaleMatrix.ScaleX(scaleX);
    }
    void Transformation::ScaleY(const float &scaleY)
    {
        isDirty = true;
        scaleMatrix.ScaleY(scaleY);
    }
    void Transformation::ScaleZ(const float &scaleZ)
    {
        isDirty = true;
        scaleMatrix.ScaleZ(scaleZ);
    }
	void Transformation::SetScale(const vec3 &scale)
	{
		isDirty = true;
		scaleMatrix.Scale(scale);
	}
    void Transformation::SetX(const float& x)
    {
        isDirty = true;
        positionMatrix.TranslateX(x);
    }
    void Transformation::SetY(const float& y)
    {
        isDirty = true;
        positionMatrix.TranslateY(y);
    }
    void Transformation::SetZ(const float& z)
    {
        isDirty = true;
        positionMatrix.TranslateZ(z);
    }
    void Transformation::SetPosition(const vec3 &position)
    {
        isDirty = true;
        positionMatrix.Translate(position);
    }
    const vec3 Transformation::GetPosition() const
    {        
        return positionMatrix.GetTranslation();
    }

    void Transformation::SetRotationMatrix(const Matrix& rotation)
    {
        isDirty = true;
        rotationMatrixCustom = rotation;
    }
    
	const vec3 Transformation::GetScale() const
	{
		return vec3(scaleMatrix.m[0],scaleMatrix.m[5], scaleMatrix.m[10]);
	}
    void Transformation::update()
    {
        if (isDirty)
        {
            Matrix transf;
            // apply translation
            transf*=positionMatrix;

            // apply rotation
			transf*=rotationMatrixCustom;
            transf*=getRotationMatrix();
            
            // apply scale
            transf *= scaleMatrix;

            // save transformation matrix
            TransformationMatrix = transf;
            
            isDirty = false;
        }
    }
    Matrix Transformation::getTransformationInverse()
    {
        if (isDirty==true)
        {
            update();
        }
        return TransformationMatrix.Inverse();
    }

    bool Transformation::IsDirty() const
    {
        return isDirty;
    }

}
