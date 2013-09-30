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
#include "../../Core/Buffers/FrameBuffer.h"

namespace p3d {

    Shader::Shader() {
    }
    Shader::Shader(uint32 type) {
        this->type = type;
    }

    Shader::~Shader() {}

    const uint32 &Shader::GetType() const
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

    void Shader::compileShader(uint32* ProgramObject)
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

        uint32 len = shaderString.length();

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
            echo(std::string(shaderType.c_str() + std::string(" COMPILATION ERROR: ") + std::string(log)));
            free(log);
        } else {
            char *log;        
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
            log = (char*)malloc(length);
            glGetShaderInfoLog(shader, length, &result, log);
            if (length>1)
                echo(std::string(shaderType.c_str() + std::string(" COMPILED WITH WARNINGS: ") + std::string(log)));
            free(log);

            if (*ProgramObject==0) 
                *ProgramObject = (uint32)glCreateProgram();
			
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
                echo(std::string(shaderType.c_str() + std::string(" LINK ERROR: ") + std::string(log)));
                free(log);
            }
        }
    }
    void Shader::DeleteShader(uint32 ProgramObject) 
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
    void Shader::DeleteProgram(uint32* ProgramObject) 
    {
        if (glIsProgram(*ProgramObject)) {
            glDeleteProgram(*ProgramObject);
    //      std::cout << "Shader Program Destroyed: " << *ProgramObject << std::endl;
            *ProgramObject = 0;			
        }
    //    else std::cout << "Shader Program Object Not Found: " << *ProgramObject << std::endl;
    }

    // Get positions
    int32 Shader::GetUniformLocation(const uint32 &program, const std::string &name) {
        return glGetUniformLocation(program, name.c_str());
    }
    int32 Shader::GetAttributeLocation(const uint32 &program, const std::string &name) {
        return glGetAttribLocation(program, name.c_str());
    }

    void Shader::SendUniform(const Uniform::Uniform &uniform, const int32 &Handle)
    {
        if (Handle>-1 && uniform.ElementCount>0)
        switch(uniform.Type)
        {
            case Uniform::DataType::Int:
            {    
                glUniform1iv(Handle,uniform.ElementCount,(int*)&uniform.Value[0]);
                break;
            }                    
            case Uniform::DataType::Float:
            {
                glUniform1fv(Handle,uniform.ElementCount,(f32*)&uniform.Value[0]);
                break;
            }                    
            case Uniform::DataType::Vec2:
            {
                glUniform2fv(Handle,uniform.ElementCount,(f32*)&uniform.Value[0]);
                break;
            }                    
            case Uniform::DataType::Vec3:
            {
                glUniform3fv(Handle,uniform.ElementCount,(f32*)&uniform.Value[0]);
                break;
            }                    
            case Uniform::DataType::Vec4:
            {
                glUniform4fv(Handle,uniform.ElementCount,(f32*)&uniform.Value[0]);
                break;
            }                    
            case Uniform::DataType::Matrix:
            {
                glUniformMatrix4fv(Handle,uniform.ElementCount,false,(f32*)&uniform.Value[0]);
                break;
            }
        }
    }
    
    void Shader::SendUniform(const Uniform::Uniform &uniform, void* data, const int32 &Handle, const uint32 &elementCount)
    {
        if (Handle>-1 && elementCount>0)
        {
            switch(uniform.Type)
            {
                case Uniform::DataType::Int:
                {
                    glUniform1iv(Handle,elementCount,(int*)data);
                    break;
                }                    
                case Uniform::DataType::Float:
                {
                    glUniform1fv(Handle,elementCount,(f32*)data);
                    break;
                }                    
                case Uniform::DataType::Vec2:
                {
                    glUniform2fv(Handle,elementCount,(f32*)data);
                    break;
                }                    
                case Uniform::DataType::Vec3:
                {
                    glUniform3fv(Handle,elementCount,(f32*)data);
                    break;
                }                    
                case Uniform::DataType::Vec4:
                {
                    glUniform4fv(Handle,elementCount,(f32*)data);
                    break;
                }                    
                case Uniform::DataType::Matrix:
                {
                    glUniformMatrix4fv(Handle,elementCount,false,(f32*)data);
                    break;
                }                    
            }
        }
    }
}