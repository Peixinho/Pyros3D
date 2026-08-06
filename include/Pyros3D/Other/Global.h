//============================================================================
// Name        : Global.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Global
//============================================================================

#ifndef GLOBAL_H
#define GLOBAL_H

#define __INDEX_TYPE__ 		GL_UNSIGNED_INT
#define __INDEX_C_TYPE__ 	uint32

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#if defined(__arm__) || defined(__aarch64__) || defined(__arm64__)
// __arm__ is 32-bit ARM only; 64-bit ARM (Apple Silicon, aarch64 Linux) needs
// its own check too - the old #ifdef __arm__ fell through to the x86-only
// "int3" branch below on arm64, which fails to even compile there.
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
