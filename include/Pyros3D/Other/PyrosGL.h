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

#if defined(GLES2_DESKTOP)
    #include <Pyros3D/Ext/gles2/glad/glad.h>
#else 
    #if defined(GLES3_DESKTOP)
        #include <Pyros3D/Ext/gles3/glad/glad.h>
    #else
        #if defined(GLES2)
            #include <GLES2/gl2.h>
            #include <GLES2/gl2ext.h>
        #else
            #if defined(GLES3)
                #include <GLES3/gl3.h>
                #include <GLES3/gl3ext.h>
            #else 
                #include <GL/glew.h>
            #endif
        #endif
    #endif
#endif

#include <Pyros3D/Other/Global.h>

// Check GL
#if defined(_DEBUG) && !defined(EMSCRIPTEN)
#define GLCHECKER(caller) { caller; int error = glGetError(); if(error != GL_NO_ERROR) { std::cout <<  "GL Error: " << std::hex << error << " FUNCTION: " << #caller << " LINE: " << std::dec << __LINE__ << " FILE: " << __FILE__ << std::endl; BRK; } }
#else
#define GLCHECKER(caller) { caller; }
#endif

#endif /* PYROSGL_H */