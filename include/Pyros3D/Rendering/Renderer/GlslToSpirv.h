//============================================================================
// Name        : GlslToSpirv.h
// Author      : Duarte Peixinho
// Description : The renderer's GLSL to SPIR-V at run time (glslang): the materials' surfaces, compiled into
//               surface.frag / surface.vert when a scene is loaded - natively and in the browser alike.
//============================================================================

#ifndef GLSLTOSPIRV_H
#define GLSLTOSPIRV_H

#include <Pyros3D/Rendering/Device/RenderDevice.h>
#include <string>
#include <vector>

namespace p3d {

	// For Vulkan 1.1 (SPIR-V 1.3), as the shaders built at build time are; false with glslang's log in `error`
	bool GlslToSpirv(const std::string &source, const gfx::Stage stage, std::vector<uint32_t> &spirv, std::string &error);

}

#endif /* GLSLTOSPIRV_H */
