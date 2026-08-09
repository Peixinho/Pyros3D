//============================================================================
// Name        : ShaderCompiler.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : GLSL -> SPIR-V cross-compilation + reflection
//============================================================================

#include <Pyros3D/Rendering/SPIRV/ShaderCompiler.h>

#ifdef SPIRV_TOOLING

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <fstream>
#include <map>
#include <mutex>
#include <regex>
#include <sstream>
#include <sys/stat.h>

namespace p3d {

	namespace {

		uint64 HashSpirvKey(const std::string &source, const uint32 stage)
		{
			// FNV-1a 64-bit
			uint64 h = 14695981039346656037ull;
			for (size_t i = 0; i < source.size(); i++)
			{
				h ^= (uint8_t)source[i];
				h *= 1099511628211ull;
			}
			h ^= (uint64)stage + 0x9e3779b97f4a7c15ull;
			h *= 1099511628211ull;
			return h;
		}

		std::string SpirvCacheDir()
		{
			const char *xdg = getenv("XDG_CACHE_HOME");
			if (xdg && xdg[0])
				return std::string(xdg) + "/pyros3d/spirv";
			const char *home = getenv("HOME");
			if (home && home[0])
				return std::string(home) + "/.cache/pyros3d/spirv";
			return std::string(".cache/pyros3d/spirv");
		}

		bool EnsureDir(const std::string &path)
		{
			if (path.empty())
				return false;
			struct stat st;
			if (stat(path.c_str(), &st) == 0)
				return S_ISDIR(st.st_mode);
			size_t slash = path.find_last_of('/');
			if (slash != std::string::npos && slash > 0)
			{
				if (!EnsureDir(path.substr(0, slash)))
					return false;
			}
			if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST)
				return false;
			return true;
		}

		std::string SpirvCachePath(uint64 key)
		{
			char name[64];
			std::snprintf(name, sizeof(name), "%016llx.spv", (unsigned long long)key);
			return SpirvCacheDir() + "/" + name;
		}

		std::mutex gSpirvCacheMutex;
		std::map<uint64, std::vector<uint32> > gSpirvMemCache;

		bool LoadSpirvFromDisk(uint64 key, std::vector<uint32> &out)
		{
			std::ifstream in(SpirvCachePath(key).c_str(), std::ios::binary);
			if (!in)
				return false;
			in.seekg(0, std::ios::end);
			std::streamoff sz = in.tellg();
			in.seekg(0, std::ios::beg);
			if (sz <= 0 || (sz % 4) != 0)
				return false;
			out.resize((size_t)sz / 4);
			in.read(reinterpret_cast<char*>(out.data()), sz);
			return (bool)in;
		}

