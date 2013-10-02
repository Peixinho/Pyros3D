//============================================================================
// Name        : RenderingList.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering List
//============================================================================

#ifndef RENDERINGLIST_H
#define	RENDERINGLIST_H

#include <list>
#include <map>
#include "../../Utils/Pointers/SuperSmartPointer.h"
#include "../../Components/LightComponents/ILightComponent.h"
#include "../../Components/RenderingComponents/IRenderingComponent.h"

namespace Fishy3D {
   
    // Circular Dependencies
    class ILightComponent;
    class IRenderingComponent;
    
    class RenderingList {

        public:

            RenderingList();
            
            // adds a Rendering Component
            void AddRenderingComponent(IRenderingComponent* comp);
            void RemoveRenderingComponent(IRenderingComponent* comp);
            // Gets the Rendering List
            const std::list<IRenderingComponent*> &GetRenderingOpaqueList() const;
            const std::list<IRenderingComponent*> &GetRenderingTranslucidList() const;
            // Clears Rendering List
            void ClearRenderingList();

            // Adds a Light Component
            void AddLightComponent(const std::string &lightID, ILightComponent* light);
            void AddLightComponent(const StringID &lightID, ILightComponent* light);
            void RemoveLightComponent(const std::string &lightID);
            void RemoveLightComponent(const StringID &lightID);
            // Finds a Light
            bool FindLight(const std::string &LightID);
            // Get a Light From the Lights List
            ILightComponent* GetLight(const unsigned &lightID);
            ILightComponent* GetLight(const std::string &lightName);

            // Compute Lights
            void ComputeLights();
			void ComputeShadows();
          
            // Get Lights
            const std::map<StringID, ILightComponent*> &GetLights() const;
            // Get Lights on a Packaging Format
            std::vector<Matrix> GetLightsPackaging();
            unsigned GetNumberOfLights();
            
            // Shadow Casting
            const std::vector<Texture> &GetShadowMaps() const;
            const std::vector<Matrix> &GetShadowMatrixBias() const;
            unsigned GetNumberOfShadows();
			const vec4 &GetShadowFar() const;
            
            // Clears Lights List
            void ClearLightsList();

            // sorts Rendering Objects
            void Sort(const vec3 &CameraPos);
            // Simple Sort
            void SimpleSort();

            virtual ~RenderingList();

        private:

            // if simple sorting is needed
            bool _dirtyRenderingList;
            
            // Rendering Components List
            std::list <IRenderingComponent*> _RenderingList;
            std::list <IRenderingComponent*> _RenderingOpaqueList;
            std::list <IRenderingComponent*> _RenderingTranslucidList;
            
            // Lights List
            std::map <StringID, ILightComponent*> _LightsID;
            std::vector<Matrix> _Lights;
            
            // Shadow Casting
            std::vector<Texture> _ShadowMaps;
            std::vector<Matrix> _ShadowMatrix;
			vec4 _ShadowFar;
            
            // Sort
            static bool CompareZ(IRenderingComponent* a, IRenderingComponent* b);

            // Static var that saves Camera position
            // to be used on Sort
            static vec3 CameraPosition;
        };

}

#endif	/* RENDERINGLIST_H */
