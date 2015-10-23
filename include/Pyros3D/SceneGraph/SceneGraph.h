//============================================================================
// Name        : SceneGraph.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SceneGraph
//============================================================================

#ifndef SCENEGRAPH_H
#define	SCENEGRAPH_H

#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Other/Export.h>

using namespace p3d::Math;

namespace p3d {

	class GameObject;

	class SceneGraph
	{

	public:

		SceneGraph();

		// Update
		void Update(const f64 &Timer);
		// Add Child to Scene
		void Add(GameObject* GO);
		// Remove Child from Scene
		void Remove(GameObject* GO);
		// Get Time
		const f64 &GetTime() const;

		void AddGameObject(GameObject* Component);
		void RemoveGameObject(GameObject* Component);

		std::vector<GameObject*> &GetAllGameObjectList();
		std::vector<GameObject*> &GetStaticGameObjectList();
		std::vector<GameObject*> &GetDynamicGameObjectList();

		const Vec3 &GetMinBounds() const;
		const Vec3 &GetMaxBounds() const;

	private:

		// GameObject Dynamic List
		std::vector<GameObject*> _GameObjectListDynamic;
		// GameObject Static Lists
		std::vector<GameObject*> _GameObjectListStaticPrevious;
		std::vector<GameObject*> _GameObjectListStaticAfter;
		// GameObject All List
		std::vector<GameObject*> _GameObjectListALL;

		// Time
		f64 timer;

		Vec3 minBounds, maxBounds;
	};

};

#endif	/* SCENEGRAPH_H */