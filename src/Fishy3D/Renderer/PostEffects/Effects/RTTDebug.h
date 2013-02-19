//============================================================================
// Name        : RTT Debug.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : RTT Debug Effect
//============================================================================

#include "IEffect.h"

#ifndef RTTDEBUG_H
#define RTTDEBUG_H

namespace Fishy3D {

    class RTTDebug : public IEffect {
        public:
            RTTDebug(const unsigned &Tex1, const unsigned &Tex2, const unsigned &Tex3, const unsigned &Tex4);
            virtual ~RTTDebug();
        private:

    };

}

#endif	/* RTTDEBUG_H */

