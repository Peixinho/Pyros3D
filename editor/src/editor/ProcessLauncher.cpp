//============================================================================
// Name        : ProcessLauncher.cpp
// Author      : Duarte Peixinho
// Description : See ProcessLauncher.h.
//============================================================================

#include "ProcessLauncher.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <map>
#elif !defined(EMSCRIPTEN)
#include <csignal>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace ProcessLauncher
{
#if defined(_WIN32)
	static std::map<long, HANDLE> &Handles() { static std::map<long, HANDLE> h; return h; }

	long Launch(const std::string &exe, const std::vector<std::string> &args, const std::string &cwd,
		const std::string &logPath, std::string &error)
	{
		(void)logPath;	// a built game attaches to its own console
		std::string cmd = "\"" + exe + "\"";
		for (size_t i = 0; i < args.size(); i++) cmd += " \"" + args[i] + "\"";
		STARTUPINFOA si = {};
		si.cb = sizeof(si);
		PROCESS_INFORMATION pi = {};
		std::vector<char> line(cmd.begin(), cmd.end());
		line.push_back(0);
		if (!CreateProcessA(NULL, line.data(), NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL,
				cwd.empty() ? NULL : cwd.c_str(), &si, &pi))
		{
			error = "CreateProcess failed (" + std::to_string((unsigned long)GetLastError()) + ")";
			return 0;
		}
		CloseHandle(pi.hThread);
		Handles()[(long)pi.dwProcessId] = pi.hProcess;
		return (long)pi.dwProcessId;
	}

	bool IsRunning(const long pid)
	{
		std::map<long, HANDLE>::iterator it = Handles().find(pid);
		if (it == Handles().end()) return false;
		DWORD code = 0;
		return GetExitCodeProcess(it->second, &code) && code == STILL_ACTIVE;
	}

	void Terminate(const long pid)
	{
		std::map<long, HANDLE>::iterator it = Handles().find(pid);
		if (it == Handles().end()) return;
		TerminateProcess(it->second, 0);
		CloseHandle(it->second);
		Handles().erase(it);
	}
#elif defined(EMSCRIPTEN)
	long Launch(const std::string &, const std::vector<std::string> &, const std::string &, const std::string &, std::string &error)
	{
		error = "the web editor cannot start programs";
		return 0;
	}
	bool IsRunning(const long) { return false; }
	void Terminate(const long) {}
#else
	// Terminated but perhaps not exited yet (a server takes a moment to
	// shut down): reaped on later calls, so none lingers as a zombie.
	static std::vector<pid_t> &Pending() { static std::vector<pid_t> p; return p; }
	static void ReapPending()
	{
		std::vector<pid_t> &p = Pending();
		for (size_t i = p.size(); i-- > 0;)
		{
			int status = 0;
			if (waitpid(p[i], &status, WNOHANG) != 0) p.erase(p.begin() + i);
		}
	}

	long Launch(const std::string &exe, const std::vector<std::string> &args, const std::string &cwd,
		const std::string &logPath, std::string &error)
	{
		ReapPending();
		std::vector<char*> argv;
		argv.push_back(const_cast<char*>(exe.c_str()));
		for (size_t i = 0; i < args.size(); i++) argv.push_back(const_cast<char*>(args[i].c_str()));
		argv.push_back(NULL);
		const pid_t pid = fork();
		if (pid < 0) { error = "fork failed"; return 0; }
		if (pid == 0)
		{
			// The child: its own process group, so it outlives nothing it
			// should not and a Ctrl-C in the editor's terminal leaves it be.
			setpgid(0, 0);
			if (!cwd.empty() && chdir(cwd.c_str()) != 0) _exit(127);
			if (!logPath.empty())
			{
				const int fd = open(logPath.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
				if (fd >= 0) { dup2(fd, 1); dup2(fd, 2); close(fd); }
			}
			execv(exe.c_str(), argv.data());
			_exit(127);
		}
		return (long)pid;
	}

	bool IsRunning(const long pid)
	{
		ReapPending();
		if (pid <= 0) return false;
		int status = 0;
		// Reaps it once it has exited, so it does not linger as a zombie.
		if (waitpid((pid_t)pid, &status, WNOHANG) != 0) return false;
		return kill((pid_t)pid, 0) == 0;
	}

	void Terminate(const long pid)
	{
		if (pid <= 0) return;
		kill((pid_t)pid, SIGTERM);
		int status = 0;
		if (waitpid((pid_t)pid, &status, WNOHANG) == 0) Pending().push_back((pid_t)pid);
		ReapPending();
	}
#endif
}
