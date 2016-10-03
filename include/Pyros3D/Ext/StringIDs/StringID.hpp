//============================================================================
// Name        : StringID.hpp
// Author      : Nuno Silva
// Version     :
// Copyright   : 
// Description : StringID
//============================================================================

#pragma once
#include <iostream>
#include <Pyros3D/Core/Math/Math.h>

namespace p3d
{
	typedef uint32 StringID;

	StringID PYROS3D_API MakeStringID(const std::string &Name);
	StringID PYROS3D_API MakeStringIDFromChar(const uchar* data, const uint32 length);
	std::string PYROS3D_API GetStringIDString(StringID ID);
};
