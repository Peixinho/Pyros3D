//============================================================================
// Name        : Grid.h
// Author      : Duarte Peixinho
// Description : Axis-colored editor grid (ported from editor_newold GridRenderable)
//============================================================================

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>
#include <Pyros3D/Assets/Renderable/Renderables.h>
#include <vector>

#ifndef RENDERINGGRID_H
#define	RENDERINGGRID_H

using namespace p3d;

namespace {

class GridGeometry : public PrimitiveGeometry {
public:
	GridGeometry(float size, int divisions, const Vec4& lineColor, const Vec4& axisColor)
	{
		const float half = size;
		const int num = divisions;
		std::vector<Vec4> colors;
		colors.reserve(static_cast<size_t>((num + 1) * 4));

		auto pushLine = [&](const Vec3& a, const Vec3& b, const Vec4& c) {
			tVertex.push_back(a);
			tVertex.push_back(b);
			tNormal.push_back(Vec3(0, 1, 0));
			tNormal.push_back(Vec3(0, 1, 0));
			tTexcoord.push_back(Vec2(0, 0));
			tTexcoord.push_back(Vec2(0, 0));
			colors.push_back(c);
			colors.push_back(c);
		};

		const float step = (num > 0) ? (size * 2.0f / float(num)) : (size * 2.0f);
		for (int i = -num / 2; i <= num / 2; ++i) {
			float p = i * step;
			Vec4 col = (i == 0) ? axisColor : lineColor;
			pushLine(Vec3(-half, 0, p), Vec3(half, 0, p), col);
			pushLine(Vec3(p, 0, -half), Vec3(p, 0, half), col);
		}

		index.resize(tVertex.size());
		for (size_t i = 0; i < index.size(); ++i)
			index[i] = static_cast<__INDEX_C_TYPE__>(i);

		AttributeBuffer* vb = new AttributeBuffer(Buffer::Type::Attribute, Buffer::Draw::Static);
		vb->AddAttribute("aPosition", Buffer::Attribute::Type::Vec3, tVertex.data(), static_cast<uint32>(tVertex.size()));
		vb->AddAttribute("aColor", Buffer::Attribute::Type::Vec4, colors.data(), static_cast<uint32>(colors.size()));
		Attributes.push_back(vb);

		materialProperties.haveColor = true;
		materialProperties.Color = Vec4(1, 1, 1, 1);
		SendBuffers();
		CalculateBounding();
	}

protected:
	virtual void CalculateBounding()
	{
		minBounds = Vec3(-1000.0f, 0.0f, -1000.0f);
		maxBounds = Vec3(1000.0f, 0.0f, 1000.0f);
		BoundingSphereCenter = Vec3::ZERO;
		BoundingSphereRadius = 1500.0f;
	}
};

}

class Grid : public Renderable {
public:
	Grid(float size, int divisions, const Vec4& lineColor, const Vec4& axisColor)
	{
		Geometries.push_back(new GridGeometry(size, divisions, lineColor, axisColor));
		CalculateBounding();
	}
};

#endif	/* RENDERINGGRID_H */
