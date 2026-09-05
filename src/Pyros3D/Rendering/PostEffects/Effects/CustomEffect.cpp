//============================================================================
// Name        : CustomEffect.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See CustomEffect.h - a post effect read from a .glsl asset.
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/CustomEffect.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <cstdlib>
#include <sstream>

namespace p3d {

	namespace {

		// Splits on whitespace. Deliberately not a tokenizer with quoting:
		// the only free-text field is the label, and it is the rest of the
		// line, so nothing here ever needs to quote a space.
		std::vector<std::string> SplitWords(const std::string &line)
		{
			std::vector<std::string> out;
			std::istringstream in(line);
			std::string word;
			while (in >> word)
				out.push_back(word);
			return out;
		}

		bool TypeFromName(const std::string &name, uint32 &typeOut, uint32 &componentsOut)
		{
			if (name == "float") { typeOut = Uniforms::DataType::Float; componentsOut = 1; return true; }
			if (name == "int")   { typeOut = Uniforms::DataType::Int;   componentsOut = 1; return true; }
			if (name == "vec2")  { typeOut = Uniforms::DataType::Vec2;  componentsOut = 2; return true; }
			if (name == "vec3")  { typeOut = Uniforms::DataType::Vec3;  componentsOut = 3; return true; }
			if (name == "vec4")  { typeOut = Uniforms::DataType::Vec4;  componentsOut = 4; return true; }
			return false;
		}

		const char *GlslTypeName(const uint32 type)
		{
			switch (type)
			{
			case Uniforms::DataType::Int:   return "int";
			case Uniforms::DataType::Vec2:  return "vec2";
			case Uniforms::DataType::Vec3:  return "vec3";
			case Uniforms::DataType::Vec4:  return "vec4";
			default:                        return "float";
			}
		}

		// One binding for every asset effect, deliberately. Two of them in one
		// chain share it safely for the same reason two materials do:
		// PostEffectsManager re-binds the effect's own buffer immediately
		// before its draw (see its extraUniformsBinding comment), so the slot
		// only ever has to be right for the draw in front of it. 44 is past
		// everything the engine itself uses.
		const uint32 kParamsBinding = 44;
		const char* const kParamsBlockName = "PyrosEffectParams";

		uint32 Std140Alignment(const uint32 type)
		{
			switch (type)
			{
			case Uniforms::DataType::Vec2: return 8;
			case Uniforms::DataType::Vec3:
			case Uniforms::DataType::Vec4: return 16;
			default:                       return 4;
			}
		}

		uint32 Std140Size(const uint32 type)
		{
			switch (type)
			{
			case Uniforms::DataType::Vec2: return 8;
			case Uniforms::DataType::Vec3: return 12;
			case Uniforms::DataType::Vec4: return 16;
			default:                       return 4;
			}
		}

		uint32 ComponentsOf(const uint32 type)
		{
			switch (type)
			{
			case Uniforms::DataType::Vec2: return 2;
			case Uniforms::DataType::Vec3: return 3;
			case Uniforms::DataType::Vec4: return 4;
			default:                       return 1;
			}
		}

		// "1,1,0.5,1" or "0.5" -> up to four floats. Missing components stay
		// at whatever the caller initialised, which is 0.
		void ParseDefaults(const std::string &text, f32 *out, const uint32 count)
		{
			std::string field;
			std::istringstream in(text);
			uint32 i = 0;
			while (i < count && std::getline(in, field, ','))
			{
				out[i] = (f32)atof(field.c_str());
				i++;
			}
		}

		struct Header
		{
			std::string name;
			std::vector<uint32> inputs;             // RTT::Color / Depth / LastRTT
			std::vector<CustomEffect::Param> params;
			std::string body;
		};

