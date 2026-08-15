//============================================================================
// Name        : MaterialCodegen.h
// Description : Node graph -> GLSL codegen for CustomShaderMaterial. Pure
//               data->string transformation, no ImGui/engine-runtime
//               dependency, so it's easy to reason about/test in isolation.
//============================================================================

#ifndef MATERIALCODEGEN_H
#define MATERIALCODEGEN_H

#include "MaterialGraphTypes.h"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct MaterialCodegenResult {
	std::string glsl;                                        // full file text, ready to write to disk
	// (nodeId, sampler uniform name) for every Texture node reached from Output,
	// in emission order - caller uses this to AddSampler() the right texture.
	std::vector<std::pair<uint32_t, std::string>> textureSamplers;
	bool usesCameraPosition = false;
	bool usesTime = false;
	std::string error; // non-empty => codegen failed (no Output node, cycle, ...)
};

// Walks the graph from its single Output node and emits a full VERTEX+FRAGMENT
// GLSL file matching this engine's deferred G-buffer contract
// (FragData_r/g/b/pbr, see resources/shaders/PyrosShader.glsl) and its
// Vulkan-safe "plain loose uniforms" convention (see resources/shaders/
// particleSystem.glsl's header comment - VulkanRenderDevice auto-fixes
// layout(location=)/layout(binding=) for anything not manually qualified).
MaterialCodegenResult GenerateGLSL(const std::vector<MaterialNode>& nodes,
                                    const std::vector<MaterialConnection>& connections);

#endif /* MATERIALCODEGEN_H */
