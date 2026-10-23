//============================================================================
// Name        : AppleDisplay.mm
// Author      : Duarte Peixinho
// Description : What the Mac's displays can show past SDR white (EDR), for
//               HDR output on any backend - see IRenderDevice::GetHDRHeadroom.
//============================================================================

#import <AppKit/AppKit.h>

// How many times SDR white the brightest screen can show now (it moves with the brightness, and rises
// only while something on it asks for EDR). 1 where none can.
extern "C" float PyrosAppleDisplayHeadroom(void)
{
	@autoreleasepool
	{
		CGFloat best = 1.0;
		for (NSScreen* s in [NSScreen screens])
			if (s.maximumExtendedDynamicRangeColorComponentValue > best) best = s.maximumExtendedDynamicRangeColorComponentValue;
		return (float)best;
	}
}
