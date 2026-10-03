//============================================================================
// Name        : ProcessLauncher.h
// Author      : Duarte Peixinho
// Description : Starting another program from the editor and leaving it
//               running - a game client or a dedicated server launched for a
//               multiplayer test. Not on the web build, which cannot spawn.
//============================================================================

#ifndef PROCESSLAUNCHER_H
#define PROCESSLAUNCHER_H

#include <string>
#include <vector>

namespace ProcessLauncher
{
	// Runs `exe` with `args`, working directory `cwd`, its output appended
	// to `logPath` (where supported). The process id, or 0 on failure.
	long Launch(const std::string &exe, const std::vector<std::string> &args, const std::string &cwd,
		const std::string &logPath, std::string &error);
	bool IsRunning(const long pid);
	void Terminate(const long pid);
}

#endif
