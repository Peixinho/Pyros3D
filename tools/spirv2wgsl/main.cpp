//============================================================================
// Name        : spirv2wgsl
// Author      : Duarte Peixinho
// Description : A SPIR-V file to WGSL, as a C++ raw string literal - the renderer's shaders for the browser build,
//               made when the engine is built (cmake/PyrosRenderer.cmake), so the page turns no SPIR-V into WGSL.
//                 pyros_spirv2wgsl <in.spv> <out.wgsl.inc>
//============================================================================

#include <Pyros3D/Rendering/Device/SpirvToWgsl.h>
#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
	if (argc != 3) { fprintf(stderr, "pyros_spirv2wgsl <in.spv> <out.wgsl.inc>\n"); return 2; }
	FILE* in = fopen(argv[1], "rb");
	if (!in) { fprintf(stderr, "pyros_spirv2wgsl: %s could not be read\n", argv[1]); return 1; }
	std::vector<uint32_t> words;
	uint32_t w;
	while (fread(&w, 4, 1, in) == 1) words.push_back(w);
	fclose(in);
	std::string wgsl, error;
	if (!p3d::gfx::SpirvToWgsl(words.data(), words.size(), wgsl, error)) { fprintf(stderr, "pyros_spirv2wgsl: %s: %s\n", argv[1], error.c_str()); return 1; }
	FILE* out = fopen(argv[2], "wb");
	if (!out) { fprintf(stderr, "pyros_spirv2wgsl: %s could not be written\n", argv[2]); return 1; }
	fprintf(out, "R\"PYROSWGSL(%s)PYROSWGSL\"\n", wgsl.c_str());
	fclose(out);
	return 0;
}
