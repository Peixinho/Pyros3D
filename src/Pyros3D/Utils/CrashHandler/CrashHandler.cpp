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
#include <tlhelp32.h>
#include <atomic>
#include <cstdio>
#include <cstring>
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

		// Mirror of the report on disk. PyrosBuilder and PyrosPlayer are
		// GUI-subsystem executables, so a crash in one launched from Explorer
		// has no terminal to print to at all - the whole backtrace would go
		// into a closed handle and the application would just vanish, which
		// is the exact failure this handler exists to end. Opened only once a
		// crash is actually happening, so a healthy run leaves no file.
		FILE *crashFile = NULL;
		char crashFilePath[MAX_PATH] = {};

		// Next to the executable, which is where someone looking for it will
		// look. Falls back to the working directory if that is not writable
		// (an install under Program Files).
		DWORD mainThreadId = 0;

		void OpenReportFile(const char *name, const char *mode);
		void OpenCrashFile() { OpenReportFile("PyrosCrash.txt", "w"); }
		void OpenReportFile(const char *name, const char *mode)
		{
			char path[MAX_PATH] = {};
			const DWORD length = GetModuleFileNameA(NULL, path, MAX_PATH);
			if (length > 0 && length < MAX_PATH)
			{
				char *slash = strrchr(path, '\\');
				if (slash)
				{
					// Room for the name plus its terminator, or leave it.
					if ((size_t)(slash - path) + strlen(name) + 2 < MAX_PATH)
					{
						slash[1] = 0;
						strcat(path, name);
						crashFile = fopen(path, mode);
						if (crashFile != NULL) strcpy(crashFilePath, path);
					}
				}
			}
			if (crashFile == NULL)
			{
				crashFile = fopen(name, mode);
				if (crashFile != NULL) strcpy(crashFilePath, name);
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

			if (crashFile != NULL)
			{
				fputs(line, crashFile);
				fputs("\n", crashFile);
				// Flushed per line: the process is going down and may well
				// take a second fault with it, and a truncated report still
				// names the frames it got to.
				fflush(crashFile);
			}
		}

		// One line of a stack: module+offset always, a name where there is one to
		// trust. True if the module had real symbols.
		bool ReportAddress(const HANDLE process, const int depth, const DWORD64 pc)
		{
			char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
			SYMBOL_INFO *symbol = (SYMBOL_INFO*)symbolBuffer;
			symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
			symbol->MaxNameLen = MAX_SYM_NAME;

			DWORD64 displacement = 0;
			const bool haveName = SymFromAddr(process, pc, &displacement, symbol) != FALSE;

			// Must come after SymFromAddr: SYMOPT_DEFERRED_LOADS means the
			// module's symbols are not even looked at until something asks
			// for them, and SymType reads back as SymDeferred until then.
			IMAGEHLP_MODULE64 mod = {};
			mod.SizeOfStruct = sizeof(mod);
			const bool haveModule = SymGetModuleInfo64(process, pc, &mod) != FALSE;
			// Only a PDB gives real function boundaries. SymExport means
			// dbghelp had nothing but the DLL's export table and picked
			// the nearest preceding exported symbol - which, in a module
			// that exports a few hundred names across megabytes of code,
			// is usually a DIFFERENT function that happens to sit earlier
			// in the image. Those names have sent more than one crash
			// report off after the wrong subsystem entirely, so they are
			// labelled as guesses here rather than printed as fact.
			const bool haveRealSymbols = haveModule && (mod.SymType == SymPdb || mod.SymType == SymDia);

			std::ostringstream s;
			s << "  [" << depth << "] ";
			// Module + RVA first, and always. This is the one part of the
			// line that survives having no symbols at all: it can be
			// resolved after the fact against a matching PDB, which the
			// absolute address cannot (ASLR moved the module).
			if (haveModule)
				s << mod.ModuleName << "+0x" << std::hex
				  << (uintptr_t)(pc - mod.BaseOfImage) << std::dec;
			else
				s << "0x" << std::hex << (uintptr_t)pc << std::dec;

			if (haveName && haveRealSymbols)
			{
				s << "  " << symbol->Name << " + 0x" << std::hex << displacement << std::dec;

				IMAGEHLP_LINE64 line = {};
				line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
				DWORD lineDisplacement = 0;
				if (SymGetLineFromAddr64(process, pc, &lineDisplacement, &line))
					s << "  (" << line.FileName << ":" << line.LineNumber << ")";
			}
			// A nearest-export guess is only worth showing when the hit is
			// close enough that it is plausibly the same function. Past a
			// few hundred bytes it is noise dressed up as information.
			else if (haveName && displacement < 0x200)
				s << "  (no pdb; near export " << symbol->Name << " + 0x"
				  << std::hex << displacement << std::dec << ")";
			else
				s << "  (no pdb)";

			Report(s.str().c_str());
			return haveRealSymbols;
		}

		LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS *info)
		{
			const EXCEPTION_RECORD *rec = info->ExceptionRecord;

			OpenCrashFile();

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

#if defined(_M_X64)
			// A call through a null (or wild) function pointer faults AT the bad
			// address: there is no code there to unwind from, the walk stopped at
			// once and the report was a header with nothing under it. The call has
			// pushed where it came from, though: that is taken off the stack, and
			// the walk starts from whoever made the call.
			if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2
				&& rec->ExceptionInformation[0] == 8 && (DWORD64)rec->ExceptionInformation[1] == ctx.Rip)
			{
				DWORD64 from = 0;
				if (ReadProcessMemory(process, (LPCVOID)ctx.Rsp, &from, sizeof(from), NULL) && from != 0)
				{
					Report("  (a call through a bad function pointer: what follows is who made the call)");
					ctx.Rip = from;
					ctx.Rsp += 8;
					frame.AddrPC.Offset = ctx.Rip;
					frame.AddrStack.Offset = ctx.Rsp;
				}
			}
