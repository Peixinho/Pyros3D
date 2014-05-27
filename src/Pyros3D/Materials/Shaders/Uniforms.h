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
#include "../../Other/Export.h"

namespace p3d {

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
                ScreenDimensions = 11,

                // Shadow Mapping
                DirectionalShadowMap = 12,
                PointShadowMap = 13,
                SpotShadowMap = 14,
                DirectionalShadowMatrix = 15,
                PointShadowMatrix = 16,
                SpotShadowMatrix = 17,
                DirectionalShadowFar = 18,
                NumberOfDirectionalShadows = 19,
                NumberOfPointShadows = 20,
                NumberOfSpotShadows = 21,

                // User Uniforms
                Other = 200,
                
                // Model Specific
                ModelMatrix = 300,
                NormalMatrix = 301,
                ModelViewMatrix = 302,
                ModelViewProjectionMatrix = 303,
                ModelMatrixInverse = 304,
                ModelViewMatrixInverse = 305,
                ModelMatrixInverseTranspose = 306,
                Skinning = 307
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
        struct PYROS3D_API Uniform {

            // default value
            std::string Name;
            uint32 Type;
            uint32 Usage;
            uint32 ElementCount;
            std::vector<uchar> Value;

            Uniform() { ElementCount = 0; Usage = 200; /* Usage = Other */ }

            Uniform(const std::string &name, const uint32 &usage, const uint32 &type = 0)
            {
                Name=name;
                Usage = usage;
                Type = type;
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
                    case DataUsage::ScreenDimensions:
                        Type = DataType::Vec2;
                        break;
                    case DataUsage::DirectionalShadowMap:
                        Type = DataType::Int;
                        break;
                    case DataUsage::DirectionalShadowMatrix:
                        Type = DataType::Matrix;
                        break;
                    case DataUsage::DirectionalShadowFar:
                        Type = DataType::Vec4;
                        break;
                    case DataUsage::NumberOfDirectionalShadows:
                        Type = DataType::Int;
                        break;
                    case DataUsage::PointShadowMap:
                        Type = DataType::Int;
                        break;
                    case DataUsage::PointShadowMatrix:
                        Type = DataType::Matrix;
                        break;
                    case DataUsage::NumberOfPointShadows:
                        Type = DataType::Int;
                        break;                        
                    case DataUsage::SpotShadowMap:
                        Type = DataType::Int;
                        break;
                    case DataUsage::SpotShadowMatrix:
                        Type = DataType::Matrix;
                        break;
                    case DataUsage::NumberOfSpotShadows:
                        Type = DataType::Int;
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
                    case DataUsage::Skinning:
                        Type = DataType::Matrix;
                        break;
                };
            
            }
            Uniform(const std::string &name, const uint32 &type, void* value, const uint32 &elementCount = 1) { Name=name; Usage = 200; Type = type; ElementCount = elementCount; SetValue(value); }

            void SetValue(void* value, uint32 elementCount = 1)
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
                        Value.resize(sizeof(f32)*ElementCount);
                        memcpy(&Value[0],value,sizeof(f32)*ElementCount);
                        break;
                    }
                    case DataType::Vec2:
                    {
                        Value.resize(sizeof(Vec2)*ElementCount);
                        memcpy(&Value[0],value,sizeof(Vec2)*ElementCount);
                        break;
                    }
                    case DataType::Vec3:
                    {
                        Value.resize(sizeof(Vec3)*ElementCount);
                        memcpy(&Value[0],value,sizeof(Vec3)*ElementCount);
                        break;
                    }
                    case DataType::Vec4:
                    {
                        Value.resize(sizeof(Vec4)*ElementCount);
                        memcpy(&Value[0],value,sizeof(Vec4)*ElementCount);
                        break;
                    }
                    case DataType::Matrix:
                    {
                        Value.resize(sizeof(Matrix)*ElementCount);
                        memcpy(&Value[0],value,sizeof(Matrix)*ElementCount);
                        break;
                    }
                };
            }
        };
    };
};

#endif	/* UNIFORMS_H */