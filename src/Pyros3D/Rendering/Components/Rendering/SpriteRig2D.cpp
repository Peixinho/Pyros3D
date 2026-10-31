//============================================================================
// Name        : SpriteRig2D.cpp
// Description : See SpriteRig2D.h.
//============================================================================

#include <Pyros3D/Rendering/Components/Rendering/SpriteRig2D.h>
#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>
#include <Pyros3D/Assets/Renderable/Models/Model.h>
#include <algorithm>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Rendering/Components/UI/UIImage.h>
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

		// A bent part: a grid placed in the rig's REST space and skinned to a
		// chain of up to four bones, its weights crossing from one bone to the
		// next around each joint between them. The renderer composes
		// world * Pivot * skinned vertex, so the part's Pivot stays identity
		// and the bones do the rest.
		class BentQuadGeometry : public ModelGeometry {
		public:
			BentQuadGeometry(const f32 halfW, const f32 halfH, const SpritePart2D &part,
				const std::vector<int32> &chain, const std::vector<Matrix> &bind)
			{
				const int n = std::max(1, std::min(64, (int)part.bendSegments));
				const f32 blend = std::max(0.02f, std::min(1.f, part.bendBlend));
				// The part's own placement in its bone's frame, as a rigid part
				// gets it through its Pivot (see RefreshSpriteParts2D).
				const f32 lx = -halfW + part.pivot.x * (halfW * 2.f);
				const f32 ly = -halfH + part.pivot.y * (halfH * 2.f);
				Matrix off; off.Translate(Vec3(part.offset.x, part.offset.y, part.z));
				Matrix sc; sc.Scale(Vec3(part.scale.x, part.scale.y, 1.f));
				Matrix pv; pv.Translate(Vec3(-lx, -ly, 0.f));
				const Matrix place = bind[chain[0]] * off * sc * pv;

				// The joints the bend crosses, root first.
				std::vector<Vec3> joints;
				for (size_t k = 0; k < chain.size(); k++) joints.push_back(bind[chain[k]].GetTranslation());
				const size_t segs = joints.size() - 1;   // >= 1

				const Vec3 nrm(0.f, 0.f, 1.f);
				for (int yi = 0; yi <= n; yi++)
					for (int xi = 0; xi <= n; xi++)
					{
						const f32 u = (f32)xi / n, v = (f32)yi / n;
						const Vec3 local(-halfW + u * 2.f * halfW, -halfH + v * 2.f * halfH, 0.f);
						const Vec3 p = place * local;
						tVertex.push_back(p);
						tNormal.push_back(nrm);
						tTexcoord.push_back(Vec2(u, 1.f - v));

						// Where along the chain this point is: s in [0, segs],
						// bone k owning [k, k+1] and the last bone everything
						// past the last joint. The nearest segment decides.
						f32 bestD = 1e30f, s = 0.f;
						for (size_t k = 0; k < segs; k++)
						{
							const Vec3 ab = joints[k + 1] - joints[k];
							const f32 l2 = std::max(1e-8f, ab.dotProduct(ab));
							f32 t = (p - joints[k]).dotProduct(ab) / l2;
							const f32 tc = std::max(0.f, std::min(1.f, t));
							const f32 d = (joints[k] + ab * tc - p).magnitudeSQR();
							// Past the ends the first and last segments extend.
							if (k > 0 && t < 0.f) t = 0.f;
							if (k + 1 < segs && t > 1.f) t = 1.f;
							if (d < bestD) { bestD = d; s = (f32)k + t; }
						}
						// The joint nearest s, and how far across its blend this is.
						int j = (int)std::floor(s + 0.5f);
						j = std::max(1, std::min((int)segs, j));
						const f32 e = std::min(1.f, std::max(0.f, (s - (f32)j + blend) / (2.f * blend)));
						const f32 wb = e * e * (3.f - 2.f * e);
						tBonesID.push_back(Vec4((f32)(j - 1), (f32)j, 0.f, 0.f));
						tBonesWeight.push_back(Vec4(1.f - wb, wb, 0.f, 0.f));
					}
				for (int yi = 0; yi < n; yi++)
					for (int xi = 0; xi < n; xi++)
					{
						const uint32 a = yi * (n + 1) + xi, b = a + 1, c = a + (n + 1), d = c + 1;
						index.push_back(a); index.push_back(b); index.push_back(d);
						index.push_back(d); index.push_back(c); index.push_back(a);
					}
				// Skinning slot k is chain bone k; each offset takes a rest-space
				// vertex into that bone's own frame.
				for (size_t k = 0; k < chain.size(); k++)
				{
					MapBoneIDs[chain[k]] = (int32)k;
					BoneOffsetMatrix[chain[k]] = bind[chain[k]].Inverse();
				}

				CreateBuffers();
				SendBuffers();

				materialProperties.haveColor = true;
				materialProperties.haveBones = true;
				materialProperties.haveSpecular = false;
				materialProperties.haveColorMap = false;
				materialProperties.haveSpecularMap = false;
				materialProperties.haveNormalMap = false;
				materialProperties.Color = Vec4(1.f, 1.f, 1.f, 1.f);
			}
		};

		// The character's renderable: one mesh per part, in part order, so
		// mesh i always corresponds to part i. RenderingComponent relies on
		// that to drive each mesh from its own bone.
		class SpriteRigRenderable : public Renderable {
		public:
			void AddQuad(const f32 halfW, const f32 halfH)
			{
				Geometries.push_back(new QuadGeometry(halfW, halfH));
			}
			void AddBent(IGeometry* g) { Geometries.push_back(g); }
			void Finish() { CalculateBounding(); }
		};

		// Model-space rest transform of every bone, composed up the parents.
		std::vector<Matrix> BindGlobals(const std::vector<Bone> &bones)
		{
			std::vector<Matrix> g(bones.size());
			for (size_t i = 0; i < bones.size(); i++)
			{
				const int32 p = bones[i].parent;
				g[i] = (p >= 0 && (size_t)p < i) ? g[p] * bones[i].bindPoseMat : bones[i].bindPoseMat;
			}
			return g;
		}

	}

	SpriteRig2DBuild BuildSpriteRig2D(const std::vector<SpritePart2D> &parts,
		const std::function<std::string(const std::string&)> &resolve,
		const std::vector<Bone>* skeleton)
	{
		SpriteRig2DBuild out;
		std::shared_ptr<SpriteRigRenderable> r = std::make_shared<SpriteRigRenderable>();
		const std::vector<Matrix> bind = skeleton ? BindGlobals(*skeleton) : std::vector<Matrix>();
		const auto boneId = [&](const std::string &name) -> int32 {
			if (!skeleton) return -1;
			for (size_t b = 0; b < skeleton->size(); b++) if ((*skeleton)[b].name == name) return (int32)b;
			return -1;
		};

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
			// The chain from the part's bone down to its bend bone, root first,
			// at most four bones (the skinning attribute's width).
			std::vector<int32> chain;
			const int32 ba = parts[i].bendBone.empty() ? -1 : boneId(parts[i].bone);
			const int32 bb = parts[i].bendBone.empty() ? -1 : boneId(parts[i].bendBone);
			if (ba >= 0 && bb >= 0 && ba != bb)
			{
				for (int32 w = bb, guard = 0; w >= 0 && guard <= (int32)skeleton->size(); guard++)
				{
					chain.insert(chain.begin(), w);
					if (w == ba) break;
					w = (*skeleton)[w].parent;
				}
				if (chain.empty() || chain[0] != ba) chain.clear();                 // not below the part's bone
				else if (chain.size() > 4) chain.erase(chain.begin() + 1, chain.end() - 3);   // keep the ends
			}
			const bool bent = chain.size() >= 2;
			if (bent) r->AddBent(new BentQuadGeometry(aspect, 1.f, parts[i], chain, bind));
			else r->AddQuad(aspect, 1.f);
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
			if (bent) usage |= ShaderUsage::Skinning;
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
