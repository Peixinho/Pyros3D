//============================================================================
// Name        : Builder.cpp
// Author      : Duarte Peixinho 
// Version     :
// Copyright   : ( ?° ?? ?°)
// Description : Pyros Builder
//============================================================================

#include "ReadDirectory.h"
#include <iostream>

#ifdef _WIN32
std::vector<const char*> AvailableDrives()
{
	std::vector<const char*> result;
	DWORD bitmask = GetLogicalDrives();
	std::vector<const char*>driveList;
	driveList = { "a:", "b:", "c:", "d:", "e:", "f:", "g:", "h:", "i:", "j:", "k:", "l:", "m:", "n:", "o:", "p:", "q:", "r:", "s:", "t:", "u:", "v:", "w:", "x:", "y:", "z:" };
	for (int i = 0; i < driveList.size(); i++)
	{
		if ((bitmask & (1 << i)) != 0) //Shift bitmask and if 0 drive is free
		{
			result.push_back(driveList.at(i)); // String of the free drive.
		}
	}
	return result;
}
#endif

std::string ExePath()
{
#ifdef _WIN32 // Windows
	char buffer[MAX_PATH];
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");
	std::string result = std::string(buffer).substr(0, pos+1);
	std::replace(result.begin(), result.end(), '\\', '/');
	return result;
#elif defined(__APPLE__)
	// /proc/self/exe is Linux-only. On macOS readlink() there fails and
	// leaves `buffer` uninitialized - the old code then built a
	// std::string from that garbage and returned it as the starting
	// directory, which is a good part of why this browser did not work
	// here.
	char buffer[PATH_MAX];
	uint32_t size = sizeof(buffer);
	if (_NSGetExecutablePath(buffer, &size) != 0) return std::string("./");
	char resolved[PATH_MAX];
	if (realpath(buffer, resolved) == NULL) return std::string("./");
	std::string result(resolved);
	std::string::size_type pos = result.find_last_of("/");
	return pos == std::string::npos ? std::string("./") : result.substr(0, pos + 1);
#else
	char buffer[PATH_MAX];
	ssize_t count = readlink("/proc/self/exe", buffer, PATH_MAX);
	if (count <= 0) return std::string("./");
	buffer[count] = '\0';
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");
	if (pos == std::string::npos) return std::string("./");
	std::string result = std::string(buffer).substr(0, pos + 1);
	return result;
#endif
}

namespace __READFILES {

	std::vector<std::string> explode(std::string const & s, char delim)
	{
		std::vector<std::string> result;
		std::istringstream iss(s);

		for (std::string token; std::getline(iss, token, delim); )
		{
			std::transform(token.begin(), token.end(), token.begin(), ::tolower);
			result.push_back(token);
		}

		return result;
	}

};