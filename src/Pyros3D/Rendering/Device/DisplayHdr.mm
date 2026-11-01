//============================================================================
// Name        : DisplayHdr.mm
// Description : macOS's EDR headroom, for every backend's HDR output
//============================================================================

#include <Pyros3D/Rendering/Device/DisplayHdr.h>
#include <TargetConditionals.h>
#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#endif
#include <algorithm>

namespace p3d { namespace gfx {

	float DisplayHeadroom()
	{
#if TARGET_OS_OSX
		NSScreen* screen = NSScreen.mainScreen;
		return screen ? std::max(1.f, (float)screen.maximumExtendedDynamicRangeColorComponentValue) : 1.f;
#else
		return 1.f;
#endif
	}

} }
