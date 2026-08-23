//============================================================================
// Name        : CrashHandler.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Symbolized stack trace on an otherwise silent crash
//============================================================================

#ifndef PYROS_CRASHHANDLER_H
#define	PYROS_CRASHHANDLER_H

#include <Pyros3D/Other/Export.h>

namespace p3d {

	// Install a last-resort handler that prints the faulting address and a
	// symbolized backtrace before the process dies.
	//
	// An access violation on Windows produces no console output whatsoever -
	// the process simply disappears - which is indistinguishable from a
	// clean exit to anyone running a downloaded build, and leaves nothing to
	// report. Every crash found on Windows so far has been a call through a
	// null function pointer (a GL entry point glad never loaded, a vk one
	// volk never loaded), and each cost a round trip through CI with a
	// debugger attached to locate. Machines that CI cannot reproduce - a
	// real GPU, a specific driver - have no such route at all.
	//
	// Resolves symbols from the .pdb shipped next to the executable. Without
	// one the trace still prints, as module+offset.
	//
	// No-op off Windows: a POSIX crash leaves a core file, and the terminal
	// says "Segmentation fault" rather than nothing at all.
	PYROS3D_API void InstallCrashHandler();

}

#endif	/* PYROS_CRASHHANDLER_H */
