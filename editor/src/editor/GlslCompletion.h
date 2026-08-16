//============================================================================
// Name        : GlslCompletion.h
// Description : Keyword / member completion for the Material Editor's Text
//               mode. Same Item/Kind/icon/color plumbing as LuaCompletion.h
//               (reused directly, not duplicated) - CodeEditorDocument picks
//               which CollectCandidates() to call based on SetupForLua() vs
//               SetupForGlsl().
//============================================================================

#ifndef GLSLCOMPLETION_H
#define GLSLCOMPLETION_H

#include "LuaCompletion.h"
#include <algorithm>
#include <string>
#include <vector>

namespace GlslCompletion {

using LuaCompletion::Item;
using LuaCompletion::Kind;
using LuaCompletion::AddList;
using LuaCompletion::EqInsensitive;

inline void CollectCandidates(const std::string& receiver, const std::string& prefix,
	const std::string& /*buffer*/, std::vector<Item>& out, int maxCount = 80)
{
	out.clear();
	std::vector<Item> matches;
	matches.reserve(96);

	// The six surface outputs a Text-mode snippet actually assigns - see
	// MaterialCodegen::kDefaultSimpleShaderText/GenerateGLSLFromSimpleText.
	static const char* const kOutputs[] = {
		"Albedo", "Normal", "Metallic", "Roughness", "Emissive", "Occlusion",
	};
	// Read-only inputs the wrapping template declares as varyings/uniforms.
	static const char* const kInputs[] = {
		"vWorldPos", "vNormalWorld", "vTexcoord", "uTime", "uCameraPosition", "uAmbientLight",
	};
	static const char* const kTypes[] = {
		"float", "int", "bool", "vec2", "vec3", "vec4", "mat3", "mat4", "sampler2D",
	};
	static const char* const kKeywords[] = {
		"if", "else", "for", "while", "return", "const", "true", "false", "void",
	};
	static const char* const kBuiltinFns[] = {
		"normalize", "dot", "cross", "mix", "clamp", "pow", "sin", "cos", "tan",
		"min", "max", "abs", "floor", "ceil", "fract", "mod", "smoothstep", "step",
		"length", "distance", "reflect", "refract", "sqrt", "inversesqrt",
		"exp", "log", "exp2", "log2", "radians", "degrees", "sign", "texture",
	};
	static const char* const kSwizzles[] = {
		"x", "y", "z", "w", "r", "g", "b", "a", "xy", "xyz", "rgb", "rgba",
	};

	if (receiver.empty())
	{
		AddList(matches, kOutputs, sizeof(kOutputs) / sizeof(kOutputs[0]), Kind::Field, prefix);
		AddList(matches, kInputs, sizeof(kInputs) / sizeof(kInputs[0]), Kind::Module, prefix);
		AddList(matches, kTypes, sizeof(kTypes) / sizeof(kTypes[0]), Kind::Type, prefix);
		AddList(matches, kBuiltinFns, sizeof(kBuiltinFns) / sizeof(kBuiltinFns[0]), Kind::Function, prefix);
		AddList(matches, kKeywords, sizeof(kKeywords) / sizeof(kKeywords[0]), Kind::Keyword, prefix);
	}
	else
	{
		// Every receiver we know of here is vector-typed (the six outputs,
		// or vWorldPos/vNormalWorld/uCameraPosition) - swizzle completion
		// covers all of them; no per-type field lists needed like Lua's.
		AddList(matches, kSwizzles, sizeof(kSwizzles) / sizeof(kSwizzles[0]), Kind::Field, prefix);
	}

	std::sort(matches.begin(), matches.end(),
		[](const Item& a, const Item& b) { return a.text < b.text; });
	if ((int)matches.size() > maxCount)
		matches.resize((size_t)maxCount);
	out.swap(matches);
}

} // namespace GlslCompletion

#endif /* GLSLCOMPLETION_H */
