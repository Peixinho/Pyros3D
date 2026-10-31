//============================================================================
// Name        : SpirvToWgsl.cpp
// Author      : Duarte Peixinho
// Description : SPIR-V to WGSL through Tint's IR (its SPIR-V reader, its WGSL writer)
//============================================================================

#include <Pyros3D/Rendering/Device/SpirvToWgsl.h>
#include "src/tint/lang/core/ir/module.h"
#include "src/tint/lang/spirv/reader/reader.h"
#include "src/tint/lang/wgsl/writer/writer.h"
#include <vector>

namespace p3d { namespace gfx {

	bool SpirvToWgsl(const uint32_t* words, uint64_t count, std::string &wgsl, std::string &error)
	{
		std::vector<uint32_t> input(words, words + count);
		auto ir = tint::spirv::reader::ReadIR(input);
		if (ir != tint::Success) { error = ir.Failure().reason; return false; }
		tint::wgsl::writer::Options options;
		// (a material's surface samples where its own branches lead - the water's foam, a terrain's layers: WGSL's
		// uniformity rule for derivatives is turned off for them with its own directive, as GLSL never had it)
		options.allow_non_uniform_derivatives = true;
		options.allowed_features = tint::wgsl::AllowedFeatures::Everything();
		auto out = tint::wgsl::writer::WgslFromIR(ir.Get(), options);
		if (out != tint::Success) { error = out.Failure().reason; return false; }
		wgsl = out->wgsl;
		return true;
	}

} }
