//============================================================================
// Name        : PostEffectsManager.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Post Effects Manager
//============================================================================

#include "../../Materials/IMaterial.h"
#include "../../Textures/Texture.h"
#include "Effects/IEffect.h"
#include "../../Core/Buffers/FrameBuffer.h"
#include "../../Core/GameObjects/GameObject.h"

#ifndef POSTEFFECTSMANAGER_H
#define	POSTEFFECTSMANAGER_H

#define GLCHECK() { int error = glGetError(); if(error != GL_NO_ERROR) { std::cout <<  "GL Error: " << std::hex << error << std::endl; } }

namespace Fishy3D {

    class PostEffectsManager {
        
        public:
            PostEffectsManager(const unsigned &width, const unsigned &height);
            virtual ~PostEffectsManager();
            
            void Resize(const unsigned &width, const unsigned &height);
            
            // Process Post Effects
            void ProcessPostEffects(Projection* projection);
            
            void AddEffect(SuperSmartPointer<IEffect> Effect);
            void RemoveEffect(SuperSmartPointer<IEffect> Effect);
            
            unsigned GetNumberEffects();
            
            SuperSmartPointer<FrameBuffer> GetExternalFrameBuffer();
            
        private:
            
            void UpdateQuad(const unsigned &width, const unsigned &height);
            
            // Set Quad Geometry
            std::vector<vec3> vertex;
            std::vector<vec2> texcoord;
            bool ChangedDimensions;
            
            // List of Effects
            std::vector<SuperSmartPointer <IEffect> > effects;
            
            // Dimensions
            unsigned Width, Height;
            
            // MRT
            Texture Color, NormalDepth, Position, LastRTT;
            
            // Frame Buffers
            SuperSmartPointer<FrameBuffer> ExternalFBO, fbo1, fbo2, activeFBO;
            // Frame Buffer Flags
            bool usingFBO1, usingFBO2;

            // Projection Matrix
            Projection proj;
    };

};
#endif	/* POSTEFFECTSMANAGER_H */