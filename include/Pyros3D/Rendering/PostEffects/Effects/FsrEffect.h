//============================================================================
// Name        : FsrEffect.h
// Description : AMD FidelityFX Super Resolution 1.0, as AMD publishes it
//               (shaders/fsr/ffx_a.h and ffx_fsr1.h, MIT - see LICENSE.txt
//               beside them): EASU, the edge-adaptive upscale, into a target
//               the size of where the frame is shown, and RCAS, the
//               sharpening, over that.
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>

#ifndef FSREFFECT_H
#define	FSREFFECT_H

namespace p3d {

	class PYROS3D_API FsrEffect : public IEffect {
	public:
		enum Pass { EASU, RCAS };
		// Width x Height: for EASU the size it upscales TO (it reads whatever
		// came before it, at whatever size that is); for RCAS, the same.
		// sharpness: 0..1, RCAS only (1 is AMD's sharpest, 0 two stops softer).
		FsrEffect(const Pass pass, const uint32 Tex1, const uint32 Width, const uint32 Height, const f32 sharpness = 0.9f);
		virtual ~FsrEffect();
		// Whether its shader was made: AMD's sources were found beside the
		// engine's own shaders and compiled here.
		bool IsValid() const { return valid; }
		// Whether the sources are there at all (asked before making one).
		static bool SourcesPresent();
	private:
		bool valid = false;
	};

};

#endif	/* FSREFFECT_H */
