//============================================================================
// Name        : ModelLoader.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Model Loader Interface
//============================================================================

#ifndef IMODELLOADER_H
#define	IMODELLOADER_H

#include <string.h>
#include <fstream>
#include <iostream>
#include <vector>

namespace p3d {    
    
    class IModelLoader {
        public:
            
            IModelLoader();
            virtual ~IModelLoader();
            
            virtual void Load(const std::string &Filename) = 0;
            
        protected:
            
            std::string LoadFile(const std::string &Filename);
            
    };

}

#endif	/* IMODELLOADER_H */