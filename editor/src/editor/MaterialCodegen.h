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

// The Text-mode equivalent of GenerateGLSL(): wraps a short user-written
// snippet (just plain GLSL statements assigning Albedo/Normal/Metallic/
// Roughness/Emissive/Occlusion locals - see kDefaultSimpleShaderText) in the
// exact same boilerplate/dual-branch template GenerateGLSL() uses, so Text
// mode never shows the user the #ifdef DEFERRED_GBUFFER machinery, the
// vertex shader, or the PBR lighting library - only the handful of lines
// that actually describe their surface.
//
// `textureNames` are the Text-mode document's named texture inputs (see
// MaterialTextureInput / MaterialEditorDocument::textTextures) - each gets a
// top-level `uniform sampler2D <name>;` declaration (the one thing a
// function-body-only snippet can't emit for itself), and `userBody` can then
// sample it directly, e.g. `Albedo = texture_2D(uAlbedoTex, vTexcoord).rgb;`.
// Duplicate-declaration-safe: only the six Albedo/Normal/... locals that
// `userBody` doesn't already declare itself get a default injected ahead of
// it (see GenerateGLSLFromSimpleText's .cpp comment) - this is what lets
// kDefaultSimpleShaderText redeclare all six with real types without
// tripping a GLSL redefinition error.
MaterialCodegenResult GenerateGLSLFromSimpleText(const std::string& userBody,
                                                  const std::vector<std::string>& textureNames = {});

// Seed text for a freshly created/mode-switched Text-mode document.
extern const char* const kDefaultSimpleShaderText;

#endif /* MATERIALCODEGEN_H */
