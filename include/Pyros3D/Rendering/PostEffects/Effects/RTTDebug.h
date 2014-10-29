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

namespace p3d {

    class RTTDebug : public IEffect {
        public:
            RTTDebug(const uint32 &Tex1);
            virtual ~RTTDebug();
        private:

    };

}

#endif	/* RTTDEBUG_H */

