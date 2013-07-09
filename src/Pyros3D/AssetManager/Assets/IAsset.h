//============================================================================
// Name        : IAsset.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Asset Interface
//============================================================================

#ifndef IASSET_H
#define IASSET_H

#include "../../Core/Math/Math.h"

namespace p3d {
    
    class IAsset {
        
        public:
        
            IAsset() {}
            const uint32 &GetID() const { return ID; }
            virtual ~IAsset() {}
            virtual void Dispose() = 0;
            
        protected:
            
            uint32 ID;
            std::string Filename;
        
    };
    
};

#endif /* IASSET_H */