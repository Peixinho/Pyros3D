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
#include "../../Core/Math/vec2.h"
#include "../../Core/Math/vec3.h"
#include "../../Core/Math/vec4.h"
#include "Uniforms.h"
#include "../../Utils/Pointers/SuperSmartPointer.h"

#include <string.h>
#include <fstream>
#include <iostream>

namespace Fishy3D {
    
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
                Shader(unsigned type);
                virtual ~Shader();        

                void loadShaderFile(const char* filename);
                void loadShaderText(const std::string &text);

                const unsigned &GetType() const;
                void compileShader(unsigned* ProgramObject);
                void DeleteShader(unsigned ProgramObject);
                static void DeleteProgram(unsigned* ProgramObject);

                // Get Location
                static long GetUniformLocation(const int& program, const std::string &name);
                static long GetAttributeLocation(const int& program, const std::string &name);

                // Uniforms          
                static void SendUniform(Uniform::Uniform uniform);
                static void SendUniform(Uniform::Uniform uniform, void* data, const unsigned &elementCount = 1);
                
            private:

                // shader type
                unsigned type;

                // shader id
                unsigned shader;

                //shader text
                std::string shaderString;
    };

    // Struct of Shaders and Program
    struct Shaders
    {
        // GL ID
        unsigned shaderProgram;
        // Vertex and Fragment Shaders
        SuperSmartPointer<Shader> vertexShader;
        SuperSmartPointer<Shader> fragmentShader;
        // Shader Usage Counter
        unsigned currentMaterials;
        
        Shaders() : shaderProgram(0), currentMaterials(0)
        {
            vertexShader = SuperSmartPointer<Shader> (new Shader(ShaderType::VertexShader));
            fragmentShader = SuperSmartPointer<Shader> (new Shader(ShaderType::FragmentShader));
        }
        
        virtual ~Shaders()
        {
            // Remove From GPU
            vertexShader->DeleteShader(shaderProgram);
            fragmentShader->DeleteShader(shaderProgram);
            Shader::DeleteProgram(&shaderProgram);
            
            // Dispose Shaders
            vertexShader.Dispose();
            fragmentShader.Dispose();
        }
    };
    
}

#endif	/* SHADERS_H */