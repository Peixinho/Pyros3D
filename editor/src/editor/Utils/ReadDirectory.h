//============================================================================
// Name        : ReadDirectory.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Read Directory
//============================================================================

#ifndef READDIRECTORY_H
#define READDIRECTORY_H

#include <string>
#include <vector>
#include <sstream>
#include <utility>
#include <algorithm>
#ifdef _WIN32
#include<Windows.h>
#else
#include <dirent.h>
#endif

#ifdef _WIN32 // Windows
#include <Windows.h>
std::vector<const char*> AvailableDrives();
#else
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#endif

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

std::string ExePath();

struct _FileInfo {
	std::string NamePrefix;
	std::string Name;
	std::string NameLowered;
	bool isFolder;

	bool operator<(const _FileInfo& a) const
	{
		std::string toUPName = Name, toUPa = a.Name;
		std::transform(toUPName.begin(), toUPName.end(), toUPName.begin(), ::toupper);
		std::transform(toUPa.begin(), toUPa.end(), toUPa.begin(), ::toupper);
		return toUPName < toUPa;
	}
};

namespace __READFILES {

	class ReadFiles {

	public:

		static std::vector<_FileInfo> OpenLocation(const std::string &Path = ".", bool ShowFolders = true, const std::string AllowedExtensions = "")
		{
			std::vector<_FileInfo> Contents;

#ifdef _WIN32 // Windows

			std::string path = Path + "*";
			WIN32_FIND_DATA data;
			std::wstring wPath(path.begin(), path.end());
			DWORD attr(::GetFileAttributesA(Path.c_str()));
			if (attr != 0xFFFFFFFF)
			{
				HANDLE hFile = FindFirstFile(wPath.c_str(), &data);

				while (FindNextFile(hFile, &data)) {
#ifdef UNICODE
					std::wstring wresult = data.cFileName;
					_FileInfo f;
					f.Name = std::string(wresult.begin(), wresult.end());
					f.isFolder = false;
					if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) f.isFolder = true;

#else
					std::string result = data.cFileName;
					_FileInfo f;
					f.Name = result;
					f.isFolder = false;
					if (data.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY) f.isFolder = true;

#endif

					f.NameLowered = f.Name;
					std::transform(f.NameLowered.begin(), f.NameLowered.end(), f.NameLowered.begin(), ::tolower);
					f.NamePrefix = (f.isFolder ? "[D]" : "[F]") + f.Name;
					if (!f.isFolder || (f.isFolder && ShowFolders))
						Contents.push_back(f);
				}

				FindClose(hFile);

			}

#else   // Mac OSX and Linux

			DIR*    dir;
			dirent* pdir;

			dir = opendir(Path.c_str());

			while (pdir = readdir(dir)) {
				_FileInfo f;
				f.Name = pdir->d_name;
				f.NameLowered = f.Name;
				std::transform(f.NameLowered.begin(), f.NameLowered.end(), f.NameLowered.begin(), ::tolower);

				// Check if is Folder or File
				std::string tempPath = Path + std::string("/") + f.Name;
				DIR *temp = opendir(tempPath.c_str());
				if (temp == NULL) f.isFolder = false;
				else f.isFolder = true;

				f.NamePrefix = (f.isFolder ? "[D]" : "[F]") + f.Name;

				if (!f.isFolder || (f.isFolder && ShowFolders))
					Contents.push_back(f);
			}

#endif

			std::sort(Contents.begin(), Contents.end());
			return Contents;
		}
	};

	std::vector<std::string> explode(std::string const & s, char delim);

};

class ReadDirectory {

public:

	ReadDirectory() {}

	std::vector<_FileInfo> Open(const std::string &path, const std::string &extension) // extensions "obj,dae,x"
	{

		std::vector<std::string> extensions;

		if (extension.size() > 0)
		{
			extensions = __READFILES::explode(extension, ',');
		}

		std::vector<_FileInfo> files = __READFILES::ReadFiles::OpenLocation(path);

		std::vector<_FileInfo> Files;

		#ifndef _WIN32
			if (path.compare(std::string("/"))!=0) {
		#endif
			_FileInfo f;
			f.isFolder = true;
			f.Name = "..";
			f.NameLowered = "..";
			Files.push_back(f);
		#ifndef _WIN32
			}
		#endif
		// Show Folders
		int countFolders = 0;
		for (std::vector<_FileInfo>::iterator i = files.begin(); i != files.end(); i++)
		{
			if ((*i).isFolder && (*i).Name.compare(std::string(".")) != 0 && (*i).Name.compare(std::string("..")) != 0) //std::cout << (*i).Name << std::endl;
			{
				Files.push_back((*i));
				countFolders++;
			}
		}

		// Show Files
		for (std::vector<_FileInfo>::iterator i = files.begin(); i != files.end(); i++)
		{
			if (!(*i).isFolder)
			{
				if (extensions.size() > 0)
				{
					for (int k = 0; k < extensions.size(); k++)
					{
						if (extensions[k].size() <= (*i).Name.size())
						{
							if ((*i).NameLowered.substr((*i).NameLowered.size() - extensions[k].size(), extensions[k].size()).compare(extensions[k]) == 0) //std::cout << (*i).Name << std::endl;
								Files.push_back((*i));
						}
					}
				}
				else {
					//std::cout << (*i).Name << std::endl;
					Files.push_back((*i));
				}
			}
		}
		return Files;
	}

private:

};

#endif /* READDIRECTORY_H */
