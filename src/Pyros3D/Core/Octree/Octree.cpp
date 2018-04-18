//============================================================================
// Name        : Octree.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Octree
//============================================================================

#include <Pyros3D/Core/Octree/Octree.h>
#include <Pyros3D/Other/PyrosGL.h>

namespace p3d {

	OctreeGroup::~OctreeGroup()
	{
		if (haveChilds)
		{
			delete childs[0];
			delete childs[1];
			delete childs[2];
			delete childs[3];
			delete childs[4];
			delete childs[5];
			delete childs[6];
			delete childs[7];
		}
	}

	OctreeGroup::OctreeGroup(const float Size, const Vec3 min, const Vec3 max, std::vector<GameObject*> &objects, uint32 ChildsPerNode, OctreeGroup* Parent) : haveChilds(false)
	{
		// Keep Actual Size
		size = Size;

		childsPerNode = ChildsPerNode;

		// Save Dimensions
		Max = max;
		Min = min;

		selected = false;

		parent = Parent;

		// Look For Members
		if (objects.size() > 0)
		{
			for (std::vector<GameObject*>::iterator i = objects.begin(); i != objects.end();)
			{
				Vec3 minTransform = (*i)->GetBoundingMinValue();
				Vec3 maxTransform = (*i)->GetBoundingMaxValue();
				if (
					(minTransform.x > min.x && maxTransform.x < max.x) &&
					(minTransform.y > min.y && maxTransform.y < max.y) &&
					(minTransform.z > min.z && maxTransform.z < max.z)
					)
				{
					Members.push_back((*i));
					i = objects.erase(i);
				}
				else {
					i++;
				}
			}
			// Create Groups
			if (Members.size() > ChildsPerNode) CreateSubGroups(size*.25f, min, max, Members, ChildsPerNode);
		}
	}

	// Return Box Members
	std::vector<GameObject*> OctreeGroup::ReturnMembers()
	{
		return Members;
	}

	// Return Box Members and Children Boxes
	std::vector<GameObject*> OctreeGroup::ReturnAllMembers()
	{
		std::vector<GameObject*> _Members = Members;
		if (haveChilds)
			for (uint32 i = 0; i < 8; i++)
			{
				std::vector<GameObject*> _m = childs[i]->ReturnAllMembers();
				if (_m.size() > 0)
					_Members.insert(_Members.end(), _m.begin(), _m.end());
			}

		return _Members;
	}

	void OctreeGroup::CreateSubGroups(const float Size, const Vec3 min, const Vec3 max, std::vector<GameObject*> &objects, uint32 ChildsPerNode)
	{
		haveChilds = true;

		//float
		Vec3 center = (min + max)*.5f;

		// build childs
		childs[0] = new OctreeGroup(Size*.25f, center, max, objects, ChildsPerNode, this);
		childs[1] = new OctreeGroup(Size*.25f, Vec3(center.x, min.y, center.z), Vec3(max.x, center.y, max.z), objects, ChildsPerNode, this);
		childs[2] = new OctreeGroup(Size*.25f, min, center, objects, ChildsPerNode, this);
		childs[3] = new OctreeGroup(Size*.25f, Vec3(min.x, center.y, center.z), Vec3(center.x, max.y, max.z), objects, ChildsPerNode, this);
		childs[4] = new OctreeGroup(Size*.25f, Vec3(center.x, center.y, min.z), Vec3(max.x, max.y, center.z), objects, ChildsPerNode, this);
		childs[5] = new OctreeGroup(Size*.25f, Vec3(center.x, min.y, min.z), Vec3(max.x, center.y, center.z), objects, ChildsPerNode, this);
		childs[6] = new OctreeGroup(Size*.25f, Vec3(min.x, min.y, center.z), Vec3(center.x, center.y, max.z), objects, ChildsPerNode, this);
		childs[7] = new OctreeGroup(Size*.25f, Vec3(min.x, center.y, min.z), Vec3(center.x, max.y, center.z), objects, ChildsPerNode, this);
	}

	bool OctreeGroup::Insert(GameObject* go)
	{

		Vec3 minTransform = go->GetBoundingMinValue();
		Vec3 maxTransform = go->GetBoundingMaxValue();

		if (
			(minTransform.x > Min.x && maxTransform.x < Max.x) &&
			(minTransform.y > Min.y && maxTransform.y < Max.y) &&
			(minTransform.z > Min.z && maxTransform.z < Max.z)
			)
		{
			// Fits this Box
			// Lets check for childs first
			if (haveChilds)
			{
				bool inserted = false;
				for (uint32 i = 0; i < 8; i++)
				{
					if (
						(minTransform.x > childs[i]->Min.x && maxTransform.x < childs[i]->Max.x) &&
						(minTransform.y > childs[i]->Min.y && maxTransform.y < childs[i]->Max.y) &&
						(minTransform.z > childs[i]->Min.z && maxTransform.z < childs[i]->Max.z)
						)
					{
						if (childs[i]->Insert(go))
						{
							inserted = true;
							break;
						}
					}
				}
				if (!inserted) // No child fits
				{
					Members.push_back(go);
					return true;
				}
			}
			else {
				// Doesn't have childs

				// Add to this members
				Members.push_back(go);

				// If members list is bigger than expected, and no children exists, build them
				if (Members.size() > childsPerNode)
					CreateSubGroups(size*.25f, Min, Max, Members, childsPerNode);

				return true;
			}
		}
		return false;
	}

