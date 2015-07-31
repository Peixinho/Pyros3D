//============================================================================
// Name        : PyrosGL.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : PyrosGL
//============================================================================

#ifndef PYROSGL_H
#define PYROSGL_H

#include <Pyros3D/Core/Math/Math.h>

#if defined(GLES2)

    #include <GLES2/gl2.h>
    #include <GLES2/gl2ext.h>

#else

    #include "GL/glew.h"

#endif

#include <Pyros3D/Other/Global.h>

// Check GL
#ifdef _DEBUG
    #define GLCHECKER(caller) { caller; int error = glGetError(); if(error != GL_NO_ERROR) { std::cout <<  "GL Error: " << std::hex << error << "Function: " << #caller << "LINE: " << __LINE__ << " FILE: " << __FILE__ << std::endl; } }
#else
    #define GLCHECKER(caller) { caller; }
#endif

#endif /* PYROSGL_H */