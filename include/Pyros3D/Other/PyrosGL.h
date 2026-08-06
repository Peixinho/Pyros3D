//============================================================================
// Name        : PyrosGL.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : PyrosGL
//============================================================================

#ifndef PYROSGL_H
#define PYROSGL_H

#if defined(GLES3)
	#if defined(DESKTOP) || defined(EMSCRIPTEN)
		#include <Pyros3D/Ext/gles3/glad/glad.h>
	#else
		#include <GLES3/gl3.h>
		#include <GLES3/gl3ext.h>
	#endif
#endif

#if defined(GL45)
    #include <Pyros3D/Ext/gl45/glad/glad.h>
#endif

#if defined(GL42)
    #include <Pyros3D/Ext/gl42/glad/glad.h>
#endif

#if defined(GL41)
    #include <Pyros3D/Ext/gl41/glad/glad.h>
#endif


#include <Pyros3D/Other/Global.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <sstream>

namespace p3d {

	inline const char* GLErrorString(const GLenum error)
	{
		switch (error)
		{
			case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
			case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
			case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
			case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
			case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
			default: return "UNKNOWN_GL_ERROR";
		}
	}

};

// Check GL
#if defined(_DEBUG) && !defined(EMSCRIPTEN)
#define GLCHECKER(caller) { \
	caller; \
	bool __glHadError = false; \
	for (GLenum __glErr = glGetError(); __glErr != GL_NO_ERROR; __glErr = glGetError()) { \
		__glHadError = true; \
		std::ostringstream __glMsg; \
		__glMsg << "GL Error: " << p3d::GLErrorString(__glErr) << " (0x" << std::hex << __glErr << std::dec << ")" \
			<< " FUNCTION: " << #caller << " LINE: " << __LINE__ << " FILE: " << __FILE__; \
		echo(__glMsg.str()); \
	} \
	if (__glHadError) { BRK; } \
}
#else
#define GLCHECKER(caller) { caller; }
#endif

#endif /* PYROSGL_H */
