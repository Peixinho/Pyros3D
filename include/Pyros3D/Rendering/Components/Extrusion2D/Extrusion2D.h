//============================================================================
// Name        : Extrusion2D.h
// Description : An outline drawn in the plane of the screen and given depth:
//               ground to ride over, a platform, a wall, a ramp - the 3D
//               shape of a 2D level, and the collider that goes with it.
//
//               The outline is a handful of points in the object's own
//               space (x, y). Open, it is a profile - the top of a piece of
//               ground, running left to right - and the shape is everything
//               under it down to a floor. Closed, it is the shape itself.
//               Either way it is drawn as two surfaces, each with its own
//               image: the skin (what is walked on, across the depth) and
//               the faces (the cut ends, towards and away from the camera).
//
//               Smoothing turns a few points into a curve - the points stay
//               what is edited, and the curve is what is drawn and collided
//               with. With Collider on, a Physics2D on the same object is
//               kept as a chain along that curve, so what is seen and what
//               is touched cannot come apart.
//
//               It is an ordinary rendering component: lit, shadowed, and
//               its two meshes take any material (they start with plain lit
//               ones made from the two images given here).
//============================================================================

#ifndef EXTRUSION2D_H
#define EXTRUSION2D_H

#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Core/Math/Math.h>
#include <string>
#include <vector>

namespace p3d {

	// A surface as plain arrays: what BuildMesh makes, with no device needed.
	struct PYROS3D_API ExtrusionMesh2D {
		std::vector<Vec3> vertex, normal;
		std::vector<Vec2> texcoord;
		std::vector<__INDEX_C_TYPE__> index;
		void Clear() { vertex.clear(); normal.clear(); texcoord.clear(); index.clear(); }
	};

	class PYROS3D_API Extrusion2D : public RenderingComponent {

	public:

		Extrusion2D();
		virtual ~Extrusion2D();

		// The outline's points, in the object's own space. Two at least.
		void SetPoints(const std::vector<Vec2> &pts);
		const std::vector<Vec2> &GetPoints() const { return points; }
		void AddPoint(const Vec2 &p, const int32 at = -1);
		void SetPoint(const uint32 index, const Vec2 &p);
		void RemovePoint(const uint32 index);

		// Closed: the points are the whole shape. Open (the default): they
		// are the top of it, and it goes down to the floor.
		void SetClosed(const bool c);
		bool IsClosed() const { return closed; }
		// How many pieces each stretch between two points is curved into.
		// 0 is straight lines from point to point.
		void SetSmooth(const uint32 pieces);
		uint32 GetSmooth() const { return smooth; }
		// How far it reaches across, centred on the object's z.
		void SetDepth(const f32 d);
		f32 GetDepth() const { return depth; }
		// For an open outline: the y its faces go down to.
		void SetFloor(const f32 y);
		f32 GetFloor() const { return floorY; }

		// The two images, and how many units one repeat of them covers.
		void SetSkinTexture(const std::string &resolvedPath, const std::string &authoredPath);
		void SetFaceTexture(const std::string &resolvedPath, const std::string &authoredPath);
		const std::string &GetSkinTexturePath() const { return skinPath; }
		const std::string &GetFaceTexturePath() const { return facePath; }
		void SetTile(const f32 unitsPerRepeat);
		f32 GetTile() const { return tile; }
		void SetColors(const Vec4 &skin, const Vec4 &face);
		const Vec4 &GetSkinColor() const { return skinColor; }
		const Vec4 &GetFaceColor() const { return faceColor; }

		// Keeps the Physics2D on the same object (if it has one) as a chain
		// along the outline. On by default.
		void SetCollider(const bool on);
		bool HasCollider() const { return collider; }

		// The outline as drawn and collided with: the points, curved.
		const std::vector<Vec2> &GetPath();
		// For an open outline: its height at x (both in the object's own
		// space). The ends' heights beyond them.
		f32 HeightAt(const f32 x);

		// Makes the meshes and the collider from what has been set. Called
		// by itself before the next frame; call it to have them now.
		void Apply();

		// The points curved: Catmull-Rom through them, `pieces` to a stretch.
		static void SmoothPath(const std::vector<Vec2> &pts, const bool closed, const uint32 pieces, std::vector<Vec2> &out);
		// The two surfaces of a path given depth. False when there is no shape to make.
		static bool BuildMesh(const std::vector<Vec2> &path, const bool closed, const f32 depth, const f32 floorY,
			const f32 tile, ExtrusionMesh2D &skin, ExtrusionMesh2D &faces);
		// A simple polygon as triangles (indices into it). False if it has no area.
		static bool Triangulate(const std::vector<Vec2> &polygon, std::vector<uint32> &triangles);

		virtual void Update(const f64 time = 0);
		virtual bool NeedsUpdate() const { return dirty; }
		virtual uint32 GetComponentType() const { return ComponentType::Extrusion2D; }

	private:

		void MakeMaterials();

		std::vector<Vec2> points, path;
		bool closed, collider;
		uint32 smooth;
		f32 depth, floorY, tile;
		std::string skinPath, skinResolved, facePath, faceResolved;
		Vec4 skinColor, faceColor;
		bool dirty, pathDirty, materialsDirty;
	};

};

#endif /* EXTRUSION2D_H */
