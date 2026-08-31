//============================================================================
// Name        : Occluder2D
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A shape that blocks 2D light
//============================================================================

#include <Pyros3D/Rendering/Components/Occluder2D/Occluder2D.h>
#include <Pyros3D/Physics/Physics2D/Physics2D.h>
#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <cmath>

namespace p3d {

	Occluder2D::Occluder2D(const uint32 shape, const Vec2 &size)
	{
		shapeType = shape;
		this->size = size;
		enabled = true;
	}

	Occluder2D::~Occluder2D() {}

	namespace {
		// One shape, in world space, appended as line segments.
		void EmitShape(std::vector<Vec4> &out, const uint32 shape, const Vec2 &h,
			const Vec3 &c, const f32 angle)
		{
			if (shape == Occluder2DShape::Circle)
			{
				// An inscribed octagon: a circle has no edges, and the segment
				// budget is the scarce thing in the shader's loop.
				static const int N = 8;
				for (int k = 0; k < N; k++)
				{
					const f32 a0 = (f32)k / N * 6.2831853f;
					const f32 a1 = (f32)(k + 1) / N * 6.2831853f;
					out.push_back(Vec4(c.x + cosf(a0) * h.x, c.y + sinf(a0) * h.x,
						c.x + cosf(a1) * h.x, c.y + sinf(a1) * h.x));
				}
				return;
			}
			const f32 ca = cosf(angle), sa = sinf(angle);
			const f32 xs[4] = { -h.x, h.x, h.x, -h.x };
			const f32 ys[4] = { -h.y, -h.y, h.y, h.y };
			for (int k = 0; k < 4; k++)
			{
				const int n = (k + 1) % 4;
				out.push_back(Vec4(
					c.x + (ca * xs[k] - sa * ys[k]), c.y + (sa * xs[k] + ca * ys[k]),
					c.x + (ca * xs[n] - sa * ys[n]), c.y + (sa * xs[n] + ca * ys[n])));
			}
		}
	}

	void Occluder2D::PublishSceneOccluders(SceneGraph* Scene)
	{
		std::vector<Vec4> segments;
		if (Scene != NULL)
		{
			std::vector<std::shared_ptr<GameObject> > &all = Scene->GetAllGameObjectList();
			for (size_t i = 0; i < all.size(); i++)
			{
				if (!all[i]) continue;
				const Vec3 c = all[i]->GetWorldPosition();
				const f32 a = all[i]->GetRotation().z;
				const std::vector<std::shared_ptr<IComponent> > &comps = all[i]->GetComponents();
				for (size_t k = 0; k < comps.size(); k++)
				{
					if (!comps[k]) continue;
					if (comps[k]->GetComponentType() == ComponentType::Occluder2D)
					{
						Occluder2D* o = static_cast<Occluder2D*>(comps[k].get());
						if (o->IsEnabled()) EmitShape(segments, o->GetShapeType(), o->GetSize(), c, a);
					}
					else if (comps[k]->GetComponentType() == ComponentType::Physics2D)
					{
						Physics2D* p = static_cast<Physics2D*>(comps[k].get());
						if (p->CastsShadow())
							EmitShape(segments, p->GetShapeType() == Shape2DType::Circle
								? Occluder2DShape::Circle : Occluder2DShape::Box,
								p->GetSize(), c, a);
					}
				}
			}
		}
		IRenderer::SetOccluders2D(segments);
	}

};
