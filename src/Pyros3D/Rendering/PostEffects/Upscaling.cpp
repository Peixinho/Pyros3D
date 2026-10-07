//============================================================================
// Name        : Upscaling.cpp
// Description : See Upscaling.h
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Upscaling.h>
#include <Pyros3D/Rendering/PostEffects/Effects/FsrEffect.h>
#include <algorithm>
#include <cctype>

namespace p3d {

	namespace Upscaling {

		namespace {
			std::string Lower(const std::string &s)
			{
				std::string out = s;
				std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return (char)std::tolower(c); });
				return out;
			}
		}

		std::string ToString(const UpscalerMode mode)
		{
			switch (mode)
			{
			case UpscalerMode::Off: return "off";
			case UpscalerMode::Sharp: return "sharp";
			case UpscalerMode::FSR1: return "fsr1";
			case UpscalerMode::FSR3: return "fsr3";
			case UpscalerMode::MetalFX: return "metalfx";
			}
			return "sharp";
		}

		bool FromString(const std::string &name, UpscalerMode &out)
		{
			const std::string n = Lower(name);
			const std::vector<UpscalerMode> &all = All();
			for (size_t i = 0; i < all.size(); i++)
				if (ToString(all[i]) == n) { out = all[i]; return true; }
			if (n == "none" || n == "stretch") { out = UpscalerMode::Off; return true; }
			if (n == "fsr") { out = UpscalerMode::FSR1; return true; }
			return false;
		}

		const char* DisplayName(const UpscalerMode mode)
		{
			switch (mode)
			{
			case UpscalerMode::Off: return "Off";
			case UpscalerMode::Sharp: return "Sharp";
			case UpscalerMode::FSR1: return "AMD FSR 1";
			case UpscalerMode::FSR3: return "AMD FSR 3.1";
			case UpscalerMode::MetalFX: return "MetalFX";
			}
			return "Sharp";
		}

		const std::vector<UpscalerMode> &All()
		{
			static const std::vector<UpscalerMode> all = { UpscalerMode::Off, UpscalerMode::Sharp, UpscalerMode::FSR1, UpscalerMode::FSR3, UpscalerMode::MetalFX };
			return all;
		}

		bool IsTemporal(const UpscalerMode mode)
		{
			return mode == UpscalerMode::FSR3 || mode == UpscalerMode::MetalFX;
		}

		bool IsAvailable(const UpscalerMode mode)
		{
			switch (mode)
			{
			case UpscalerMode::Off:
			case UpscalerMode::Sharp:
				return true;
			case UpscalerMode::FSR1:
				return FsrEffect::SourcesPresent();
			// Neither has a backend in this engine yet. They are in the list so
			// that a project can already name them: it gets the nearest thing
			// until it can have the thing itself.
			case UpscalerMode::FSR3:
			case UpscalerMode::MetalFX:
				return false;
			}
			return false;
		}

		UpscalerMode Resolve(const UpscalerMode requested)
		{
			if (IsAvailable(requested)) return requested;
			if (IsTemporal(requested) && IsAvailable(UpscalerMode::FSR1)) return UpscalerMode::FSR1;
			return UpscalerMode::Sharp;
		}

		std::vector<UpscalerMode> Supported()
		{
			std::vector<UpscalerMode> out;
			const std::vector<UpscalerMode> &all = All();
			for (size_t i = 0; i < all.size(); i++)
				if (IsAvailable(all[i])) out.push_back(all[i]);
			return out;
		}

		std::string FallbackReason(const UpscalerMode requested)
		{
			const UpscalerMode effective = Resolve(requested);
			if (effective == requested) return std::string();
			return std::string(DisplayName(requested)) + " is not available on this machine - using " + DisplayName(effective);
		}

		std::string ToString(const UpscaleQuality quality)
		{
			switch (quality)
			{
			case UpscaleQuality::Native: return "native";
			case UpscaleQuality::Quality: return "quality";
			case UpscaleQuality::Balanced: return "balanced";
			case UpscaleQuality::Performance: return "performance";
			case UpscaleQuality::Auto: return "auto";
			}
			return "auto";
		}

		bool FromString(const std::string &name, UpscaleQuality &out)
		{
			const std::string n = Lower(name);
			const std::vector<UpscaleQuality> &all = AllQualities();
			for (size_t i = 0; i < all.size(); i++)
				if (ToString(all[i]) == n) { out = all[i]; return true; }
			return false;
		}

		const char* DisplayName(const UpscaleQuality quality)
		{
			switch (quality)
			{
			case UpscaleQuality::Native: return "Native (100%)";
			case UpscaleQuality::Quality: return "Quality (67%)";
			case UpscaleQuality::Balanced: return "Balanced (59%)";
			case UpscaleQuality::Performance: return "Performance (50%)";
			case UpscaleQuality::Auto: return "Auto";
			}
			return "Auto";
		}

		const std::vector<UpscaleQuality> &AllQualities()
		{
			static const std::vector<UpscaleQuality> all = { UpscaleQuality::Native, UpscaleQuality::Quality, UpscaleQuality::Balanced, UpscaleQuality::Performance, UpscaleQuality::Auto };
			return all;
		}

		f32 Scale(const UpscaleQuality quality)
		{
			switch (quality)
			{
			case UpscaleQuality::Native: return 1.f;
			case UpscaleQuality::Quality: return 0.67f;
			case UpscaleQuality::Balanced: return 0.59f;
			case UpscaleQuality::Performance: return 0.5f;
			case UpscaleQuality::Auto: return 0.f;
			}
			return 0.f;
		}
	}

}
