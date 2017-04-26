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
#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Ext/StringIDs/StringID.hpp>

namespace p3d {

	namespace Uniforms {
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

				ClipPlanes = 22,

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
	};

	// Uniforms Struct
	struct Uniform {

		std::string Name;
		uint32 NameID;
		uint32 Type;
		uint32 Usage;
		uint32 ElementCount;
		std::vector<uchar> Value;

		Uniform()
		{
			ElementCount = 0;
			Usage = 200; /* Usage = Other */
		}

		Uniform(const std::string &name, const uint32 usage, const uint32 type = 0)
		{
			Name = name;
			NameID = MakeStringID(Name);
			Usage = usage;
			Type = type;
			ElementCount = 0;

			switch (usage)
			{
			case Uniforms::DataUsage::ViewMatrix:
				Type = Uniforms::DataType::Matrix;
				break;
			case Uniforms::DataUsage::ProjectionMatrix:
				Type = Uniforms::DataType::Matrix;
				break;
			case Uniforms::DataUsage::ViewProjectionMatrix:
				Type = Uniforms::DataType::Matrix;
				break;
			case Uniforms::DataUsage::ViewMatrixInverse:
				Type = Uniforms::DataType::Matrix;
				break;
			case Uniforms::DataUsage::ProjectionMatrixInverse:
				Type = Uniforms::DataType::Matrix;
				break;
			case Uniforms::DataUsage::CameraPosition:
				Type = Uniforms::DataType::Vec3;
				break;
			case Uniforms::DataUsage::Timer:
				Type = Uniforms::DataType::Float;
				break;
			case Uniforms::DataUsage::GlobalAmbientLight:
				Type = Uniforms::DataType::Vec4;
				break;
			case Uniforms::DataUsage::Lights:
				Type = Uniforms::DataType::Matrix;
				break;
			case Uniforms::DataUsage::NumberOfLights:
				Type = Uniforms::DataType::Int;
				break;
			case Uniforms::DataUsage::NearFarPlane:
				Type = Uniforms::DataType::Vec2;
				break;
			case Uniforms::DataUsage::ScreenDimensions:
				Type = Uniforms::DataType::Vec2;
				break;
			case Uniforms::DataUsage::DirectionalShadowMap:
				Type = Uniforms::DataType::Int;
				break;
			case Uniforms::DataUsage::DirectionalShadowMatrix:
				Type = Uniforms::DataType::Matrix;
				break;
			case Uniforms::DataUsage::DirectionalShadowFar:
				Type = Uniforms::DataType::Vec4;
				break;
			case Uniforms::DataUsage::NumberOfDirectionalShadows:
				Type = Uniforms::DataType::Int;
				break;
			case Uniforms::DataUsage::PointShadowMap:
				Type = Uniforms::DataType::Int;
				break;
			case Uniforms::DataUsage::PointShadowMatrix:
				Type = Uniforms::DataType::Matrix;
				break;
			case Uniforms::DataUsage::NumberOfPointShadows:
				Type = Uniforms::DataType::Int;
				break;
			case Uniforms::DataUsage::SpotShadowMap:
				Type = Uniforms::DataType::Int;
				break;
			case Uniforms::DataUsage::SpotShadowMatrix:
				Type = Uniforms::DataType::Matrix;
				break;
			case Uniforms::DataUsage::NumberOfSpotShadows:
				Type = Uniforms::DataType::Int;
				break;
			case Uniforms::DataUsage::ModelMatrix:
				Type = Uniforms::DataType::Matrix;
				break;
			case Uniforms::DataUsage::NormalMatrix:
				Type = Uniforms::DataType::Matrix;
				break;
			case Uniforms::DataUsage::ModelViewMatrix:
				Type = Uniforms::DataType::Matrix;
				break;
			case Uniforms::DataUsage::ModelViewProjectionMatrix:
				Type = Uniforms::DataType::Matrix;
				break;
			case Uniforms::DataUsage::ModelMatrixInverse:
				Type = Uniforms::DataType::Matrix;
				break;
			case Uniforms::DataUsage::ModelViewMatrixInverse:
				Type = Uniforms::DataType::Matrix;
				break;
			case Uniforms::DataUsage::ModelMatrixInverseTranspose:
				Type = Uniforms::DataType::Matrix;
				break;
			case Uniforms::DataUsage::Skinning:
				Type = Uniforms::DataType::Matrix;
				break;
			case Uniforms::DataUsage::ClipPlanes:
				Type = Uniforms::DataType::Vec4;
				break;
			};

		}

		Uniform(const std::string &name, const uint32 type, void* value, const uint32 elementCount = 1) 
		{ 
			Name = name;
			NameID = MakeStringID(Name);
			Usage = 200;
			Type = type;
			ElementCount = elementCount;
			SetValue(value);
		}

		void SetValue(void* value, const uint32 elementCount = 1)
		{

			// clear data
			Value.clear();

			ElementCount = elementCount;
			
			switch (Type)
			{

			case Uniforms::DataType::Int:
			{
				Value.resize(sizeof(int)*ElementCount);
				memcpy(&Value[0], value, sizeof(int)*ElementCount);
				break;
			}
			case Uniforms::DataType::Float:
			{
				Value.resize(sizeof(f32)*ElementCount);
				memcpy(&Value[0], value, sizeof(f32)*ElementCount);
				break;
			}
			case Uniforms::DataType::Vec2:
			{
				Value.resize(sizeof(Vec2)*ElementCount);
				memcpy(&Value[0], value, sizeof(Vec2)*ElementCount);
				break;
			}
			case Uniforms::DataType::Vec3:
			{
				Value.resize(sizeof(Vec3)*ElementCount);
				memcpy(&Value[0], value, sizeof(Vec3)*ElementCount);
				break;
			}
			case Uniforms::DataType::Vec4:
			{
				Value.resize(sizeof(Vec4)*ElementCount);
				memcpy(&Value[0], value, sizeof(Vec4)*ElementCount);
				break;
			}
			case Uniforms::DataType::Matrix:
			{
				Value.resize(sizeof(Matrix)*ElementCount);
				memcpy(&Value[0], value, sizeof(Matrix)*ElementCount);
				break;
			}
			};
		}
	};
};

#endif	/* UNIFORMS_H */