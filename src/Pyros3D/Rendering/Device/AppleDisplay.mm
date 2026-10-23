//============================================================================
// Name        : AppleDisplay.mm
// Author      : Duarte Peixinho
// Description : What the Mac's displays can show past SDR white (EDR), for
//               HDR output on any backend - see IRenderDevice::GetHDRHeadroom.
//============================================================================

#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif

// How many times SDR white the brightest screen can show now (it moves with the brightness, and rises
// only while something on it asks for EDR). 1 where none can.
extern "C" float PyrosAppleDisplayHeadroom(void)
{
	@autoreleasepool
	{
		CGFloat best = 1.0;
#if TARGET_OS_IPHONE
		// (an iPhone's screen: what it can show now, iOS 16 on)
		if (@available(iOS 16.0, *)) best = [UIScreen mainScreen].currentEDRHeadroom;
#else
		for (NSScreen* s in [NSScreen screens])
			if (s.maximumExtendedDynamicRangeColorComponentValue > best) best = s.maximumExtendedDynamicRangeColorComponentValue;
#endif
		return (float)(best > 1.0 ? best : 1.0);
	}
}
