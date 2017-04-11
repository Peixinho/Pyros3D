//============================================================================
// Name        : Terrain
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Terrain
//============================================================================

#ifndef TERRAIN_H
#define TERRAIN_H

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Other/Export.h>

namespace p3d {

	class PYROS3D_API Terrain : public Primitive {

		friend class TerrainRenderingComponent;

		public:

			Terrain(Texture* heightmap, const f32 dimensions, const uint32 segments, const f32 height = 1, bool smooth = false, bool repeatUV = false);
		
		protected:

			int imgWidth, imgHeight, seg;
			f32 unit;
	};

	class PYROS3D_API TerrainRenderingComponent : public RenderingComponent
	{

	protected:

		f32 segunits;

	public:

		TerrainRenderingComponent(Terrain* renderable, IMaterial* Material = NULL);
		bool GetHeightFromLocalCoords(Vec3 &coords);
		bool GetHeightFromGlobalCoords(Vec3 &coords);

	};

};

#endif /* TERRAIN_H */