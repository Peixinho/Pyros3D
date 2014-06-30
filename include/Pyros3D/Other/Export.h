//============================================================================
// Name        : Export.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Export and Import for VS shared lib projects
//============================================================================

#ifndef EXPORT_H
#define	EXPORT_H

    #if defined(_WIN32)

        #if defined(_IMPORT)

            #define PYROS3D_API __declspec(dllimport)

        #elif defined(_EXPORT)
        
            #define PYROS3D_API __declspec(dllexport)

        #else

            #define PYROS3D_API

        #endif

    #else

        #define PYROS3D_API

    #endif

#endif	/* EXPORT_H */