//============================================================================
// Name        : CustomEffect.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A post effect authored as an asset instead of as a C++ class.
//============================================================================

#ifndef CUSTOMEFFECT_H
#define CUSTOMEFFECT_H

#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>
#include <string>
#include <vector>

namespace p3d {

	// One .glsl file is the whole effect: a few `//!` directives at the top
	// declare what it reads and what can be tweaked, and everything after them
	// is the fragment shader body. The boilerplate every effect shader
	// repeats - the GLES3 precision line, FragColor, vTexcoord, and one
	// sampler per declared input - is generated rather than retyped, so the
	// author writes main() and nothing else.
	//
	//   //! effect Sharpen
	//   //! input LastRTT
	//   //! param float uAmount 0.5 0.0 2.0 Amount
	//   //! param vec4 uTint 1,1,1,1 Tint
	//
	//   void main() {
	//       vec4 c = texture_2D(uTex0, vTexcoord);
	//       FragColor = c * uTint * uAmount;
	//   }
	//
	// Inputs become uTex0, uTex1... in the order they are declared - the same
	// naming IEffect::UseColor()/UseDepth()/UseLastRTT() already produce, so a
	// hand-written C++ effect and an asset one read identically. Valid input
	// names are Color, Depth and LastRTT.
	//
	// A param becomes both a uniform and something the editor can put a widget
	// on: `param <type> <name> <default> [min max] [label...]`, where type is
	// float, vec2, vec3, vec4 or int. min/max are optional and only affect the
	// UI. The label is everything left on the line, and falls back to the
	// uniform name.
	class PYROS3D_API CustomEffect : public IEffect {
	public:

		struct Param
		{
			std::string name;      // uniform name, e.g. "uAmount"
			std::string label;     // what the editor shows
			uint32 type;           // Uniforms::DataType::*
			f32 value[4];          // current value, [0] used for float/int
			f32 min, max;          // UI range; equal means "no range given"
			bool hasRange;
			Param() : type(0), min(0.f), max(0.f), hasRange(false) { value[0] = value[1] = value[2] = value[3] = 0.f; }
		};

		// Both take the shader source, not a path: the engine has no opinion
		// about where assets live, and the editor already reads files.
		// `errorOut` gets a human-readable reason when this returns NULL -
		// a broken effect must say why rather than silently not appear.
		static CustomEffect* CreateFromSource(const std::string &source, const uint32 width, const uint32 height,
			std::string &errorOut);
		// Parses only the header. For listing what a project has without
		// compiling anything - the editor's effect picker uses this.
		static bool ReadMetadata(const std::string &source, std::string &nameOut,
			std::vector<Param> &paramsOut, std::string &errorOut);

		virtual ~CustomEffect();

		const std::string &GetEffectName() const { return effectName; }
		const std::vector<Param> &GetParams() const { return params; }
		// Pushes a new value into the live uniform. Returns false if the
		// effect has no such param, so a stale saved override says so instead
		// of vanishing.
		bool SetParam(const std::string &name, const f32 *values, const uint32 count);

	private:

		CustomEffect(const uint32 width, const uint32 height);

		std::string effectName;
		std::vector<Param> params;
		// Parallel to params - the Uniform* AddUniform() handed back, so
		// SetParam does not have to look the uniform up by name every time.
		std::vector<Uniform*> paramUniforms;
	};

}

#endif /* CUSTOMEFFECT_H */