#endif
			{
				std::ostringstream t;
				t << "  thread " << GetCurrentThreadId() << (GetCurrentThreadId() == mainThreadId ? " (the main thread)" : " (not the main thread)");
				Report(t.str().c_str());
			}

			bool anyModuleUnsymbolized = false;

			for (int depth = 0; depth < 48; depth++)
			{
				if (!StackWalk64(machine, process, GetCurrentThread(), &frame, &ctx,
				                 NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
					break;
				if (frame.AddrPC.Offset == 0)
					break;

				if (!ReportAddress(process, depth, frame.AddrPC.Offset))
					anyModuleUnsymbolized = true;
			}

			SymCleanup(process);
			if (anyModuleUnsymbolized)
			{
				Report("=== Frames marked (no pdb) have NO function names: only the module+offset");
				Report("=== on each line is meaningful. Drop the matching .pdb files next to the");
				Report("=== .exe and the .dll (the release's symbols archive mirrors the package");
				Report("=== layout) and reproduce again for a trace with real names.");
			}
			Report("=== end of stack. Please include everything above when reporting this. ===");
			if (crashFile != NULL)
			{
				fclose(crashFile);
				crashFile = NULL;
				// Named in full rather than described: the fallback location
				// is the working directory, which is not necessarily where
				// the .exe is, and "go and find it" is a poor thing to leave
				// someone with at this point.
				std::ostringstream s;
				s << "=== This report was also written to " << crashFilePath << " ===";
				Report(s.str().c_str());
			}

			// Let Windows carry on with its own error handling, so a
			// configured crash dump is still written.
			return EXCEPTION_CONTINUE_SEARCH;
		}
	}

	namespace {
		std::atomic<unsigned long long> beats(0);
		unsigned hangSeconds = 8;

		// Where a thread is, without asking it: its registers are read while it is
		// held still and the stack is unwound from them. Nothing here allocates or
		// takes the symbol engine's lock - the thread held still may be the one
		// that has it.
		int CaptureThread(const HANDLE thread, DWORD64 *pcs, const int most)
		{
			int n = 0;
#if defined(_M_X64)
			__try
			{
				CONTEXT ctx;
				memset(&ctx, 0, sizeof(ctx));
				ctx.ContextFlags = CONTEXT_FULL;
				if (!GetThreadContext(thread, &ctx)) return 0;
				while (n < most && ctx.Rip != 0)
				{
					pcs[n++] = ctx.Rip;
					DWORD64 base = 0;
					PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(ctx.Rip, &base, NULL);
					if (fn == NULL)
					{
						// (a leaf: where it returns to is on top of its stack)
						DWORD64 from = 0;
						if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)ctx.Rsp, &from, sizeof(from), NULL)) break;
						ctx.Rip = from;
						ctx.Rsp += 8;
					}
					else
					{
						PVOID handlerData = NULL;
						DWORD64 establisher = 0;
						RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, ctx.Rip, fn, &ctx, &handlerData, &establisher, NULL);
					}
				}
			}
			__except (EXCEPTION_EXECUTE_HANDLER) {}
