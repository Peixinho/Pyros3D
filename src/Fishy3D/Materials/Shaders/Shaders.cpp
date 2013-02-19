//============================================================================
// Name        : Shader.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Shader
//============================================================================

#include "Shaders.h"
#include <stdlib.h>
#include "GL/glew.h"

namespace Fishy3D {

    Shader::Shader() {
    }
    Shader::Shader(unsigned type) {
        this->type = type;
    }

    Shader::~Shader() {}

    const unsigned &Shader::GetType() const
    {
        return type;
    }
    void Shader::loadShaderFile(const char* filename)
    {
        std::ifstream t(filename);
        std::string str;

        t.seekg(0, std::ios::end);   
        str.reserve(t.tellg());
        t.seekg(0, std::ios::beg);

        str.assign((std::istreambuf_iterator<char>(t)),
        std::istreambuf_iterator<char>());

        shaderString = str.c_str();
    }

    void Shader::loadShaderText(const std::string &text)
    {
            shaderString = text;
    }

    void Shader::compileShader(unsigned* ProgramObject)
    {
        
        std::string shaderType;

        switch (type) {
            case ShaderType::VertexShader:
                shader = glCreateShader(GL_VERTEX_SHADER);
                shaderType = "Vertex Shader";
                break;
            case ShaderType::FragmentShader:
                shader = glCreateShader(GL_FRAGMENT_SHADER);
                shaderType = "Fragment Shader";
                break;
            case ShaderType::GeometryShader:
                //shader = glCreateShader(GL_GEOMETRY_SHADER);
                shaderType = "Geometry Shader";
            break;
        }    

        unsigned long int len = shaderString.length();

        // batatas because is a good example :P
        const char *batatas = shaderString.c_str();

        glShaderSource(shader,1,(const GLchar**) &batatas,(const GLint *)&len);
        glCompileShader(shader);

        GLint result, length = 0;

        glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
        if (result==GL_FALSE)
        {
            char *log;        
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
                    log = (char*)malloc(length);
                    glGetShaderInfoLog(shader, length, &result, log);
            std::cout << shaderType.c_str() << " COMPILATION ERROR: " << log << std::endl;
            free(log);
        } else {
            char *log;        
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
                    log = (char*)malloc(length);
                    glGetShaderInfoLog(shader, length, &result, log);
            if (length>1)
                    std::cout << shaderType.c_str() << ": " << " COMPILED WITH WARNINGS: " << log << std::endl;
            free(log);

            if (*ProgramObject==0) 
                *ProgramObject = glCreateProgram();

            // Attach shader
            glAttachShader(*ProgramObject, shader);
            // Link Program
            glLinkProgram(*ProgramObject);
            // Get Linkage error
            glGetProgramiv(*ProgramObject, GL_LINK_STATUS, &result);        
            if (result==GL_FALSE)
            {                      
                glGetProgramiv(*ProgramObject, GL_INFO_LOG_LENGTH, &length);
                log = (char*)malloc(length);
                glGetProgramInfoLog(*ProgramObject, length, &result, log);            
                std::cout << shaderType.c_str() << " LINK ERROR: " << log << std::endl;
                free(log);
            }
        }
    }
    void Shader::DeleteShader(unsigned ProgramObject) 
    {
        if (glIsProgram(ProgramObject)) {
            if (glIsShader(shader)) {
                glDetachShader(ProgramObject, shader);
                glDeleteShader(shader);
    //            std::cout << "Shader Destroyed: " << shader << std::endl;
            }
    //        else std::cout << "Shader Not Found: " << shader << std::endl;
        }
    //    else std::cout << "Shader Program Object Not Found: " << ProgramObject << std::endl;
    }
    void Shader::DeleteProgram(unsigned* ProgramObject) 
    {
        if (glIsProgram(*ProgramObject)) {
            glDeleteProgram(*ProgramObject);
    //      std::cout << "Shader Program Destroyed: " << *ProgramObject << std::endl;
            *ProgramObject = 0;			
        }
    //    else std::cout << "Shader Program Object Not Found: " << *ProgramObject << std::endl;
    }

    // Get positions
    long Shader::GetUniformLocation(const int &program, const std::string &name) {
        return glGetUniformLocation(program, name.c_str());
    }
    long Shader::GetAttributeLocation(const int &program, const std::string &name) {
        return glGetAttribLocation(program, name.c_str());
    }

    void Shader::SendUniform(Uniform::Uniform uniform)
    {
        if (uniform.Handle>-1 && uniform.ElementCount>0)
        {
            switch(uniform.Type)
            {
                case Uniform::DataType::Int:
                {    
                    glUniform1iv(uniform.Handle,uniform.ElementCount,(int*)&uniform.Value[0]);
                    break;
                }                    
                case Uniform::DataType::Float:
                {
                    glUniform1fv(uniform.Handle,uniform.ElementCount,(float*)&uniform.Value[0]);
                    break;
                }                    
                case Uniform::DataType::Vec2:
                {
                    glUniform2fv(uniform.Handle,uniform.ElementCount,(float*)&uniform.Value[0]);
                    break;
                }                    
                case Uniform::DataType::Vec3:
                {
                    glUniform3fv(uniform.Handle,uniform.ElementCount,(float*)&uniform.Value[0]);
                    break;
                }                    
                case Uniform::DataType::Vec4:
                {
                    glUniform4fv(uniform.Handle,uniform.ElementCount,(float*)&uniform.Value[0]);
                    break;
                }                    
                case Uniform::DataType::Matrix:
                {
                    glUniformMatrix4fv(uniform.Handle,uniform.ElementCount,false,(float*)&uniform.Value[0]);
                    break;
                }
            }
        }
    }
    
    void Shader::SendUniform(Uniform::Uniform uniform, void* data, const unsigned &elementCount)
    {
        if (uniform.Handle>-1 && elementCount>0)
        {
            switch(uniform.Type)
            {
                case Uniform::DataType::Int:
                {    
                    glUniform1iv(uniform.Handle,elementCount,(int*)data);
                    break;
                }                    
                case Uniform::DataType::Float:
                {
                    glUniform1fv(uniform.Handle,elementCount,(float*)data);
                    break;
                }                    
                case Uniform::DataType::Vec2:
                {
                    glUniform2fv(uniform.Handle,elementCount,(float*)data);
                    break;
                }                    
                case Uniform::DataType::Vec3:
                {
                    glUniform3fv(uniform.Handle,elementCount,(float*)data);
                    break;
                }                    
                case Uniform::DataType::Vec4:
                {
                    glUniform4fv(uniform.Handle,elementCount,(float*)data);
                    break;
                }                    
                case Uniform::DataType::Matrix:
                {
                    glUniformMatrix4fv(uniform.Handle,elementCount,false,(float*)data);
                    break;
                }                    
            }
        }
    }
}