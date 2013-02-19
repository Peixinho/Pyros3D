//============================================================================
// Name        : CastShadows.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Class to Create the Shadow Maps based on the IRenderer class, but to avoid dependencies,
// it is a simple renderer
//============================================================================

#ifndef CASTSHADOWS_H
#define CASTSHADOWS_H

#include "../../IRenderer.h"
#include "../../../Utils/Pointers/SuperSmartPointer.h"
#include "../../../Core/Buffers/FrameBuffer.h"
#include "../../../SceneGraph/SceneGraph.h"
#include "../../../Components/LightComponents/ILightComponent.h"
#include "Material/CastShadowsMaterial.h"
#include "../../Culling/FrustumCulling/FrustumCulling.h"

namespace Fishy3D {

    class CastShadows {

        public:
            
            CastShadows();            
            void Cast(SceneGraph* scene, const std::list<IRenderingComponent*> &rcomps, GameObject* camera);
            virtual ~CastShadows();
            
        private:                        
            
            // Render Shadow Map
            void Render();
            void ProcessRenderingComponent(IRenderingComponent* rcomp);
            // Material to Draw Shadow Maps
            SuperSmartPointer<IMaterial> material;
            // Opaque List
            std::list<IRenderingComponent*> RCompList;
            
            // Culling            
            bool IsCulling;            
            SuperSmartPointer<Culling> culling;
            bool CullingTest(IRenderingComponent* rcomp);
            void UpdateCulling(const Matrix &view, const Matrix &projection);
            unsigned cullMode;
            void ActivateCulling(const unsigned &cullingType);
            void DeactivateCulling();
            
            // Face Culling
            unsigned cullFace;
            
            // Uniforms
            void SendGlobalUniforms(IMaterial* material);
            void SendUserUniforms(IMaterial* material);
            void SendModelUniforms(IRenderingComponent* rcomp, IMaterial* material);
            void SendModelUniforms(IRenderingComponent* rcomp, GameObject* Owner, IMaterial* material);

            // Universal Uniforms Cache
            Matrix 
                        ProjectionMatrix,
                        ViewMatrix;
            
            vec2
                        NearFarPlane;            
            
            // Model Specific Cache
            Matrix
                        ModelMatrix;
            
            // Camera
            GameObject*
                        Camera;
            
            // Draw Type
            unsigned
                        drawType;
               
    };

};

#endif /* CASTSHADOWS_H */