//============================================================================
// Name        : Log.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Log
//============================================================================

#include "Log.h"

namespace p3d {
    
    namespace LOG {
      
        std::ofstream _LOG::outputFile(LOG_FILE_PATH, std::ios::app);
        bool _LOG::_initiated = false;
        
    };
    
};