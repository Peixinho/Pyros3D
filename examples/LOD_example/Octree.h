#pragma once

#include <iostream>
#include <vector>
#include <algorithm>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>

using namespace p3d;

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

	virtual ~OctreeGroup() 
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

	OctreeGroup(const float Size, const Vec3 min, const Vec3 max, std::vector<GameObject*> &objects, uint32 ChildsPerNode) : OctreeGroup(Size, min, max, objects, ChildsPerNode, NULL) {}

	OctreeGroup(const float Size, const Vec3 min, const Vec3 max, std::vector<GameObject*> &objects, uint32 ChildsPerNode, OctreeGroup* Parent) : haveChilds(false)
	{
		// Keep Actual Size
		size = Size;

		// Save Dimensions
		Max = max;
		Min = min;

		parent = Parent;
		
		// Look For Members
		for (std::vector<GameObject*>::iterator i = objects.begin(); i != objects.end();)
		{
			Vec3 minTransform = (*i)->GetWorldTransformation()*(*i)->GetMinBounds();
			Vec3 maxTransform = (*i)->GetWorldTransformation()*(*i)->GetMaxBounds();
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

	// Return Box Members
	std::vector<GameObject*> ReturnMembers()
	{
		return Members;
	}

	// Return Box and Sub Boxes Members
	std::vector<GameObject*> ReturnAllMembers()
	{
		std::vector<GameObject*> _Members = Members;
		if (haveChilds)
			for (uint32 i = 0; i < 8; i++)
				_Members.insert(_Members.end(), childs[i]->ReturnAllMembers().begin(), childs[i]->ReturnAllMembers().end());

		return _Members;
	}

private:
	
	bool haveChilds;

	void CreateSubGroups(const float Size, const Vec3 min, const Vec3 max, std::vector<GameObject*> &objects, uint32 ChildsPerNode)
	{
		haveChilds = true;

		//float
		Vec3 center = (min+max)*.5f;

		// build childs
		childs[0] = (new OctreeGroup(Size*.25f, center, max, objects, ChildsPerNode, this));
		childs[1] = (new OctreeGroup(Size*.25f, Vec3(center.x, min.y, center.z), Vec3(max.x,center.y,max.z), objects, ChildsPerNode, this));
		childs[2] = (new OctreeGroup(Size*.25f, min, center, objects, ChildsPerNode, this));
		childs[3] = (new OctreeGroup(Size*.25f, Vec3(min.x,center.y,center.z), Vec3(center.x, max.y, max.z), objects, ChildsPerNode, this));
		childs[4] = (new OctreeGroup(Size*.25f, Vec3(center.x,center.y,min.z), Vec3(max.x, max.y, center.z), objects, ChildsPerNode, this));
		childs[5] = (new OctreeGroup(Size*.25f, Vec3(center.x,min.y,min.z), Vec3(max.x, center.y, center.z), objects, ChildsPerNode, this));
		childs[6] = (new OctreeGroup(Size*.25f, Vec3(min.x,min.y,center.z), Vec3(center.x, center.y, max.z), objects, ChildsPerNode, this));
		childs[7] = (new OctreeGroup(Size*.25f, Vec3(min.x,center.y,min.z), Vec3(center.x, max.y, center.z), objects, ChildsPerNode, this));
	}
};

class Octree
{
public:

	Octree() { Root = NULL; }

	virtual ~Octree() { delete Root; }

	OctreeGroup* Root;

	void BuildOctree(const Vec3 min, const Vec3 max, std::vector<GameObject*> objects, uint32 ChildsPerNode = 10)
	{
		if (Root != NULL) delete Root;

		float Size = max.distance(min);

		Root = new OctreeGroup(Size, min, max, objects, ChildsPerNode);
	}

	void Insert(GameObject* go)
	{

	}

	void Remove(GameObject* go)
	{

	}

	/******************************* DEBUG DRAW ******************************* */
	void Draw(Projection projection, Matrix camera)
	{
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		glLoadMatrixf(&projection.m.m[0]);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glLoadMatrixf(&camera.m[0]);

		glTranslatef(1.5f, 0.0f, -7.0f);

		glBegin(GL_LINES);
		glColor3f(1.0f, 0.0f, 0.0f);

		if (Root != NULL)
		{
			_Draw(Root);
		}

		glEnd(); 
	}

	void _Draw(OctreeGroup* Child)
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

	void __Draw(OctreeGroup* Child)
	{
			glVertex3f(Child->Max.x, Child->Max.y, Child->Min.z);
			glVertex3f(Child->Min.x, Child->Max.y, Child->Min.z);
			glVertex3f(Child->Min.x, Child->Max.y, Child->Min.z);
			glVertex3f(Child->Min.x, Child->Max.y, Child->Max.z);
			glVertex3f(Child->Min.x, Child->Max.y, Child->Max.z);
			glVertex3f(Child->Max.x, Child->Max.y, Child->Max.z);
			glVertex3f(Child->Max.x, Child->Max.y, Child->Max.z);
			glVertex3f(Child->Max.x, Child->Max.y, Child->Min.z);

			glVertex3f(Child->Max.x, Child->Min.y, Child->Max.z);
			glVertex3f(Child->Min.x, Child->Min.y, Child->Max.z);
			glVertex3f(Child->Min.x, Child->Min.y, Child->Max.z);
			glVertex3f(Child->Min.x, Child->Min.y, Child->Min.z);
			glVertex3f(Child->Min.x, Child->Min.y, Child->Min.z);
			glVertex3f(Child->Max.x, Child->Min.y, Child->Min.z);
			glVertex3f(Child->Max.x, Child->Min.y, Child->Min.z);
			glVertex3f(Child->Max.x, Child->Min.y, Child->Max.z);

			glVertex3f(Child->Max.x, Child->Max.y, Child->Max.z);
			glVertex3f(Child->Min.x, Child->Max.y, Child->Max.z);
			glVertex3f(Child->Min.x, Child->Max.y, Child->Max.z);
			glVertex3f(Child->Min.x, Child->Min.y, Child->Max.z);
			glVertex3f(Child->Min.x, Child->Min.y, Child->Max.z);
			glVertex3f(Child->Max.x, Child->Min.y, Child->Max.z);
			glVertex3f(Child->Max.x, Child->Min.y, Child->Max.z);
			glVertex3f(Child->Max.x, Child->Max.y, Child->Max.z);

			glVertex3f(Child->Max.x, Child->Min.y, Child->Min.z);
			glVertex3f(Child->Min.x, Child->Min.y, Child->Min.z);
			glVertex3f(Child->Min.x, Child->Min.y, Child->Min.z);
			glVertex3f(Child->Min.x, Child->Max.y, Child->Min.z);
			glVertex3f(Child->Min.x, Child->Max.y, Child->Min.z);
			glVertex3f(Child->Max.x, Child->Max.y, Child->Min.z);
			glVertex3f(Child->Max.x, Child->Max.y, Child->Min.z);
			glVertex3f(Child->Max.x, Child->Min.y, Child->Min.z);
	}

	/******************************* DEBUG DRAW ******************************* */

};