		bool ParseHeader(const std::string &source, Header &out, std::string &errorOut)
		{
			std::istringstream in(source);
			std::string line;
			uint32 lineNo = 0;
			std::ostringstream body;
			while (std::getline(in, line))
			{
				lineNo++;
				// Directives are `//!` so the file stays a comment away from
				// being valid GLSL, and so an editor highlights it as one.
				// They may appear anywhere, but everything that is not one is
				// body - which keeps line numbers in compiler errors close to
				// the file's own.
				const size_t bang = line.find("//!");
				const bool isDirective = (bang != std::string::npos && line.find_first_not_of(" \t") == bang);
				if (!isDirective)
				{
					body << line << "\n";
					continue;
				}
				body << "\n"; // keep the line count aligned with the file
				std::vector<std::string> w = SplitWords(line.substr(bang + 3));
				if (w.empty())
					continue;

				if (w[0] == "effect")
				{
					if (w.size() < 2) { errorOut = "line " + std::to_string(lineNo) + ": effect needs a name"; return false; }
					out.name = w[1];
					for (size_t i = 2; i < w.size(); i++) out.name += " " + w[i];
				}
				else if (w[0] == "input")
				{
					if (w.size() < 2) { errorOut = "line " + std::to_string(lineNo) + ": input needs Color, Depth or LastRTT"; return false; }
					if (w[1] == "Color")        out.inputs.push_back(RTT::Color);
					else if (w[1] == "Depth")   out.inputs.push_back(RTT::Depth);
					else if (w[1] == "LastRTT") out.inputs.push_back(RTT::LastRTT);
					else { errorOut = "line " + std::to_string(lineNo) + ": unknown input '" + w[1] + "' (Color, Depth or LastRTT)"; return false; }
				}
				else if (w[0] == "param")
				{
					// param <type> <name> <default> [min max] [label...]
					if (w.size() < 4) { errorOut = "line " + std::to_string(lineNo) + ": param needs a type, a name and a default"; return false; }
					CustomEffect::Param p;
					uint32 components = 1;
					if (!TypeFromName(w[1], p.type, components))
					{ errorOut = "line " + std::to_string(lineNo) + ": unknown param type '" + w[1] + "' (float, int, vec2, vec3, vec4)"; return false; }
					p.name = w[2];
					ParseDefaults(w[3], p.value, components);
					size_t next = 4;
					// A range only makes sense for a single number, and only
					// if both ends are there.
					if (components == 1 && w.size() >= 6 && !w[4].empty() && !w[5].empty()
						&& (isdigit((unsigned char)w[4][0]) || w[4][0] == '-' || w[4][0] == '.')
						&& (isdigit((unsigned char)w[5][0]) || w[5][0] == '-' || w[5][0] == '.'))
					{
						p.min = (f32)atof(w[4].c_str());
						p.max = (f32)atof(w[5].c_str());
						p.hasRange = (p.max > p.min);
						next = 6;
					}
					for (size_t i = next; i < w.size(); i++)
						p.label += (p.label.empty() ? "" : " ") + w[i];
					if (p.label.empty()) p.label = p.name;
					out.params.push_back(p);
				}
				// Anything else is ignored on purpose - an unknown directive
				// is a comment, not a reason to refuse to load the effect.
			}

			if (out.name.empty())
			{
				errorOut = "no `//! effect <name>` line";
				return false;
			}
			// An effect that reads nothing would sample an undeclared uTex0.
			// Defaulting to LastRTT is what every chained effect wants anyway.
			if (out.inputs.empty())
				out.inputs.push_back(RTT::LastRTT);
			out.body = body.str();
			return true;
		}

	}

	CustomEffect::CustomEffect(const uint32 width, const uint32 height) : IEffect(width, height) {}
	CustomEffect::~CustomEffect() {}

	bool CustomEffect::ReadMetadata(const std::string &source, std::string &nameOut,
		std::vector<Param> &paramsOut, std::string &errorOut)
	{
		Header h;
		if (!ParseHeader(source, h, errorOut))
			return false;
		nameOut = h.name;
		paramsOut = h.params;
		return true;
	}