#else
			(void)thread; (void)pcs; (void)most;
#endif
			return n;
		}

		struct Held { DWORD id; int n; DWORD64 pcs[48]; };
		Held held[64];

		void ReportHang(const int seconds, const bool first)
		{
			int count = 0;
			const DWORD pid = GetCurrentProcessId(), me = GetCurrentThreadId();
			const HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
			if (snap == INVALID_HANDLE_VALUE) return;
			THREADENTRY32 te;
			te.dwSize = sizeof(te);
			if (Thread32First(snap, &te))
			{
				do
				{
					if (te.th32OwnerProcessID != pid || te.th32ThreadID == me || count >= 64) continue;
					const HANDLE t = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
					if (t == NULL) continue;
					if (SuspendThread(t) != (DWORD)-1)
					{
						held[count].id = te.th32ThreadID;
						held[count].n = CaptureThread(t, held[count].pcs, 48);
						count++;
						ResumeThread(t);
					}
					CloseHandle(t);
				} while (Thread32Next(snap, &te));
			}
			CloseHandle(snap);

			// (every thread is running again: names can be looked up now)
			OpenReportFile("PyrosHang.txt", first ? "w" : "a");
			{
				std::ostringstream s;
				s << "=== Pyros3D: no frame for " << seconds << " s. Where every thread is (a long load reads the same: then the main thread is in the loader) ===";
				Report(s.str().c_str());
			}
			const HANDLE process = GetCurrentProcess();
			SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
			SymInitialize(process, NULL, TRUE);
			for (int i = 0; i < count; i++)
			{
				std::ostringstream s;
				s << "--- thread " << held[i].id << (held[i].id == mainThreadId ? " (the main thread)" : "") << " ---";
				Report(s.str().c_str());
				for (int k = 0; k < held[i].n; k++) ReportAddress(process, k, held[i].pcs[k]);
			}
			SymCleanup(process);
			Report("=== end. Lines marked (no pdb) carry only module+offset, which is enough: please send this file. ===");
			if (crashFile != NULL) { fclose(crashFile); crashFile = NULL; }
		}

		DWORD WINAPI Watchdog(LPVOID)
		{
			unsigned long long seen = 0;
			unsigned still = 0;
			int reports = 0;
			bool told = false;
			for (;;)
			{
				Sleep(1000);
				const unsigned long long now = beats.load(std::memory_order_relaxed);
				if (now == 0) continue;
				if (now != seen) { seen = now; still = 0; told = false; continue; }
				if (++still < hangSeconds || told || reports >= 4) continue;
				told = true;
				reports++;
				ReportHang((int)still, reports == 1);
			}
		}
	}

	void InstallCrashHandler()
	{
		mainThreadId = GetCurrentThreadId();
		SetUnhandledExceptionFilter(OnUnhandledException);
	}

	void InstallHangWatchdog(const unsigned seconds)
	{
		static bool installed = false;
		if (installed) return;
		installed = true;
		hangSeconds = seconds < 2 ? 2 : seconds;
		if (mainThreadId == 0) mainThreadId = GetCurrentThreadId();
		const HANDLE t = CreateThread(NULL, 0, Watchdog, NULL, 0, NULL);
		if (t != NULL) CloseHandle(t);
	}

	void FrameHeartbeat() { beats.fetch_add(1, std::memory_order_relaxed); }

}

#else

namespace p3d {
	// POSIX already prints "Segmentation fault" and can leave a core file.
	void InstallCrashHandler() {}
	void InstallHangWatchdog(const unsigned) {}
	void FrameHeartbeat() {}
}

#endif
