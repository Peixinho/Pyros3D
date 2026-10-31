//============================================================================
// Name        : SpirvToWgsl.h
// Author      : Duarte Peixinho
// Description : SPIR-V to WGSL (Tint), for the device layer on WebGPU: the renderer's shaders and the materials'
//               surfaces are SPIR-V; WebGPU takes WGSL. Built where the engine links Tint (PYROS_TINT).
//============================================================================

#ifndef SPIRVTOWGSL_H
#define SPIRVTOWGSL_H

#include <cstdint>
#include <string>

namespace p3d { namespace gfx {

	// The WGSL, or false with what went wrong in `error`
	bool SpirvToWgsl(const uint32_t* words, uint64_t count, std::string &wgsl, std::string &error);

} }

#endif /* SPIRVTOWGSL_H */
