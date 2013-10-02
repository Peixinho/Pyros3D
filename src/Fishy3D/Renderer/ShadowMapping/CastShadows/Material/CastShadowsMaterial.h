//============================================================================
// Name        : CastShadowsMaterial.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Material to Render Depth to Texture
//============================================================================

#ifndef CASTSHADOWSMATERIAL_H
#define	CASTSHADOWSMATERIAL_H

#include "../../../../Materials/GenericShaderMaterials/GenericShaderMaterial.h"

namespace Fishy3D {

    class CastShadowsMaterial : public GenericShaderMaterial {
        public:
            
            CastShadowsMaterial();            
            virtual ~CastShadowsMaterial();

            virtual void PreRender();
            virtual void Render() {}
            virtual void AfterRender();            
            
        private:

    };

};

#endif	/* CASTSHADOWSMATERIAL_H */