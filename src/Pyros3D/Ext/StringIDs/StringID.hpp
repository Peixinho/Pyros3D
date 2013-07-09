//============================================================================
// Name        : StringID.hpp
// Author      : Nuno Silva
// Version     :
// Copyright   : 
// Description : StringID
//============================================================================

#pragma once
#include <iostream>

namespace p3d
{
	typedef unsigned long StringID;

	StringID MakeStringID(const std::string &Name);
	std::string GetStringIDString(StringID ID);
};
