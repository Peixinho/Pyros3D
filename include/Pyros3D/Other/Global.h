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

#ifdef __arm__
	#include <signal.h>
	#define BRK raise(SIGTRAP)
#elif defined(_WIN32)
	#define BRK __debugbreak()
#else
	#define BRK asm volatile("int3")
#endif

#ifndef ASSERT
#define ASSERT( x ){if( !(x) ){fprintf( stderr, "assert failed %s %d: %s\n", __FILE__, __LINE__, #x ); BRK;} }
#endif

#endif /* GLOBAL_H */
