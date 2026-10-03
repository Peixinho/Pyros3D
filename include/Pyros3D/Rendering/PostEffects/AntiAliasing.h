//============================================================================
// Name        : AntiAliasing.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Anti-aliasing modes and what each renderer/device can run.
//============================================================================

#ifndef ANTIALIASING_H
#define ANTIALIASING_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Core/Math/Math.h>
#include <string>
#include <vector>

namespace p3d {

	// One setting, not a stack of toggles: the modes are alternatives, and
	// several of them cannot be combined anyway (TAA over MSAA reprojects
	// edges MSAA already resolved; FXAA over SMAA blurs what SMAA kept).
	//
	// Appended, never reordered - the value is what a script gets back from
	// the numeric getters and what tests compare against.
	enum class AntiAliasingMode {
		Off = 0,
		FXAA,
		SMAA,
		TAA,
		MSAA2x,
		MSAA4x,
		MSAA8x
	};

	namespace AntiAliasing {

		// What a project, a game.json and a script spell a mode as.
		// "off", "fxaa", "smaa", "taa", "msaa2x", "msaa4x", "msaa8x".
		PYROS3D_API std::string ToString(const AntiAliasingMode mode);
		// Case-insensitive. Returns false and leaves `out` alone for anything
		// else, so a caller decides what an unknown name falls back to.
		PYROS3D_API bool FromString(const std::string &name, AntiAliasingMode &out);
		// For a settings menu.
		PYROS3D_API const char* DisplayName(const AntiAliasingMode mode);
		// Every mode, in the order a menu should list them.
		PYROS3D_API const std::vector<AntiAliasingMode> &All();

		PYROS3D_API bool IsMSAA(const AntiAliasingMode mode);
		PYROS3D_API uint32 SampleCount(const AntiAliasingMode mode);

		// What runs when `requested` is asked for. Two things can stand in
		// the way:
		//
		//  - The deferred renderer. Its lighting runs once per G-buffer
		//    texel, so a multisample G-buffer would mean lighting every
		//    sample - and the G-buffer is MRT, four times the memory. That is
		//    a property of deferred shading, not a missing feature, so MSAA
		//    under it becomes SMAA: the post-process mode closest to MSAA's
		//    look, with no temporal smear.
		//  - The device. maxSamples is IRenderDevice::GetMaxSamples(); a
		//    count above it comes down to the highest one that fits, and with
		//    no MSAA at all (1) it falls back exactly as deferred does.
		PYROS3D_API AntiAliasingMode Resolve(const AntiAliasingMode requested, const bool deferred, const uint32 maxSamples);

		// The modes Resolve() returns unchanged - what an options menu should
		// offer, so the player never picks something that is quietly
		// replaced.
		PYROS3D_API std::vector<AntiAliasingMode> Supported(const bool deferred, const uint32 maxSamples);

		// Why Resolve() changed `requested`, for the one log line that says
		// so. Empty when it did not.
		PYROS3D_API std::string FallbackReason(const AntiAliasingMode requested, const bool deferred, const uint32 maxSamples);
	}

}

#endif /* ANTIALIASING_H */
