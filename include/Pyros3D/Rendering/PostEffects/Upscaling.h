//============================================================================
// Name        : Upscaling.h
// Description : What brings a frame rendered below the size it is shown at
//               up to that size, and how far below it is rendered. One list
//               for the engine, a project's settings, a game's options menu
//               and the editor: each names a choice, and the machine it runs
//               on decides what that comes to (Resolve), as the
//               anti-aliasing modes do.
//============================================================================

#ifndef UPSCALING_H
#define UPSCALING_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>
#include <string>
#include <vector>

namespace p3d {

	// Appended, never reordered.
	enum class UpscalerMode {
		Off = 0,     // a plain stretch
		Sharp,       // the engine's own filter: one pass, next to no cost (SharpUpscaleEffect)
		FSR1,        // AMD FidelityFX Super Resolution 1.0, spatial (FsrEffect)
		FSR3,        // AMD FSR 3.1, temporal - through AMD's library, where there is one
		MetalFX      // Apple's temporal upscaler, on the Metal backend
	};

	// How far below the size shown the scene is rendered.
	enum class UpscaleQuality {
		Native = 0,  // 100%
		Quality,     // 67%
		Balanced,    // 59%
		Performance, // 50%
		Auto         // moved by the application to hold its frame rate
	};

	namespace Upscaling {

		// "off", "sharp", "fsr1", "fsr3", "metalfx" - what a project, a
		// game.json and a script spell them as.
		PYROS3D_API std::string ToString(const UpscalerMode mode);
		PYROS3D_API bool FromString(const std::string &name, UpscalerMode &out);
		PYROS3D_API const char* DisplayName(const UpscalerMode mode);
		PYROS3D_API const std::vector<UpscalerMode> &All();
		// Whether it reads the frame's history (motion vectors, depth, a
		// jittered camera) - and so stands in for anti-aliasing as well.
		PYROS3D_API bool IsTemporal(const UpscalerMode mode);

		// Whether this build, on this machine, can run it at all.
		PYROS3D_API bool IsAvailable(const UpscalerMode mode);
		// What runs when `requested` is asked for: itself where it is
		// available, else the nearest that is (a temporal one comes down to
		// FSR 1, FSR 1 to Sharp, Sharp is always there).
		PYROS3D_API UpscalerMode Resolve(const UpscalerMode requested);
		// The ones Resolve() returns unchanged: what an options menu lists.
		PYROS3D_API std::vector<UpscalerMode> Supported();
		// Why Resolve() changed `requested`, for the one log line that says so. Empty when it did not.
		PYROS3D_API std::string FallbackReason(const UpscalerMode requested);

		// "native", "quality", "balanced", "performance", "auto".
		PYROS3D_API std::string ToString(const UpscaleQuality quality);
		PYROS3D_API bool FromString(const std::string &name, UpscaleQuality &out);
		PYROS3D_API const char* DisplayName(const UpscaleQuality quality);
		PYROS3D_API const std::vector<UpscaleQuality> &AllQualities();
		// The share of each side that is rendered; 0 for Auto (the caller's to move).
		PYROS3D_API f32 Scale(const UpscaleQuality quality);
	}

}

#endif /* UPSCALING_H */
