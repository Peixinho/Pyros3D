//============================================================================
// Name        : Octree.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Octree
//============================================================================

#ifndef OCTREE_H
#define	OCTREE_H

#include <iostream>
#include <vector>
#include <algorithm>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Core/Projection/Projection.h>

namespace p3d {

	namespace OctreeObjectType {
		enum {
			Renderable,
			Light
		};
	}

	class OctreeGroup;
	class OctreeObject {
		public:
			uint32 type;
			GameObject* objPtr;
			OctreeGroup* boxPTR;
	};

	class Octree;
	class OctreeGroup
	{
		friend class Octree;

	public:
		float size;

		Vec3 Max, Min;

		std::vector<GameObject*> Members;

		OctreeGroup* childs[8];
		OctreeGroup* parent;

		bool selected;

		virtual ~OctreeGroup();

		OctreeGroup(const float Size, const Vec3 min, const Vec3 max, std::vector<GameObject*> &objects, uint32 ChildsPerNode) 
		{
			OctreeGroup(Size, min, max, objects, ChildsPerNode, NULL);
		}

		OctreeGroup(const float Size, const Vec3 min, const Vec3 max, std::vector<GameObject*> &objects, uint32 ChildsPerNode, OctreeGroup* Parent);

		std::vector<GameObject*> ReturnMembers();

		std::vector<GameObject*> ReturnAllMembers();

	protected:

		bool Insert(GameObject* go);
		bool Remove(GameObject* go);
		
		uint32 childsPerNode;

	private:

		bool haveChilds;

		void CreateSubGroups(const float Size, const Vec3 min, const Vec3 max, std::vector<GameObject*> &objects, uint32 ChildsPerNode);
	};

	class Octree
	{
	public:

		Octree() { Root = NULL; }

		virtual ~Octree() { delete Root; }

		OctreeGroup* Root;

		void BuildOctree(const Vec3 min, const Vec3 max, std::vector<GameObject*> objects, uint32 ChildsPerNode = 10);

		std::vector<GameObject*> SearchObjects(const Vec3 Position, const float radius);

		void Insert(GameObject* go);

		void Remove(GameObject* go);

		void Draw(Projection projection, Matrix camera);

	private:

		bool SearchInChildBox(OctreeGroup* box, const Vec3 &Position, const float radius, std::vector<GameObject*>* members);

		void _Draw(OctreeGroup* Child);

		void __Draw(OctreeGroup* Child);

	};

};
#endif /*OCTREE_H*/