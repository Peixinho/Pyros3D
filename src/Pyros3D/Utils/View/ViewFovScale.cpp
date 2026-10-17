//============================================================================
// Name        : ViewFovScale.cpp
// Description : See ViewFovScale.h
//============================================================================

#include <Pyros3D/Utils/View/ViewFovScale.h>
#include <algorithm>

namespace p3d {

	static f32 viewFovScale = 1.f;

	f32 GetViewFovScale() { return viewFovScale; }

	void SetViewFovScale(const f32 scale)
	{
		viewFovScale = std::max(0.1f, std::min(2.f, scale));
	}

}
