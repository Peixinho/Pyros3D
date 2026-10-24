//============================================================================
// Name        : GrassTuft.h
// Description : Grass as grass is: blades. A tuft is a handful of them out
//               of a patch of ground - each a narrow strip that tapers to a
//               point and leans its own way - made to be scattered by the
//               thousand (a foliage layer's mesh), where a card with a
//               picture of grass on it reads as a card.
//============================================================================

#ifndef GRASSTUFT_H
#define GRASSTUFT_H

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>

namespace p3d {

	// `blades` blades within `spread` metres of the middle, about `height` tall.
	// Each vertex has a colour (aColor: draw with ShaderUsage::VertexColor): `base`
	// at the ground to `tip` at the point, a little different for every blade - so
	// a field of them needs no texture and no cut-out, and its foot can be the
	// colour of the ground it stands in. `flowers` of the blades are stems with a
	// head of `flower` colour instead.
	//
	// A blade is one-sided geometry meant to be seen from both: draw it unculled
	// with a wind whose sky lean is above zero (GenericShaderMaterial::SetWind), which
	// is what keeps a leaf's normal from being turned on its back face.
	class PYROS3D_API GrassTuft : public Primitive {

	public:

		GrassTuft(const uint32 blades, const f32 height, const f32 spread, const uint32 seed,
			const Vec3 &base = Vec3(0.20f, 0.27f, 0.08f), const Vec3 &tip = Vec3(0.60f, 0.66f, 0.24f),
			const uint32 flowers = 0, const Vec3 &flower = Vec3(1.f, 1.f, 1.f), const f32 width = 0.018f,
			// the same tuft, every blade where it was, as tall, leaning and coloured as it was, but one
			// triangle from its root to its tip in place of three bending through a middle: the tuft
			// seen from further off than its bend shows (a third of the triangles)
			const bool simple = false);

		virtual uint32 GetPrimitiveType() const { return PrimitiveType::GrassTuft; }
		uint32 GetBlades() const { return blades; }
		f32 GetHeight() const { return height; }
		f32 GetSpread() const { return spread; }
		uint32 GetSeed() const { return seed; }
		const Vec3 &GetBaseColour() const { return base; }
		const Vec3 &GetTipColour() const { return tip; }
		uint32 GetFlowers() const { return flowers; }
		const Vec3 &GetFlowerColour() const { return flower; }
		f32 GetWidth() const { return width; }
		bool IsSimple() const { return simple; }

	protected:

		uint32 blades, seed, flowers;
		f32 height, spread, width;
		bool simple = false;
		Vec3 base, tip, flower;
	};
};

#endif /* GRASSTUFT_H */