		void StoreSpirvToDisk(uint64 key, const std::vector<uint32> &spirv)
		{
			if (spirv.empty())
				return;
			if (!EnsureDir(SpirvCacheDir()))
				return;
			std::ofstream out(SpirvCachePath(key).c_str(), std::ios::binary | std::ios::trunc);
			if (!out)
				return;
			out.write(reinterpret_cast<const char*>(spirv.data()), (std::streamsize)(spirv.size() * sizeof(uint32)));
		}

	}

	bool SpirvShaderCompiler::Compile(const std::string &source, const uint32 stage, std::vector<uint32> &outSpirv, std::string &errorLog)
	{
		outSpirv.clear();

		const uint64 key = HashSpirvKey(source, stage);
		{
			std::lock_guard<std::mutex> lock(gSpirvCacheMutex);
			std::map<uint64, std::vector<uint32> >::iterator it = gSpirvMemCache.find(key);
			if (it != gSpirvMemCache.end())
			{
				outSpirv = it->second;
				return true;
			}
			if (LoadSpirvFromDisk(key, outSpirv))
			{
				gSpirvMemCache[key] = outSpirv;
				return true;
			}
		}

		shaderc_shader_kind kind = (stage == SpirvShaderStage::Fragment) ? shaderc_glsl_fragment_shader : shaderc_glsl_vertex_shader;

		shaderc::Compiler compiler;
		shaderc::CompileOptions options;
		options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_0);
		options.SetOptimizationLevel(shaderc_optimization_level_zero);

		shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source, kind, "shader", options);

		if (result.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			errorLog = result.GetErrorMessage();
			return false;
		}

		outSpirv.assign(result.cbegin(), result.cend());

		{
			std::lock_guard<std::mutex> lock(gSpirvCacheMutex);
			gSpirvMemCache[key] = outSpirv;
			StoreSpirvToDisk(key, outSpirv);
		}
		return true;
	}

	bool SpirvShaderCompiler::NeedsAutoFixForVulkan(const std::string &source)
	{
		// Cheap line scan - no shaderc. Matches AutoFixForVulkan's rule that
		// anything already starting with layout( is left alone; if every
		// in/out/uniform line is already laid out, AutoFix would be a no-op
		// after an expensive PreprocessGlsl.
		std::stringstream ss(source);
		std::string line;
		while (std::getline(ss, line))
		{
			size_t a = line.find_first_not_of(" \t\r\n");
			if (a == std::string::npos)
				continue;
			if (line.compare(a, 2, "//") == 0)
				continue;
			if (line.compare(a, 6, "layout") == 0)
				continue;
			if (line.compare(a, 8, "uniform ") == 0)
				return true;
			if (line.compare(a, 3, "in ") == 0 || line.compare(a, 4, "out ") == 0)
				return true;
		}
		return false;
	}

	namespace {

		struct Std140TypeInfo { uint32 size; uint32 align; bool valid; };

		// std140 size/alignment for the plain (non-array, non-struct)
		// scalar/vector/matrix types this transform supports - matrices
		// are column-major, and every column is padded to vec4 (16 bytes)
		// regardless of the matrix's row count, per the std140 spec.
		Std140TypeInfo GetStd140TypeInfo(const std::string &type)
		{
			if (type == "float" || type == "int" || type == "uint" || type == "bool") return { 4, 4, true };
			if (type == "vec2" || type == "ivec2" || type == "uvec2" || type == "bvec2") return { 8, 8, true };
			if (type == "vec3" || type == "ivec3" || type == "uvec3" || type == "bvec3") return { 12, 16, true };
			if (type == "vec4" || type == "ivec4" || type == "uvec4" || type == "bvec4") return { 16, 16, true };
			if (type == "mat2" || type == "mat2x2") return { 32, 16, true };
			if (type == "mat3" || type == "mat3x3") return { 48, 16, true };
			if (type == "mat4" || type == "mat4x4") return { 64, 16, true };
			return { 0, 0, false };
		}

		// Opaque (sampler/image) types are exempt from Vulkan's "non-opaque
		// uniform outside a block" rule - see PyrosShader.glsl's header
		// comment - so these get an explicit layout(set=1,binding=N)
		// instead of being folded into the synthesized UBO.
		bool IsOpaqueUniformType(const std::string &type)
		{
			return type.compare(0, 7, "sampler") == 0
				|| type.compare(0, 8, "isampler") == 0
				|| type.compare(0, 8, "usampler") == 0
				|| type.compare(0, 5, "image") == 0
				|| type.compare(0, 7, "texture") == 0;
		}

		std::string TrimWhitespace(const std::string &s)
		{
			size_t a = s.find_first_not_of(" \t\r\n");
			if (a == std::string::npos) return "";
			size_t b = s.find_last_not_of(" \t\r\n");
			return s.substr(a, b - a + 1);
		}

		// Splits a declaration's comma-separated variable list ("uName,
		// uOther[3]") into (name, arrayCount) pairs - arrayCount 0 means
		// "not an array".
		void SplitDeclarationList(const std::string &list, std::vector<std::pair<std::string, uint32> > &out)
		{
			std::stringstream ss(list);
			std::string item;
			while (std::getline(ss, item, ','))
			{
				item = TrimWhitespace(item);
				if (item.empty())
					continue;
				size_t bracket = item.find('[');
				if (bracket == std::string::npos)
				{
					out.push_back(std::make_pair(item, (uint32)0));
					continue;
				}
				std::string name = TrimWhitespace(item.substr(0, bracket));
				size_t close = item.find(']', bracket);
				uint32 count = (close != std::string::npos) ? (uint32)atoi(item.substr(bracket + 1, close - bracket - 1).c_str()) : 0;
				out.push_back(std::make_pair(name, count));
			}
		}

	}

	bool SpirvShaderCompiler::AutoFixForVulkan(std::string &source, const uint32 stage, const uint32 uboBinding, const std::string &blockName, SpirvAutoUboResult &outResult, std::string &errorLog)
	{
		outResult = SpirvAutoUboResult();

		// Preprocess first - resolves this stage's #ifdef VERTEX/FRAGMENT
		// branch and expands the shader's own macros (e.g. a shader-local
		// `#define varying_in in`), so every regex below only ever has to
		// deal with flat, single-line, fully-resolved GLSL - see the
		// header comment for why this is much more robust than pattern-
		// matching the raw, still-conditional source text directly.
		shaderc_shader_kind kind = (stage == SpirvShaderStage::Fragment) ? shaderc_glsl_fragment_shader : shaderc_glsl_vertex_shader;
		shaderc::Compiler compiler;
		shaderc::CompileOptions options;
		options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_0);

		shaderc::PreprocessedSourceCompilationResult preResult = compiler.PreprocessGlsl(source, kind, "shader", options);
		if (preResult.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			errorLog = preResult.GetErrorMessage();
			return false;
		}
		std::string flat(preResult.cbegin(), preResult.cend());

		std::vector<std::string> lines;
		{
			std::stringstream ss(flat);
			std::string line;
			while (std::getline(ss, line))
				lines.push_back(line);
		}

		static const std::regex reLayoutPrefix("^\\s*layout\\s*\\(");
		static const std::regex reInOut("^(\\s*)(in|out)\\s+(\\w+)\\s+([^;]+);\\s*$");
		static const std::regex reUniform("^(\\s*)uniform\\s+(\\w+)\\s+([^;]+);\\s*$");

		// Pass 1: collect every in/out/sampler-uniform *name* declared in
		// this stage (skipping anything already explicitly laid out - an
		// already Vulkan-correct shader, i.e. every shader this engine
		// ships today, is a no-op below). Each category gets numbered by
		// sorting its name set alphabetically - deterministic and entirely
		// local to this one stage's text, yet still guaranteed to agree
		// with the sibling stage's independent numbering for varyings
		// (see the header comment: Vulkan matches VS-out/FS-in by location
		// number, not name, so this alphabetical trick is what keeps a
		// vertex `out vTexcoord` and fragment `in vTexcoord` on the same
		// location without this function ever seeing both stages at once).
		std::vector<std::string> inNames, outNames, samplerNames;
		for (size_t i = 0; i < lines.size(); i++)
		{
			const std::string &line = lines[i];
			if (std::regex_search(line, reLayoutPrefix))
				continue;
			std::smatch m;
			if (std::regex_match(line, m, reInOut))
			{
				std::string type = m[3].str();
				if (IsOpaqueUniformType(type))
					continue;
				std::vector<std::pair<std::string, uint32> > names;
				SplitDeclarationList(m[4].str(), names);
				std::vector<std::string> &target = (m[2].str() == "in") ? inNames : outNames;
				for (size_t n = 0; n < names.size(); n++)
					if (std::find(target.begin(), target.end(), names[n].first) == target.end())
						target.push_back(names[n].first);
			}
			else if (std::regex_match(line, m, reUniform))
			{
				std::string type = m[2].str();
				if (!IsOpaqueUniformType(type))
					continue;
				std::vector<std::pair<std::string, uint32> > names;
				SplitDeclarationList(m[3].str(), names);
				for (size_t n = 0; n < names.size(); n++)
					if (std::find(samplerNames.begin(), samplerNames.end(), names[n].first) == samplerNames.end())
						samplerNames.push_back(names[n].first);
			}
		}
		std::map<std::string, uint32> inLocation, outLocation, samplerBindingMap;
		{
			std::vector<std::string> s = inNames; std::sort(s.begin(), s.end());
			for (size_t i = 0; i < s.size(); i++) inLocation[s[i]] = (uint32)i;
		}
		{
			std::vector<std::string> s = outNames; std::sort(s.begin(), s.end());
			for (size_t i = 0; i < s.size(); i++) outLocation[s[i]] = (uint32)i;
		}
		{
			std::vector<std::string> s = samplerNames; std::sort(s.begin(), s.end());
			for (size_t i = 0; i < s.size(); i++) samplerBindingMap[s[i]] = (uint32)i;
		}

		// Pass 2: rewrite - add layout() to in/out/sampler declarations
		// (splitting any comma-separated multi-name declaration into one
		// line per name, since each needs its own location/binding
		// number), and pull every loose non-opaque uniform out into
		// `members` (in declaration order - this drives the synthesized
		// block's offsets) instead of emitting it.
		struct Member { std::string name; std::string type; uint32 arrayCount; };
		std::vector<Member> members;
		int32 firstRemovedLine = -1;

		std::vector<std::string> outLines;
		outLines.reserve(lines.size());
		for (size_t i = 0; i < lines.size(); i++)
		{
			const std::string &line = lines[i];
			if (std::regex_search(line, reLayoutPrefix))
			{
				outLines.push_back(line);
				continue;
			}

			std::smatch m;
			if (std::regex_match(line, m, reInOut))
			{
				std::string indent = m[1].str();
				std::string qualifier = m[2].str();
				std::string type = m[3].str();
				if (IsOpaqueUniformType(type))
				{
					outLines.push_back(line);
					continue;
				}
				std::vector<std::pair<std::string, uint32> > names;
				SplitDeclarationList(m[4].str(), names);
				std::map<std::string, uint32> &table = (qualifier == "in") ? inLocation : outLocation;
				bool allResolved = !names.empty();
				for (size_t n = 0; n < names.size(); n++)
					if (table.find(names[n].first) == table.end())
						allResolved = false;
				if (!allResolved)
				{
					outLines.push_back(line);
					continue;
				}
				for (size_t n = 0; n < names.size(); n++)
				{
					std::string decl = indent + "layout(location=" + std::to_string(table[names[n].first]) + ") " + qualifier + " " + type + " " + names[n].first;
					if (names[n].second > 0)
						decl += "[" + std::to_string(names[n].second) + "]";
					decl += ";";
					outLines.push_back(decl);
				}
				continue;
			}

			if (std::regex_match(line, m, reUniform))
			{
				std::string indent = m[1].str();
				std::string type = m[2].str();
				std::vector<std::pair<std::string, uint32> > names;
				SplitDeclarationList(m[3].str(), names);

				if (IsOpaqueUniformType(type))
				{
					bool allResolved = !names.empty();
					for (size_t n = 0; n < names.size(); n++)
						if (samplerBindingMap.find(names[n].first) == samplerBindingMap.end())
							allResolved = false;
					if (!allResolved)
					{
						outLines.push_back(line);
						continue;
					}
					for (size_t n = 0; n < names.size(); n++)
					{
						std::string decl = indent + "layout(set=1,binding=" + std::to_string(samplerBindingMap[names[n].first]) + ") uniform " + type + " " + names[n].first;
						if (names[n].second > 0)
							decl += "[" + std::to_string(names[n].second) + "]";
						decl += ";";
						outLines.push_back(decl);
					}
					continue;
				}

				Std140TypeInfo info = GetStd140TypeInfo(type);
				if (!info.valid || names.empty())
				{
					// Unsupported (struct/unknown) type - leave as-is;
					// still loose, so compilation will fail with Vulkan's
					// own clear "uniform outside a block" error rather
					// than a silently-wrong rewrite.
					outLines.push_back(line);
					continue;
				}

				if (firstRemovedLine < 0)
					firstRemovedLine = (int32)outLines.size();
				for (size_t n = 0; n < names.size(); n++)
				{
					Member mem;
					mem.name = names[n].first;
					mem.type = type;
					mem.arrayCount = names[n].second;
					members.push_back(mem);
				}
				continue; // drop the original loose declaration
			}

			outLines.push_back(line);
		}

		if (members.empty())
		{
			std::string rebuilt;
			for (size_t i = 0; i < outLines.size(); i++) { rebuilt += outLines[i]; rebuilt += "\n"; }
			source = rebuilt;
			return true;
		}

		// Compute std140 offsets in declaration order.
		uint32 offset = 0;
		std::string blockBody;
		for (size_t i = 0; i < members.size(); i++)
		{
			Std140TypeInfo info = GetStd140TypeInfo(members[i].type);
			uint32 align = info.align;
			uint32 size = info.size;
			if (members[i].arrayCount > 0)
			{
				// std140 array rule: every element is padded to a
				// multiple of vec4 (16 bytes) regardless of base type.
				align = 16;
				uint32 elemSize = ((info.size + 15) / 16) * 16;
				if (elemSize < 16) elemSize = 16;
				size = elemSize * members[i].arrayCount;
			}
			offset = ((offset + align - 1) / align) * align;
			outResult.offsets[members[i].name] = offset;
			blockBody += "\t" + members[i].type + " " + members[i].name;
			if (members[i].arrayCount > 0)
				blockBody += "[" + std::to_string(members[i].arrayCount) + "]";
			blockBody += ";\n";
			offset += size;
		}
		outResult.size = ((offset + 15) / 16) * 16;
		outResult.hasBlock = true;
		outResult.binding = uboBinding;
		outResult.blockName = blockName;

		std::string blockDecl = "layout(std140,binding=" + std::to_string(uboBinding) + ") uniform " + blockName + " {\n" + blockBody + "};";

		std::string rebuilt;
		for (size_t i = 0; i < outLines.size(); i++)
		{
			if ((int32)i == firstRemovedLine) { rebuilt += blockDecl; rebuilt += "\n"; }
			rebuilt += outLines[i];
			rebuilt += "\n";
		}
		if (firstRemovedLine >= 0 && (size_t)firstRemovedLine >= outLines.size())
		{
			rebuilt += blockDecl;
			rebuilt += "\n";
		}
		source = rebuilt;
		return true;
	}

	static uint32 TranslateResourceType(const uint32 spirvCrossResourceType)
	{
		switch (spirvCrossResourceType)
		{
		case spirv_cross::ResourceTypeUniformBuffer:
			return SpirvResourceType::UniformBuffer;
		case spirv_cross::ResourceTypeSampledImage:
			return SpirvResourceType::SampledImage;
		default:
			return SpirvResourceType::Unknown;
		}
	}

	static void ReflectResourceList(const spirv_cross::Compiler &compiler, const spirv_cross::SmallVector<spirv_cross::Resource> &resources, const uint32 resourceType, std::vector<SpirvResourceBinding> &out)
	{
		for (size_t i = 0; i < resources.size(); i++)
		{
			const spirv_cross::Resource &resource = resources[i];
			SpirvResourceBinding binding;
			binding.name = resource.name;
			binding.type = resourceType;
			binding.set = compiler.has_decoration(resource.id, spv::DecorationDescriptorSet) ? compiler.get_decoration(resource.id, spv::DecorationDescriptorSet) : 0;
			binding.binding = compiler.has_decoration(resource.id, spv::DecorationBinding) ? compiler.get_decoration(resource.id, spv::DecorationBinding) : 0;
			// A fixed-size GLSL array (e.g. `uPointShadowMaps[4]`) shows
			// up as a single resource whose *type* is array-of-N, not as
			// N separate resources - so the array length has to come off
			// the type, not the resource list. array[0] is the outermost
			// dimension's literal length for a compile-time-sized array
			// (the only kind this engine's shaders ever declare); an
			// empty `array` vector means "not an array" (length 1).
			const spirv_cross::SPIRType &type = compiler.get_type(resource.type_id);
			binding.arraySize = type.array.empty() ? 1 : (type.array[0] > 0 ? type.array[0] : 1);
			// See SpirvResourceBinding::isCube/isDepthCompare. Both come
			// off the image type, not the resource: SPIR-V models
			// sampler2D/samplerCube/sampler2DShadow as one OpTypeImage
			// differing only in Dim and the depth flag. Meaningless for a
			// UBO, so left false there rather than read off a type that
			// has no `image` member filled in.
			binding.isCube = false;
			binding.isDepthCompare = false;
			if (resourceType == SpirvResourceType::SampledImage)
			{
				binding.isCube = (type.image.dim == spv::DimCube);
				// image.depth is SPIR-V's "this is a depth *comparison*
				// image" flag (1), distinct from 0 (not) and 2 (unknown -
				// GLSL never produces it for these declarations).
				binding.isDepthCompare = (type.image.depth == 1);
			}
			out.push_back(binding);
		}
	}

	std::vector<SpirvResourceBinding> SpirvShaderCompiler::Reflect(const std::vector<uint32> &spirv)
	{
		std::vector<SpirvResourceBinding> out;
		if (spirv.empty())
			return out;

		spirv_cross::Compiler compiler(spirv);
		spirv_cross::ShaderResources resources = compiler.get_shader_resources();

		ReflectResourceList(compiler, resources.uniform_buffers, SpirvResourceType::UniformBuffer, out);
		ReflectResourceList(compiler, resources.sampled_images, SpirvResourceType::SampledImage, out);

		return out;
	}

	std::vector<SpirvStageInput> SpirvShaderCompiler::ReflectStageInputs(const std::vector<uint32> &spirv)
	{
		std::vector<SpirvStageInput> out;
		if (spirv.empty())
			return out;

		spirv_cross::Compiler compiler(spirv);
		spirv_cross::ShaderResources resources = compiler.get_shader_resources();

		for (size_t i = 0; i < resources.stage_inputs.size(); i++)
		{
			const spirv_cross::Resource &resource = resources.stage_inputs[i];
			SpirvStageInput input;
			input.name = resource.name;
			input.location = compiler.has_decoration(resource.id, spv::DecorationLocation) ? compiler.get_decoration(resource.id, spv::DecorationLocation) : 0;
			out.push_back(input);
		}

		return out;
	}

};

#endif /* SPIRV_TOOLING */