	CustomEffect* CustomEffect::CreateFromSource(const std::string &source, const uint32 width, const uint32 height,
		std::string &errorOut)
	{
		Header h;
		if (!ParseHeader(source, h, errorOut))
			return NULL;

		CustomEffect* effect = new CustomEffect(width, height);
		effect->effectName = h.name;
		effect->params = h.params;

		// Inputs first: UseColor()/UseDepth()/UseLastRTT() name their sampler
		// uTex<n> from a counter, so declaring them in header order is what
		// makes uTex0 mean the first `//! input`.
		for (size_t i = 0; i < h.inputs.size(); i++)
			effect->UseRTT(h.inputs[i]);

		// The preamble is what an author would otherwise have to retype, and
		// what they would get wrong: Vulkan and Metal reject a sampler with no
		// set/binding and an output with no location, and GL rejects those
		// same qualifiers - hence the macro pair every engine effect shader
		// carries. Generated once here, correctly, for every asset effect.
		std::ostringstream shader;
		shader <<
			"#define varying_in in\n"
			"#define varying_out out\n"
			"#define attribute_in in\n"
			"#define texture_2D texture\n"
			"#define texture_cube texture\n"
			"#if defined(GLES3)\n"
			// highp, never mediump: mediump is genuinely fp16 on Apple GPUs,
			// and a post effect that touches gl_FragCoord or a depth value
			// loses it immediately. Same reason every shader in
			// resources/shaders had to be changed.
			"precision highp float;\n"
			"#endif\n"
			"#if defined(VULKAN)\n"
			"#define UBO_BINDING(n) layout(std140, binding = n)\n"
			"#define SAMPLER_BINDING(n) layout(set = 1, binding = n)\n"
			"#define IO_LOCATION(n) layout(location = n)\n"
			"#else\n"
			// std140 is not optional on GL either: without it the block gets
			// the default `shared` layout, whose offsets are the driver's
			// business and not the ones computed below.
			"#define UBO_BINDING(n) layout(std140)\n"
			"#define SAMPLER_BINDING(n)\n"
			"#define IO_LOCATION(n)\n"
			"#endif\n"
			"IO_LOCATION(0) out vec4 FragColor;\n"
			"IO_LOCATION(0) varying_in vec2 vTexcoord;\n";
		for (size_t i = 0; i < h.inputs.size(); i++)
			shader << "SAMPLER_BINDING(" << i << ") uniform sampler2D uTex" << i << ";\n";
		// Parameters go in one explicit std140 block rather than as loose
		// uniforms. Vulkan rejects non-opaque uniforms outside a block
		// outright; the device can auto-wrap them, but nothing then creates
		// the buffer that block needs and the first draw walks off a null
		// descriptor inside MoltenVK - measured, and not a pleasant crash to
		// read. Declaring the block here means PostEffectsManager's existing
		// extraUniforms path (see its comment on extraUniformsBinding) owns
		// the buffer, on every backend, with offsets this code computed
		// rather than reflected.
		if (!h.params.empty())
		{
			shader << "UBO_BINDING(" << kParamsBinding << ") uniform " << kParamsBlockName << " {\n";
			for (size_t i = 0; i < h.params.size(); i++)
				shader << "\t" << GlslTypeName(h.params[i].type) << " " << h.params[i].name << ";\n";
			shader << "};\n";
		}
		// The author's own line 1 lands on a line of its own, so a compiler
		// error's line number is at least in the right neighbourhood.
		shader << "#line 1\n" << h.body;

		effect->FragmentShaderString = shader.str();

		// std140, laid out in declaration order. The manager copies each
		// Uniform's value into extraUniformsScratch at these offsets every
		// frame, so a param only ever has to update its Uniform.
		uint32 offset = 0;
		for (size_t i = 0; i < h.params.size(); i++)
		{
			const Param &p = h.params[i];
			const uint32 align = Std140Alignment(p.type);
			const uint32 size = Std140Size(p.type);
			offset = (offset + align - 1) / align * align;
			effect->extraUniformOffsets[p.name] = offset;
			offset += size;

			// The four-argument Uniform is the one that carries a value and
			// records the real DataType; the shorter one takes a *usage* in
			// that position, which is not what a plain tweakable is.
			int32 asInt = (int32)p.value[0];
			void* initial = (p.type == Uniforms::DataType::Int) ? (void*)&asInt : (void*)p.value;
			effect->paramUniforms.push_back(effect->AddUniform(Uniform(p.name, p.type, initial)));
		}
		if (!h.params.empty())
		{
			// A std140 block rounds up to a multiple of 16.
			effect->extraUniformsSize = (offset + 15u) / 16u * 16u;
			effect->extraUniformsScratch.assign(effect->extraUniformsSize, 0);
			effect->extraUniformsBlockName = kParamsBlockName;
			effect->extraUniformsBinding = kParamsBinding;
		}

		effect->CompileShaders();
		return effect;
	}

	bool CustomEffect::SetParam(const std::string &name, const f32 *values, const uint32 count)
	{
		for (size_t i = 0; i < params.size(); i++)
		{
			if (params[i].name != name)
				continue;
			const uint32 n = ComponentsOf(params[i].type) < count ? ComponentsOf(params[i].type) : count;
			for (uint32 c = 0; c < n; c++)
				params[i].value[c] = values[c];
			if (i < paramUniforms.size() && paramUniforms[i] != NULL)
			{
				if (params[i].type == Uniforms::DataType::Int)
				{
					int32 v = (int32)params[i].value[0];
					paramUniforms[i]->SetValue(&v);
				}
				else
				{
					paramUniforms[i]->SetValue((void*)params[i].value);
				}
			}
			return true;
		}
		return false;
	}

}
