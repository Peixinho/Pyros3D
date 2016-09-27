//============================================================================
// Name        : Log.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Log
//============================================================================

#include <Pyros3D/Core/Logs/Log.h>

namespace p3d {

	namespace LOG {
#ifdef LOG_TO_FILE
		std::ofstream _LOG::outputFile(LOG_FILE_PATH, std::ios::app);
#endif
		bool _LOG::_initiated = false;

	};

};