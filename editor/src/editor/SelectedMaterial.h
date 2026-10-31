//============================================================================
// Name        : SelectedMaterial.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Selected Material - the selection's highlight, drawn over the
//               selected mesh (its surface: assets/selectedmaterial.glsl.surface)
//============================================================================

#ifndef SELECTEDMATERIAL_H
#define	SELECTEDMATERIAL_H

#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>

using namespace p3d;

class SelectedMaterial : public CustomShaderMaterial {
	public:
		SelectedMaterial() : CustomShaderMaterial("assets/selectedmaterial.glsl")
		{
			SetTransparencyFlag(true);
			DisableDepthWrite();
			SetCullFace(CullFace::BackFace);
		}
};

#endif	/* SELECTEDMATERIAL_H */
