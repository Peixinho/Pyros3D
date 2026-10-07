//============================================================================
// Name        : AntiAliasing.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See AntiAliasing.h.
//============================================================================

#include <Pyros3D/Rendering/PostEffects/AntiAliasing.h>
#include <algorithm>
#include <cctype>

namespace p3d {

	namespace AntiAliasing {

		std::string ToString(const AntiAliasingMode mode)
		{
			switch (mode)
			{
			case AntiAliasingMode::FXAA: return "fxaa";
			case AntiAliasingMode::SMAA: return "smaa";
			case AntiAliasingMode::TAA: return "taa";
			case AntiAliasingMode::MSAA2x: return "msaa2x";
			case AntiAliasingMode::MSAA4x: return "msaa4x";
			case AntiAliasingMode::MSAA8x: return "msaa8x";
			case AntiAliasingMode::Off:
			default: return "off";
			}
		}

		bool FromString(const std::string &name, AntiAliasingMode &out)
		{
			std::string n = name;
			std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return (char)std::tolower(c); });
			const std::vector<AntiAliasingMode> &all = All();
			for (size_t i = 0; i < all.size(); i++)
			{
				if (ToString(all[i]) == n)
				{
					out = all[i];
					return true;
				}
			}
			return false;
		}

		const char* DisplayName(const AntiAliasingMode mode)
		{
			switch (mode)
			{
			case AntiAliasingMode::FXAA: return "FXAA";
			case AntiAliasingMode::SMAA: return "SMAA";
			case AntiAliasingMode::TAA: return "TAA (temporal)";
			case AntiAliasingMode::MSAA2x: return "MSAA 2x";
			case AntiAliasingMode::MSAA4x: return "MSAA 4x";
			case AntiAliasingMode::MSAA8x: return "MSAA 8x";
			case AntiAliasingMode::Off:
			default: return "Off";
			}
		}

		const std::vector<AntiAliasingMode> &All()
		{
			static const std::vector<AntiAliasingMode> all = {
				AntiAliasingMode::Off, AntiAliasingMode::FXAA, AntiAliasingMode::SMAA, AntiAliasingMode::TAA,
				AntiAliasingMode::MSAA2x, AntiAliasingMode::MSAA4x, AntiAliasingMode::MSAA8x
			};
			return all;
		}

		bool IsMSAA(const AntiAliasingMode mode)
		{
			return mode == AntiAliasingMode::MSAA2x || mode == AntiAliasingMode::MSAA4x || mode == AntiAliasingMode::MSAA8x;
		}

		uint32 SampleCount(const AntiAliasingMode mode)
		{
			switch (mode)
			{
			case AntiAliasingMode::MSAA2x: return 2;
			case AntiAliasingMode::MSAA4x: return 4;
			case AntiAliasingMode::MSAA8x: return 8;
			default: return 1;
			}
		}

		AntiAliasingMode Resolve(const AntiAliasingMode requested, const bool deferred, const uint32 maxSamples)
		{
#if defined(METAL_BACKEND)
			// On the Metal backend the velocity pass does not come out still
			// for a still picture, and TAA built on it never settles: grain
			// standing, streaks at edges. Until that pass is right there, TAA
			// asked for on Metal is FXAA. (Vulkan - MoltenVK on a Mac
			// included - is not affected.)
			if (requested == AntiAliasingMode::TAA)
				return AntiAliasingMode::FXAA;
#endif
			if (!IsMSAA(requested))
				return requested;
			if (deferred || maxSamples < 2)
				return AntiAliasingMode::SMAA;
			if (SampleCount(requested) <= maxSamples)
				return requested;
			if (maxSamples >= 4)
				return AntiAliasingMode::MSAA4x;
			return AntiAliasingMode::MSAA2x;
		}

		std::vector<AntiAliasingMode> Supported(const bool deferred, const uint32 maxSamples)
		{
			std::vector<AntiAliasingMode> out;
			const std::vector<AntiAliasingMode> &all = All();
			for (size_t i = 0; i < all.size(); i++)
				if (Resolve(all[i], deferred, maxSamples) == all[i])
					out.push_back(all[i]);
			return out;
		}

		std::string FallbackReason(const AntiAliasingMode requested, const bool deferred, const uint32 maxSamples)
		{
			const AntiAliasingMode effective = Resolve(requested, deferred, maxSamples);
			if (effective == requested)
				return std::string();
			const std::string from = DisplayName(requested), to = DisplayName(effective);
			if (requested == AntiAliasingMode::TAA)
				return from + " does not settle on the Metal backend yet - using " + to;
			if (deferred)
				return from + " is not available under the deferred renderer - using " + to;
			if (maxSamples < 2)
				return from + " is not supported by this device - using " + to;
			return from + " is above this device's limit - using " + to;
		}
	}

}
