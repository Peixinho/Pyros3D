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
#include "../../../Core/Math/Math.h"

// Assimp Lib
#include <assimp/postprocess.h>
#include <assimp/anim.h>
#include <assimp/scene.h>
#include <assimp/cimport.h>

namespace p3d {

    // Stores Positions
    struct PositionData {
        // Time
        d64 Time;
        // Position
        Vec3 Pos;
        PositionData() {}
        PositionData(const d64 &time, const Vec3 &pos) : Time(time), Pos(pos) {}
    };
    
    // Stores Rotations
    struct RotationData {
        // Time
        d64 Time;
        // Rotation
        Quaternion Rot;
        RotationData() {}
        RotationData(const d64 &time, const Quaternion &rot) : Time(time), Rot(rot) {}
    };
    
    // Stores Scaling
    struct ScalingData {
        // Time
        d64 Time;
        // Scale
        Vec3 Scale;
        ScalingData() {}
        ScalingData(const d64 &time, const Vec3 &scale) : Time(time), Scale(scale) {}
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
        uint32 Duration;
        // Ticks per Second
        d64 TicksPerSecond;
        
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