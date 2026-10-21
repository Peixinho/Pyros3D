//============================================================================
// Name        : ClothComponent.h
// Description : Cloth: the object's own mesh, simulated.
//============================================================================
#ifndef CLOTHCOMPONENT_H
#define CLOTHCOMPONENT_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Core/Math/Math.h>
#include <string>
#include <vector>

namespace p3d {

	class RenderingComponent;
	class ModelGeometry;

	// Makes the mesh this object renders (its RenderingComponent's, every submesh of the nearest
	// level: a subdivided Plane, or a model's cape or curtain) behave as cloth. Vertices at one place
	// are welded into one particle; each edge keeps its length (stretch) and each pair of triangles
	// sharing an edge keeps the distance between their far corners (bend). Solved by XPBD in small
	// steps, in world space: move the object and its pinned vertices go with it while the rest
	// trails behind - a flag on a moving car, a cape on a running character.
	//
	// Wind pushes on each triangle by how square-on it meets it (with turbulence so a flag
	// flutters); it collides with spheres, capsules and the ground, and not with itself. The
	// result is written into the object's own copy of the mesh (normals too), so other objects
	// using the same model are untouched.
	//
	// Like an Animator it runs once the game does (RenderingComponent::StartAutoPlayOn, or made
	// from a script); in a scene being edited the mesh stays as made.
	class PYROS3D_API ClothComponent : public IComponent {
	public:
		ClothComponent();
		virtual ~ClothComponent() {}

		virtual void Register(SceneGraph* Scene) {}
		virtual void Init() {}
		virtual void Update(const f64 time = 0);
		virtual void Destroy() { built = false; }
		virtual void Unregister(SceneGraph* Scene) {}
		virtual uint32 GetComponentType() const { return ComponentType::Cloth; }

		// --- Pins: what holds it up (as made, in the mesh's own space) ---
		// Every vertex within `band` (a fraction of the mesh's size that way) of one side:
		// "+y" the top edge (a curtain), "-x" the left edge (a flag on a pole), ... Several add up.
		void PinSide(const std::string &side, const f32 band = 0.01f);
		void PinVertex(const uint32 meshVertex);
		void ClearPins();
		const std::vector<std::pair<std::string, f32> > &GetPinSides() const { return pinSides; }

		// --- Material ---
		// 0..1: 1 does not stretch (or bend) at all, 0 is very soft (log scale between).
		void SetStretch(const f32 s) { stretch = Clamp01(s); }
		void SetBend(const f32 b) { bend = Clamp01(b); }
		void SetDamping(const f32 d) { damping = d < 0.f ? 0.f : d; }
		void SetThickness(const f32 t) { thickness = t < 0.f ? 0.f : t; }
		void SetFriction(const f32 f) { friction = Clamp01(f); }
		void SetDrag(const f32 d) { drag = d < 0.f ? 0.f : d; }
		void SetSubsteps(const uint32 n) { substeps = n < 1 ? 1 : (n > 64 ? 64 : n); }
		// Its weight a square metre (kg): 0.2 a flag or a shirt, 0.5 a heavy curtain, 1.5 canvas.
		void SetDensity(const f32 d) { density = d > 0.001f ? d : 0.001f; }
		f32 GetDensity() const { return density; }
		f32 GetStretch() const { return stretch; }
		f32 GetBend() const { return bend; }
		f32 GetDamping() const { return damping; }
		f32 GetThickness() const { return thickness; }
		f32 GetFriction() const { return friction; }
		f32 GetDrag() const { return drag; }
		uint32 GetSubsteps() const { return substeps; }

		// --- Surroundings ---
		void SetGravity(const Vec3 &g) { gravity = g; }
		void SetWind(const Vec3 &w) { wind = w; }
		// How much the wind gusts and swirls (0: steady).
		void SetTurbulence(const f32 t) { turbulence = t < 0.f ? 0.f : t; }
		void SetGround(const bool on, const f32 y = 0.f) { ground = on; groundY = y; }
		const Vec3 &GetGravity() const { return gravity; }
		const Vec3 &GetWind() const { return wind; }
		f32 GetTurbulence() const { return turbulence; }
		bool HasGround() const { return ground; }
		f32 GetGroundY() const { return groundY; }

		// --- Colliders ---
		// In world space, the object's own space (a pole beside a flag) or its parent's (the
		// character a cape hangs from: the cape is the character's child). A script can move one
		// every frame by its index.
		enum Space { World = 0, Owner, Parent };
		uint32 AddSphere(const Vec3 &centre, const f32 radius, const uint32 space = World);
		uint32 AddCapsule(const Vec3 &a, const Vec3 &b, const f32 radius, const uint32 space = World);
		void SetSphere(const uint32 i, const Vec3 &centre, const f32 radius);
		void SetCapsule(const uint32 i, const Vec3 &a, const Vec3 &b, const f32 radius);
		void ClearColliders() { colliders.clear(); }
		struct Collider { bool capsule; Vec3 a, b; f32 radius; uint32 space; };
		const std::vector<Collider> &GetColliders() const { return colliders; }

		// Back to as made, where the object is now (after a teleport the cloth would otherwise
		// be dragged across the world in one step; one of over 5 m does this by itself).
		void Reset();

		// (from stopped: it starts again from as made, where the object is now)
		void SetRunning(const bool r) { if (r && !running && built) Reset(); running = r; }
		// Stops and lets go of the mesh (the editor's Stop, before the mesh is put back as made):
		// the next run builds again from what is there then.
		void Stop() { running = false; built = false; unusable = false; pieces.clear(); rc = NULL; }
		bool IsRunning() const { return running; }

		uint32 GetParticleCount() const { return (uint32)x.size(); }
		uint32 GetConstraintCount() const { return (uint32)(stretchC.size() + bendC.size()); }
		uint32 GetPinnedCount() const;
		// Where particle `i` is now (world space): a test, or something hung off the cloth.
		Vec3 GetParticlePosition(const uint32 i) const { return i < x.size() ? x[i] : Vec3(); }
		// The particle a vertex of submesh `mesh` became.
		int32 GetParticleOf(const uint32 mesh, const uint32 vertex) const;

	private:
		static f32 Clamp01(const f32 v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
		bool Build();
		void ApplyPins();
		void Step(const f32 h, const f32 t, const f32 alongFrame);
		void WriteBack();
		Matrix SpaceMatrix(const uint32 space) const;

		struct Distance { uint32 a, b; f32 rest; };
		struct Piece { ModelGeometry* geometry; std::vector<uint32> particleOf; };

		RenderingComponent* rc;
		std::vector<Piece> pieces;
		std::vector<Vec3> x, prev, v, rest, normal, force;
		std::vector<f32> invMass, mass;
		std::vector<uint8> pinned;
		std::vector<uint32> tris;                  // particle triples
		std::vector<Distance> stretchC, bendC;
		std::vector<std::pair<std::string, f32> > pinSides;
		std::vector<uint32> pinVertices;
		std::vector<Collider> colliders;
		Matrix lastWorld;
		bool built, running, hasLast;
		f64 lastTime, simTime;
		f32 carry = 0.f;
		bool warnedSkinned = false, unusable = false;   // (unusable: it has a mesh, and none it can simulate)
		f32 stretch, bend, density, damping, thickness, friction, drag, turbulence, groundY;
		uint32 substeps;
		Vec3 gravity, wind;
		bool ground;
	};
}

#endif /* CLOTHCOMPONENT_H */
