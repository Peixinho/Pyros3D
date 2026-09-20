//============================================================================
// Name        : IrradianceProbeBaker.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See IrradianceProbeBaker.h.
//============================================================================

#include <Pyros3D/Rendering/GI/IrradianceProbeBaker.h>
#include <Pyros3D/Rendering/Renderer/SpecialRenderers/CubemapRenderer/CubemapRenderer.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <cmath>

namespace p3d {

	namespace {

		// The six faces in the order ProjectCubemapToSH expects, which is
		// also the order Texture and the skybox loader use.
		const uint32 kFaceOrder[6] = {
			TextureType::CubemapPositive_X, TextureType::CubemapNegative_X,
			TextureType::CubemapPositive_Y, TextureType::CubemapNegative_Y,
			TextureType::CubemapPositive_Z, TextureType::CubemapNegative_Z
		};

		// The capture target is RGB8, so what comes back is sRGB-encoded
		// bytes. Irradiance is an integral of radiance and radiance is
		// linear, so these have to be decoded before projecting - the
		// same point as the skybox bake, and the same failure if skipped:
		// ambient that is too bright and too flat.
		inline f32 SrgbToLinear(const uchar v)
		{
			const f32 c = (f32)v / 255.f;
			return (c <= 0.04045f) ? (c / 12.92f) : powf((c + 0.055f) / 1.055f, 2.4f);
		}

	} // namespace

	IrradianceProbeBaker::IrradianceProbeBaker(const uint32 size)
		: renderer(NULL), eye(NULL), faceSize(size < 4 ? 4 : size), cursor(0),
		  probeNear(0.1f), probeFar(200.f)
	{
		renderer = new CubemapRenderer(faceSize, faceSize);
		eye = new GameObject();
	}

	IrradianceProbeBaker::~IrradianceProbeBaker()
	{
		delete renderer;
		delete eye;
	}

	bool IrradianceProbeBaker::IsSupported() const
	{
#if defined(GLES3)
		// Texture::GetTextureData() is compiled out there - glGetTexImage
		// does not exist in GLES or WebGL. Probes must be baked elsewhere
		// and shipped in the scene file.
		return false;
#else
		return renderer != NULL && eye != NULL;
#endif
	}

	bool IrradianceProbeBaker::CaptureProbe(SceneGraph *scene, const Vec3 &position, SphericalHarmonicsL2 &outSH)
	{
		if (!IsSupported() || scene == NULL)
			return false;

		eye->SetPosition(position);
		eye->RefreshTransformation();
		renderer->RenderCubeMap(scene, eye, probeNear, probeFar);

		Texture *cube = renderer->GetTexture();
		if (cube == NULL)
			return false;

		std::vector<CubemapFacePixels> faces;
		for (uint32 f = 0; f < 6; f++)
		{
			const std::vector<uchar> raw = cube->GetTextureData(0, (int32)kFaceOrder[f]);
			const size_t texels = (size_t)faceSize * (size_t)faceSize;
			if (raw.size() < texels * 3)
			{
				// Short read means the readback did not happen - on a
				// backend without glGetTexImage this comes back empty
				// rather than failing loudly, and projecting it would
				// produce a confident black probe.
				echo("IrradianceProbeBaker: face readback returned "
					+ std::to_string(raw.size()) + " bytes, expected "
					+ std::to_string(texels * 3) + " - probe skipped.");
				return false;
			}
			faceScratch[f].resize(texels * 3);
			for (size_t i = 0; i < texels * 3; i++)
				faceScratch[f][i] = SrgbToLinear(raw[i]);
			faces.push_back(CubemapFacePixels(faceScratch[f].data(), faceSize));
		}

		SphericalHarmonicsL2 sh;
		if (!ProjectCubemapToSH(faces, sh))
			return false;
		outSH = sh;
		return true;
	}

	uint32 IrradianceProbeBaker::Update(SceneGraph *scene, IrradianceProbeGrid &grid, const uint32 budget)
	{
		if (!IsSupported() || scene == NULL || !grid.IsValid())
			return 0;

		const uint32 total = grid.ProbeCount();
		const uint32 wanted = (budget == 0 || budget > total) ? total : budget;
		uint32 captured = 0;

		for (uint32 n = 0; n < wanted; n++)
		{
			if (cursor >= total)
				cursor = 0;
			const uint32 linear = cursor++;

			// Index() is row-major with X fastest; this is its inverse,
			// and the two must agree or probes land in the wrong cells -
			// which looks like plausible lighting shifted by an axis.
			const uint32 x = linear % grid.counts[0];
			const uint32 y = (linear / grid.counts[0]) % grid.counts[1];
			const uint32 z = linear / (grid.counts[0] * grid.counts[1]);

			SphericalHarmonicsL2 sh;
			if (CaptureProbe(scene, grid.ProbePosition(x, y, z), sh))
			{
				grid.probes[grid.Index(x, y, z)] = sh;
				captured++;
			}
		}
		if (cursor >= total)
			cursor = 0;
		return captured;
	}

};
