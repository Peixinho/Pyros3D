//============================================================================
// Name        : FileDropQueue.h
// Description : Cross-backend queue for SDL_DROPFILE paths (editor assets DnD)
//============================================================================

#ifndef FILEDROPQUEUE_H
#define FILEDROPQUEUE_H

#include <string>
#include <vector>
#include <mutex>

namespace FileDropQueue {

inline std::mutex& Mutex()
{
	static std::mutex m;
	return m;
}

inline std::vector<std::string>& Paths()
{
	static std::vector<std::string> paths;
	return paths;
}

inline void Push(const char* path)
{
	if (!path || !path[0]) return;
	std::lock_guard<std::mutex> lock(Mutex());
	Paths().push_back(std::string(path));
}

inline void Drain(std::vector<std::string>& out)
{
	std::lock_guard<std::mutex> lock(Mutex());
	out.swap(Paths());
	Paths().clear();
}

}

#endif /* FILEDROPQUEUE_H */
