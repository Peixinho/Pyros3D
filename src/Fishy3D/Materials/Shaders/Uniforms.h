//============================================================================
// Name        : Uniforms.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Uniforms
//============================================================================

#ifndef UNIFORMS_H
#define	UNIFORMS_H
#include <vector>
#include <string.h>
#include <iostream>
#include "Shaders.h"
namespace Fishy3D {

    namespace Uniform {
        // Uniform Data Usage
        namespace DataUsage {
            enum {
                // Global Uniforms
                ProjectionMatrix = 0,
                ViewMatrix = 1,                
                ViewProjectionMatrix = 2,
                ProjectionMatrixInverse = 3,
                ViewMatrixInverse = 4,
                CameraPosition = 5,
                Timer = 6,
                GlobalAmbientLight = 7,
                Lights = 8,
                NumberOfLights = 9,
                NearFarPlane = 10,
                ShadowMap = 11,
                ShadowMatrix = 12,
                NumberOfShadows = 13,
                ShadowFar = 14,

                // User Uniforms
                Other = 20,
                
                // Model Specific
                ModelMatrix = 30,
                NormalMatrix = 31,
                ModelViewMatrix = 32,
                ModelViewProjectionMatrix = 33,
                ModelMatrixInverse = 34,
                ModelViewMatrixInverse = 35,
                ModelMatrixInverseTranspose = 36,
                SkinningBones = 37
            };
        };

        // Uniform Data Type
        namespace DataType {
            enum {
                Int = 0,
                Float,
                Vec2,
                Vec3,
                Vec4,
                Matrix
            };
        };

        // Uniforms Struct
        struct Uniform {

            // default value
        	std::string Name;
        	int Handle;
        	unsigned Type;
        	unsigned Usage;
        	unsigned ElementCount;
        	std::vector<unsigned char> Value;

            Uniform() { Handle=-2; ElementCount = 0; Usage = 20; /* Usage = Other */ }
            Uniform(const std::string &name, const unsigned &usage)
            {
                Name=name;
                Usage = usage;
                Handle = -2;
                ElementCount = 0;
                
                switch (usage)
                {
                    case DataUsage::ViewMatrix:
                        Type = DataType::Matrix;
                        break;
                    case DataUsage::ProjectionMatrix:
                        Type = DataType::Matrix;
                        break;
                    case DataUsage::ViewProjectionMatrix:
                        Type = DataType::Matrix;
                        break;
                    case DataUsage::ViewMatrixInverse:
                        Type = DataType::Matrix;
                        break;
                    case DataUsage::ProjectionMatrixInverse:
                        Type = DataType::Matrix;
                        break;
                    case DataUsage::CameraPosition:
                        Type = DataType::Vec3;
                        break;
                    case DataUsage::Timer:
                        Type = DataType::Float;
                        break;
                    case DataUsage::GlobalAmbientLight:
                        Type = DataType::Vec4;
                        break;
                    case DataUsage::Lights:
                        Type = DataType::Matrix;
                        break;
                    case DataUsage::NumberOfLights:
                        Type = DataType::Int;
                        break;
                    case DataUsage::NearFarPlane:
                        Type = DataType::Vec2;
                        break;
                    case DataUsage::ShadowMap:
                        Type = DataType::Int;
                        break;
                    case DataUsage::ShadowMatrix:
                        Type = DataType::Matrix;
                        break;
                    case DataUsage::NumberOfShadows:
                        Type = DataType::Int;
                        break;
                    case DataUsage::ShadowFar:
                        Type = DataType::Vec4;
                        break;
                    case DataUsage::ModelMatrix:
                        Type = DataType::Matrix;
                        break;
                    case DataUsage::NormalMatrix:
                        Type = DataType::Matrix;
                        break;
                    case DataUsage::ModelViewMatrix:
                        Type = DataType::Matrix;
                        break;
                    case DataUsage::ModelViewProjectionMatrix:
                        Type = DataType::Matrix;
                        break;
                    case DataUsage::ModelMatrixInverse:
                        Type = DataType::Matrix;
                        break;
                    case DataUsage::ModelViewMatrixInverse:
                        Type = DataType::Matrix;
                        break;
                    case DataUsage::ModelMatrixInverseTranspose:
                        Type = DataType::Matrix;
                        break;
                    case DataUsage::SkinningBones:
                        Type = DataType::Matrix;
                        break;
                };
            
            }
            Uniform(const std::string &name, const unsigned &type, void* value, unsigned int elementCount = 1) { Name=name; Usage = 20; Type = type; Handle = -2; ElementCount = 0; SetValue(value); }

            void SetValue(void* value, unsigned int elementCount = 1)
            {
                
                // clear data
                Value.clear();
                
                ElementCount = elementCount;                                
                
                switch(Type)
                {
                    
                    case DataType::Int:
                    {                        
                        Value.resize(sizeof(int)*ElementCount);
                        memcpy(&Value[0],value,sizeof(int)*ElementCount);
                        break;
                    }
                    case DataType::Float:
                    {
                        Value.resize(sizeof(float)*ElementCount);
                        memcpy(&Value[0],value,sizeof(float)*ElementCount);
                        break;
                    }
                    case DataType::Vec2:
                    {
                        Value.resize(sizeof(vec2)*ElementCount);
                        memcpy(&Value[0],value,sizeof(vec2)*ElementCount);
                        break;
                    }
                    case DataType::Vec3:
                    {
                        Value.resize(sizeof(vec3)*ElementCount);
                        memcpy(&Value[0],value,sizeof(vec3)*ElementCount);
                        break;
                    }
                    case DataType::Vec4:
                    {
                        Value.resize(sizeof(vec4)*ElementCount);
                        memcpy(&Value[0],value,sizeof(vec4)*ElementCount);
                        break;
                    }
                    case DataType::Matrix:
                    {
                        Value.resize(sizeof(Matrix)*ElementCount);
                        memcpy(&Value[0],value,sizeof(Matrix)*ElementCount);
                        break;
                    }
                }
            }
        };
    };
};

#endif	/* UNIFORMS_H */