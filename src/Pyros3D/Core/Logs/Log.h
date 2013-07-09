//============================================================================
// Name        : Log.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Log
//============================================================================

#ifndef LOG_H
#define	LOG_H

#include <string>
#include <iostream>
#include <fstream>

// Filename
#define LOG_FILE_PATH "Pyros.log"

namespace p3d {
    
    namespace LOG {
  
        class _LOG {
            public:
                static std::ofstream outputFile;
                static bool _initiated;
                static void _message(const std::string &Message)
                {
                    #ifdef LOG_TO_FILE

                        outputFile << Message << std::endl;

                    #else

                        std::cout << Message << std::endl;

                    #endif
                }
                static void _echo(const std::string &Message)
                {
                    if (!_initiated)
                    {
                        _initiated = true;
                        _message("=== Pyros3D Start ===");
                    }

                    _message(Message);

                }
        };

        #define echo( x ) ( p3d::LOG::_LOG::_echo(x) )
        
    };
};

#endif	/* LOG_H */