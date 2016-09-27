//============================================================================
// Name        : ModelLoader.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Loads Pyros3D Own Model Format
//============================================================================

#ifndef MODELLOADER_H
#define	MODELLOADER_H

#include <map>
#include <vector>
#include <Pyros3D/Utils/ModelLoaders/IModelLoader.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Ext/StringIDs/StringID.hpp>
#include <Pyros3D/Assets/Renderable/Renderables.h>
#include <Pyros3D/Utils/Binary/BinaryFile.h>


namespace p3d {

	class PYROS3D_API ModelLoader : public IModelLoader {
	public:

		ModelLoader();

		virtual ~ModelLoader();

		virtual bool Load(const std::string &Filename);
	};
}

#endif	/* MODELLOADER_H */