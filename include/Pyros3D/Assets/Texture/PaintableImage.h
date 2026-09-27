//============================================================================
// Name        : PaintableImage.h
// Author      : Duarte Peixinho
// Description : An image kept on the CPU so it can be painted - a terrain
//               tile's splat map (four blend weights, RGBA) or a foliage
//               layer's density map (one channel) - with an optional GPU
//               texture it re-uploads after a stroke, and a PNG it saves
//               back to.
//
//               Brushes are circles in the image's own 0..1 space; a world-
//               space brush over a terrain tile divides by the tile's size.
//               Hardness 1 is a hard disc, 0 a smooth falloff to the rim.
//============================================================================

#ifndef PAINTABLEIMAGE_H
#define PAINTABLEIMAGE_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Other/Global.h>
#include <Pyros3D/Core/Math/Math.h>
#include <memory>
#include <string>
#include <vector>

namespace p3d {

	class Texture;

	class PYROS3D_API PaintableImage
	{
	public:
		int32 width = 0, height = 0;
		uint32 channels = 0;		// 1 or 4
		std::vector<uchar> pixels;	// row 0 is v = 0

		// The texture showing this image, if any; Upload() refreshes it.
		// For a splat map, the material's own map (Texture::LoadShared with
		// the same path and wrap mode returns that very object).
		std::shared_ptr<Texture> texture;

		// A blank image: every pixel `fill` in every channel.
		void Create(const int32 w, const int32 h, const uint32 ch, const uchar fill);
		// Any format stb reads, converted to `ch` channels.
		bool Load(const std::string &path, const uint32 ch);
		bool Save(const std::string &path) const;

		// Bilinear, 0..1.
		f32 Sample(const f32 u, const f32 v, const uint32 channel) const;

		// Moves `channel` toward `target` (0..1) by up to `strength` at the
		// brush centre. normalize keeps the four channels of an RGBA splat
		// summing to one: what one layer gains the others lose, in
		// proportion. Returns whether any pixel changed.
		bool Paint(const f32 u, const f32 v, const f32 radiusU, const f32 radiusV, const uint32 channel,
			const f32 target, const f32 strength, const f32 hardness, const bool normalize);

		// Pushes the pixels to `texture`. RGBA only - a texture is RGBA8.
		void Upload();

		// Brush falloff: 1 at the centre, 0 at the rim.
		static f32 Falloff(const f32 distance01, const f32 hardness);

		// 8-bit PNG, 1 to 4 channels; 16-bit greyscale PNG (heightmaps).
		static bool WritePNG(const std::string &path, const int32 w, const int32 h, const uint32 ch, const uchar* data);
		static bool WritePNG16(const std::string &path, const int32 w, const int32 h, const uint16* data);
	};

}

#endif /* PAINTABLEIMAGE_H */
