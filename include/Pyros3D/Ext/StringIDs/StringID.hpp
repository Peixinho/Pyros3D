//============================================================================
// Name        : StringID.hpp
// Author      : Nuno Silva
// Version     :
// Copyright   : 
// Description : StringID
//============================================================================

#pragma once
#include <iostream>
#include "../../Core/Math/Math.h"

namespace p3d
{
	typedef uint32 StringID;

	StringID MakeStringID(const std::string &Name);
	StringID MakeStringIDFromChar(const uchar* data, const uint32 &length);
	std::string GetStringIDString(StringID ID);
};
