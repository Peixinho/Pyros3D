//============================================================================
// Name        : Console.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Reconnect stdio to the terminal a GUI build was launched from
//============================================================================

#include <Pyros3D/Utils/Console/Console.h>

#ifdef _WIN32

#include <windows.h>
#include <cstdio>
#include <iostream>

namespace p3d {

	namespace {

		// A standard handle is NULL (or INVALID_HANDLE_VALUE) when this
		// process has nothing behind it. A GUI-subsystem process started from
		// a shell is in exactly that state: the loader does not attach the
		// parent's console to it, so there is nowhere for stdout to go even
		// though a terminal is sitting right there. A REDIRECTED stream is
		// the opposite case - `PyrosBuilder.exe > log.txt`, or PowerShell's
		// Start-Process -RedirectStandardOutput - and comes through as a
		// perfectly valid file handle that must be left exactly as it is.
		bool StreamIsDead(const DWORD which)
		{
			const HANDLE h = GetStdHandle(which);
			return h == NULL || h == INVALID_HANDLE_VALUE;
		}

	}

	void AttachToParentConsole()
	{
		// Nothing to repair. Covers a console-subsystem build, a process that
		// called AllocConsole, and - the one that matters - any redirection
		// the caller set up, which reopening the streams would destroy.
		if (!StreamIsDead(STD_OUTPUT_HANDLE)) return;

		// Fails when there is no parent console, which is the Explorer
		// double-click case: nothing to attach to, and nothing to do.
		if (!AttachConsole(ATTACH_PARENT_PROCESS)) return;

		// Attaching gives the PROCESS a console; it does not move the CRT's
		// streams, which were bound to nothing when the CRT started up and
		// stay that way. CONOUT$/CONIN$ name the console itself rather than
		// any particular handle, so this works whether or not the parent had
		// redirected its own.
		//
		// Each stream is tested separately because they can be redirected
		// separately - `PyrosPlayer.exe 2> errors.txt` leaves stdout dead and
		// stderr pointing at a file, and only the first of those wants
		// touching.
		//
		// freopen rather than freopen_s: the _s variants are an MSVC/Annex K
		// extension, and _CRT_SECURE_NO_WARNINGS is already set project-wide
		// for exactly this reason (see cmake/PyrosWindows.cmake).
		freopen("CONOUT$", "w", stdout);
		if (StreamIsDead(STD_ERROR_HANDLE)) freopen("CONOUT$", "w", stderr);
		if (StreamIsDead(STD_INPUT_HANDLE)) freopen("CONIN$", "r", stdin);

		// The iostreams cached the old (closed) file descriptors when they
		// were constructed, so they need telling as well - echo() goes
		// through stdio, but plenty of the editor's diagnostics use std::cout.
		std::cout.clear();
		std::cerr.clear();
		std::cin.clear();
		std::ios::sync_with_stdio(true);

		// The shell printed its prompt the moment it launched a
		// GUI-subsystem child, so the first line would otherwise land on the
		// end of it.
		fputs("\n", stdout);
		fflush(stdout);
	}

}

#else

namespace p3d {
	// Every other platform leaves stdout connected to whatever launched the
	// process, and a GUI app started from a file manager simply has it going
	// nowhere - there is no console to attach to and nothing to repair.
	void AttachToParentConsole() {}
}

#endif
