//============================================================================
// Name        : Rendering Torus Knot Component.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering Torus Component
//============================================================================

#ifndef RENDERINGTORUSKNOTCOMPONENT_H
#define	RENDERINGTORUSKNOTCOMPONENT_H

#include "../RenderingPrimitiveComponent.h"

namespace Fishy3D {

    class RenderingTorusKnotComponent : public RenderingPrimitiveComponent {
        
        public:
            float radius, tube;
            int segmentsW, segmentsH, heightScale;
            float p, q;
            
            RenderingTorusKnotComponent();       
            RenderingTorusKnotComponent(const std::string &Name, const float &radius, const float &tube, SuperSmartPointer<IMaterial> material, const int &segmentsW = 60, const int &segmentsH = 6, const float &p = 2, const float &q = 3, const int &heightscale = 1, bool SNormals = false);
            virtual ~RenderingTorusKnotComponent();        
            
            virtual void Start();
            virtual void Update();
            virtual void Shutdown();            

            
    private:
        vec3 GetPos(const float &u, const float &v) const;
    };

}

#endif	/* RENDERINGTORUSKNOTCOMPONENT_H */