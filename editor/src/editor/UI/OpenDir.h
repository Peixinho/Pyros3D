//============================================================================
// Name        : OPENDIR.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : OPENDIR
//============================================================================

#ifndef OPENDIR_H
#define	OPENDIR_H

#include "IUInterface.h"
#include "../Utils/ReadDirectory.h"
#include <string>

using namespace p3d;

namespace ImGui {

	namespace _priv {

		void OpenLocation(const std::string &path, const std::string &extension, bool* Open);

	};

	bool FilePath(const std::string &label, const std::string &path, const std::string &extension, std::string* buffer, const uint32 bufferSize, bool* Open);
};
#endif	/* OPENDIR_H */
