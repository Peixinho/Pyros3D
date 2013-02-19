//============================================================================
// Name        : Renderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Renderer
//============================================================================

#ifndef MATH_H
#define	MATH_H

// Constants
#define EPSILON 1e-8 
#define PI 3.14159265358979323846
#define DEGTORAD(x)	( ((x) * PI) / 180.0 )
#define RADTODEG(x)	( ((x) * 180.0) / PI )

#define SQR(x) ( (x) * (x) )

#define LIMIT_RANGE(low, value, high) { if (value < low)	value = low; else if(value > high) value = high; }

#include "vec2.h"
#include "vec3.h"
#include "vec4.h"
#include "Matrix.h"
#include "Quaternion.h"

#endif	/* MATH_H */

