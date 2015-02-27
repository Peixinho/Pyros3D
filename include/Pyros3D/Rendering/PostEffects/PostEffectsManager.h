//============================================================================
// Name        : PostEffectsManager.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Post Effects Manager
//============================================================================

#include <Pyros3D/Materials/IMaterial.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Rendering/Renderer/IRenderer.h>

#ifndef POSTEFFECTSMANAGER_H
#define	POSTEFFECTSMANAGER_H

namespace p3d {

    using namespace Uniforms;

    class PYROS3D_API PostEffectsManager {
        friend class IEffect;
        
        public:

            PostEffectsManager(const uint32 width, const uint32 height);
            virtual ~PostEffectsManager();
            
            void Resize(const uint32 width, const uint32 height);

			void Start();
			void End();

            // Process Post Effects
            void ProcessPostEffects(Projection* projection);
            
            void AddEffect(IEffect* Effect);
            void RemoveEffect(IEffect* Effect);
            
            const uint32 GetNumberEffects() const;
            
            FrameBuffer* GetExternalFrameBuffer();
            
            Texture* GetColor() { return Color; }
            Texture* GetDepth() { return Depth; }
            Texture* GetLastRTT() { return LastRTT; }

        private:
            
            void UpdateQuad(const uint32 width, const uint32 height);
            
            // Set Quad Geometry
            std::vector<Vec3> vertex;
            std::vector<Vec2> texcoord;
            bool ChangedDimensions;
            
			uint32 Width, Height;

            // List of Effects
            std::vector<IEffect*> effects;
            
            // MRT
            Texture *Color, *Depth, *Result1, *Result2, *LastRTT;
            
            // Frame Buffers
            FrameBuffer *ExternalFBO, *fbo1, *fbo2, *activeFBO;

            // Frame Buffer Flags
            bool usingFBO1, usingFBO2;

            // Projection Matrix
            Projection proj;
    };

};
#endif	/* POSTEFFECTSMANAGER_H */