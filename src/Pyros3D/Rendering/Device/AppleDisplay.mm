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

#if !TARGET_OS_IPHONE
#include <CoreVideo/CoreVideo.h>
#include <atomic>
#include <cstdlib>
#include <chrono>

// Frames begun on the display's refresh, for any backend on a Mac. Begun when the window server hands a
// drawable back (Metal's nextDrawable, MoltenVK's acquire), they came unevenly - two drawables at once now
// and then - so a still scene's frames went 33 ms, then 0.8, then 16.7 apart though the display showed one
// every 16.7: a zig-zag in the profiler's graph, and each frame's time step with it (on Vulkan the device
// thread passed it on to the main thread as 14-21 ms frames). A display link ticks a semaphore each refresh,
// on the display the window is on; a frame waits for a tick, unless one came since the last frame began (a
// frame that ran long loses nothing more). Called by RefreshPacer::Wait - see RefreshPacer.h.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
static dispatch_semaphore_t g_refreshTicks = nil;
static CVDisplayLinkRef g_refreshLink = NULL;
static std::atomic<uint32_t> g_windowDisplay(0);
static uint32_t g_linkDisplay = 0, g_refreshCheck = 0;

static CVReturn PyrosRefreshTick(CVDisplayLinkRef, const CVTimeStamp*, const CVTimeStamp*, CVOptionFlags, CVOptionFlags*, void*)
{
	dispatch_semaphore_signal(g_refreshTicks);
	return kCVReturnSuccess;
}

// layer: the CAMetalLayer drawn into, when the caller has it (else the app's key window). Called from
// the thread that begins frames - the device thread, for Vulkan: AppKit is asked on the main thread,
// without waiting for it. Returns the milliseconds waited.
extern "C" double PyrosAppleWaitForRefresh(void* layer)
{
	// (whether to wait at all: RefreshPacer::Wait, which calls this)
	if (g_refreshTicks == nil)
	{
		if (CVDisplayLinkCreateWithActiveCGDisplays(&g_refreshLink) != kCVReturnSuccess || g_refreshLink == NULL) { g_refreshLink = NULL; return 0.0; }
		g_refreshTicks = dispatch_semaphore_create(0);
		CVDisplayLinkSetOutputCallback(g_refreshLink, PyrosRefreshTick, NULL);
		CVDisplayLinkStart(g_refreshLink);
	}
	// (the display the window is on, looked at now and then: the refreshes are that display's)
	if (g_refreshCheck-- == 0)
	{
		g_refreshCheck = 30;
		CALayer* l = (__bridge CALayer*)layer;
		dispatch_async(dispatch_get_main_queue(), ^{
			NSWindow* w = nil;
			if (l != nil && [l.delegate isKindOfClass:[NSView class]]) w = ((NSView*)l.delegate).window;
			if (w == nil) w = [NSApp keyWindow] ? [NSApp keyWindow] : [NSApp mainWindow];
			NSScreen* s = w != nil ? w.screen : nil;
			if (s != nil) g_windowDisplay.store([s.deviceDescription[@"NSScreenNumber"] unsignedIntValue]);
		});
	}
	const uint32_t display = g_windowDisplay.load();
	if (display != 0 && display != g_linkDisplay) { CVDisplayLinkSetCurrentCGDisplay(g_refreshLink, display); g_linkDisplay = display; }
	const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
	// (not forever, should the display stop ticking - asleep, unplugged)
	dispatch_semaphore_wait(g_refreshTicks, dispatch_time(DISPATCH_TIME_NOW, 50 * NSEC_PER_MSEC));
	while (dispatch_semaphore_wait(g_refreshTicks, DISPATCH_TIME_NOW) == 0) {}
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}
#pragma clang diagnostic pop
#endif
