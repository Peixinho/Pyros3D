//============================================================================
// Name        : Tree.h
// Description : Trees grown, not loaded: a species and a seed make a tree -
//               its trunk and branches, its leaves, and the pictures both
//               are drawn with.
//============================================================================

#ifndef TREE_H
#define TREE_H

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <memory>

namespace p3d {

	// What grows. Each is a set of numbers (TreeMesh.cpp's table): how tall, how the
	// limbs leave the trunk, the shape the crown fills, what a spray of its leaves
	// looks like. A seed makes one individual of the species; no two seeds are alike.
	namespace TreeSpecies {
		enum {
			Oak = 0,        // broad, heavy limbs, a round full crown
			Birch,          // slender and pale, fine hanging twigs
			Spruce,         // a cone of drooping boughs to the ground
			ScotsPine,      // a tall bare stem, a flat crown at the top
			Palm,           // a curved stem and a head of fronds
			Dead,           // limbs only
			Bush,           // many stems from the ground, leaves to the bottom
			Count
		};
	}

	// A tree is two meshes, drawn with two materials: wood (opaque, tiling bark) and
	// leaves (cut out of a picture of a spray of them).
	namespace TreePart {
		enum { Wood = 0, Leaves };
	}

	// One part of one tree at one level of detail.
	//
	// Detail 0 is the tree to stand under; 1 has no twigs and half the sprays, each
	// bigger (the crown keeps its outline and its density); 2 is a stem and a few
	// large sprays, for far away. The three are the same individual: same seed, same
	// limbs, so one can stand in for another without the tree changing shape.
	//
	// Leaves are cards, two-sided by being there twice (a card shown from behind with
	// its normal turned is black in a deferred frame), and their normals are not the
	// cards' own: they point out of the crown, so a crown is lit as the round thing it
	// is and not as a thousand flat ones. Each vertex carries a colour (aColor: draw
	// it with ShaderUsage::VertexColor): the tint of its spray, darker inside the
	// crown where the sky does not reach.
	class PYROS3D_API TreeMesh : public Primitive {

	public:

		TreeMesh(const uint32 species, const uint32 seed, const uint32 part, const uint32 detail = 0);

		virtual uint32 GetPrimitiveType() const { return PrimitiveType::Tree; }
		uint32 GetSpecies() const { return species; }
		uint32 GetSeed() const { return seed; }
		uint32 GetPart() const { return part; }
		uint32 GetDetail() const { return detail; }
		// How tall this individual is, and how wide its crown (metres).
		f32 GetHeight() const { return height; }
		f32 GetCrownRadius() const { return crownRadius; }
		// The stem's radius at the ground (what a collider wants).
		f32 GetTrunkRadius() const { return trunkRadius; }

		// The pictures a species is drawn with. Made once each, kept.
		// Bark tiles (repeat on both axes); a leaf spray does not (clamp), and its
		// alpha is the cut-out: draw it with ShaderUsage::AlphaTest at about 0.35.
		static std::shared_ptr<Texture> BarkTexture(const uint32 species);
		static std::shared_ptr<Texture> LeafTexture(const uint32 species);
		static const char* SpeciesName(const uint32 species);
		static bool HasLeaves(const uint32 species) { return species != TreeSpecies::Dead; }

	protected:

		uint32 species, seed, part, detail;
		f32 height, crownRadius, trunkRadius;
	};
};

#endif /* TREE_H */
