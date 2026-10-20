//============================================================================
// Name        : BlockCompress.h
// Description : GPU block compression of 8-bit images, done on the CPU: BC1
//               (colour, 4 bits a texel - an eighth of RGBA8) for an opaque
//               image, BC3 (colour and a smooth alpha, 8 bits a texel) for one
//               with any transparency. Every desktop GPU samples them directly
//               (Metal on Apple silicon, Vulkan, GL 4), so a material's
//               textures take a quarter to an eighth of the memory and the
//               bandwidth they did. Encoded once and kept beside the source
//               image (TextureCompressionCache in Texture.cpp).
//
//               The encoder: a block's colours projected onto their principal
//               axis give the two endpoints, rounded to 565; each texel takes
//               the nearest of the four colours between them; one least-squares
//               pass then refits the endpoints to those choices and is kept if
//               it is closer. Plain numbers - tools/tests can try it without a
//               device.
//============================================================================
#ifndef PYROS3D_BLOCKCOMPRESS_H
#define PYROS3D_BLOCKCOMPRESS_H

#include <Pyros3D/Other/Export.h>
#include <cstdint>
#include <vector>

namespace p3d {

	namespace BlockCompress {

		// The formats (IRenderDevice::SupportsCompressedFormat / UploadCompressedTexture2D).
		enum Format { None = 0, BC1 = 1, BC3 = 2 };

		inline uint32_t BlockBytes(const uint32_t format) { return format == BC1 ? 8u : 16u; }
		inline uint32_t LevelBytes(const uint32_t format, const uint32_t w, const uint32_t h)
		{
			return ((w + 3) / 4) * ((h + 3) / 4) * BlockBytes(format);
		}

		// One 4x4 block of RGBA8 (16 texels, row by row) into `out` (8 or 16 bytes).
		PYROS3D_API void EncodeBlockBC1(const uint8_t rgba[64], uint8_t out[8]);
		PYROS3D_API void EncodeBlockBC3(const uint8_t rgba[64], uint8_t out[16]);
		// (and back, for tests: 16 texels of RGBA8)
		PYROS3D_API void DecodeBlockBC1(const uint8_t in[8], uint8_t rgba[64]);
		PYROS3D_API void DecodeBlockBC3(const uint8_t in[16], uint8_t rgba[64]);

		// Whole image (RGBA8, w x h) -> blocks. Edge blocks repeat the last row/column.
		// Spread over the job system's workers.
		PYROS3D_API void EncodeImage(const uint32_t format, const uint8_t* rgba, const uint32_t w, const uint32_t h, std::vector<uint8_t> &out);

		// The image and every mip below it, down to 1x1 (a 2x2 box each step - what a
		// GPU's GenerateMipmap does), each level encoded: levels[0] is the image itself.
		PYROS3D_API void EncodeMipChain(const uint32_t format, const std::vector<uint8_t> &rgba, const uint32_t w, const uint32_t h,
			std::vector<std::vector<uint8_t> > &levels);

		// BC3 when any texel is not fully opaque, else BC1.
		PYROS3D_API uint32_t ChooseFormat(const std::vector<uint8_t> &rgba);
	}
}

#endif /* PYROS3D_BLOCKCOMPRESS_H */
