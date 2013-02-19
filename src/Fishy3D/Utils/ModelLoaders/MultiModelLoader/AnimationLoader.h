//============================================================================
// Name        : AnimationLoader.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Loads model animation based on Assimp
//============================================================================

#ifndef ANIMATIONLOADER_H
#define	ANIMATIONLOADER_H

#include "../IModelLoader.h"
#include "../../../Core/Math/Quaternion.h"
#include "../../../Core/Math/Matrix.h"
#include "../../../Core/Math/vec4.h"
#include "../../../Core/Math/vec3.h"
#include "../../../Core/Math/vec2.h"

// Assimp Lib
#include <assimp/postprocess.h>
#include <assimp/anim.h>
#include <assimp/scene.h>
#include <assimp/cimport.h>

namespace Fishy3D {

    // Stores Positions
    struct PositionData {
        // Time
        double Time;
        // Position
        vec3 Pos;
        PositionData() {}
        PositionData(const double &time, const vec3 &pos) : Time(time), Pos(pos) {}
    };
    
    // Stores Rotations
    struct RotationData {
        // Time
        double Time;
        // Rotation
        Quaternion Rot;
        RotationData() {}
        RotationData(const double &time, const Quaternion &rot) : Time(time), Rot(rot) {}
    };
    
    // Stores Scaling
    struct ScalingData {
        // Time
        double Time;
        // Scale
        vec3 Scale;
        ScalingData() {}
        ScalingData(const double &time, const vec3 &scale) : Time(time), Scale(scale) {}
    };
    
    // Channel Struct
    struct Channel {
        
        // Node Name
        std::string NodeName;
        
        // position, rotation and scale
        std::vector<PositionData> positions;
        std::vector<RotationData> rotations;
        std::vector<ScalingData> scales;
    };
    
    // Animation Struct
    struct Animation {
        
        // Animation Name
        std::string AnimationName;
        // each animation has at least one channel
        std::vector<Channel> Channels;
        // Animation Duration
        unsigned int Duration;
        // Ticks per Second
        double TicksPerSecond;
        
    };
    
    class AnimationLoader : public IModelLoader {
        public:

            AnimationLoader();
            virtual ~AnimationLoader();
            
            virtual void Load(const std::string &Filename);

            // animations
            std::vector<Animation> animations;
            
        private:

            // assimp model
            const aiScene* assimp_model;                        
    };

}

#endif	/* ANIMATIONLOADER_H */