//============================================================================
// Name        : Easing.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Shared easing curves - see Easing.h
//============================================================================

#include <Pyros3D/Core/Math/Easing.h>

namespace p3d {

	const char* InterpolationModeName(const uchar mode)
	{
		switch (mode)
		{
		case INTERP_STEP:      return "Step";
		case INTERP_EASE_IN:   return "Ease In";
		case INTERP_EASE_OUT:  return "Ease Out";
		case INTERP_EASE_BOTH: return "Ease In/Out";
		case INTERP_BEZIER:    return "Bezier";
		case INTERP_LINEAR:
		default:               return "Linear";
		}
	}

}
