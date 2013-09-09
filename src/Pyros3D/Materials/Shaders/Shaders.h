//============================================================================
// Name        : Shaders.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Shaders
//============================================================================

#ifndef SHADERS_H
#define	SHADERS_H

#include "../../Core/Math/Matrix.h"
#include "../../Core/Math/Math.h"
#include "../../Core/Logs/Log.h"
#include "Uniforms.h"

#include <string.h>
#include <fstream>
#include <iostream>

namespace p3d {
    
    namespace ShaderType {
        enum {
            VertexShader = 0,
            FragmentShader,
            GeometryShader 
        };
    }     

    class Shader {
        public:

            Shader();
            Shader(uint32 type);
            virtual ~Shader();        

            void loadShaderFile(const char* filename);
            void loadShaderText(const std::string &text);

            const uint32 &GetType() const;
            void compileShader(uint32* ProgramObject);
            void DeleteShader(uint32 ProgramObject);
            static void DeleteProgram(uint32* ProgramObject);

            // Get Location
            static long GetUniformLocation(const int32& program, const std::string &name);
            static long GetAttributeLocation(const int32& program, const std::string &name);

            // Uniforms          
            static void SendUniform(const Uniform::Uniform &uniform, const int32 &Handle);
            static void SendUniform(const Uniform::Uniform &uniform, void* data, const int32 &Handle, const uint32 &elementCount = 1);

        private:

            // shader type
            uint32 type;

            // shader id
            uint32 shader;

            //shader text
            std::string shaderString;
    };

    // Struct of Shaders and Program
    struct Shaders
    {
        // GL ID
        uint32 shaderProgram;
        // Vertex and Fragment Shaders
        Shader* vertexShader;
        Shader* fragmentShader;
        // Shader Usage Counter
        uint32 currentMaterials;
        
        Shaders() : shaderProgram(0), currentMaterials(0)
        {
            vertexShader = new Shader(ShaderType::VertexShader);
            fragmentShader = new Shader(ShaderType::FragmentShader);
        }
        
        virtual ~Shaders()
        {
            // Remove From GPU
            vertexShader->DeleteShader(shaderProgram);
            fragmentShader->DeleteShader(shaderProgram);
            Shader::DeleteProgram(&shaderProgram);
            
            // Dispose Shaders
            delete vertexShader;
            delete fragmentShader;
        }
    };
    
}

#endif	/* SHADERS_H */