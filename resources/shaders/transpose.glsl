#ifndef GLSL_TRANSPOSE
#define GLSL_TRANSPOSE

    #if defined(EMSCRIPTEN) 
        #define _highpMat3 highp mat3
        #define _highpMat4 highp mat4
        #define _highpVec3 highp vec3
        #define _highpVec4 highp vec4
    #else
        #define _highpMat3 mat3
        #define _highpMat4 mat4
        #define _highpVec3 vec3
        #define _highpVec4 vec4
    #endif

    _highpMat3 _transpose3(in _highpMat3 inMatrix) {
        _highpVec3 i0 = inMatrix[0];
        _highpVec3 i1 = inMatrix[1];
        _highpVec3 i2 = inMatrix[2];

        _highpMat3 outMatrix = mat3(
                        vec3(i0.x, i1.x, i2.x),
                        vec3(i0.y, i1.y, i2.y),
                        vec3(i0.z, i1.z, i2.z)
                        );

        return outMatrix;
    }
    _highpMat4 _transpose4(in _highpMat4 inMatrix) {
        _highpVec4 i0 = inMatrix[0];
        _highpVec4 i1 = inMatrix[1];
        _highpVec4 i2 = inMatrix[2];
        _highpVec4 i3 = inMatrix[3];

        _highpMat4 outMatrix = mat4(
                        vec4(i0.x, i1.x, i2.x, i3.x),
                        vec4(i0.y, i1.y, i2.y, i3.y),
                        vec4(i0.z, i1.z, i2.z, i3.z),
                        vec4(i0.w, i1.w, i2.w, i3.w)
                        );

        return outMatrix;
    }

    #ifdef transpose
        mat3 transpose3(in mat3 m) { return transpose(m); }
        mat4 transpose4(in mat4 m) { return transpose(m); }
    #else
        mat3 transpose3(in mat3 m) { return _transpose3(m); }
        mat4 transpose4(in mat4 m) { return _transpose4(m); }
    #endif

#endif