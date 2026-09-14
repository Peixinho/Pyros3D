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
	// Every frame prints as module+offset, which is what a PDB can be pointed
	// at after the fact - the absolute address cannot, since ASLR moved the
	// module somewhere new. Function names are added on top of that only for
	// modules that actually have a .pdb next to them; for the rest dbghelp
	// can offer nothing but the nearest preceding entry in the export table,
	// which in a module exporting a few hundred names across megabytes of
	// code names the wrong function far more often than the right one, so
	// those are printed as an explicit guess or not at all.
	//
	// No-op off Windows: a POSIX crash leaves a core file, and the terminal
	// says "Segmentation fault" rather than nothing at all.
	PYROS3D_API void InstallCrashHandler();

}

#endif	/* PYROS_CRASHHANDLER_H */
