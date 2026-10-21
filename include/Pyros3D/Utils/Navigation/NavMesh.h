//============================================================================
// Name        : NavMesh.h
// Description : Where a character can walk, and the way there.
//============================================================================
#ifndef NAVMESH_H
#define NAVMESH_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>
#include <string>
#include <vector>

class dtNavMesh;
class dtNavMeshQuery;
class dtCrowd;

namespace p3d {

	class SceneGraph;
	class GameObject;
	class RenderingComponent;

	// How the mesh is made: the size of the character it is for, and how finely the level is
	// sampled. Lengths in metres, the slope in degrees.
	struct PYROS3D_API NavMeshSettings {
		f32 agentRadius = 0.4f;       // how far from walls it keeps
		f32 agentHeight = 1.8f;       // the headroom it needs
		f32 agentMaxClimb = 0.4f;     // the highest step it walks up
		f32 agentMaxSlope = 45.f;     // the steepest ground it walks on
		f32 cellSize = 0.2f;          // across: about half the radius
		f32 cellHeight = 0.1f;        // up: about a quarter of the climb
		f32 tileSize = 32.f;          // metres a tile (a big world is many, built in parallel)
		f32 regionMinSize = 8.f;      // islands smaller than this (cells across) are dropped
		f32 regionMergeSize = 20.f;
		f32 edgeMaxLength = 12.f;
		f32 edgeMaxError = 1.3f;
		f32 detailSampleDistance = 6.f;   // (cells) how closely the polygons follow the ground's height
		f32 detailSampleMaxError = 1.f;
	};

	// A place walking does not join (a drop, a ladder, a gap to jump): from a to b, or both ways.
	struct PYROS3D_API NavLink {
		Vec3 a, b;
		f32 radius = 0.5f;
		bool bidirectional = false;
	};

	// A navigation mesh: the walkable surface of a level for a character of one size, as convex
	// polygons (Recast), and paths on it (Detour). Feed it the level's triangles - a scene's static
	// meshes, or any you give it - Build(), then ask it the way from anywhere to anywhere: up a
	// ramp, round a wall, through the one gap, up the stairs to another floor. Save() keeps the
	// result, Load() takes it back without building (bake it in the editor, load it in the game).
	//
	// Off-mesh links join places walking does not (a jump down from a ledge, a ladder); a path may
	// use them, and says where (IsLinkPoint) so the character can play the jump.
	//
	// It says where to go; how to get through other people on the way is Crowd's.
	class PYROS3D_API NavMesh {
	public:
		NavMesh();
		~NavMesh();

		// --- What it is made from (world space) ---
		void ClearGeometry();
		void AddTriangles(const std::vector<Vec3> &vertices, const std::vector<uint32> &indices, const Matrix &world = Matrix());
		// One object's meshes (the nearest level), where it is now. Skinned ones are left out.
		void AddObject(GameObject* go, const bool withChildren = true);
		// Every object in the scene that renders something, or only the ones marked static (the
		// level, not the people and props in it).
		void AddScene(SceneGraph* scene, const bool staticOnly = true);
		uint32 GetTriangleCount() const { return (uint32)(indices.size() / 3); }

		// A link from a to b (bidirectional: both ways, else a to b only - a drop).
		void AddLink(const Vec3 &a, const Vec3 &b, const f32 radius = 0.5f, const bool bidirectional = false);
		void ClearLinks();
		const std::vector<NavLink> &GetLinks() const { return links; }

		// --- Making it ---
		bool Build(const NavMeshSettings &settings = NavMeshSettings());
		bool IsBuilt() const { return navMesh != NULL; }
		const NavMeshSettings &GetSettings() const { return settings; }
		f64 GetBuildMilliseconds() const { return buildMs; }
		uint32 GetTileCount() const;
		uint32 GetPolygonCount() const;

		bool Save(const std::string &path) const;
		bool Load(const std::string &path);

		// --- Asking it ---
		// The way from `from` to `to` as the corners to walk to (the first is `from` on the mesh,
		// the last `to` on it, or the nearest the mesh reaches if `to` cannot be got to - then
		// false). Points not on the mesh are taken to the nearest that is, within `searchExtent`.
		bool FindPath(const Vec3 &from, const Vec3 &to, std::vector<Vec3> &path, const Vec3 &searchExtent = Vec3(2.f, 4.f, 2.f)) const;
		// Whether path point `i` of the last FindPath starts an off-mesh link (a jump, a ladder).
		bool IsLinkPoint(const uint32 i) const { return i < lastLinkFlags.size() && lastLinkFlags[i]; }
		// The nearest point on the mesh (false: none within the extent).
		bool NearestPoint(const Vec3 &p, Vec3 &out, const Vec3 &searchExtent = Vec3(2.f, 4.f, 2.f)) const;
		// Walking in a straight line from `from` towards `to`: true if it gets there, otherwise
		// false with `hit` where it is stopped (a wall, a drop).
		bool Raycast(const Vec3 &from, const Vec3 &to, Vec3 &hit) const;
		// Somewhere on it, at random (anywhere, or within `radius` of `around` when radius > 0).
		bool RandomPoint(Vec3 &out, const Vec3 &around = Vec3(), const f32 radius = 0.f) const;
		// The mesh as triangles (each polygon's detail), for drawing it.
		void GetDebugTriangles(std::vector<Vec3> &out) const;

		// --- Agents walking on it (Detour's crowd: NavAgentComponent is the usual way in) ---
		// Each follows its path round walls and the other agents (the walls near it are obstacles
		// too, so a crowd gets through a door both ways instead of jamming in it), takes the
		// links on its way, and is kept on the mesh. -1: the mesh is full, or `p` is off it.
		int32 AddAgent(const Vec3 &p, const f32 radius, const f32 height, const f32 maxSpeed, const f32 maxAcceleration);
		void RemoveAgent(const int32 id);
		void SetAgentLimits(const int32 id, const f32 radius, const f32 maxSpeed, const f32 maxAcceleration);
		bool SetAgentTarget(const int32 id, const Vec3 &target);
		void StopAgent(const int32 id);
		// Where it is, how fast it moves, whether it is crossing a link, and the walk left to its
		// target - which means something only once `planned` (the crowd has its way there: until
		// then the walk left reads 0, and an agent that took that for arrival never set off).
		bool GetAgent(const int32 id, Vec3 &position, Vec3 &velocity, bool &onLink, f32 &remaining, bool &planned) const;
		void UpdateAgents(const f32 dt);
		uint32 GetAgentCount() const;

	private:
		bool InitQuery();
		NavMeshSettings settings;
		std::vector<f32> verts;          // xyz
		std::vector<int32> indices;
		std::vector<NavLink> links;
		dtNavMesh* navMesh;
		dtNavMeshQuery* query;
		dtCrowd* crowd;
		bool InitCrowd();
		f64 buildMs;
		mutable std::vector<uint8> lastLinkFlags;
	};
}

#endif /* NAVMESH_H */
