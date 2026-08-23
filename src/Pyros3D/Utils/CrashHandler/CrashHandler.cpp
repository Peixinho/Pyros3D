//============================================================================
// Name        : CrashHandler.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Symbolized stack trace on an otherwise silent crash
//============================================================================

#include <Pyros3D/Utils/CrashHandler/CrashHandler.h>

#ifdef _WIN32

#include <Pyros3D/Core/Logs/Log.h>
#include <windows.h>
#include <dbghelp.h>
#include <cstdio>
#include <sstream>

#pragma comment(lib, "dbghelp.lib")

namespace p3d {

	namespace {

		const char *ExceptionName(const DWORD code)
		{
			switch (code)
			{
				case EXCEPTION_ACCESS_VIOLATION:      return "access violation";
				case EXCEPTION_ILLEGAL_INSTRUCTION:   return "illegal instruction";
				case EXCEPTION_STACK_OVERFLOW:        return "stack overflow";
				case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "integer divide by zero";
				case EXCEPTION_FLT_DIVIDE_BY_ZERO:    return "float divide by zero";
				case EXCEPTION_PRIV_INSTRUCTION:      return "privileged instruction";
				case EXCEPTION_IN_PAGE_ERROR:         return "in-page error";
				case EXCEPTION_DATATYPE_MISALIGNMENT: return "datatype misalignment";
				default:                              return "exception";
			}
		}

		// Writes straight to stderr rather than through echo(): the ring
		// buffer and any log sink are the least trustworthy things in the
		// process at this point, and stderr is unbuffered.
		void Report(const char *line)
		{
			fputs(line, stderr);
			fputs("\n", stderr);
			fflush(stderr);
		}

		LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS *info)
		{
			const EXCEPTION_RECORD *rec = info->ExceptionRecord;

			{
				std::ostringstream s;
				s << "=== Pyros3D crashed: " << ExceptionName(rec->ExceptionCode)
				  << " (0x" << std::hex << rec->ExceptionCode << std::dec << ")"
				  << " at 0x" << std::hex << (uintptr_t)rec->ExceptionAddress << std::dec;
				// For an access violation the record says what was being
				// done and to which address - "execute at 0x0" is a null
				// function pointer, which is a different bug from a null
				// object read, and worth distinguishing without a debugger.
				if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2)
				{
					const ULONG_PTR op = rec->ExceptionInformation[0];
					s << " ("
					  << (op == 0 ? "read" : op == 1 ? "write" : op == 8 ? "execute" : "access")
					  << " at 0x" << std::hex << (uintptr_t)rec->ExceptionInformation[1] << std::dec
					  << ")";
				}
				s << " ===";
				Report(s.str().c_str());
			}

			const HANDLE process = GetCurrentProcess();
			SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
			SymInitialize(process, NULL, TRUE);

			CONTEXT ctx = *info->ContextRecord;
			STACKFRAME64 frame = {};
			frame.AddrPC.Mode = frame.AddrFrame.Mode = frame.AddrStack.Mode = AddrModeFlat;
#if defined(_M_X64)
			const DWORD machine = IMAGE_FILE_MACHINE_AMD64;
			frame.AddrPC.Offset = ctx.Rip;
			frame.AddrFrame.Offset = ctx.Rbp;
			frame.AddrStack.Offset = ctx.Rsp;
#elif defined(_M_ARM64)
			const DWORD machine = IMAGE_FILE_MACHINE_ARM64;
			frame.AddrPC.Offset = ctx.Pc;
			frame.AddrFrame.Offset = ctx.Fp;
			frame.AddrStack.Offset = ctx.Sp;
#else
			const DWORD machine = IMAGE_FILE_MACHINE_I386;
			frame.AddrPC.Offset = ctx.Eip;
			frame.AddrFrame.Offset = ctx.Ebp;
			frame.AddrStack.Offset = ctx.Esp;
#endif

			// SYMBOL_INFO carries the name inline past the end of the struct.
			char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
			SYMBOL_INFO *symbol = (SYMBOL_INFO*)symbolBuffer;
			symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
			symbol->MaxNameLen = MAX_SYM_NAME;

			for (int depth = 0; depth < 48; depth++)
			{
				if (!StackWalk64(machine, process, GetCurrentThread(), &frame, &ctx,
				                 NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
					break;
				if (frame.AddrPC.Offset == 0)
					break;

				std::ostringstream s;
				s << "  [" << depth << "] 0x" << std::hex << (uintptr_t)frame.AddrPC.Offset << std::dec;

				DWORD64 displacement = 0;
				if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol))
					s << "  " << symbol->Name << " + 0x" << std::hex << displacement << std::dec;
				else
					s << "  (no symbol - is the .pdb next to the executable?)";

				IMAGEHLP_LINE64 line = {};
				line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
				DWORD lineDisplacement = 0;
				if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisplacement, &line))
					s << "  (" << line.FileName << ":" << line.LineNumber << ")";

				Report(s.str().c_str());
			}

			SymCleanup(process);
			Report("=== end of stack. Please include everything above when reporting this. ===");

			// Let Windows carry on with its own error handling, so a
			// configured crash dump is still written.
			return EXCEPTION_CONTINUE_SEARCH;
		}
	}

	void InstallCrashHandler()
	{
		SetUnhandledExceptionFilter(OnUnhandledException);
	}

}

#else

namespace p3d {
	// POSIX already prints "Segmentation fault" and can leave a core file.
	void InstallCrashHandler() {}
}

#endif
