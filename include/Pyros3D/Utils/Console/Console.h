//============================================================================
// Name        : Console.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Reconnect stdio to the terminal a GUI build was launched from
//============================================================================

#ifndef PYROS_CONSOLE_H
#define	PYROS_CONSOLE_H

#include <Pyros3D/Other/Export.h>

namespace p3d {

	// Sends stdout/stderr back to the terminal, if this process was started
	// from one.
	//
	// PyrosBuilder and PyrosPlayer are WINDOWS-subsystem executables (see
	// pyros_windows_gui_app in cmake/PyrosWindows.cmake), so double-clicking
	// one does not raise a console window behind the app - which is what an
	// application is expected to do, and what a console-subsystem build could
	// not avoid. The cost is that such a process starts with nowhere for
	// stdout to go, so everything the engine prints - every echo(), every
	// warning, the crash handler's whole backtrace - was written into a
	// closed handle even when the user HAD run it from a command prompt and
	// was watching for exactly that output.
	//
	// AttachConsole(ATTACH_PARENT_PROCESS) recovers it: a process launched
	// from cmd or PowerShell inherits that terminal and can reattach to it,
	// and one launched from Explorer has no parent console and is left alone.
	// So both cases get what they want from the same binary.
	//
	// One cosmetic wrinkle is inherent to the trick and not worth fighting:
	// the shell does not wait for a GUI-subsystem child, so it prints its
	// next prompt immediately and this program's output arrives underneath
	// it. The text is all there; the prompt is just sitting in the middle of
	// it.
	//
	// Call it once, first thing in main(), before anything prints. No-op off
	// Windows, where stdio is never disconnected to begin with.
	PYROS3D_API void AttachToParentConsole();

}

#endif	/* PYROS_CONSOLE_H */
