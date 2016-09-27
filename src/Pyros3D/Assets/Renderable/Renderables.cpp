//============================================================================
// Name        : Renderables.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Renderables
//============================================================================

#include <Pyros3D/Assets/Renderable/Renderables.h>

namespace p3d {

	uint32 IGeometry::_InternalID = 0;

	void Renderable::BuildMaterials(const uint32 &MaterialOptions)
	{
		for (std::vector<IGeometry*>::iterator i = Geometries.begin(); i != Geometries.end(); i++)
		{
			if (Materialsvector.find((*i)->materialProperties.id) == Materialsvector.end())
			{
				// From Properties
				uint32 options = 0;
				// Get Material Options
				if ((*i)->materialProperties.haveColor) options = options | ShaderUsage::Color;
				if ((*i)->materialProperties.haveSpecular) options = options | ShaderUsage::SpecularColor;
				if ((*i)->materialProperties.haveColorMap) options = options | ShaderUsage::Texture;
				if ((*i)->materialProperties.haveSpecularMap) options = options | ShaderUsage::SpecularMap;
				if ((*i)->materialProperties.haveNormalMap) options = options | ShaderUsage::BumpMapping;

				GenericShaderMaterial* genMat = new GenericShaderMaterial(options | MaterialOptions);

				// Material Properties
				if ((*i)->materialProperties.Twosided) genMat->SetCullFace(CullFace::DoubleSided);
				if ((*i)->materialProperties.haveColor) genMat->SetColor((*i)->materialProperties.Color);
				if ((*i)->materialProperties.haveSpecular) genMat->SetSpecular((*i)->materialProperties.Specular);
				if ((*i)->materialProperties.Opacity) genMat->SetOpacity((*i)->materialProperties.Opacity);
				if ((*i)->materialProperties.haveColorMap)
				{
					Texture* colorMap = new Texture();
					colorMap->LoadTexture((*i)->materialProperties.colorMap, TextureType::Texture);
					colorMap->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
					genMat->SetColorMap(colorMap);
					Texturesvector.push_back(colorMap);
				}
				if ((*i)->materialProperties.haveSpecularMap)
				{
					Texture* specularMap = new Texture();
					specularMap->LoadTexture((*i)->materialProperties.specularMap, TextureType::Texture);
					specularMap->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
					genMat->SetSpecularMap(specularMap);
					Texturesvector.push_back(specularMap);
				}
				if ((*i)->materialProperties.haveNormalMap)
				{
					Texture* normalMap = new Texture();
					normalMap->LoadTexture((*i)->materialProperties.normalMap, TextureType::Texture);
					normalMap->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
					genMat->SetNormalMap(normalMap);
					Texturesvector.push_back(normalMap);
				}
				Materialsvector[(*i)->materialProperties.id] = genMat;
				(*i)->Material = genMat;
			}
			else {
				(*i)->Material = Materialsvector[(*i)->materialProperties.id];
			}
		}
	}

};