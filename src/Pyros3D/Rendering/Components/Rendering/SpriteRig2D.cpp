//============================================================================
// Name        : SpriteRig2D.cpp
// Description : See SpriteRig2D.h.
//============================================================================

#include <Pyros3D/Rendering/Components/Rendering/SpriteRig2D.h>
#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Rendering/Components/UI/UIImage.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Core/Logs/Log.h>

namespace p3d {

	namespace {

		// One part's quad. Built at the texture's aspect so a part is never
		// stretched by default; its authored size lives in the part's scale,
		// which the bone matrix carries - so replacing a texture with one of a
		// different pixel size does not resize the character.
		class QuadGeometry : public PrimitiveGeometry {
		public:
			QuadGeometry(const f32 halfW, const f32 halfH)
			{
				const Vec3 n(0.f, 0.f, 1.f);
				tVertex.push_back(Vec3(-halfW, -halfH, 0.f)); tNormal.push_back(n); tTexcoord.push_back(Vec2(0.f, 1.f));
				tVertex.push_back(Vec3( halfW, -halfH, 0.f)); tNormal.push_back(n); tTexcoord.push_back(Vec2(1.f, 1.f));
				tVertex.push_back(Vec3( halfW,  halfH, 0.f)); tNormal.push_back(n); tTexcoord.push_back(Vec2(1.f, 0.f));
				tVertex.push_back(Vec3(-halfW,  halfH, 0.f)); tNormal.push_back(n); tTexcoord.push_back(Vec2(0.f, 0.f));

				index.push_back(0); index.push_back(1); index.push_back(2);
				index.push_back(2); index.push_back(3); index.push_back(0);

				// The full sequence Primitive::Build() does. CreateBuffers()
				// alone builds the attribute list but never uploads it, and a
				// mesh whose buffers were never sent crashes the renderer the
				// first time it is bound.
				CreateBuffers(false);
				SendBuffers();

				materialProperties.haveColor = true;
				materialProperties.haveBones = false;
				materialProperties.haveSpecular = false;
				materialProperties.haveColorMap = false;
				materialProperties.haveSpecularMap = false;
				materialProperties.haveNormalMap = false;
				materialProperties.Color = Vec4(1.f, 1.f, 1.f, 1.f);
			}
		};

		// The character's renderable: one quad per part, in part order, so
		// mesh i always corresponds to part i. RenderingComponent relies on
		// that to drive each mesh from its own bone.
		class SpriteRigRenderable : public Renderable {
		public:
			void AddQuad(const f32 halfW, const f32 halfH)
			{
				Geometries.push_back(new QuadGeometry(halfW, halfH));
			}
			void Finish() { CalculateBounding(); }
		};

	}

	SpriteRig2DBuild BuildSpriteRig2D(const std::vector<SpritePart2D> &parts,
		const std::function<std::string(const std::string&)> &resolve)
	{
		SpriteRig2DBuild out;
		std::shared_ptr<SpriteRigRenderable> r = std::make_shared<SpriteRigRenderable>();

		for (size_t i = 0; i < parts.size(); i++)
		{
			std::shared_ptr<Texture> tex;
			f32 aspect = 1.f;
			if (!parts[i].texture.empty())
			{
				const std::string path = resolve ? resolve(parts[i].texture) : parts[i].texture;
				std::shared_ptr<Texture> t = std::make_shared<Texture>();
				if (t->LoadTexture(path, TextureType::Texture))
				{
					t->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
					t->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
					if (t->GetHeight() > 0)
						aspect = (f32)t->GetWidth() / (f32)t->GetHeight();
					tex = t;
				}
				else
					echo("WARNING: sprite part '" + parts[i].name + "' could not load " + parts[i].texture);
			}

			// Half-extents (aspect, 1) - the same quad Plane(aspect, 1.f)
			// builds, which is what every sprite in an existing scene is. Half
			// that size and every migrated character came out at half scale
			// with its pivots in the wrong place.
			r->AddQuad(aspect, 1.f);
			out.halfExtents.push_back(Vec2(aspect, 1.f));

			// One material per part: each carries its own texture, and they
			// are all cut-outs, so blending and double-sided are the right
			// defaults rather than a 3D mesh's.
			//
			// Lighting2D is distance falloff with no N.L, which is what a flat
			// quad needs - with N.L a light in the sprite's own plane leaves it
			// unlit. See SceneEditor::OpMakeSprite2DLit.
			uint32 usage = ShaderUsage::Color | ShaderUsage::Diffuse | ShaderUsage::Texture;
			if (parts[i].lit) usage |= ShaderUsage::Lighting2D;
			std::shared_ptr<GenericShaderMaterial> mat = std::make_shared<GenericShaderMaterial>(usage);
			mat->SetColor(Vec4(1.f, 1.f, 1.f, 1.f));
			mat->SetColorMap(tex ? tex : UIImage::WhiteTexture());
			mat->EnableBlending();
			mat->BlendingEquation(BlendEq::Add);
			mat->BlendingFunction(BlendFunc::Src_Alpha, BlendFunc::One_Minus_Src_Alpha);
			mat->SetTransparencyFlag(true);
			mat->SetCullFace(CullFace::DoubleSided);
			out.materials.push_back(mat);
		}

		r->Finish();
		out.renderable = r;
		return out;
	}

}
