//============================================================================
// Name        : Transform.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Transform
//============================================================================

#ifndef TRANSFORMATION_H
#define	TRANSFORMATION_H

#include "../Math/vec3.h"
#include "../Math/Matrix.h"

namespace Fishy3D {

class Transformation {
public:

    Transformation();    
    virtual ~Transformation();

    // updates transformations
    void update();

    // return inverse transformation
    Matrix getTransformationInverse();
    
    // returns matrix transformation
    Matrix getTransformationMatrix();

    // returns specific matrix
    Matrix getRotationMatrix();
    Matrix getTranslationMatrix() const;
    Matrix getScaleMatrix() const;
    
    // set positioning
    void SetX(const float &x);
    void SetY(const float &y);
    void SetZ(const float &z);
    const vec3 GetPosition() const;
    void SetPosition(const vec3 &position);
    // set rotations
    void SetRotation(const vec3 &rotation);
    void SetRotationMatrix(const Matrix &rotation);
    void RotationX(const float &angleX);
    void RotationY(const float &angleY);
    void RotationZ(const float &angleZ);
	const vec3 GetRotation() const;
    // set scale
    void ScaleX(const float &scaleX);
    void ScaleY(const float &scaleY);
    void ScaleZ(const float &scaleZ);
	void SetScale(const vec3 &scale);
	const vec3 GetScale() const;

    // get if transformation needs update
    bool IsDirty() const;

private:
   
    // Dirty Flag
    bool isDirty;
    
    // Calculate Rotation Matrix
    Matrix positionMatrix, rotationMatrix, rotationMatrixCustom, scaleMatrix;
    
	// Store Rotation data
	vec3 rotationValues;

    // Transformation Matrix
    Matrix TransformationMatrix;
    Matrix InverseTransformationMatrix;
};

}

#endif	/* TRANSFORMATION_H */