	bool OctreeGroup::Remove(GameObject* go)
	{
		Vec3 minTransform = go->GetBoundingMinValue();
		Vec3 maxTransform = go->GetBoundingMaxValue();

		if (
			(minTransform.x > Min.x && maxTransform.x < Max.x) &&
			(minTransform.y > Min.y && maxTransform.y < Max.y) &&
			(minTransform.z > Min.z && maxTransform.z < Max.z)
			)
		{
			if (haveChilds)
			{
				bool removed = false;
				for (uint32 i = 0; i < 8; i++)
				{
					if (
						(minTransform.x > childs[i]->Min.x && maxTransform.x < childs[i]->Max.x) &&
						(minTransform.y > childs[i]->Min.y && maxTransform.y < childs[i]->Max.y) &&
						(minTransform.z > childs[i]->Min.z && maxTransform.z < childs[i]->Max.z)
						)
					{
						if (childs[i]->Remove(go))
						{
							removed = true;
							break;
						}
					}
				}
				if (!removed) // No child fits
				{
					for (std::vector<GameObject*>::iterator g = Members.begin(); g != Members.end(); g++)
					{
						if ((*g) == go)
						{
							Members.erase(g);
							break;
							return true;
						}
					}
				}
			}
			for (std::vector<GameObject*>::iterator g = Members.begin(); g != Members.end(); g++)
			{
				if ((*g) == go)
				{
					Members.erase(g);
					break;
					return true;
				}
			}
		}
		return false;
	}

	void Octree::BuildOctree(const Vec3 min, const Vec3 max, std::vector<GameObject*> objects, uint32 ChildsPerNode)
	{
		if (Root != NULL) delete Root;

		float Size = max.distance(min);

		Root = new OctreeGroup(Size, min, max, objects, ChildsPerNode);
	}

	std::vector<GameObject*> Octree::SearchObjects(const Vec3 Position, const float radius)
	{
		std::vector<GameObject*> _members;

		SearchInChildBox(Root, Position, radius, &_members);

		return _members;
	}

	void Octree::Insert(GameObject* go)
	{
		Root->Insert(go);
	}

	void Octree::Remove(GameObject* go)
	{
		Root->Remove(go);
	}

	void Octree::Draw(Projection projection, Matrix camera)
	{
	}

	bool Octree::SearchInChildBox(OctreeGroup* box, const Vec3 &Position, const float radius, std::vector<GameObject*>* members)
	{
		if (Position.x + radius<box->Max.x &&
			Position.y + radius<box->Max.y &&
			Position.z + radius<box->Max.z &&
			Position.x - radius>box->Min.x &&
			Position.y - radius>box->Min.y &&
			Position.z - radius>box->Min.z)
		{
			// Its in this Box
			std::vector<GameObject*> _m = box->ReturnMembers();
			members->insert(members->end(), _m.begin(), _m.end());
			box->selected = true;

			// Test if it have children
			if (box->haveChilds)
			{
				// Test if one of them fits
				bool _search = false;
				for (int i = 0; i < 8; i++)
				{
					if (SearchInChildBox(box->childs[i], Position, radius, members))
						_search = true;
				}

				if (!_search)
				{
					// Set this box and children too
					std::vector<GameObject*> _m = box->ReturnAllMembers();
					if (_m.size() > 0)
						members->insert(members->end(), _m.begin(), _m.end());
				}
			}

			return true;
		}
		else {
			// Doesn't Fit this Box
			box->selected = false;
			return false;
		}
	}

	void Octree::_Draw(OctreeGroup* Child)
	{
		__Draw(Child);
		if (Child->haveChilds)
		{
			for (uint32 i = 0; i < 8; i++)
			{
				_Draw(Child->childs[i]);
			}
		}
	}

	void Octree::__Draw(OctreeGroup* Child)
	{
		//if (Child->selected) glColor3f(1.0f, 0.0f, 0.0f);
		if (Child->selected)
		{
		}
	}

	/******************************* DEBUG DRAW ******************************* */
};
