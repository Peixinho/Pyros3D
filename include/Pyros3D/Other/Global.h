//============================================================================
// Name        : Global.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Global
//============================================================================

#ifndef GLOBAL_H
#define GLOBAL_H

// Fix Raspberry and other stupid devices
#if defined(GLES2)
// GLES 2
	#define __INDEX_TYPE__ 		GL_UNSIGNED_SHORT
	#define __INDEX_C_TYPE__ 	short
#else
// OpenGL
	#define __INDEX_TYPE__ 		GL_UNSIGNED_INT
	#define __INDEX_C_TYPE__ 	uint32
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define MAX_LIGHTS 				4
#define MAX_BONES 				25

#endif /* GLOBAL_H */