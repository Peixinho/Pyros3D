//============================================================================
// Name        : Shader.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Shader
//============================================================================

#include <Pyros3D/Materials/Shaders/Shaders.h>
#include <stdlib.h>
#if defined(ANDROID) || defined(EMSCRIPTEN)
    #include <GLES2/gl2.h>
    #include <GLES2/gl2ext.h>
#else
    #include "GL/glew.h"
#endif

namespace p3d {

    Shader::Shader() 
	{
		vertexID = fragmentID = geometryID = shaderProgram = currentMaterials = 0;
	}

    Shader::~Shader() {}

    void Shader::LoadShaderFile(const char* filename)
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

    void Shader::LoadShaderText(const std::string &text)
    {
        shaderString = text;
    }

    void Shader::CompileShader(const uint32 type)
    {
        
        std::string shaderType;
		uint32 shader;
        switch (type) {
            case ShaderType::VertexShader:
                vertexID = shader = glCreateShader(GL_VERTEX_SHADER);
                shaderType = "Vertex Shader";
                break;
            case ShaderType::FragmentShader:
				fragmentID = shader = glCreateShader(GL_FRAGMENT_SHADER);
                shaderType = "Fragment Shader";
                break;
            case ShaderType::GeometryShader:
				//geometryID = shader = glCreateShader(GL_GEOMETRY_SHADER);
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

            if (shaderProgram==0) 
                shaderProgram = (uint32)glCreateProgram();
			
            // Attach shader
            glAttachShader(shaderProgram, shader);
        }
    }
    void Shader::LinkProgram()
    {
        // Link Program
        glLinkProgram(shaderProgram);

        GLint result, length = 0;
        std::string log; 

        // Get Linkage error
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &result);
        if (result==GL_FALSE)
        {
            glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &length);
            log.resize(length);
            glGetProgramInfoLog(shaderProgram, length, &result, &log[0]);            
            echo(std::string(std::string("SHADER PROGRAM LINK ERROR: ") + log.c_str()));
        }
    }

    const uint32 &Shader::ShaderProgram() const{
        return shaderProgram;
    }

    void Shader::DeleteShader() 
    {
        if (glIsProgram(shaderProgram)) {
            if (glIsShader(vertexID)) {
                glDetachShader(shaderProgram, vertexID);
                glDeleteShader(vertexID);
    //            std::cout << "Shader Destroyed: " << shader << std::endl;
            }
			if (glIsShader(fragmentID)) {
				glDetachShader(shaderProgram, fragmentID);
                glDeleteShader(fragmentID);
    //            std::cout << "Shader Destroyed: " << shader << std::endl;
            }
			//if (glIsShader(geometryID)) {
			//	glDetachShader(shaderProgram, geometryID);
			//	glDeleteShader(geometryID);
   // //            std::cout << "Shader Destroyed: " << shader << std::endl;
   //         }
    //        else std::cout << "Shader Not Found: " << shader << std::endl;
        }
    //    else std::cout << "Shader Program Object Not Found: " << shaderProgram << std::endl;

		if (glIsProgram(shaderProgram)) {
            glDeleteProgram(shaderProgram);
    //      std::cout << "Shader Program Destroyed: " << *shaderProgram << std::endl;
            shaderProgram = 0;			
        }
    //    else std::cout << "Shader Program Object Not Found: " << *shaderProgram << std::endl;
    }
	
    // Get positions
    const int32 Shader::GetUniformLocation(const uint32 program, const std::string &name) {
        return glGetUniformLocation(program, name.c_str());
    }
    const int32 Shader::GetAttributeLocation(const uint32 program, const std::string &name) {
        return glGetAttribLocation(program, name.c_str());
    }

    void Shader::SendUniform(const Uniform::Uniform &uniform, const int32 Handle)
    {
        if (Handle>-1 && uniform.ElementCount>0)
        switch(uniform.Type)
        {
            case Uniform::DataType::Int:
            {
				glUniform1iv(Handle,uniform.ElementCount,(GLint*)((int32*)&uniform.Value[0]));
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
    
    void Shader::SendUniform(const Uniform::Uniform &uniform, void* data, const int32 Handle, const uint32 elementCount)
    {
        if (Handle>-1 && elementCount>0)
        {
            switch(uniform.Type)
            {
                case Uniform::DataType::Int:
                {
                    glUniform1iv(Handle,elementCount,(GLint*)((int32*)data));
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
