//============================================================================
// Name        : StringID.hpp
// Author      : Nuno Silva
// Version     :
// Copyright   : 
// Description : StringID
//============================================================================

#pragma once
#include <iostream>

namespace Fishy3D
{
	typedef unsigned long StringID;

	StringID MakeStringID(const std::string &Name);
	std::string GetStringIDString(StringID ID);
};
